// cg_defrag.c -- DeFRaG client-side race tracking: checkpoint popup and HUD list
//
// Ported from the DeFRaG cgame (see disasm/CGAME/cgame_cvar_consumers.md): 0x50b6 popup, 0x5153 HUD list,
// 0x1c8b event derivation (stats[12] bit toggles), 0x5248 time capture.
#include "cg_local.h"

#define DF_MAX_CHECKPOINTS	33		// 0x84 / 4: size of the DeFRaG checkpoint time buffer

vmCvar_t	df_checkpoints;				// 2 = delta vs reference run, 3 = total time, else split time
vmCvar_t	df_drawCheckpointEvents;	// popup on each checkpoint
vmCvar_t	df_drawCheckpointStats;		// list of the last checkpoints on the HUD

static int	cpTimes[DF_MAX_CHECKPOINTS];	// 0x11d6b8: checkpoint times of the current run (ms)
int			cg_dfOptions;				// 0x11d2b8: set by the "df_options" server command (unused by DeFRaG's own qagame)
static int	cpLast = -1;					// 0x11d738: index of the newest checkpoint, -1 = none

// DeFRaG ms -> string (cgame 0x1024): "S:mmm" below one minute, "M:SS:mmm" otherwise, leading '-' for negatives
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

// text DeFRaG shows for checkpoint i (0x50b6 / 0x5153). The reference run (mode 2 "delta") is not tracked, which is
// the case where DeFRaG itself falls back to the split time (A[0] == 0).
static const char *CG_DF_CheckpointText( int i ) {
	if ( df_checkpoints.integer == 3 ) {
		return CG_DF_FormatTime( cpTimes[i] );
	}
	if ( cpLast != 0 && i > 0 ) {
		return CG_DF_FormatTime( cpTimes[i] - cpTimes[i - 1] );
	}
	return CG_DF_FormatTime( cpTimes[i] );
}

// race time published by the server in ps.stats[7]/[8]; only the plain form (dedicated/variant servers,
// defrag_gametype >= 5) is read here -- the obfuscated form (cgame 0x5fd7) is not decoded on this side.
static qboolean CG_DF_RaceTime( const playerState_t *ps, int *ms ) {
	char	info[MAX_INFO_STRING];

	Q_strncpyz( info, CG_ConfigString( CS_SERVERINFO ), sizeof( info ) );
	if ( atoi( Info_ValueForKey( info, "defrag_gametype" ) ) < 5 ) {
		return qfalse;
	}
	*ms = ( ps->stats[STAT_RACE_TIME_HI] << 16 ) | ( ps->stats[STAT_RACE_TIME_LO] & 0xffff );
	return qtrue;
}


// ---- run-validity reporter (cgame 0x59b4) and per-snapshot checker (0x6237), see disasm/CGAME/cheat_report.md ----
vmCvar_t	df_validityChecks;

static const struct { const char *category; const char *format; } dfViolations[18] = {
	{ "Physics",  "invalid player speed %i (expected: %i)" },
	{ "Physics",  "invalid player gravity %i (expected: %i)" },
	{ "Timer",    "checksum failed" },
	{ "Timer",    "abnormal stepping" },
	{ "Timer",    "incomplete run" },
	{ "Bsp crc",  "checksum failed" },
	{ "Bsp crc",  "missing local record" },
	{ "Defrag",   "invalid version report %i" },
	{ "Defrag",   "beta version %s" },
	{ "Settings", "high server frame-rate: %i" },
	{ "Server",   "timescaling reported" },
	{ "Server",   "illegal server frame-rate reported" },
	{ "Cvar",     "%s is missing" },
	{ "Cvar",     "%s %i (invalid)" },
	{ "Cvar",     "%s %.2f (invalid)" },
	{ "Cvar",     "latched cvar %s changed from %f to %f" },
	{ "Cvar",     "physics do not match reported physics" },
	{ "Client",   "illegal client roll detected" },
};

