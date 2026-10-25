/*
 * steps.c - per-step physics check against a demo
 *
 *   dfgolden steps --qvm pmove.qvm (--bsp map.bsp | --maps dir) [-v] [--max N] <demo> ...
 *
 * For every pair of consecutive regular snapshots (A, B) of a demo the state of A is fed to the
 * QVM's Pmove() together with each candidate usercmd, and the result is compared to B. The usercmd
 * angles are fixed by B's view angles; the candidates differ in forward/right/up move. A pair
 * "matches" when some candidate reproduces B's movement state exactly.
 *
 * Every pair is tested on its own, starting from the recorded state of A, so a deviation cannot
 * grow into later steps: the report points at the exact snapshot and field.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include "demo.h"
#include "steps.h"
#include "surfaceflags.h"
#include "../host/pmhost.h"

#define EF_TELEPORT_BIT		0x00000004
#define PMF_FOLLOW			4096
#define PM_NORMAL			0
#define POS_JUMP_UNITS		64.0f

#define MASK_PLAYERSOLID	( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY )

typedef struct {
	const char	*qvm;
	const char	*bsp;
	const char	*mapsDir;
	int			verbose;
	int			deep;
	const char	*timesFile;		/* write the mismatch list with demo timestamps here */
	int			showExternal;	/* also list the misses explained as server-side effects */
	int			noEntities;		/* world only, like the first version */
	int			entTimeB;		/* evaluate mover trajectories at B's server time instead of A's */
	int			noPromodeParam;	/* hand Pmove() promode 0, as client prediction does */
	int			deepMax;	/* deep searches per demo */
	int			maxReport;
} stepOpts_t;

static void InfoVal( const char *info, const char *key, char *out, int size ) {
	Q_strncpyz( out, Info_ValueForKey( info, key ), size );
}

static void Cfg( const demo_t *d, const char *key, char *out, int size ) {
	InfoVal( d->systeminfo, key, out, size );
	if ( !out[0] ) {
		InfoVal( d->serverinfo, key, out, size );
	}
}

static const char *Basename_( const char *path ) {
	const char *s = strrchr( path, '/' );
	const char *t = strrchr( path, '\\' );

	if ( t > s ) {
		s = t;
	}
	return s ? s + 1 : path;
}

static unsigned char *ReadFileAll( const char *path, int *length ) {
	FILE *f = fopen( path, "rb" );
	unsigned char *buf;
	long n;

	if ( !f ) {
		return NULL;
	}
	fseek( f, 0, SEEK_END );
	n = ftell( f );
	fseek( f, 0, SEEK_SET );
	buf = (unsigned char *)malloc( n + 16 );
	if ( fread( buf, 1, n, f ) != (size_t)n ) {
		fclose( f );
		free( buf );
		return NULL;
	}
	fclose( f );
	*length = (int)n;
	return buf;
}

/* fields compared after a step; float fields by value so that -0 equals 0 (the network sends
   whole numbers as integers) */
typedef struct {
	const char	*name;
	int			offset;
	int			count;		/* number of elements */
	int			isFloat;
	int			mod16;		/* networked as 16 bits: compare modulo 65536 */
} cmpField_t;

#define F_INT( n, c )	{ #n, (int)offsetof( playerState_t, n ), c, 0, 0 }
#define F_INT16( n, c )	{ #n, (int)offsetof( playerState_t, n ), c, 0, 1 }
#define F_FLT( n, c )	{ #n, (int)offsetof( playerState_t, n ), c, 1, 0 }

static const cmpField_t cmpFields[] = {
	F_INT( commandTime, 1 ),
	F_INT( pm_type, 1 ),
	F_INT( pm_flags, 1 ),
	F_INT( pm_time, 1 ),
	F_FLT( origin, 3 ),
	F_FLT( velocity, 3 ),
	F_FLT( viewangles, 3 ),
	F_INT( groundEntityNum, 1 ),
	F_INT( viewheight, 1 ),
	F_INT( movementDir, 1 ),
	F_INT16( delta_angles, 3 ),
	F_INT( gravity, 1 ),
	F_INT( speed, 1 ),
	F_INT( stats[13], 1 ),		/* STAT_DEFRAG_INPUT: the keys of the command */
};

static int			gUmExpanded;
static refineReq_t	*gRefine;
static int			gRefineNum;

int				gRecoverDeep;
recoveredCmd_t	*gRecovered;
int				gRecoveredNum;
int				gRecoverWanted;

static int gUlps;		/* tolerated distance of float fields, in representable floats */

/* origin-only matching for a step the server changed after Pmove (knockback, damage, jump pad): the velocity,
   the knockback time and the knockback flag are not compared */
static int gSkipVel;

static int FloatsClose( float a, float b ) {
	int ia, ib;

	if ( a == b ) {
		return 1;
	}
	if ( !gUlps ) {
		return 0;
	}
	memcpy( &ia, &a, 4 );
	memcpy( &ib, &b, 4 );
	/* map the sign-magnitude ordering of IEEE floats onto a monotonic integer line */
	if ( ia < 0 ) ia = (int)0x80000000 - ia;
	if ( ib < 0 ) ib = (int)0x80000000 - ib;
	return abs( ia - ib ) <= gUlps;
}

