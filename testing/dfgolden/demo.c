/*
 * demo.c - dm_68 demo reader for dfgolden (see demo.h)
 *
 * Mirrors CL_ReadDemoMessage / CL_ParseServerMessage / CL_ParseGamestate /
 * CL_ParseSnapshot of the engine, minus everything that is not needed to get
 * the playerState of each snapshot.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#include "demo.h"
#include "../common/enginestubs.h"

/* engine errors (Com_Error in enginestubs.c) and our own Fail() both come back as a parse failure */
static jmp_buf	demoJmp;
#define demoErr	Stub_ErrorMsg

/* ---- parser state ---- */

#define SOLID_BMODEL		0xffffff
#define MAX_RING_ENTITIES	1024

typedef struct {
	int				seq;
	int				valid;
	playerState_t	ps;
	entityState_t	*ents;			/* the snapshot's entity states, for later deltas */
	int				numEnts;
} ringEntry_t;

typedef struct {
	demo_t			*d;
	ringEntry_t		ring[PACKET_BACKUP];
	entityState_t	*baselines;		/* MAX_GENTITIES, zero where the server sent none */
	int				lastSeq;		/* sequence of the previous message */
	int				lastSnapSeq;	/* sequence of the previous snapshot */
	int				haveSnap;
	char			bcs[BIG_INFO_STRING];
	int				bcsIndex;
} parse_t;

static void Fail( const char *fmt, ... ) {
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( demoErr, sizeof( demoErr ), fmt, ap );
	va_end( ap );
	longjmp( demoJmp, 1 );
}

static void SetConfigstring( parse_t *p, int index, const char *text ) {
	demo_t *d = p->d;

	if ( index != DEMO_CS_SERVERINFO && index != DEMO_CS_SYSTEMINFO ) {
		return;
	}
	if ( index == DEMO_CS_SERVERINFO ) {
		Q_strncpyz( d->serverinfoNow, text, sizeof( d->serverinfoNow ) );
	} else {
		Q_strncpyz( d->systeminfoNow, text, sizeof( d->systeminfoNow ) );
	}
	if ( d->numCsChanges < DEMO_MAX_CSCHANGES ) {
		d->csChange[d->numCsChanges].snap = d->numSnaps;
		d->csChange[d->numCsChanges].index = index;
		d->numCsChanges++;
	}
}

/* returns the text between the first and the last double quote, or NULL */
static char *QuotedText( char *s ) {
	char *a = strchr( s, '"' );
	char *b = strrchr( s, '"' );

	if ( !a || b <= a ) {
		return NULL;
	}
	*b = '\0';
	return a + 1;
}

static void ParseCommand( parse_t *p, const char *cmd ) {
	char	buf[BIG_INFO_STRING + 64];
	char	*q;
	int		index;

	Q_strncpyz( buf, cmd, sizeof( buf ) );

	if ( !strncmp( buf, "cs ", 3 ) ) {
		index = atoi( buf + 3 );
		q = QuotedText( buf );
		SetConfigstring( p, index, q ? q : "" );
	} else if ( !strncmp( buf, "bcs0 ", 5 ) || !strncmp( buf, "bcs1 ", 5 ) || !strncmp( buf, "bcs2 ", 5 ) ) {
		int part = buf[3] - '0';

		index = atoi( buf + 5 );
		q = QuotedText( buf );
		if ( !q ) {
			q = (char *)"";
		}
		if ( part == 0 ) {
			p->bcs[0] = '\0';
			p->bcsIndex = index;
		}
		if ( strlen( p->bcs ) + strlen( q ) < sizeof( p->bcs ) ) {
			strcat( p->bcs, q );
		}
		if ( part == 2 ) {
			SetConfigstring( p, p->bcsIndex, p->bcs );
		}
	}
}