static int dfReported;		// 0x11d73c: bitmask of violation codes already printed
static char dfViolationLog[32][128];	// 0x11d7a0: printed violations, listed by svinfo_report
static int  dfViolationCount;		// 0x11e7a0

// Each code is printed the first time it is seen (DeFRaG additionally keeps a log of repeats; not ported).
static void CG_DF_Violation( int code, const char *text ) {
	if ( dfReported & ( 1 << code ) ) {
		return;
	}
	dfReported |= 1 << code;
	if ( dfViolationCount < 32 ) {
		Com_sprintf( dfViolationLog[dfViolationCount++], sizeof( dfViolationLog[0] ), "%s: %s", dfViolations[code].category, text );
	}
	CG_Printf( "^2%s: ^3%s\n", dfViolations[code].category, text );
}

static void CG_DF_CheckCvar( const char *info, const char *name, const char *expected, qboolean isFloat ) {
	const char *v = Info_ValueForKey( info, name );

	if ( !v[0] ) {
		CG_DF_Violation( 12, va( dfViolations[12].format, name ) );
	} else if ( isFloat ? ( atof( v ) != atof( expected ) ) : ( atoi( v ) != atoi( expected ) ) ) {
		CG_DF_Violation( isFloat ? 14 : 13, isFloat ? va( dfViolations[14].format, name, atof( v ) ) : va( dfViolations[13].format, name, atoi( v ) ) );
	}
}

// the server settings DeFRaG requires (cgame 0x5b89 systeminfo / 0x5dd5 serverinfo validators)
static void CG_DF_CheckServerSettings( void ) {
	char	sys[BIG_INFO_STRING], srv[MAX_INFO_STRING];

	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	Q_strncpyz( srv, CG_ConfigString( CS_SERVERINFO ), sizeof( srv ) );
	CG_DF_CheckCvar( sys, "sv_cheats", "0", qfalse );
	CG_DF_CheckCvar( sys, "timescale", "1", qtrue );
	CG_DF_CheckCvar( sys, "g_speed", "320", qfalse );
	CG_DF_CheckCvar( sys, "g_gravity", "800", qfalse );
	CG_DF_CheckCvar( sys, "g_knockback", "1000", qfalse );
	CG_DF_CheckCvar( srv, "dmflags", "0", qfalse );
}

static void CG_DF_Validate( const playerState_t *ops, const playerState_t *ps ) {
	static int	startSeen, lastGravity, lastSpeed, hasteMemo;
	int			changed = ops->stats[STAT_DEFRAG_FLAGS] ^ ps->stats[STAT_DEFRAG_FLAGS];
	int			expected;
	char		info[MAX_INFO_STRING];

	if ( !df_validityChecks.integer ) {
		return;
	}
	CG_DF_CheckServerSettings();

	if ( ps->stats[STAT_DEFRAG_FLAGS] & 0x20 ) {
		CG_DF_Violation( 10, dfViolations[10].format );
	}
	if ( ps->stats[STAT_DEFRAG_FLAGS] & 0x40 ) {
		CG_DF_Violation( 11, dfViolations[11].format );
	}
	// a finish without ever having seen the start (joined mid-run)
	if ( changed & 4 ) {
		startSeen = 1;
	} else if ( ( changed & 8 ) && !startSeen ) {
		CG_DF_Violation( 4, dfViolations[4].format );
	}

	if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0 ) {
		return;
	}
	if ( ps->viewangles[ROLL] != 0 ) {
		CG_DF_Violation( 17, dfViolations[17].format );
	}
	if ( !ps->gravity && !ps->speed && !ps->velocity[0] && !ps->velocity[1] && !ps->velocity[2] ) {
		return;		// frozen player state
	}
	if ( !lastGravity ) {
		lastGravity = 800;
	}
	if ( ps->gravity != 800 && ps->gravity != lastGravity ) {
		CG_DF_Violation( 1, va( dfViolations[1].format, ps->gravity, 800 ) );
	}
	lastGravity = ps->gravity;

	if ( ps->powerups[PW_HASTE] ) {
		hasteMemo = ps->powerups[PW_HASTE];
	}
	expected = ( cg.snap && cg.snap->serverTime < hasteMemo + 250 ) ? 416 : 320;
	if ( ps->speed != 320 && ps->speed != expected && ps->speed != lastSpeed ) {
		CG_DF_Violation( 0, va( dfViolations[0].format, ps->speed, expected ) );
	}
	lastSpeed = ps->speed;

	// pm_flags promode bit must match the reported df_promode
	Q_strncpyz( info, CG_ConfigString( CS_SERVERINFO ), sizeof( info ) );
	if ( ( ( ps->pm_flags & PMF_PROMODE ) != 0 ) != ( atoi( Info_ValueForKey( info, "df_promode" ) ) != 0 ) ) {
		CG_DF_Violation( 16, dfViolations[16].format );
	}
}