/* number of differing fields; the first few are described in text when it is not NULL */
static int DiffState( const playerState_t *got, const playerState_t *want, char *text, int textSize, int *mask ) {
	int i, k, diffs = 0;

	if ( mask ) {
		*mask = 0;
	}

	if ( text ) {
		text[0] = '\0';
	}
	for ( i = 0; i < (int)( sizeof( cmpFields ) / sizeof( cmpFields[0] ) ); i++ ) {
		const cmpField_t *f = &cmpFields[i];

		if ( gSkipVel && ( !strcmp( f->name, "velocity" ) || !strcmp( f->name, "pm_time" ) ) ) {
			continue;
		}
		for ( k = 0; k < f->count; k++ ) {
			char buf[128];
			int differ;

			if ( f->isFloat ) {
				float a = ( (const float *)( (const char *)got + f->offset ) )[k];
				float b = ( (const float *)( (const char *)want + f->offset ) )[k];

				differ = !FloatsClose( a, b );
				if ( differ && text && diffs < 6 ) {
					Com_sprintf( buf, sizeof( buf ), " %s[%d] want %.9g got %.9g;", f->name, k, b, a );
				}
			} else {
				int a = ( (const int *)( (const char *)got + f->offset ) )[k];
				int b = ( (const int *)( (const char *)want + f->offset ) )[k];

				if ( f->mod16 ) {
					a &= 65535;
					b &= 65535;
				}
				if ( gSkipVel && !strcmp( f->name, "pm_flags" ) ) {
					a &= ~64;
					b &= ~64;
				}
				differ = a != b;
				if ( differ && text && diffs < 6 ) {
					Com_sprintf( buf, sizeof( buf ), " %s[%d] want %d got %d;", f->name, k, b, a );
				}
			}
			if ( differ ) {
				if ( mask ) {
					*mask |= 1 << i;
				}
				if ( text && diffs < 6 ) {
					Q_strcat( text, textSize, buf );
				}
				diffs++;
			}
		}
	}
	return diffs;
}

/* the same notion of a regular pair as `dfgolden check` */
static int RegularPair( const demo_t *d, int i, int step ) {
	const playerState_t *a = &d->snaps[i - 1].ps;
	const playerState_t *b = &d->snaps[i].ps;
	vec3_t delta;

	if ( d->snaps[i].serverTime - d->snaps[i - 1].serverTime != step ) {
		return 0;
	}
	if ( ( a->eFlags ^ b->eFlags ) & EF_TELEPORT_BIT ) {
		return 0;
	}
	if ( a->pm_type != b->pm_type || a->pm_type != PM_NORMAL ) {
		return 0;
	}
	if ( a->clientNum != b->clientNum || b->clientNum != d->clientNum ) {
		return 0;
	}
	if ( ( a->pm_flags | b->pm_flags ) & PMF_FOLLOW ) {
		return 0;
	}
	if ( b->commandTime - a->commandTime != step ) {
		return 0;
	}
	VectorSubtract( b->origin, a->origin, delta );
	return VectorLength( delta ) <= POS_JUMP_UNITS;
}

#define SIG_MAX	( 1 << 12 )

/* names of the fields set in a difference mask */
static const char *SigNames( int mask ) {
	static char buf[256];
	int i;

	buf[0] = '\0';
	for ( i = 0; i < (int)( sizeof( cmpFields ) / sizeof( cmpFields[0] ) ); i++ ) {
		if ( mask & ( 1 << i ) ) {
			Q_strcat( buf, sizeof( buf ), " " );
			Q_strcat( buf, sizeof( buf ), cmpFields[i].name );
		}
	}
	return buf;
}

/* how far a result is from the wanted state; 0 means an exact match. Position counts per
   1/1000 unit, velocity per unit, flag / ground differences are heavy */
static double Metric( const playerState_t *got, const playerState_t *want ) {
	double m = 0;
	int k;

	for ( k = 0; k < 3; k++ ) {
		m += fabs( (double)got->origin[k] - want->origin[k] ) * 1000.0;
		if ( !gSkipVel ) {
			m += fabs( (double)got->velocity[k] - want->velocity[k] );
		}
	}
	if ( gSkipVel ) {
		m += ( ( got->pm_flags ^ want->pm_flags ) & ~64 ) ? 100.0 : 0.0;
	} else {
		m += ( got->pm_flags != want->pm_flags ) * 100.0;
		m += abs( got->pm_time - want->pm_time ) * 10.0;
	}
	m += ( got->groundEntityNum != want->groundEntityNum ) * 100.0;
	m += ( got->stats[13] != want->stats[13] ) * 100.0;
	m += ( got->movementDir != want->movementDir ) * 10.0;
	return m;
}

static double lastMetric;

/* the range each move value can have in the step being searched: the input word the game publishes (stats[13])
   holds the sign of every move value, so a key that was not down means exactly 0 and a key that was down means
   any value of that sign (a key released or pressed part way through a frame gives a fraction of full speed) */
static int gRangeOn, gLo[3], gHi[3];

static void AxisRange( int input, int posBit, int negBit, int axis ) {
	if ( input & posBit ) {
		gLo[axis] = 1;
		gHi[axis] = 127;
	} else if ( input & negBit ) {
		gLo[axis] = -127;
		gHi[axis] = -1;
	} else {
		gLo[axis] = gHi[axis] = 0;
	}
}

/* the usercmd of the step A -> B: the command time and view angles follow from B */
void Steps_BuildCmd( usercmd_t *cmd, const playerState_t *A, const playerState_t *B, int buttons, int fm, int rm,
					int um, int weapon ) {
	int k;

	memset( cmd, 0, sizeof( *cmd ) );
	cmd->serverTime = B->commandTime;
	for ( k = 0; k < 3; k++ ) {
		cmd->angles[k] = ANGLE2SHORT( B->viewangles[k] ) - A->delta_angles[k];
	}
	/* PM_UpdateViewAngles clamps the pitch to +-16000 and then rewrites delta_angles so that
	   cmd + delta = the clamp; the raw command pitch is recoverable from the new delta_angles */
	if ( B->viewangles[PITCH] == SHORT2ANGLE( 16000 ) ) {
		cmd->angles[PITCH] = ( 16000 - B->delta_angles[PITCH] ) & 65535;
	} else if ( B->viewangles[PITCH] == SHORT2ANGLE( -16000 ) ) {
		cmd->angles[PITCH] = ( -16000 - B->delta_angles[PITCH] ) & 65535;
	}
	cmd->buttons = buttons;
	cmd->weapon = weapon;
	cmd->forwardmove = fm;
	cmd->rightmove = rm;
	cmd->upmove = um;
}

/* runs Pmove() from A's state with one candidate command and compares the result to B;
   returns -1 on a host error */