static void ParseGamestate( parse_t *p, msg_t *msg ) {
	demo_t			*d = p->d;
	entityState_t	nullstate, es;
	int				cmd, i, num;
	const char		*s;

	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	d->numGamestates++;

	MSG_ReadLong( msg );		/* server command sequence */
	for ( ;; ) {
		cmd = MSG_ReadByte( msg );
		if ( cmd == svc_EOF ) {
			break;
		}
		if ( cmd == svc_configstring ) {
			i = MSG_ReadShort( msg );
			if ( i < 0 || i >= MAX_CONFIGSTRINGS ) {
				Fail( "gamestate: configstring %d out of range", i );
			}
			s = MSG_ReadBigString( msg );
			if ( d->numGamestates == 1 ) {
				if ( i == DEMO_CS_SERVERINFO ) {
					Q_strncpyz( d->serverinfo, s, sizeof( d->serverinfo ) );
					Q_strncpyz( d->serverinfoNow, s, sizeof( d->serverinfoNow ) );
				} else if ( i == DEMO_CS_SYSTEMINFO ) {
					Q_strncpyz( d->systeminfo, s, sizeof( d->systeminfo ) );
					Q_strncpyz( d->systeminfoNow, s, sizeof( d->systeminfoNow ) );
				}
			}
		} else if ( cmd == svc_baseline ) {
			num = MSG_ReadEntitynum( msg );
			if ( num < 0 || num >= MAX_GENTITIES ) {
				Fail( "gamestate: baseline %d out of range", num );
			}
			MSG_ReadDeltaEntity( msg, &nullstate, &es, num );
			p->baselines[num] = es;
		} else {
			Fail( "gamestate: bad command byte %d", cmd );
		}
	}
	d->clientNum = MSG_ReadLong( msg );
	MSG_ReadLong( msg );		/* checksum feed */
}

/* CL_ParsePacketEntities / CL_DeltaEntity: the entity list of a snapshot is a delta against the
   list of the snapshot it was built from (or the baselines) */
static void DeltaEntity( parse_t *p, msg_t *msg, ringEntry_t *cur, int newnum, const entityState_t *old, int unchanged ) {
	entityState_t *state = &cur->ents[cur->numEnts];

	if ( cur->numEnts >= MAX_RING_ENTITIES ) {
		Fail( "snapshot: too many entities" );
	}
	if ( unchanged ) {
		*state = *old;
	} else {
		MSG_ReadDeltaEntity( msg, old, state, newnum );
	}
	if ( state->number == MAX_GENTITIES - 1 ) {
		return;		/* delta removed */
	}
	cur->numEnts++;
}

static void ParseEntities( parse_t *p, msg_t *msg, const ringEntry_t *oldf, ringEntry_t *cur ) {
	const entityState_t	*oldstate = NULL;
	int					newnum, oldindex = 0, oldnum;

	cur->numEnts = 0;
	if ( !oldf || oldindex >= oldf->numEnts ) {
		oldnum = MAX_GENTITIES + 1;
	} else {
		oldstate = &oldf->ents[oldindex];
		oldnum = oldstate->number;
	}

#define NEXT_OLD() 	do { 		oldindex++; 		if ( !oldf || oldindex >= oldf->numEnts ) { 			oldnum = MAX_GENTITIES + 1; 		} else { 			oldstate = &oldf->ents[oldindex]; 			oldnum = oldstate->number; 		} 	} while ( 0 )

	for ( ;; ) {
		newnum = MSG_ReadEntitynum( msg );
		if ( newnum < 0 ) {
			Fail( "snapshot: end of message in entities" );
		}
		if ( newnum == MAX_GENTITIES - 1 ) {
			break;
		}
		while ( oldnum < newnum ) {
			DeltaEntity( p, msg, cur, oldnum, oldstate, 1 );
			NEXT_OLD();
		}
		if ( oldnum == newnum ) {
			DeltaEntity( p, msg, cur, newnum, oldstate, 0 );
			NEXT_OLD();
			continue;
		}
		if ( oldnum > newnum ) {
			DeltaEntity( p, msg, cur, newnum, &p->baselines[newnum], 0 );
		}
	}
	while ( oldnum != MAX_GENTITIES + 1 ) {
		DeltaEntity( p, msg, cur, oldnum, oldstate, 1 );
		NEXT_OLD();
	}
#undef NEXT_OLD
}

int gKeepFrom, gKeepTo;

/* diagnostics: remember all entity states of a snapshot */
static void KeepEntities( demo_t *d, const ringEntry_t *cur, int snapIndex ) {
	int n = gKeepTo - gKeepFrom;

	if ( snapIndex < gKeepFrom || snapIndex >= gKeepTo ) {
		return;
	}
	if ( !d->keepStart ) {
		d->keepStart = (int *)calloc( n, sizeof( int ) );
		d->keepCount = (int *)calloc( n, sizeof( int ) );
	}
	if ( d->numKeep + cur->numEnts > d->capKeep ) {
		d->capKeep = ( d->numKeep + cur->numEnts ) * 2 + 1024;
		d->keepEnts = (entityState_t *)realloc( d->keepEnts, d->capKeep * sizeof( entityState_t ) );
	}
	d->keepStart[snapIndex - gKeepFrom] = d->numKeep;
	d->keepCount[snapIndex - gKeepFrom] = cur->numEnts;
	memcpy( &d->keepEnts[d->numKeep], cur->ents, cur->numEnts * sizeof( entityState_t ) );
	d->numKeep += cur->numEnts;
}

