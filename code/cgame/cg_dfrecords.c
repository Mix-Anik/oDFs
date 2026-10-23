// cg_dfrecords.c -- DeFRaG client-side run records: checkpoint times, reference records and the .rec file format
//
// Ported from the DeFRaG cgame (see disasm/CGAME/hud_widget_system.md, "local record subsystem"):
//   0x4e9c GetRecord, 0x4f46 WriteRecord, 0x4e75 checksum, 0x586d/0x5543/0x5431/0x5504 file name template,
//   0x506c/0x5092 reference loaders, 0x5248 run state machine, 0x50b6 popup, 0x5153 list, 0x4f9d summary.
#include "cg_local.h"

#define DF_REC_INTS		33				// 0x84 bytes: [0..31] checkpoint times, [32] = checkpoint count
#define DF_REC_BYTES	( DF_REC_INTS * 4 )
#define DF_REC_COUNT	32

vmCvar_t	df_recfiles_session;		// 0x11ba94: sub directory of system/records the session reference lives in
vmCvar_t	df_recfiles_keepLatest;		// 0x11bba4: also write every finished run to system/records/latest
vmCvar_t	df_tieOverwrites;			// 0x11b440 area: a tying time replaces the record

static int	dfRecB[DF_REC_INTS];		// 0x11d6b8 current run; [32] is the checkpoint index N (0x11d738)
static int	dfRecR1[DF_REC_INTS];		// 0x11d5b0 session reference (delta mode, "best time" widget)
static int	dfRecR2[DF_REC_INTS];		// 0x11d634 personal record (new-record test)
static qboolean	dfRecDirty;				// 0x11d5a8 a better record was written: R2 must be replaced
static int	dfSessionModCount = -1;		// 0x11d5ac

// ---- DeFRaG ms -> string (cgame 0x1024) ---------------------------------------------------------------------

const char *CG_DF_FormatTime( int ms ) {
	static char	buf[2][32];
	static int	idx;
	char		*s;
	const char	*sign = "";

	idx ^= 1;
	s = buf[idx];
	if ( ms < 0 ) {
		ms = -ms;
		sign = "-";
	}
	if ( ms < 60000 ) {
		Com_sprintf( s, sizeof( buf[0] ), "%s%i:%03i", sign, ms / 1000, ms % 1000 );
	} else {
		Com_sprintf( s, sizeof( buf[0] ), "%s%i:%02i:%03i", sign, ms / 60000, ( ms / 1000 ) % 60, ms % 1000 );
	}
	return s;
}

// DeFRaG 0x10c8( value, 0, 0, 1, 1, 1, 0 ): signed, colour-coded delta -- green '+' (ahead of the reference), red '-' (behind), white at 0
const char *CG_DF_FormatDelta( int v ) {
	static char	buf[2][32];
	static int	idx;
	char		*s, col = '2';
	const char	*sign = "+";

	idx ^= 1;
	s = buf[idx];
	if ( v < 0 ) {
		v = -v;
		sign = "-";
		col = '1';
	} else if ( v == 0 ) {
		col = '7';
	}
	if ( v < 60000 ) {
		Com_sprintf( s, sizeof( buf[0] ), "^%c%s%i:%03i", col, sign, v / 1000, v % 1000 );
	} else {
		Com_sprintf( s, sizeof( buf[0] ), "^%c%s%i:%02i:%03i", col, sign, v / 60000, ( v % 60000 ) / 1000, v % 1000 );
	}
	return s;
}

static qboolean dfRecRunStarted;		// 0x11d2a4+... (1168804): set by a start event, cleared by a respawn
void CG_DF_RecRunStopped( void ) { dfRecRunStarted = qfalse; }

// ---- file names (template expander 0x586d) -------------------------------------------------------------------

static const char *dfGtShort[8]  = { "ac", "df", "tm", "fc", "res", "mdf", "mtm", "mfc" };