static int TryCmdText( pmhost_t *host, const pmParams_t *pp, const playerState_t *A, const playerState_t *B,
						int btn, signed char fm, signed char rm, signed char um, char *text, int textSize,
						int *mask ) {
	playerState_t ps = *A;
	usercmd_t cmd;
	int diffs;

	/* ClientThink_real writes speed (haste) and gravity into the playerState right before Pmove() */
	ps.speed = B->speed;
	ps.gravity = B->gravity;

	/* BUTTON_ATTACK keeps PMF_RESPAWNED set */
	Steps_BuildCmd( &cmd, A, B, btn ? 1 : 0, fm, rm, um, 0 );

	if ( PMHost_Step( host, &ps, &cmd, pp, NULL ) ) {
		return -1;
	}
	diffs = DiffState( &ps, B, text, textSize, mask );
	lastMetric = Metric( &ps, B );
	return diffs;
}

static int TryCmd( pmhost_t *host, const pmParams_t *pp, const playerState_t *A, const playerState_t *B,
					int btn, signed char fm, signed char rm, signed char um, int *diffs ) {
	int mask, r;

	r = TryCmdText( host, pp, A, B, btn, fm, rm, um, NULL, 0, &mask );
	if ( r < 0 ) {
		return -1;
	}
	*diffs = r;
	return 0;
}

/* Pmove() from A with a command; the resulting state */
static int RunPm( pmhost_t *host, const pmParams_t *pp, const playerState_t *A, const playerState_t *B, int btn,
					signed char fm, signed char rm, signed char um, playerState_t *out ) {
	usercmd_t cmd;

	*out = *A;
	out->speed = B->speed;
	out->gravity = B->gravity;
	Steps_BuildCmd( &cmd, A, B, btn ? 1 : 0, fm, rm, um, 0 );
	return PMHost_Step( host, out, &cmd, pp, NULL ) ? -1 : 0;
}

typedef struct {
	int		btn, fm, rm, um;
	double	metric;
} cand_t;

/* coordinate search on forward / right / up with shrinking steps, minimising Metric();
   returns 1 on an exact match, 0 otherwise, -1 on a host error */
static int HillClimb( pmhost_t *host, const pmParams_t *pp, const playerState_t *A, const playerState_t *B,
						int btn, int cur[3], int *outDiff, signed char outMove[3], double *outMetric ) {
	double curM;
	int s2, improved, axis, dir, iter = 0, diffs;

	if ( TryCmd( host, pp, A, B, btn, (signed char)cur[0], (signed char)cur[1], (signed char)cur[2], &diffs ) ) {
		return -1;
	}
	curM = lastMetric;
	*outDiff = diffs;
	for ( s2 = 32; s2 >= 1 && curM > 0; s2 /= 2 ) {
		improved = 1;
		while ( improved && iter < 200 ) {
			improved = 0;
			for ( axis = 0; axis < 3; axis++ ) {
				for ( dir = -1; dir <= 1; dir += 2 ) {
					int cand[3];

					cand[0] = cur[0]; cand[1] = cur[1]; cand[2] = cur[2];
					cand[axis] += dir * s2;
					if ( cand[axis] < -127 || cand[axis] > 127 ) {
						continue;
					}
					if ( gRangeOn && ( cand[axis] < gLo[axis] || cand[axis] > gHi[axis] ) ) {
						continue;
					}
					iter++;
					if ( TryCmd( host, pp, A, B, btn, (signed char)cand[0], (signed char)cand[1], (signed char)cand[2], &diffs ) ) {
						return -1;
					}
					if ( diffs == 0 ) {
						outMove[0] = (signed char)cand[0]; outMove[1] = (signed char)cand[1]; outMove[2] = (signed char)cand[2];
						*outDiff = 0;
						*outMetric = 0;
						return 1;
					}
					if ( lastMetric < curM ) {
						curM = lastMetric;
						cur[0] = cand[0]; cur[1] = cand[1]; cur[2] = cand[2];
						*outDiff = diffs;
						improved = 1;
					}
				}
			}
		}
	}
	outMove[0] = (signed char)cur[0]; outMove[1] = (signed char)cur[1]; outMove[2] = (signed char)cur[2];
	*outMetric = curM;
	return 0;
}

/* the keys the player held, from the DeFRaG input word the game publishes in stats[13]
   (PM_UpdateInputState): a pressed key is full speed (127) or walking speed (64) */
static int AxisCandidates( int input, int posBit, int negBit, int fullOnly, signed char out[2] ) {
	if ( input & posBit ) {
		out[0] = 127;
		out[1] = 64;
		return fullOnly ? 1 : 2;
	}
	if ( input & negBit ) {
		out[0] = -127;
		out[1] = -64;
		return fullOnly ? 1 : 2;
	}
	out[0] = 0;
	return 1;
}

/* Things the server does to a player outside Pmove(); a pair that does not match and shows one of
   these is not a physics difference of the movement code */
enum { X_KNOCKBACK = 1, X_FRACVEL = 2, X_JUMPPAD = 4, X_DAMAGE = 8, X_MOVER = 16 };

#define PMF_TIME_KNOCKBACK_BIT	64

static int ExternalReason( const playerState_t *A, const playerState_t *B ) {
	int x = 0, k;

	if ( ( B->pm_flags & PMF_TIME_KNOCKBACK_BIT ) && !( A->pm_flags & PMF_TIME_KNOCKBACK_BIT ) ) {
		x |= X_KNOCKBACK;		/* G_Damage set the knockback time */
	}
	for ( k = 0; k < 3; k++ ) {
		if ( B->velocity[k] != floorf( B->velocity[k] ) ) {
			x |= X_FRACVEL;		/* Pmove ends with SnapVector: only later server code leaves fractions */
		}
	}
	if ( B->jumppad_ent != A->jumppad_ent ) {
		x |= X_JUMPPAD;
	}
	if ( B->damageEvent != A->damageEvent || B->stats[0] < A->stats[0] ) {
		x |= X_DAMAGE;
	}
	/* standing on an entity: a moving platform carries the player (G_MoverPush) after Pmove */
	if ( A->groundEntityNum < ENTITYNUM_MAX_NORMAL || B->groundEntityNum < ENTITYNUM_MAX_NORMAL ) {
		x |= X_MOVER;
	}
	return x;
}