/* remembers the solid brush models of a snapshot; a list identical to the previous one is shared */
static void StoreBmodels( demo_t *d, const ringEntry_t *cur, int snapIndex ) {
	demoBmodel_t	tmp[MAX_RING_ENTITIES];
	int				i, n = 0;
	const demoBmList_t *last = d->numBmLists ? &d->bmLists[d->numBmLists - 1] : NULL;

	for ( i = 0; i < cur->numEnts; i++ ) {
		const entityState_t *e = &cur->ents[i];

		if ( e->solid == SOLID_BMODEL ) {
			tmp[n].number = e->number;
			tmp[n].modelindex = e->modelindex;
			tmp[n].pos = e->pos;
			tmp[n].apos = e->apos;
			n++;
		}
	}
	if ( last && last->count == n && !memcmp( &d->bmodels[last->start], tmp, n * sizeof( tmp[0] ) ) ) {
		d->snaps[snapIndex].bmList = d->numBmLists - 1;
		return;
	}
	if ( d->numBmodels + n > d->capBmodels ) {
		d->capBmodels = ( d->numBmodels + n ) * 2 + 256;
		d->bmodels = (demoBmodel_t *)realloc( d->bmodels, d->capBmodels * sizeof( demoBmodel_t ) );
	}
	if ( d->numBmLists == d->capBmLists ) {
		d->capBmLists = d->capBmLists ? d->capBmLists * 2 : 256;
		d->bmLists = (demoBmList_t *)realloc( d->bmLists, d->capBmLists * sizeof( demoBmList_t ) );
	}
	memcpy( &d->bmodels[d->numBmodels], tmp, n * sizeof( tmp[0] ) );
	d->bmLists[d->numBmLists].start = d->numBmodels;
	d->bmLists[d->numBmLists].count = n;
	d->numBmodels += n;
	d->snaps[snapIndex].bmList = d->numBmLists++;
}

static void ParseSnapshot( parse_t *p, msg_t *msg, int seq, int cmdCount ) {
	demo_t			*d = p->d;
	demoSnap_t		*sn;
	ringEntry_t		*old = NULL;
	int				deltaByte, areabytes;
	byte			areamask[MAX_MAP_AREA_BYTES];

	if ( d->numSnaps == d->capSnaps ) {
		d->capSnaps = d->capSnaps ? d->capSnaps * 2 : 4096;
		d->snaps = (demoSnap_t *)realloc( d->snaps, d->capSnaps * sizeof( demoSnap_t ) );
		if ( !d->snaps ) {
			Fail( "out of memory" );
		}
	}
	sn = &d->snaps[d->numSnaps];
	Com_Memset( sn, 0, sizeof( *sn ) );

	sn->seq = seq;
	sn->serverTime = MSG_ReadLong( msg );
	deltaByte = MSG_ReadByte( msg );
	sn->deltaNum = deltaByte ? seq - deltaByte : -1;
	sn->snapFlags = MSG_ReadByte( msg );
	sn->cmdCount = cmdCount;
	sn->csChanges = d->numCsChanges;

	areabytes = MSG_ReadByte( msg );
	if ( areabytes > (int)sizeof( areamask ) ) {
		Fail( "snapshot: bad areamask size %d", areabytes );
	}
	MSG_ReadData( msg, areamask, areabytes );

	if ( sn->deltaNum > 0 ) {
		old = &p->ring[sn->deltaNum & PACKET_MASK];
		if ( !old->valid || old->seq != sn->deltaNum ) {
			old = NULL;
			sn->badDelta = 1;
			d->badDeltas++;
		}
	}
	MSG_ReadDeltaPlayerstate( msg, old ? &old->ps : NULL, &sn->ps );

	{
		ringEntry_t *slot = &p->ring[seq & PACKET_MASK];

		if ( !slot->ents ) {
			slot->ents = (entityState_t *)calloc( MAX_RING_ENTITIES, sizeof( entityState_t ) );
		}
		/* the entities are parsed into the slot's array, but the slot only becomes a valid delta
		   source below; the old frame lives in another slot (its sequence differs by 1..31) */
		ParseEntities( p, msg, old, slot );
	}

	/* the server sends full snapshots until the client's first ack arrives;
	   that startup window is normal, a full snapshot after it is not */
	if ( sn->deltaNum > 0 && d->firstDeltaSnap < 0 ) {
		d->firstDeltaSnap = d->numSnaps;
	}
	if ( p->haveSnap ) {
		if ( sn->deltaNum <= 0 && d->firstDeltaSnap >= 0 ) {
			d->fullSnaps++;
		}
		if ( seq <= p->lastSnapSeq ) {
			d->seqOrder++;
		} else if ( seq != p->lastSnapSeq + 1 ) {
			if ( !d->seqGaps ) {
				d->firstSeqGapSnap = d->numSnaps;
			}
			d->seqGaps++;
		}
	} else if ( p->lastSeq >= 0 && seq != p->lastSeq + 1 ) {
		d->gapBeforeFirstSnap = seq - p->lastSeq - 1;
	}

	/* keep it as a delta source (the engine only does so for valid snapshots) */
	if ( !sn->badDelta ) {
		p->ring[seq & PACKET_MASK].seq = seq;
		p->ring[seq & PACKET_MASK].valid = 1;
		p->ring[seq & PACKET_MASK].ps = sn->ps;
		StoreBmodels( d, &p->ring[seq & PACKET_MASK], d->numSnaps );
		KeepEntities( d, &p->ring[seq & PACKET_MASK], d->numSnaps );
	}

	p->haveSnap = 1;
	p->lastSnapSeq = seq;
	d->numSnaps++;
}