// ---- end-of-run statistics (cgame 0x4388 dump, trackers 0x3a4a / 0x4677) ----
// Not ported: "Highest Jump" (tracker 0x458e) and the per-axis counters.
vmCvar_t	df_noEndOfRunStats;

static qboolean	runActive;			// 0x11d288
static float	statMaxSpeed;		// 0x11d230
static float	statDistance;		// 0x11d234
static int		statStartTime;		// race time at the last start (ms)

static float CG_DF_HorizontalLength( const vec3_t v ) {
	return sqrt( v[0] * v[0] + v[1] * v[1] );
}

static void CG_DF_ResetStats( void ) {
	statMaxSpeed = 0;
	statDistance = 0;
}

// per snapshot while a run is active: max horizontal speed, horizontal distance travelled
static void CG_DF_TrackStats( const playerState_t *ops, const playerState_t *ps, int changed ) {
	vec3_t	d;
	float	speed;

	if ( !runActive ) {
		return;
	}
	speed = CG_DF_HorizontalLength( ps->velocity );
	if ( speed > statMaxSpeed ) {
		statMaxSpeed = speed;
	}
	if ( ps->stats[STAT_HEALTH] <= 0 || ( changed & 1 ) ) {		// dead, or a target_init event (0x20)
		return;
	}
	VectorSubtract( ps->origin, ops->origin, d );
	statDistance += CG_DF_HorizontalLength( d );
}

static const char *CG_DF_StatSuffixSpeed( float ups ) {
	if ( df_unit.integer == 1 ) {
		return va( " (%i Km/h)", (int)CG_ConvertSpeed( ups ) );
	}
	if ( df_unit.integer == 2 ) {
		return va( " (%i miles/h)", (int)CG_ConvertSpeed( ups ) );
	}
	return "";
}

static int CG_DF_CvarInt( const char *name ) {
	char buf[MAX_CVAR_VALUE_STRING];

	trap_Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atoi( buf );
}

// prints the "Stats" block; runMs = elapsed run time for the average speed
void CG_DF_PrintStats( int runMs ) {
	char	sys[BIG_INFO_STRING];
	const char *type;
	int		msec;
	float	avg = runMs ? ( statDistance * 1000.0f ) / (float)runMs : 0;

	if ( df_noEndOfRunStats.integer ) {
		return;
	}
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	msec = atoi( Info_ValueForKey( sys, "pmove_msec" ) );
	type = ( cg.snap && ( cg.snap->ps.pm_flags & PMF_PROMODE ) ) ? "cpm" : "vq3";

	CG_Printf( "----- ^2Stats^7 -----\nPhysics\n" );
	CG_Printf( " type           : %s\n", type );
	CG_Printf( " pmove_fixed    : %i\n", msec != 0 );
	CG_Printf( " pmove_msec     : %i\n", msec );		// DeFRaG prints one cached value for both lines (see cgame_stats_dump.md caveat)
	CG_Printf( " sv_fps         : %i\n", atoi( Info_ValueForKey( sys, "sv_fps" ) ) );
	CG_Printf( " g_synch        : %i\n", atoi( Info_ValueForKey( sys, "g_synchronousClients" ) ) );
	CG_Printf( " com_maxFps     : %i\n", CG_DF_CvarInt( "com_maxfps" ) );
	CG_Printf( "\nPerformance\n" );
	CG_Printf( " Max Speed      : %i ups%s\n", (int)statMaxSpeed, CG_DF_StatSuffixSpeed( statMaxSpeed ) );
	CG_Printf( " Average Speed  : %i ups%s\n", (int)avg, CG_DF_StatSuffixSpeed( avg ) );
	CG_Printf( " Distance Run   : %i units", (int)statDistance );
	if ( df_unit.integer == 1 ) {
		CG_Printf( " (%i meters)", (int)CG_ConvertDistance( statDistance ) );
	} else if ( df_unit.integer == 2 ) {
		CG_Printf( " (%i miles)", (int)CG_ConvertDistance( statDistance ) );
	}
	CG_Printf( "\n" );
}