/* ---- list of mismatches with demo timestamps (--times FILE) ---- */

typedef struct {
	int		snap;			/* snapshot index of B */
	int		kind;			/* 'E' external, 'A' ambiguous, 'U' unexplained */
	int		tag;			/* external class bits, or the differing-field mask for 'U' */
} missRec_t;

static FILE		*gTimes;

/* m:ss.mmm, with a leading minus for negative times */
void FormatSignedTime( char *out, int size, int ms ) {
	const char *sign = "";

	if ( ms < 0 ) {
		sign = "-";
		ms = -ms;
	}
	Com_sprintf( out, size, "%s%d:%02d.%03d", sign, ms / 60000, ( ms / 1000 ) % 60, ms % 1000 );
}

#define STAT_FLAGS_RUN_STARTED		4		/* ps.stats[12] bit: toggles on with the start trigger */
#define STAT_FLAGS_FINISHED			8		/* toggles at the finish */
#define PERS_SPAWN_COUNT_INDEX		4

/* time of snapshot `snap` relative to the race timer: seconds since the start trigger while a run is
   active, negative (time until the start trigger) when not. Returns 0 when the demo has no timer start. */
int TimerRelative( const demo_t *d, int snap, int *relMs ) {
	int i, lastStart = -1, active = 0;

	for ( i = 1; i <= snap && i < d->numSnaps; i++ ) {
		const playerState_t *a = &d->snaps[i - 1].ps, *b = &d->snaps[i].ps;
		int changed = a->stats[12] ^ b->stats[12];

		if ( ( changed & STAT_FLAGS_RUN_STARTED ) && ( b->stats[12] & STAT_FLAGS_RUN_STARTED ) ) {
			lastStart = i;
			active = 1;
		} else if ( active && ( ( changed & STAT_FLAGS_FINISHED ) ||
				( ( a->stats[12] & STAT_FLAGS_RUN_STARTED ) && !( b->stats[12] & STAT_FLAGS_RUN_STARTED ) ) ||
				a->persistant[PERS_SPAWN_COUNT_INDEX] != b->persistant[PERS_SPAWN_COUNT_INDEX] ) ) {
			active = 0;
		}
	}
	if ( active ) {
		*relMs = d->snaps[snap].serverTime - d->snaps[lastStart].serverTime;
		return 1;
	}
	/* before a start (or after a finish / respawn): count down to the next start trigger */
	for ( i = snap + 1; i < d->numSnaps; i++ ) {
		const playerState_t *a = &d->snaps[i - 1].ps, *b = &d->snaps[i].ps;

		if ( ( ( a->stats[12] ^ b->stats[12] ) & STAT_FLAGS_RUN_STARTED ) && ( b->stats[12] & STAT_FLAGS_RUN_STARTED ) ) {
			*relMs = d->snaps[snap].serverTime - d->snaps[i].serverTime;
			return 1;
		}
	}
	if ( lastStart >= 0 ) {		/* after the last run ended and no further start: relative to that start */
		*relMs = d->snaps[snap].serverTime - d->snaps[lastStart].serverTime;
		return 1;
	}
	return 0;
}

static const char *ExternalText( int x ) {
	static char buf[128];

	buf[0] = '\0';
	if ( x & X_MOVER ) Q_strcat( buf, sizeof( buf ), " riding-a-moving-platform" );
	if ( x & X_JUMPPAD ) Q_strcat( buf, sizeof( buf ), " jump-pad" );
	if ( x & X_KNOCKBACK ) Q_strcat( buf, sizeof( buf ), " knockback(explosion/hit)" );
	else if ( x & X_DAMAGE ) Q_strcat( buf, sizeof( buf ), " damage" );
	else if ( x & X_FRACVEL ) Q_strcat( buf, sizeof( buf ), " velocity-changed-by-server" );
	return buf;
}

/* writes the misses of one demo as time ranges: consecutive snapshots of the same kind are merged.
   "timer" is the race timer (negative = before the start trigger), "demo" the time since the first snapshot */
static void WriteMissTimes( FILE *f, const demo_t *d, const char *demoName, const missRec_t *recs, int n,
							int showAll ) {
	int i, j, hasTimer, dummy;

	hasTimer = TimerRelative( d, d->numSnaps - 1, &dummy ) ||
		TimerRelative( d, 0, &dummy );
	fprintf( f, "== %s%s\n", demoName, hasTimer ? "" : "   (no timer start in this demo: only demo time)" );
	for ( i = 0; i < n; i = j ) {
		const demoSnap_t *s0, *s1;
		char t0[32], t1[32], d0[32], desc[200];
		int rel0 = 0, rel1 = 0, ok0, ok1;

		if ( !showAll && recs[i].kind != 'U' ) {
			j = i + 1;
			continue;
		}
		for ( j = i + 1; j < n && recs[j].kind == recs[i].kind && recs[j].tag == recs[i].tag &&
				recs[j].snap == recs[j - 1].snap + 1; j++ ) {
		}
		s0 = &d->snaps[recs[i].snap];
		s1 = &d->snaps[recs[j - 1].snap];
		ok0 = TimerRelative( d, recs[i].snap, &rel0 );
		ok1 = TimerRelative( d, recs[j - 1].snap, &rel1 );
		if ( ok0 ) FormatSignedTime( t0, sizeof( t0 ), rel0 ); else Q_strncpyz( t0, "--", sizeof( t0 ) );
		if ( ok1 ) FormatSignedTime( t1, sizeof( t1 ), rel1 ); else Q_strncpyz( t1, "--", sizeof( t1 ) );
		FormatSignedTime( d0, sizeof( d0 ), s0->serverTime - d->snaps[0].serverTime );
		if ( recs[i].kind == 'E' ) {
			Com_sprintf( desc, sizeof( desc ), "server-side:%s", ExternalText( recs[i].tag ) );
		} else if ( recs[i].kind == 'A' ) {
			Com_sprintf( desc, sizeof( desc ), "input ambiguity (movementDir only)" );
		} else {
			Com_sprintf( desc, sizeof( desc ), "UNEXPLAINED, differs in:%s", SigNames( recs[i].tag ) );
		}
		fprintf( f, "  timer %-9s .. %-9s %4d steps  (demo %-8s) at (%.0f %.0f %.0f) speed %.0f  %s\n", t0, t1, j - i,
			d0, s0->ps.origin[0], s0->ps.origin[1], s0->ps.origin[2],
			sqrt( s0->ps.velocity[0] * s0->ps.velocity[0] + s0->ps.velocity[1] * s0->ps.velocity[1] ), desc );
	}
}