static void ParseMessage( parse_t *p, byte *data, int len, int seq ) {
	demo_t	*d = p->d;
	msg_t	msg;
	int		cmd, cmdCount = 0, snaps = 0;

	MSG_Init( &msg, data, MAX_MSGLEN );
	msg.cursize = len;
	msg.readcount = 0;
	MSG_Bitstream( &msg );

	MSG_ReadLong( &msg );		/* reliable acknowledge */

	for ( ;; ) {
		if ( msg.readcount > msg.cursize ) {
			Fail( "read past end of message" );
		}
		cmd = MSG_ReadByte( &msg );
		if ( cmd == svc_EOF ) {
			break;
		}
		switch ( cmd ) {
		case svc_nop:
			break;
		case svc_serverCommand:
			MSG_ReadLong( &msg );
			ParseCommand( p, MSG_ReadString( &msg ) );
			cmdCount++;
			break;
		case svc_gamestate:
			ParseGamestate( p, &msg );
			break;
		case svc_snapshot:
			ParseSnapshot( p, &msg, seq, cmdCount );
			snaps++;
			/* entities follow; the message is length framed, so skip them */
			goto done;
		default:
			d->unknownSvc++;
			Fail( "unknown server message %d", cmd );
		}
	}
done:
	if ( snaps > 1 ) {
		d->multiSnapMsgs++;
	}
	if ( !snaps && d->numGamestates && p->haveSnap ) {
		d->msgsWithoutSnap++;
	}
}

int Demo_Load( const char *path, demo_t *d ) {
	static byte		buf[MAX_MSGLEN_BUF];
	FILE			*f;
	parse_t			*p;
	int				seq, len;

	Com_Memset( d, 0, sizeof( *d ) );
	d->firstSeqGapSnap = -1;
	d->firstDeltaSnap = -1;

	f = fopen( path, "rb" );
	if ( !f ) {
		return -1;
	}

	p = (parse_t *)calloc( 1, sizeof( *p ) );
	p->d = d;
	p->baselines = (entityState_t *)calloc( MAX_GENTITIES, sizeof( entityState_t ) );
	p->lastSeq = -1;

	Stub_ErrorJmp = &demoJmp;
	if ( setjmp( demoJmp ) ) {
		Q_strncpyz( d->err, demoErr, sizeof( d->err ) );
		d->errMsg = d->numMsgs;
		goto out;
	}

	for ( ;; ) {
		if ( fread( &seq, 4, 1, f ) != 1 ) {
			break;
		}
		if ( fread( &len, 4, 1, f ) != 1 ) {
			Fail( "truncated message header" );
		}
		if ( len == -1 ) {
			d->endMarker = 1;
			break;
		}
		if ( len < 0 || len > MAX_MSGLEN ) {
			Fail( "bad message length %d", len );
		}
		memset( buf, 0, sizeof( buf ) );
		if ( (int)fread( buf, 1, len, f ) != len ) {
			Fail( "truncated message body" );
		}
		ParseMessage( p, buf, len, seq );
		p->lastSeq = seq;
		d->numMsgs++;
	}

out:
	Stub_ErrorJmp = NULL;
	{
		int i;

		for ( i = 0; i < PACKET_BACKUP; i++ ) {
			free( p->ring[i].ents );
		}
	}
	free( p->baselines );
	free( p );
	fclose( f );
	return 0;
}

void Demo_Free( demo_t *d ) {
	free( d->snaps );
	d->snaps = NULL;
	free( d->bmodels );
	d->bmodels = NULL;
	free( d->bmLists );
	d->bmLists = NULL;
	free( d->keepEnts );
	free( d->keepStart );
	free( d->keepCount );
	d->keepEnts = NULL;
	d->keepStart = d->keepCount = NULL;
}