static char CG_DF_NameChar( char c ) {
	// characters that cannot be part of a file name become '-'
	if ( c <= 0x20 || c >= 0x7e || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '\\' || c == '/' ) {
		return '-';
	}
	return c;
}

typedef struct {
	char	*out;
	char	*start;
	int		rem;
} dfName_t;

static void CG_DF_NamePut( dfName_t *n, int maxLen, const char *s ) {
	while ( *s && maxLen-- > 0 && n->rem > 0 ) {
		*n->out++ = CG_DF_NameChar( *s++ );
		n->rem--;
	}
}

// DeFRaG 0x5504: an empty $-variable removes the separator that precedes it, an empty $+variable skips the one after it
static void CG_DF_NameEmpty( dfName_t *n, const char **p, qboolean minus, qboolean plus ) {
	if ( minus && n->out != n->start ) {
		n->out--;
		n->rem++;
	}
	if ( plus && **p && **p != '$' ) {
		( *p )++;
	}
}

const char *CG_DF_Serverinfo( const char *key ) {
	static char	info[MAX_INFO_STRING];

	Q_strncpyz( info, CG_ConfigString( CS_SERVERINFO ), sizeof( info ) );
	return Info_ValueForKey( info, key );
}

// DeFRaG 0x5543: expand one $variable; *p points just behind the '$'
static void CG_DF_NameVar( dfName_t *n, const char **p ) {
	qboolean	minus = qfalse, plus = qfalse;
	char		value[MAX_QPATH];
	int			v;

	for ( ;; ) {
		if ( **p == '-' ) {
			minus = qtrue;
			( *p )++;
		} else if ( **p == '+' ) {
			plus = qtrue;
			( *p )++;
		} else {
			break;
		}
	}
	if ( !Q_stricmpn( *p, "map", 3 ) ) {
		CG_DF_NamePut( n, 0x40, CG_DF_Serverinfo( "mapname" ) );
		*p += 3;
	} else if ( !Q_stricmpn( *p, "phys", 4 ) ) {
		CG_DF_NamePut( n, 3, atoi( CG_DF_Serverinfo( "df_promode" ) ) ? "cpm" : "vq3" );		// 0x3b5e: serverinfo df_promode, known before the first snapshot
		*p += 4;
	} else if ( !Q_stricmpn( *p, "gt", 2 ) ) {
		v = atoi( CG_DF_Serverinfo( "defrag_gametype" ) );
		CG_DF_NamePut( n, 3, dfGtShort[( v >= 0 && v < 8 ) ? v : 0] );
		*p += 2;
	} else if ( !Q_stricmpn( *p, "plc", 3 ) ) {
		trap_Cvar_VariableStringBuffer( "df_country", value, sizeof( value ) );
		CG_DF_NamePut( n, 0x14, value );
		*p += 3;
	} else if ( !Q_stricmpn( *p, "pl", 2 ) ) {
		CG_DF_NamePut( n, 0x14, cg.snap ? cgs.clientinfo[cg.snap->ps.clientNum].name : "" );
		*p += 2;
	} else if ( !Q_stricmpn( *p, "route", 5 ) ) {
		Q_strncpyz( value, CG_DF_Serverinfo( "defrag_route" ), sizeof( value ) );
		if ( value[0] ) {
			CG_DF_NamePut( n, 0x12, value );
		} else {
			CG_DF_NameEmpty( n, p, minus, plus );
		}
		*p += 5;
	} else if ( !Q_stricmpn( *p, "mode", 4 ) ) {
		v = CG_DF_Serverinfo( "defrag_mode" )[0] ? atoi( CG_DF_Serverinfo( "defrag_mode" ) ) : -1;
		if ( v >= 0 ) {
			CG_DF_NamePut( n, 1, va( "%i", v ) );		// DeFRaG copies one character (maxlen 1)
		} else {
			CG_DF_NameEmpty( n, p, minus, plus );
		}
		*p += 4;
	}
	// time/date variables ($h, $ms, $m, $datetime, $date ...) are not used by the record templates and not ported
}