static int MaskOf( const char *name ) {
	int i;

	for ( i = 0; i < (int)( sizeof( cmpFields ) / sizeof( cmpFields[0] ) ); i++ ) {
		if ( !strcmp( cmpFields[i].name, name ) ) {
			return 1 << i;
		}
	}
	return 0;
}

static int RunDemo( const char *path, const stepOpts_t *o ) {
	static demo_t	d;
	pmhost_t		*host;
	pmParams_t		pp;
	char			err[256], mapPath[512], map[128], val[128];
	unsigned char	*bsp;
	int				bspLen = 0, checksum = 0, wantCrc;
	int				i, step, pairs = 0, matched = 0, failed = 0, reported = 0, vmErrors = 0;
	int				hasInputWord = 0;
	int				tier1 = 0, tier2 = 0, tier3 = 0, deepsDone = 0, external = 0, ambiguous = 0, unexplained = 0, skipVelSteps = 0;
	missRec_t		*recs = NULL;
	int				numRecs = 0, capRecs = 0;
	static int		xCount[32];
	static int		sigCount[SIG_MAX];

	if ( Demo_Load( path, &d ) || d.err[0] ) {
		printf( "%s: cannot read demo (%s)\n", path, d.err[0] ? d.err : "open failed" );
		return -1;
	}
	if ( d.numSnaps < 2 ) {
		printf( "%s: no snapshots\n", path );
		Demo_Free( &d );
		return -1;
	}
	step = d.snaps[1].serverTime - d.snaps[0].serverTime;
	if ( gRecoverWanted ) {
		free( gRecovered );
		gRecovered = (recoveredCmd_t *)calloc( d.numSnaps, sizeof( recoveredCmd_t ) );
		gRecoveredNum = d.numSnaps;
	}

	InfoVal( d.serverinfo, "version", val, sizeof( val ) );
	gHostColl = Steps_EngineColl( val );
	InfoVal( d.serverinfo, "mapname", map, sizeof( map ) );
	Q_strlwr( map );
	if ( o->bsp ) {
		Q_strncpyz( mapPath, o->bsp, sizeof( mapPath ) );
	} else {
		Com_sprintf( mapPath, sizeof( mapPath ), "%s/%s.bsp", o->mapsDir, map );
	}
	bsp = ReadFileAll( mapPath, &bspLen );
	if ( !bsp ) {
		printf( "%s: no map file %s\n", path, mapPath );
		Demo_Free( &d );
		return -1;
	}

	host = PMHost_Create( o->qvm, err, sizeof( err ) );
	if ( !host ) {
		printf( "cannot start host: %s\n", err );
		free( bsp );
		Demo_Free( &d );
		return -1;
	}
	if ( PMHost_LoadMap( host, bsp, bspLen, &checksum, err, sizeof( err ) ) ) {
		printf( "%s: %s\n", path, err );
		PMHost_Destroy( host );
		free( bsp );
		Demo_Free( &d );
		return -1;
	}
	free( bsp );

	InfoVal( d.systeminfo, "defrag_bspcrc", val, sizeof( val ) );
	wantCrc = atoi( val );
	if ( val[0] && wantCrc != checksum && getenv( "DFG_IGNORE_CRC" ) ) {
		printf( "  note: map version differs (demo wants bsp crc %d, file has %d); continuing\n", wantCrc, checksum );
	} else if ( val[0] && wantCrc != checksum ) {
		printf( "%s: WRONG MAP VERSION: demo wants bsp crc %d, file has %d\n", path, wantCrc, checksum );
		PMHost_Destroy( host );
		Demo_Free( &d );
		return -1;
	}

	/* settings the way the server derives them */
	memset( &pp, 0, sizeof( pp ) );
	InfoVal( d.serverinfo, "df_promode", val, sizeof( val ) );
	pp.promode = atoi( val );
	Cfg( &d, "pmove_fixed", val, sizeof( val ) );
	pp.pmove_fixed = atoi( val );
	Cfg( &d, "pmove_msec", val, sizeof( val ) );
	pp.pmove_msec = val[0] ? atoi( val ) : 8;
	InfoVal( d.serverinfo, "g_killWallbug", val, sizeof( val ) );
	pp.killWallbug = atoi( val );
	pp.obs_disabled = 0;
	if ( o->noPromodeParam ) {
		pp.promode = 0;
	}
	pp.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;		/* no other players exist in the host */

	/* recordings of old DeFRaG versions do not publish the input word */
	for ( i = 0; i < d.numSnaps && i < 400; i++ ) {
		if ( d.snaps[i].ps.stats[13] ) {
			hasInputWord = 1;
			break;
		}
	}

	for ( i = 1; i < d.numSnaps; i++ ) {
		const playerState_t *A = &d.snaps[i - 1].ps;
		const playerState_t *B = &d.snaps[i].ps;
		int a, b, c, btn, found = 0, bestDiff = 1 << 30, deep = 0, nCands = 0;
		cand_t cands[64];
		double bestMetric = 1e300;
		char bestText[512];
		signed char bestMove[3] = { 0, 0, 0 };
		int bestBtn = 0;

		refineReq_t *rq = NULL;

		if ( gRefine ) {
			int q;

			for ( q = 0; q < gRefineNum; q++ ) {
				if ( gRefine[q].step == i ) {
					rq = &gRefine[q];
				}
			}
			if ( !rq ) {
				continue;
			}
		}
		if ( !RegularPair( &d, i, step ) ) {
			continue;
		}
		pairs++;
		gRangeOn = hasInputWord;
		if ( gRangeOn ) {
			AxisRange( B->stats[13], 0x001, 0x002, 0 );
			AxisRange( B->stats[13], 0x010, 0x008, 1 );
			AxisRange( B->stats[13], 0x020, 0x040, 2 );
		}

		/* movers: the entities of snapshot A at the server time the step ran in */
		if ( !o->noEntities ) {
			const demoBmList_t *bl = &d.bmLists[d.snaps[i - 1].bmList];

			PMHost_SetEntities( host, (const pmEntity_t *)&d.bmodels[bl->start], bl->count,
				o->entTimeB ? d.snaps[i].serverTime : d.snaps[i - 1].serverTime );
		}

		if ( rq ) {
			playerState_t ps0, ps1;
			double kvel[3], bestScore = 1e300;
			int umTry, fm, rm, um, btn1 = ( B->stats[13] & 0x100 ) ? 1 : 0, dv;

			if ( RunPm( host, &pp, A, B, btn1, rq->fm, rq->rm, rq->um, &ps0 ) ) {
				goto nextPair;
			}
			for ( dv = 0; dv < 3; dv++ ) {
				kvel[dv] = (double)rq->simVel[dv] - ps0.velocity[dv];
			}
			gSkipVel = 1;
			gUlps = 0;
			/* the up value first as it is, then (the move values feed the movement scale through the largest and the
			   length of the three) any other value of its key, for a few steps */
			for ( umTry = 0; umTry < 128 && bestScore >= 50.0; umTry++ ) {
			if ( umTry ) {
				if ( !gRangeOn || gHi[2] <= gLo[2] || gUmExpanded >= ( getenv( "DFG_UM_MAX" ) ? atoi( getenv( "DFG_UM_MAX" ) ) : 40 ) ) {
					break;
				}
				um = gHi[2] - umTry + 1 > gLo[2] - 1 ? gHi[2] - umTry + 1 : 0;
				if ( um < gLo[2] || um == rq->um ) {
					continue;
				}
				if ( umTry == 1 ) {
					gUmExpanded++;
				}
			} else {
				um = rq->um;
			}
			for ( fm = gRangeOn ? gLo[0] : -127; fm <= ( gRangeOn ? gHi[0] : 127 ); fm++ ) {
				for ( rm = gRangeOn ? gLo[1] : -127; rm <= ( gRangeOn ? gHi[1] : 127 ); rm++ ) {
					double err = 0;

					if ( RunPm( host, &pp, A, B, btn1, (signed char)fm, (signed char)rm, (signed char)um, &ps1 ) ) {
						goto nextPair;
					}
					if ( DiffState( &ps1, B, NULL, 0, NULL ) ) {
						continue;
					}
					for ( dv = 0; dv < 3; dv++ ) {
						err += fabs( ps1.velocity[dv] + kvel[dv] - B->velocity[dv] );
					}
					/* the closest to the old command among the equally good ones */
					err = err * 1000.0 + 0.001 * ( abs( fm - rq->fm ) + abs( rm - rq->rm ) );
					if ( err < bestScore ) {
						bestScore = err;
						rq->nfm = (signed char)fm;
						rq->nrm = (signed char)rm;
						rq->num = (signed char)um;
					}
				}
			}
			}
			gSkipVel = 0;
			if ( getenv( "DFG_DEBUG_REFINE" ) ) {
				printf( "  refine step %d: Vpm0 %.9g %.9g %.9g sim %.9g %.9g %.9g B %.9g %.9g %.9g best %g found cmd %d %d range %d..%d %d..%d\n", i, ps0.velocity[0], ps0.velocity[1], ps0.velocity[2], rq->simVel[0], rq->simVel[1], rq->simVel[2], B->velocity[0], B->velocity[1], B->velocity[2], bestScore, rq->nfm, rq->nrm, gLo[0], gHi[0], gLo[1], gHi[1] );
			}
			if ( bestScore < 50.0 ) {
				rq->found = 1;
			}
			goto nextPair;
		}

		/* tier 1: the keys are either fully down or up */
		{
			signed char fmC[2], rmC[2], umC[2];
			int nF = AxisCandidates( B->stats[13], 0x001, 0x002, 0, fmC );
			int nR = AxisCandidates( B->stats[13], 0x010, 0x008, 0, rmC );
			int nU = AxisCandidates( B->stats[13], 0x020, 0x040, 1, umC );
			int btnOnly = ( B->stats[13] & 0x100 ) ? 1 : 0;

		for ( btn = btnOnly; btn <= btnOnly && !found; btn++ ) {
			for ( a = 0; a < nF && !found; a++ ) {
				for ( b = 0; b < nR && !found; b++ ) {
					for ( c = 0; c < nU && !found; c++ ) {
						int diffs, r;

						r = TryCmd( host, &pp, A, B, btn, fmC[a], rmC[b], umC[c], &diffs );
						if ( r ) {
							if ( !vmErrors++ ) {
								printf( "  host error at snapshot %d: %s\n", i, PMHost_Error( host ) );
							}
							goto nextPair;
						}
						if ( diffs < bestDiff ) {
							bestDiff = diffs;
							bestBtn = btn;
							bestMove[0] = fmC[a];
							bestMove[1] = rmC[b];
							bestMove[2] = umC[c];
						}
						cands[nCands].btn = btn;
						cands[nCands].fm = fmC[a];
						cands[nCands].rm = rmC[b];
						cands[nCands].um = umC[c];
						cands[nCands].metric = lastMetric;
						nCands++;
						if ( diffs == 0 ) {
							found = 1;
							tier1++;
						}
					}
				}
			}
		}
		}

		/* tier 1b: local search on forward / right / up from the best few basic tries; a key pressed or
		   released part way through a frame gives a fraction of full speed, and the move values all
		   feed the movement scale */
		if ( !found ) {
			int n, starts = nCands < 6 ? nCands : 6, k, order[64];

			for ( k = 0; k < nCands; k++ ) {
				order[k] = k;
			}
			/* selection sort of the first `starts` by metric */
			for ( n = 0; n < starts; n++ ) {
				int best = n;

				for ( k = n + 1; k < nCands; k++ ) {
					if ( cands[order[k]].metric < cands[order[best]].metric ) {
						best = k;
					}
				}
				k = order[n]; order[n] = order[best]; order[best] = k;
			}
			bestMetric = 1e300;
			for ( n = 0; n < starts && !found; n++ ) {
				const cand_t *c0 = &cands[order[n]];
				int cur[3] = { c0->fm, c0->rm, c0->um };
				signed char outMove[3];
				int outDiff, r;
				double outMetric;

				r = HillClimb( host, &pp, A, B, c0->btn, cur, &outDiff, outMove, &outMetric );
				if ( r < 0 ) {
					goto nextPair;
				}
				if ( outMetric < bestMetric ) {
					bestMetric = outMetric;
					bestDiff = outDiff;
					bestBtn = c0->btn;
					bestMove[0] = outMove[0]; bestMove[1] = outMove[1]; bestMove[2] = outMove[2];
				}
				if ( r > 0 ) {
					found = 1;
					tier2++;
				}
			}
		}

		/* tier 2: any forward / right value (a key pressed or released part way through a frame
		   gives a fraction of full speed); jump / crouch and the attack button stay as in the best try */
		if ( !found && o->deep && deepsDone < o->deepMax ) {
			deepsDone++;
			int fm, rm;

			deep = 1;
			for ( fm = gRangeOn ? gLo[0] : -127; fm <= ( gRangeOn ? gHi[0] : 127 ) && !found; fm++ ) {
				for ( rm = gRangeOn ? gLo[1] : -127; rm <= ( gRangeOn ? gHi[1] : 127 ) && !found; rm++ ) {
					int diffs;

					if ( TryCmd( host, &pp, A, B, bestBtn, (signed char)fm, (signed char)rm, bestMove[2], &diffs ) ) {
						goto nextPair;
					}
					if ( diffs < bestDiff ) {
						bestDiff = diffs;
						bestMove[0] = (signed char)fm;
						bestMove[1] = (signed char)rm;
					}
					if ( diffs == 0 ) {
						found = 1;
						tier3++;
					}
				}
			}
		}

		/* the server changed this step after Pmove: find the command from the position alone */
		if ( !found && ( ExternalReason( A, B ) & ( X_KNOCKBACK | X_DAMAGE | X_JUMPPAD | X_FRACVEL ) ) ) {
			int n, skipFound = 0;

			int savedUlps = gUlps;

			gSkipVel = 1;
			gUlps = 0;		/* with only the position to go by, nothing but an exact match counts */
			for ( n = 0; n < nCands && !skipFound; n++ ) {
				int cur[3] = { cands[n].fm, cands[n].rm, cands[n].um };
				signed char outMove[3];
				int outDiff, r;
				double outMetric;

				r = HillClimb( host, &pp, A, B, cands[n].btn, cur, &outDiff, outMove, &outMetric );
				if ( r < 0 ) {
					gSkipVel = 0;
					gUlps = savedUlps;
					goto nextPair;
				}
				if ( r > 0 ) {
					skipFound = 1;
					bestBtn = cands[n].btn;
					bestMove[0] = outMove[0]; bestMove[1] = outMove[1]; bestMove[2] = outMove[2];
					skipVelSteps++;
				}
			}
			gSkipVel = 0;
			gUlps = savedUlps;
		}

		if ( found ) {
			matched++;
		} else {
			char text[512];
			int mask, dummy;

			int ext = ExternalReason( A, B );

			failed++;
			/* describe the closest attempt again, and remember which fields failed */
			TryCmdText( host, &pp, A, B, bestBtn, bestMove[0], bestMove[1], bestMove[2], text, sizeof( text ), &mask );
			if ( ext ) {
				external++;
				xCount[ext]++;
			} else if ( mask == MaskOf( "movementDir" ) ) {
				/* position, velocity and flags are exact; only the leg direction label differs, so some
				   other usercmd explains the step equally well */
				ambiguous++;
				ext = 16;
			} else {
				unexplained++;
			}
			if ( gTimes ) {
				if ( numRecs == capRecs ) {
					capRecs = capRecs ? capRecs * 2 : 256;
					recs = (missRec_t *)realloc( recs, capRecs * sizeof( missRec_t ) );
				}
				recs[numRecs].snap = i;
				recs[numRecs].kind = ext == 16 && mask == MaskOf( "movementDir" ) ? 'A' : ext ? 'E' : 'U';
				recs[numRecs].tag = recs[numRecs].kind == 'U' ? mask : ext;
				numRecs++;
			}
			(void)dummy;
			Q_strncpyz( bestText, text, sizeof( bestText ) );
			if ( !ext ) {
				sigCount[mask & ( SIG_MAX - 1 )]++;
			}
			if ( reported < o->maxReport && ( !ext || o->showExternal ) ) {
				reported++;
				printf( "  MISS snapshot %d (t=%d, %d diff fields, closest cmd f%d r%d u%d btn%d%s):%s\n", i,
					d.snaps[i].serverTime, bestDiff, bestMove[0], bestMove[1], bestMove[2], bestBtn,
					deep ? ", deep" : "", bestText );
				if ( o->verbose ) {
					printf( "      A: ground %d vel (%g %g %g) org (%g %g %g) pmf 0x%x pmt %d | B: ground %d pmf 0x%x pmt %d\n",
						A->groundEntityNum, A->velocity[0], A->velocity[1], A->velocity[2], A->origin[0], A->origin[1],
						A->origin[2], A->pm_flags, A->pm_time, B->groundEntityNum, B->pm_flags, B->pm_time );
				}
			}
		}
		if ( gRecovered ) {
			gRecovered[i].fm = bestMove[0];
			gRecovered[i].rm = bestMove[1];
			gRecovered[i].um = bestMove[2];
			gRecovered[i].btn = (unsigned char)bestBtn;
			gRecovered[i].regular = 1;
			gRecovered[i].exact = (unsigned char)found;
		}
nextPair:;
	}

	if ( !gRefine )
		printf( "%-60.60s %-16s pairs=%-6d matched=%-6d (basic %d, local %d, exhaustive %d) missed=%-5d external=%-4d ambiguous=%-4d "
		"unexplained=%-4d (%.2f%% match, %.2f%% of the rest)%s\n",
		Basename_( path ), map, pairs, matched, tier1, tier2, tier3, failed, external, ambiguous, unexplained,
		pairs ? 100.0 * matched / pairs : 0.0, pairs ? 100.0 * ( matched + external + ambiguous ) / pairs : 0.0,
		vmErrors ? "  [host errors]" : "" );
	if ( skipVelSteps ) {
		printf( "  %d external steps: command found from the position alone\n", skipVelSteps );
	}
	if ( external && o->verbose ) {
		int x;

		for ( x = 1; x < 32; x++ ) {
			if ( xCount[x] ) {
				printf( "      %5d external:%s%s%s%s%s\n", xCount[x], ( x & X_KNOCKBACK ) ? " knockback" : "",
					( x & X_FRACVEL ) ? " fractional-velocity" : "", ( x & X_JUMPPAD ) ? " jumppad" : "",
					( x & X_DAMAGE ) ? " damage" : "", ( x & X_MOVER ) ? " riding-mover" : "" );
			}
		}
	}
	if ( failed && o->verbose ) {
		int m;

		for ( m = 0; m < SIG_MAX; m++ ) {
			if ( sigCount[m] ) {
				printf( "      %5d misses differ in:%s\n", sigCount[m], SigNames( m ) );
			}
		}
	}

	if ( gTimes && numRecs ) {
		WriteMissTimes( gTimes, &d, Basename_( path ), recs, numRecs, o->showExternal );
	}
	free( recs );
	PMHost_Destroy( host );
	Demo_Free( &d );
	return failed;
}