// the "stats" server command (finish handler, non-variant servers)
void CG_DF_StatsCommand( void ) {
	const playerState_t *ps = cg.nextSnap ? &cg.nextSnap->ps : ( cg.snap ? &cg.snap->ps : NULL );
	int		ms = 0;

	if ( ps ) {
		CG_DF_RaceTime( ps, &ms );
	}
	CG_DF_PrintStats( ms );
	runActive = qfalse;
}


// ---- svinfo_report (cgame 0x4795): writes a server/run information report to system/reports/<name>.txt ----
static const char *dfGametypeNames[8] = {
	"Auto-config mode", "Defrag", "Tricks Mode", "Fast Caps", "Reserved", "MP Defrag", "MP Tricks Mode", "MP Fast Caps"
};

static void CG_DF_ReportPrintf( fileHandle_t f, const char *fmt, ... ) Q_PRINTF_FUNC(2,3);
static void CG_DF_ReportPrintf( fileHandle_t f, const char *fmt, ... ) {
	va_list	argptr;
	char	text[4096];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	trap_FS_Write( text, strlen( text ), f );
}

// "\key\value\key\value" -> "%-24s %s" lines
static void CG_DF_ReportInfo( fileHandle_t f, const char *info ) {
	char	key[BIG_INFO_KEY], value[BIG_INFO_VALUE];
	int		n = 0;

	if ( *info == '\\' ) {
		info++;
	}
	while ( *info && n++ < 64 ) {
		char *o = key;

		while ( *info && *info != '\\' && o < key + sizeof( key ) - 1 ) {
			*o++ = *info++;
		}
		*o = 0;
		if ( *info ) {
			info++;
		}
		o = value;
		while ( *info && *info != '\\' && o < value + sizeof( value ) - 1 ) {
			*o++ = *info++;
		}
		*o = 0;
		if ( *info ) {
			info++;
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", key, value );
	}
}

// pairs up the checksum list and the name list of sv_paks / sv_referencedPaks
static void CG_DF_ReportPaks( fileHandle_t f, const char *sums, const char *names ) {
	const char	*s = sums, *n = names;
	char	*cs, *nm;

	for ( ;; ) {
		cs = COM_Parse( &s );
		nm = COM_Parse( &n );
		if ( !cs[0] && !nm[0] ) {
			break;
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", cs, nm );
	}
}

void CG_DF_SvinfoReport_f( void ) {
	char		arg[MAX_QPATH], name[MAX_QPATH], path[MAX_QPATH + 16], si[BIG_INFO_STRING], sys[BIG_INFO_STRING];
	char		vers[32], servername[64];
	fileHandle_t f;
	qtime_t		tm;
	int			i, v, gt, mode, obs;
	const char	*physics;

	trap_Argv( 1, arg, sizeof( arg ) );
	if ( !arg[0] ) {
		CG_Printf( "usage: svinfo_report [file]\n" );
		return;
	}
	// file name: keep it inside system/reports/ (the original's name filter 0x153d is not decoded)
	for ( i = 0; arg[i] && i < (int)sizeof( name ) - 5; i++ ) {
		name[i] = ( arg[i] == '/' || arg[i] == '\\' || arg[i] == ':' || arg[i] == '.' ) ? '_' : arg[i];
	}
	name[i] = 0;
	Com_sprintf( path, sizeof( path ), "system/reports/%s.txt", name );
	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		CG_Printf( "SVI_WriteReport(): failed to open file for writing.\n" );
		return;
	}

	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	Q_strncpyz( servername, Info_ValueForKey( si, "defrag_servername" ), sizeof( servername ) );
	trap_RealTime( &tm );
	v = atoi( Info_ValueForKey( si, "defrag_vers" ) );
	if ( v > 0 && v < 1000 ) {
		v *= 100;
	}
	gt = atoi( Info_ValueForKey( si, "defrag_gametype" ) );
	mode = Info_ValueForKey( si, "defrag_mode" )[0] ? atoi( Info_ValueForKey( si, "defrag_mode" ) ) : -1;
	obs = Info_ValueForKey( si, "defrag_obs" )[0] ? atoi( Info_ValueForKey( si, "defrag_obs" ) ) : -1;
	physics = ( cg.snap && ( cg.snap->ps.pm_flags & PMF_PROMODE ) ) ? "cpm" : "vq3";

	CG_DF_ReportPrintf( f, "=\r\n= Report for %s (%04i-%02i-%02i)\r\n=\r\n\r\n", servername, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday );
	CG_DF_ReportPrintf( f, "*** Info\r\n" );
	CG_DF_ReportPrintf( f, "%-24s %s\r\n", "validation", dfViolationCount ? "failed" : "passed" );
	CG_DF_ReportPrintf( f, "%-24s %s\r\n", "map", Info_ValueForKey( si, "mapname" ) );
	if ( v ) {
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "gametype", dfGametypeNames[gt >= 0 && gt < 8 ? gt : 0] );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "physics", physics );
		if ( mode != -1 ) {
			CG_DF_ReportPrintf( f, "%-24s %i\r\n", "mode", mode );
		}
		if ( obs != -1 ) {
			CG_DF_ReportPrintf( f, "%-24s %i\r\n", "overbounces", obs );
		}
	} else {
		CG_DF_ReportPrintf( f, "%-24s %i\r\n", "gametype", gt );
	}
	CG_DF_ReportPrintf( f, "%-24s %s\r\n", "player", cg.snap ? cgs.clientinfo[cg.snap->ps.clientNum].name : "" );
	if ( v ) {
		Com_sprintf( vers, sizeof( vers ), "%i.%02i.%02i", v / 10000, ( v / 100 ) % 100, v % 100 );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "comp", Info_ValueForKey( si, "df_competition" ) );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "version", vers );
	} else {
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "gamename", Info_ValueForKey( sys, "gamename" ) );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "fs_game", Info_ValueForKey( sys, "fs_game" ) );
	}

	if ( dfViolationCount ) {
		CG_DF_ReportPrintf( f, "\r\n*** Validation Report : %i\r\n", dfViolationCount );
		for ( i = 0; i < dfViolationCount; i++ ) {
			CG_DF_ReportPrintf( f, "%s\r\n", dfViolationLog[i] );
		}
	}
	CG_DF_ReportPrintf( f, "\r\n*** Server Info\r\n" );
	CG_DF_ReportInfo( f, si );
	CG_DF_ReportPrintf( f, "\r\n*** System Info\r\n" );
	CG_DF_ReportInfo( f, sys );
	CG_DF_ReportPrintf( f, "\r\n*** sv_referencedPaks\r\n" );
	CG_DF_ReportPaks( f, Info_ValueForKey( sys, "sv_referencedPaks" ), Info_ValueForKey( sys, "sv_referencedPakNames" ) );
	CG_DF_ReportPrintf( f, "\r\n*** sv_paks\r\n" );
	CG_DF_ReportPaks( f, Info_ValueForKey( sys, "sv_paks" ), Info_ValueForKey( sys, "sv_pakNames" ) );

	trap_FS_FCloseFile( f );
	CG_Printf( "report written to %s\n", path );
}