// DeFRaG 0x586d( dst, size, template, dir, ext, ms )
static void CG_DF_ExpandName( char *dst, int size, const char *tmpl, const char *dir, const char *ext ) {
	dfName_t	n;
	const char	*p = tmpl;
	int			len;

	n.out = dst;
	n.rem = size;
	if ( dir && dir[0] ) {
		Q_strncpyz( n.out, dir, n.rem );
		len = strlen( dir );
		n.out += len;
		n.rem -= len;
		if ( n.out[-1] != '/' ) {
			*n.out++ = '/';
			n.rem--;
		}
	}
	n.start = n.out;
	if ( ext ) {
		n.rem -= strlen( ext );
		if ( ext[0] != '.' ) {
			n.rem--;
		}
	}
	n.rem--;
	while ( *p && n.rem > 0 ) {
		if ( p[0] == '^' && p[1] && p[1] != '^' ) {		// colour codes are dropped
			p += 2;
			continue;
		}
		if ( *p == '$' ) {
			p++;
			CG_DF_NameVar( &n, &p );
			continue;
		}
		*n.out++ = CG_DF_NameChar( *p++ );
		n.rem--;
	}
	*n.out = 0;
	if ( ext ) {
		if ( ext[0] != '.' ) {
			*n.out++ = '.';
		}
		strcpy( n.out, ext );
	}
}

// 0x1185f: the ghost/record base name (template without directory and extension)
void CG_DF_RecordBaseName( char *dst, int size ) {
	CG_DF_ExpandName( dst, size, "$map_$-route_$gt_$phys_$-mode", NULL, NULL );
}

static void CG_DF_RecordPath( char *dst, int size, const char *dir ) {
	CG_DF_ExpandName( dst, size, "$map_$-route_$gt_$phys_$-mode", dir, "rec" );
}

// ---- record files (0x4e9c / 0x4f46) -------------------------------------------------------------------------

static byte CG_DF_RecChecksum( const byte *data, int len ) {
	byte sum = 0;

	while ( len-- > 0 ) {
		sum += *data++;
	}
	return sum;
}

static void CG_DF_ReadRecord( const char *path, int *rec ) {
	byte			buf[0x200];
	fileHandle_t	f;
	int				len;

	memset( rec, 0, DF_REC_BYTES );
	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( !f ) {
		return;
	}
	if ( len > (int)sizeof( buf ) || len < 4 ) {
		trap_FS_FCloseFile( f );
		return;
	}
	trap_FS_Read( buf, len, f );
	trap_FS_FCloseFile( f );

	if ( buf[0] == 2 && len >= 4 + 0x24 ) {
		// v2: nine checkpoint ints; the count word moved from +0x20 to +0x80 in v3
		if ( buf[1] != CG_DF_RecChecksum( buf + 4, 0x24 ) ) {
			CG_Printf( "GetRecord(): %s : v2 file is corrupted.\n", path );
			return;
		}
		memcpy( rec, buf + 4, 0x24 );
		rec[DF_REC_COUNT] = rec[8];
		rec[8] = 0;
	} else if ( buf[0] == 3 && len >= 4 + DF_REC_BYTES ) {
		if ( buf[1] != CG_DF_RecChecksum( buf + 4, DF_REC_BYTES ) ) {
			CG_Printf( "GetRecord(): %s : v3 file is corrupted.\n", path );
			return;
		}
		memcpy( rec, buf + 4, DF_REC_BYTES );
	} else {
		CG_Printf( "GetRecord(): %s : unknown version.\n", path );
	}
}

static void CG_DF_WriteRecord( const char *path, const int *rec ) {
	byte			hdr[4];
	fileHandle_t	f;

	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		CG_Printf( "SetRecord(): failed to open \"%s\" for writing.\n", path );
		return;
	}
	hdr[0] = 3;
	hdr[1] = CG_DF_RecChecksum( (const byte *)rec, DF_REC_BYTES );
	hdr[2] = hdr[3] = 0;
	trap_FS_Write( hdr, 4, f );
	trap_FS_Write( rec, DF_REC_BYTES, f );
	trap_FS_FCloseFile( f );
}