int Steps_Recover( const char *demoPath, const char *qvm, const char *bspPath, const char *mapsDir, int ulps,
					recoveredCmd_t **cmds, int *num ) {
	stepOpts_t o;
	int oldUlps = gUlps;

	memset( &o, 0, sizeof( o ) );
	o.qvm = qvm;
	o.bsp = bspPath;
	o.mapsDir = mapsDir;
	o.deep = gRecoverDeep;
	o.deepMax = 1000000;
	gUlps = ulps;
	gRecoverWanted = 1;
	gRecovered = NULL;
	RunDemo( demoPath, &o );
	gRecoverWanted = 0;
	gUlps = oldUlps;
	*cmds = gRecovered;
	*num = gRecoveredNum;
	gRecovered = NULL;
	return *cmds ? 0 : -1;
}

int Steps_Refine( const char *demoPath, const char *qvm, const char *bspPath, const char *mapsDir, refineReq_t *reqs, int num ) {
	stepOpts_t o;
	int oldUlps = gUlps;

	memset( &o, 0, sizeof( o ) );
	o.qvm = qvm;
	o.bsp = bspPath;
	o.mapsDir = mapsDir;
	gUlps = 0;
	gRefine = reqs;
	gRefineNum = num;
	gUmExpanded = 0;
	RunDemo( demoPath, &o );
	gRefine = NULL;
	gRefineNum = 0;
	gUlps = oldUlps;
	return 0;
}