// ---- authentication reply (cgame 0x4337 / hash 0x11f9), see disasm/G_CLIENT/auth_handshake.md ----
// The server (df_auth 1, non-dedicated DeFRaG-style servers) sends "auth_request <token> <mapChecksum>"; the client
// answers "auth_reply <hash><random decoy digit>" or is dropped. The bsp-checksum comparison of the second argument
// (cgame 0x6222) is inactive for defrag_gametype < 5 and is not ported.
static int CG_DF_AuthHash( int token, const char *str ) {
	int				sum = 0;
	int				d;
	const byte		*p;

	if ( !*str ) {
		return 0;
	}
	if ( token < 0x14d || token > 0x18405 ) {
		return 0;
	}
	d = (signed char)( ( token - ( ( token % 100 ) / 10 ) ) % 10 );
	p = (const byte *)str;
	do {
		sum += ( *p - 3 ) - d;
		p++;
	} while ( *p );
	return token - 3 - 3 * sum;
}

void CG_DF_AuthRequest( int token ) {
	int h = CG_DF_AuthHash( token, va( "%i.%02i.%02i", 1, 91, 31 ) );

	if ( h ) {
		trap_SendClientCommand( va( "auth_reply %i%i", h, rand() % 10 ) );
	}
}

void CG_DF_Reset( void ) {
	cpLast = -1;
	memset( cpTimes, 0, sizeof( cpTimes ) );
	dfReported = 0;
}