static const char *CG_DF_RecordDir( void ) {
	return cg.demoPlayback ? "system/records/demos" : "system/records";
}

// 0x506c: personal record R2
static void CG_DF_LoadR2( void ) {
	char path[256];

	CG_DF_RecordPath( path, sizeof( path ), CG_DF_RecordDir() );
	CG_DF_ReadRecord( path, dfRecR2 );
}

// 0x5092: session reference R1
static void CG_DF_LoadR1( void ) {
	char path[256];

	CG_DF_RecordPath( path, sizeof( path ), va( "%s/%s", "system/records", df_recfiles_session.string ) );
	CG_DF_ReadRecord( path, dfRecR1 );
}

// ---- checkpoint text (0x50b6 / 0x5153) -----------------------------------------------------------------------

// text DeFRaG shows for entry i: delta vs the reference (df_checkpoints 2), total (3) or split (otherwise)
const char *CG_DF_CheckpointText( int i ) {
	int N = dfRecB[DF_REC_COUNT];

	if ( df_checkpoints.integer == 2 && dfRecRunStarted && dfRecR1[0] != 0 ) {		// 0x50c0..0x50f9 / 0x5179..0x51b2
		return CG_DF_FormatDelta( dfRecR1[i] - dfRecB[i] );
	}
	if ( df_checkpoints.integer == 3 ) {
		return CG_DF_FormatTime( dfRecB[i] );
	}
	if ( N != 0 ) {
		// DeFRaG reads B[-1] for i == 0, which is the count word of the personal record R2
		return CG_DF_FormatTime( dfRecB[i] - ( i > 0 ? dfRecB[i - 1] : dfRecR2[DF_REC_COUNT] ) );
	}
	return CG_DF_FormatTime( dfRecB[i] );
}

static void CG_DF_CheckpointPopup( int i ) {
	if ( df_drawCheckpointEvents.integer ) {
		CG_CenterPrint( CG_DF_CheckpointText( i ), 200, 10 );
	}
}

// best finish time of the session reference record (0x4f8c)
int CG_DF_ReferenceTime( void ) {
	return dfRecR1[dfRecR1[DF_REC_COUNT]];
}

// ---- run state machine (0x5248) ------------------------------------------------------------------------------

void CG_DF_RecInit( void ) {
	memset( dfRecB, 0, sizeof( dfRecB ) );
	memset( dfRecR1, 0, sizeof( dfRecR1 ) );
	memset( dfRecR2, 0, sizeof( dfRecR2 ) );
	dfRecDirty = qfalse;
	dfSessionModCount = -1;
}

// called when the map's configstrings are known (record names depend on map, route, gametype, physics)
void CG_DF_RecLoad( void ) {
	CG_DF_LoadR2();
	CG_DF_LoadR1();
	dfSessionModCount = df_recfiles_session.modificationCount;
}

// start of a run (stats[12] bit 4 set): clear the current run, refresh the references
void CG_DF_RecStart( void ) {
	dfRecRunStarted = qtrue;
	if ( dfRecDirty ) {
		memcpy( dfRecR2, dfRecB, DF_REC_BYTES );
		CG_DF_LoadR1();
		dfRecDirty = qfalse;
	} else if ( df_recfiles_keepLatest.integer ) {
		CG_DF_LoadR1();
	}
	memset( dfRecB, 0, sizeof( dfRecB ) );
	if ( dfSessionModCount != df_recfiles_session.modificationCount ) {
		CG_DF_LoadR1();
		dfSessionModCount = df_recfiles_session.modificationCount;
	}
}

// the current (open) entry of the run record follows the race time every snapshot: the last row of the checkpoint list is the running timer
void CG_DF_RecTrack( int ms ) {
	dfRecB[dfRecB[DF_REC_COUNT]] = ms;
}