int Steps_EngineColl( const char *version ) {
	char v[128];

	Q_strncpyz( v, version, sizeof( v ) );
	Q_strlwr( v );
	if ( strstr( v, "iodfe" ) ) {
		return 2;
	}
#ifdef DFG_HAVE_X87
	if ( ( strstr( v, "x86" ) || strstr( v, "i386" ) ) && !strstr( v, "x86_64" ) && !strstr( v, "x64" ) ) {
		return 1;
	}
#endif
	return 0;
}

int Steps_Main( int argc, char **argv ) {
	stepOpts_t o;
	int i, any = 0;

	memset( &o, 0, sizeof( o ) );
	o.maxReport = 5;
	o.deepMax = 25;

	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--qvm" ) && i + 1 < argc ) {
			o.qvm = argv[++i];
		} else if ( !strcmp( argv[i], "--bsp" ) && i + 1 < argc ) {
			o.bsp = argv[++i];
		} else if ( !strcmp( argv[i], "--maps" ) && i + 1 < argc ) {
			o.mapsDir = argv[++i];
		} else if ( !strcmp( argv[i], "--max" ) && i + 1 < argc ) {
			o.maxReport = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--ulps" ) && i + 1 < argc ) {
			gUlps = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--times" ) && i + 1 < argc ) {
			o.timesFile = argv[++i];
		} else if ( !strcmp( argv[i], "--show-external" ) ) {
			o.showExternal = 1;
		} else if ( !strcmp( argv[i], "--no-entities" ) ) {
			o.noEntities = 1;
		} else if ( !strcmp( argv[i], "--ent-time-b" ) ) {
			o.entTimeB = 1;
		} else if ( !strcmp( argv[i], "--no-promode-param" ) ) {
			o.noPromodeParam = 1;
		} else if ( !strcmp( argv[i], "--deep-max" ) && i + 1 < argc ) {
			o.deepMax = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--deep" ) ) {
			o.deep = 1;
		} else if ( !strcmp( argv[i], "-v" ) ) {
			o.verbose = 1;
		}
	}
	if ( !o.qvm || ( !o.bsp && !o.mapsDir ) ) {
		fprintf( stderr, "usage: dfgolden steps --qvm pmove.qvm (--bsp map.bsp | --maps dir) [--deep] [-v] [--max N] <demo> ...\n" );
		return 1;
	}
	if ( o.timesFile ) {
		gTimes = fopen( o.timesFile, "w" );
	}
	for ( i = 0; i < argc; i++ ) {
		if ( argv[i][0] == '-' ) {
			if ( strcmp( argv[i], "-v" ) && strcmp( argv[i], "--deep" ) && strcmp( argv[i], "--no-promode-param" ) &&
				strcmp( argv[i], "--no-entities" ) && strcmp( argv[i], "--show-external" ) && strcmp( argv[i], "--ent-time-b" ) ) {		/* options with a value skip it */
				i++;			/* option with a value */
			}
			continue;
		}
		any = 1;
		RunDemo( argv[i], &o );
	}
	if ( gTimes ) {
		fclose( gTimes );
	}
	return any ? 0 : 1;
}