// called for every snapshot transition with the previous and the new player state
void CG_DF_SnapshotTransition( const playerState_t *ops, const playerState_t *ps ) {
	int	changed = ops->stats[STAT_DEFRAG_FLAGS] ^ ps->stats[STAT_DEFRAG_FLAGS];
	int	ms;

	if ( ops->clientNum != ps->clientNum ) {
		CG_DF_Reset();
		return;
	}
	CG_DF_Validate( ops, ps );
	CG_DF_TrackStats( ops, ps, ops->stats[STAT_DEFRAG_FLAGS] ^ ps->stats[STAT_DEFRAG_FLAGS] );
	// bit 0x4: run (re)started
	if ( ( changed & 4 ) && ( ps->stats[STAT_DEFRAG_FLAGS] & 4 ) ) {
		CG_DF_Reset();
		CG_DF_ResetStats();
		runActive = qtrue;
	}
	// bit 0x8 toggles at the finish: DeFRaG's own 0x1b10 prints the stats there on defrag_gametype >= 5 servers
	// (other servers get the "stats" command instead)
	if ( ( changed & 8 ) && runActive && CG_DF_RaceTime( ps, &ms ) ) {
		CG_DF_PrintStats( ms );
		runActive = qfalse;
	}
	// bit 0x10 toggles once per checkpoint (event 0x80)
	if ( ( changed & 0x10 ) && cpLast < DF_MAX_CHECKPOINTS - 1 && CG_DF_RaceTime( ps, &ms ) ) {
		cpTimes[++cpLast] = ms;
		if ( df_drawCheckpointEvents.integer ) {
			CG_CenterPrint( CG_DF_CheckpointText( cpLast ), 200, 10 );
		}
	}
}

// HUD list of the last 8 checkpoints, newest at the bottom, right aligned (cgame 0x5153).
// Placement is ours: the DeFRaG caller's x/y (0x2a12) are not decoded.
void CG_DF_DrawCheckpointList( void ) {
	int		i, first, w;
	float	y;
	const char *text;

	if ( !df_drawCheckpointStats.integer || cpLast < 0 || cpTimes[0] == 0 ) {
		return;
	}
	first = ( cpLast > 7 ) ? cpLast - 7 : 0;
	y = SCREEN_HEIGHT * 0.5f;		// the loop below walks oldest->newest and moves up (as 0x5153 does)
	for ( i = first; i <= cpLast; i++ ) {
		text = CG_DF_CheckpointText( i );
		w = (int)strlen( text );
		CG_DrawStringExt( SCREEN_WIDTH - 8 - 16 * w, (int)y, text, colorWhite, qfalse, qtrue, 16, 16, 0 );
		y -= 16.0f;
	}
}