void CG_DF_RecCheckpoint( int t ) {
	int *N = &dfRecB[DF_REC_COUNT];

	dfRecB[*N] = t;
	if ( *N + 1 < 32 ) {
		CG_DF_CheckpointPopup( *N );
		( *N )++;
		dfRecB[*N] = t;
	}
}

static qboolean dfRecKeep;
qboolean CG_DF_RecKeep( void ) { return dfRecKeep; }

void CG_DF_RecFinish( int t ) {
	int		*N = &dfRecB[DF_REC_COUNT];
	int		best;
	qboolean isNew = qfalse, isTie = qfalse;
	char	path[256];

	dfRecKeep = qfalse;
	dfRecB[*N] = t;
	// DeFRaG skips both the checkpoint popups and this whole block with sv_cheats; here the checkpoints stay visible (user choice),
	// only the record files are not written for cheated runs
	if ( !CG_DF_Cheats() && cg.snap && cgs.clientinfo[cg.snap->ps.clientNum].team != TEAM_SPECTATOR ) {
		best = dfRecR2[dfRecR2[DF_REC_COUNT]];
		if ( best == 0 || t < best ) {
			isNew = qtrue;
		} else if ( t == best ) {
			isTie = qtrue;
		}
		if ( df_recfiles_keepLatest.integer ) {
			CG_DF_RecordPath( path, sizeof( path ), "system/records/latest" );
			CG_DF_WriteRecord( path, dfRecB );
		}
		dfRecKeep = isNew || ( isTie && df_tieOverwrites.integer );
		if ( isNew || ( isTie && df_tieOverwrites.integer ) ) {
			CG_DF_RecordPath( path, sizeof( path ), CG_DF_RecordDir() );
			CG_DF_WriteRecord( path, dfRecB );
			dfRecDirty = qtrue;
		}
	}
	CG_DF_SetLastRunTime( t );
	CG_DF_CheckpointPopup( *N );
}

// ---- HUD checkpoint list (0x5153) and summary (0x4f9d) -----------------------------------------------------------

// DeFRaG 0x4f9d: largest |delta| vs the reference / total / longest split
int CG_DF_CheckpointSummary( void ) {
	int N = dfRecB[DF_REC_COUNT];
	int i, c = 0, d;

	for ( i = 0; i <= N; i++ ) {
		if ( df_checkpoints.integer == 2 && dfRecR1[0] != 0 ) {
			d = dfRecR1[i] - dfRecB[i];
			if ( abs( c ) < abs( d ) ) {
				c = d;
			}
		} else if ( df_checkpoints.integer == 3 ) {
			return dfRecB[N];
		} else if ( i > 0 ) {
			if ( c < dfRecB[i] - dfRecB[i - 1] ) {
				c = dfRecB[i] - dfRecB[i - 1];
			}
		} else {
			c = dfRecB[0];
		}
	}
	return c;
}

int CG_DF_CheckpointCount( void ) {
	return dfRecB[DF_REC_COUNT];
}

// 0x5153( x, y ): the last 8 entries, right aligned at x, growing upwards from y
void CG_DF_DrawCheckpointListAt( float x, float y ) {
	int		N = dfRecB[DF_REC_COUNT];
	int		i, first, w;
	const char *text;

	if ( !df_drawCheckpointStats.integer || dfRecB[0] == 0 ) {
		return;
	}
	first = ( N > 7 ) ? N - 7 : 0;
	for ( i = first; i <= N; i++ ) {
		text = CG_DF_CheckpointText( i );
		w = CG_DrawStrlen( text );		// 0x3265e: colour codes do not count
		CG_DrawStringExt( (int)( x - 16.0f * w ), (int)y, text, colorWhite, qfalse, qtrue, 16, 16, 0 );
		y -= 16.0f;
	}
}

// vanilla-HUD placement (df_hud 0): ours, DeFRaG's own caller is the HUD widget
void CG_DF_DrawCheckpointList( void ) {
	CG_DF_DrawCheckpointListAt( SCREEN_WIDTH - 8, SCREEN_HEIGHT * 0.5f );
}
