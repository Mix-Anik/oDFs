// cg_defrag.c -- DeFRaG client-side race tracking: checkpoint popup and HUD list
//
// Ported from the DeFRaG cgame (see disasm/CGAME/cgame_cvar_consumers.md): 0x50b6 popup, 0x5153 HUD list,
// 0x1c8b event derivation (stats[12] bit toggles), 0x5248 time capture.
#include "cg_local.h"

vmCvar_t	df_checkpoints;				// 2 = delta vs reference run, 3 = total time, else split time
vmCvar_t	df_drawCheckpointEvents;	// popup on each checkpoint
vmCvar_t	df_drawCheckpointStats;		// list of the last checkpoints on the HUD

int			cg_dfOptionsSet;				// "df_options" received
int			cg_dfOptions;				// 0x11d2b8: set by the "df_options" server command (unused by DeFRaG's own qagame)

// Race time published by the server in ps.stats[7]/[8] (qagame func_0x38ba). stats[12] bit 2 = timer initialised. Plain milliseconds
// (< 2^22) on dedicated servers or with sv_cheats; otherwise the obfuscated form of qagame 0x3f0c (G_EncodeRaceTime), undone here (cgame 0x5fd7):
// undo the playerstate xors (origin.x, velocity.x, health, movementDir), the byte chain, the level-time/map salt and the replicated nibble, then
// verify the 6-bit checksum. Returns qfalse (ms = 0) when the timer is not running or the value fails the check.
static byte CG_DF_MapSalt( void ) {
	static byte		mapSalt;
	static char		saltMap[MAX_QPATH];
	char			map[MAX_QPATH];

	Q_strncpyz( map, Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ), sizeof( map ) );
	Q_strlwr( map );
	if ( strcmp( map, saltMap ) ) {
		const char *c;

		Q_strncpyz( saltMap, map, sizeof( saltMap ) );
		mapSalt = 0;
		for ( c = map; *c; c++ ) {
			mapSalt += (byte)*c;
		}
	}
	return mapSalt;
}

// 0x5fd7 core: e = the two stats words, t = the time the server scrambled it with
static qboolean CG_DF_DecodeCore( unsigned int e, unsigned int t, float originX, float velX, int health, int moveDir, byte mapSalt, int *ms ) {
	unsigned int	x, r, a;
	int				i, v;

	e ^= (unsigned int)( moveDir & 0xf ) << 28;
	e ^= ( health > 0 ) ? (unsigned int)( health & 0xff ) : 0x96u;
	v = (int)floor( velX );
	e ^= (unsigned int)( v < 0 ? -v : v ) << 16;
	v = (int)floor( originX );
	e ^= (unsigned int)( v < 0 ? -v : v ) & 0xffff;
	for ( i = 2; i >= 0; i-- ) {
		( (byte *)&e )[i + 1] ^= ( (byte *)&e )[i];
	}
	x = t << 2;
	x += ( (unsigned int)mapSalt + DF_PROTOCOL_SALT ) << 8;
	x ^= t << 24;
	e ^= x;
	r = e >> 28;
	r |= ( ~r << 4 ) & 0xff;
	r |= r << 8;
	r |= r << 16;
	e = ( e & 0xf0000000u ) | ( ( e ^ r ) & 0x0fffffffu );
	a = 0;
	for ( i = 0; i < 3; i++ ) {
		a += ( ( e & 0x3fffff ) >> ( 6 * i ) ) & 0x3f;
	}
	a += ( ( e & 0x3fffff ) >> 18 ) & 0xf;
	if ( ( ( e >> 22 ) & 0x3f ) != ( a & 0x3f ) ) {
		return qfalse;
	}
	*ms = (int)( e & 0x3fffff );
	return qtrue;
}

// Recorded demos on some engines carry snapshot times that are shifted against the game clock the time was scrambled with (seen: a constant
// -3072 ms in a listen-server demo; the shift is not part of DeFRaG). While a demo plays, after four consecutive undecodable snapshots the
// shift is searched (multiples of 1024 ms up to ~4.6 h either way, then single milliseconds within +-8 s) and accepted only if all four
// samples decode at the same shift and their times are consistent with each other.
typedef struct { int time; unsigned int e; float ox, vx; int hp, md; } dfTimeSample_t;
static dfTimeSample_t	dfSamples[4];
static int				dfSampleCount;
static qboolean			dfDemoOffValid;
static int				dfDemoOff, dfDemoFails;
static qboolean			dfSeenZero;		// the previous snapshot published time 0 (a run is about to start)

static qboolean CG_DF_SamplesDecode( int off, byte salt ) {
	int i, ms[4];

	for ( i = 0; i < 4; i++ ) {
		if ( !CG_DF_DecodeCore( dfSamples[i].e, (unsigned int)( dfSamples[i].time + off ), dfSamples[i].ox, dfSamples[i].vx,
				dfSamples[i].hp, dfSamples[i].md, salt, &ms[i] ) ) {
			return qfalse;
		}
	}
	// the samples start with the first snapshot of the run (a wrong shift can also decode, to a constant larger time, so the first value
	// must be a run that just started) and advance with the snapshot clock
	if ( ms[0] > 300 ) {
		return qfalse;
	}
	for ( i = 1; i < 4; i++ ) {
		int dms = ms[i] - ms[i - 1], dt = dfSamples[i].time - dfSamples[i - 1].time;

		if ( dms < dt - 20 || dms > dt + 20 ) {
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean CG_DF_FindDemoOffset( byte salt ) {
	int k, off;

	for ( k = -64; k <= 16384; k++ ) {
		if ( CG_DF_SamplesDecode( k * 1024, salt ) ) {
			dfDemoOff = k * 1024;
			return qtrue;
		}
	}
	for ( off = -8192; off <= 8192; off++ ) {
		if ( CG_DF_SamplesDecode( off, salt ) ) {
			dfDemoOff = off;
			return qtrue;
		}
	}
	return qfalse;
}

static int	dfDecodeTime;		// serverTime of the snapshot whose events are being processed (0 outside CG_DF_ReadSnapshot)

static qboolean CG_DF_RaceTime( const playerState_t *ps, int *ms ) {
	unsigned int	e;
	byte			salt;
	int				t;

	*ms = 0;
	if ( !( ps->stats[STAT_DEFRAG_FLAGS] & 2 ) ) {
		return qfalse;
	}
	e = ( (unsigned int)ps->stats[STAT_RACE_TIME_HI] << 16 ) | ( (unsigned int)ps->stats[STAT_RACE_TIME_LO] & 0xffff );
	if ( !e ) {
		dfSeenZero = qtrue;
		return qtrue;		// 0x5fd7: both words zero -> time 0
	}
	// DeFRaG 0x5fd7: the plain form is used by variant servers (defrag_gametype >= 5; this server also publishes it plain with sv_cheats,
	// as reported by the server), everything else is decoded. (The port used to guess 'plain' whenever the top 10 bits were zero; an
	// encoded value can hit that about once in a thousand snapshots, which showed up as a one-snapshot garbage time.)
	if ( atoi( CG_DF_Serverinfo( "defrag_gametype" ) ) >= 5 || CG_DF_Cheats() ) {
		*ms = (int)e;
		return qtrue;
	}
	salt = CG_DF_MapSalt();
	t = dfDecodeTime ? dfDecodeTime : cg.snap->serverTime;		// the snapshot being read (see CG_DF_ReadSnapshot), else the current one
	if ( !cg.demoPlayback ) {
		return CG_DF_DecodeCore( e, (unsigned int)t, ps->origin[0], ps->velocity[0], ps->stats[STAT_HEALTH], ps->movementDir, salt, ms );
	}
	if ( dfDemoOffValid ) {
		if ( CG_DF_DecodeCore( e, (unsigned int)( t + dfDemoOff ), ps->origin[0], ps->velocity[0], ps->stats[STAT_HEALTH], ps->movementDir, salt, ms ) ) {
			dfDemoFails = 0;
			return qtrue;
		}
		if ( ++dfDemoFails > 40 ) {		// the shift changed (seek / another segment): search again
			dfDemoOffValid = qfalse;
			dfSampleCount = 0;
		}
		return qfalse;
	}
	if ( CG_DF_DecodeCore( e, (unsigned int)t, ps->origin[0], ps->velocity[0], ps->stats[STAT_HEALTH], ps->movementDir, salt, ms ) ) {
		dfSampleCount = 0;
		return qtrue;
	}
	// collect consecutive failing snapshots, starting with the first one after the time was 0 (one sample per snapshot time)
	if ( dfSampleCount == 0 && !dfSeenZero ) {
		return qfalse;
	}
	if ( dfSampleCount == 0 || dfSamples[dfSampleCount - 1].time != t ) {
		dfSeenZero = qfalse;
		if ( dfSampleCount == 4 ) {
			memmove( &dfSamples[0], &dfSamples[1], 3 * sizeof( dfSamples[0] ) );
			dfSampleCount = 3;
		}
		dfSamples[dfSampleCount].time = t;
		dfSamples[dfSampleCount].e = e;
		dfSamples[dfSampleCount].ox = ps->origin[0];
		dfSamples[dfSampleCount].vx = ps->velocity[0];
		dfSamples[dfSampleCount].hp = ps->stats[STAT_HEALTH];
		dfSamples[dfSampleCount].md = ps->movementDir;
		dfSampleCount++;
		if ( dfSampleCount == 4 ) {
			qboolean found = CG_DF_FindDemoOffset( salt );

			dfSampleCount = 0;		// one search per run start; the next one waits for the time to be 0 again
			if ( found ) {
				dfDemoOffValid = qtrue;
				dfDemoFails = 0;
				return CG_DF_DecodeCore( e, (unsigned int)( t + dfDemoOff ), ps->origin[0], ps->velocity[0], ps->stats[STAT_HEALTH], ps->movementDir, salt, ms );
			}
		}
	}
	return qfalse;
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

// 0x59b4: the first time a code is seen it is LOGGED (the log is what svinfo_report / svinfo_summary list, up to 32 lines) and not printed;
// printing happens only when the log is full or not used.  A repeat is logged only for the cvar codes 12 / 13 / 14 (their callers pass arg1 = 1),
// and the speed / gravity / cvar-invalid / latched codes (0, 1, 13, 14, 15) are printed on every occurrence.
static void CG_DF_Violation( int code, const char *text ) {
	qboolean first = !( dfReported & ( 1 << code ) );
	qboolean forced = ( code == 0 || code == 1 || ( code >= 13 && code <= 15 ) ) ? qtrue : qfalse;
	qboolean logIt = ( first || code == 12 || code == 13 || code == 14 ) ? qtrue : qfalse;
	qboolean printIt = ( first || forced ) ? qtrue : qfalse;

	dfReported |= 1 << code;
	if ( logIt && dfViolationCount < 32 ) {
		Com_sprintf( dfViolationLog[dfViolationCount++], sizeof( dfViolationLog[0] ), "%s: %s", dfViolations[code].category, text );
	} else if ( printIt ) {
		CG_Printf( "^2%s: ^3%s\n", dfViolations[code].category, text );
	}
}

static float	dfCvarCache[16];			// 0x11d744.. / 0x11d76c..: last value looked at, per cvar
static qboolean	dfSysPassed, dfSrvPassed;	// 0x11d740 / 0x11d76c: the first pass of the systeminfo / serverinfo validator is done

// A cvar is looked at when it is first seen or when its value changed (0x5b89 / 0x5dd5); an acceptable one is listed as "Cvar: name value",
// an unacceptable one is reported (codes 13 / 14). `expected` NULL = information only.
static void CG_DF_CheckCvar( const char *info, const char *name, const char *expected, qboolean isFloat, int slot, qboolean passed ) {
	const char *v = Info_ValueForKey( info, name );
	float		val;

	if ( !v[0] ) {
		CG_DF_Violation( 12, va( dfViolations[12].format, name ) );
		return;
	}
	val = isFloat ? atof( v ) : (float)atoi( v );
	if ( passed && val == dfCvarCache[slot] ) {
		return;
	}
	dfCvarCache[slot] = val;
	if ( expected && val != (float)atof( expected ) ) {
		CG_DF_Violation( isFloat ? 14 : 13, isFloat ? va( dfViolations[14].format, name, val ) : va( dfViolations[13].format, name, (int)val ) );
	} else if ( isFloat ) {
		CG_Printf( "^2Cvar: ^3%s %.2f\n", name, val );
	} else {
		CG_Printf( "^2Cvar: ^3%s %i\n", name, (int)val );
	}
}

// 0x5c8c..0x5d07: defrag_svfps / defrag_clfps must lie in 60..140 (reported under the cvar names sv_fps / com_maxfps)
static void CG_DF_CheckRange( const char *sys, const char *key, const char *shown, int slot ) {
	const char *v = Info_ValueForKey( sys, key );
	int			val;

	if ( !v[0] ) {
		CG_DF_Violation( 12, va( dfViolations[12].format, key ) );
		return;
	}
	val = atoi( v );
	if ( dfSysPassed && (float)val == dfCvarCache[slot] ) {
		return;
	}
	dfCvarCache[slot] = (float)val;
	if ( val < 60 || val > 140 ) {
		CG_DF_Violation( 13, va( dfViolations[13].format, shown, val ) );
	} else {
		CG_Printf( "^2Cvar: ^3%s %i\n", shown, val );
	}
}

// cached server version (0x11d29c): defrag_vers, x100 below 1000
static int CG_DF_ServerVersionNumber( void ) {
	int v = atoi( CG_DF_Serverinfo( "defrag_vers" ) );

	return v < 1000 ? v * 100 : v;
}

// ---- map BSP checksum check (cgame 0x61c6 / 0x6222; the server side is qagame 0x2d02) ----
// The server publishes the BSP checksum of its map as defrag_bspcrc (systeminfo) and every qagame stores the checksum of the maps it
// loads in system/checksums/<map>.crc.  At cgame init that local record is read (not on the multiplayer gametypes); a missing or
// unreadable record is reported as "Bsp crc: missing local record", and a server value that differs from the record as
// "Bsp crc: checksum failed".
static qboolean	dfBspRecord;		// 0x11d794
static int		dfBspCrc;			// 0x11d798

static void CG_DF_LoadBspRecord( void ) {		// 0x61c6
	fileHandle_t	f;
	char			buf[33];
	int			len;

	if ( !df_validityChecks.integer || CG_DF_Multiplayer() ) {		// (df_validityChecks 0 switches all run-validity checks off)
		return;
	}
	len = trap_FS_FOpenFile( va( "system/checksums/%s.crc", Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ) ), &f, FS_READ );
	if ( !f || len < 0 || len > 32 ) {
		dfBspRecord = qfalse;
		CG_DF_Violation( 6, dfViolations[6].format );
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		return;
	}
	memset( buf, 0, sizeof( buf ) );
	trap_FS_Read( buf, len, f );
	trap_FS_FCloseFile( f );
	dfBspCrc = atoi( buf );
	dfBspRecord = qtrue;
}

static void CG_DF_CheckBspCrc( int serverCrc ) {		// 0x6222
	if ( dfBspRecord && serverCrc != dfBspCrc ) {
		CG_DF_Violation( 5, dfViolations[5].format );
	}
}

// the server settings DeFRaG requires (cgame 0x5b89 systeminfo / 0x5dd5 serverinfo validators).  The speed / gravity / knockback / fps lines
// need a server version >= 18100; sv_fps / com_maxfps are checked on the non-multiplayer gametypes, sv_maxclients on those with
// 18100 <= version < 19114.
static void CG_DF_CheckServerSettings( void ) {
	char	sys[BIG_INFO_STRING], srv[MAX_INFO_STRING];
	int		ver = CG_DF_ServerVersionNumber();
	qboolean mp = CG_DF_Multiplayer();

	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	Q_strncpyz( srv, CG_ConfigString( CS_SERVERINFO ), sizeof( srv ) );
	CG_DF_CheckCvar( sys, "sv_cheats", "0", qfalse, 0, dfSysPassed );
	CG_DF_CheckCvar( sys, "timescale", "1", qtrue, 1, dfSysPassed );
	CG_DF_CheckCvar( sys, "g_synchronousClients", NULL, qfalse, 2, dfSysPassed );
	CG_DF_CheckCvar( sys, "pmove_fixed", NULL, qfalse, 3, dfSysPassed );
	CG_DF_CheckCvar( sys, "pmove_msec", NULL, qfalse, 4, dfSysPassed );
	if ( ver >= 18100 ) {
		if ( !mp ) {
			CG_DF_CheckRange( sys, "defrag_svfps", "sv_fps", 10 );
			CG_DF_CheckRange( sys, "defrag_clfps", "com_maxfps", 11 );
		}
		CG_DF_CheckCvar( sys, "g_speed", "320", qfalse, 5, dfSysPassed );
		CG_DF_CheckCvar( sys, "g_gravity", "800", qfalse, 6, dfSysPassed );
		CG_DF_CheckCvar( sys, "g_knockback", "1000", qfalse, 7, dfSysPassed );
		CG_DF_CheckBspCrc( atoi( Info_ValueForKey( sys, "defrag_bspcrc" ) ) );		// 0x5dc1
	}
	dfSysPassed = qtrue;
	if ( CG_DF_Serverinfo( "defrag_vers" )[0] || CG_DF_Serverinfo( "defrag_version" )[0] ) {
		CG_DF_CheckCvar( srv, "df_promode", NULL, qfalse, 8, dfSrvPassed );
	}
	CG_DF_CheckCvar( srv, "dmflags", "0", qfalse, 9, dfSrvPassed );
	if ( ver >= 18100 && ver < 19114 && !mp ) {
		CG_DF_CheckCvar( srv, "sv_maxclients", "1", qfalse, 12, dfSrvPassed );
	}
	if ( CG_DF_DefragFlags() & 8 ) {
		CG_DF_Violation( 8, va( dfViolations[8].format, va( "%i.%02i.%02i", ver / 10000, ( ver / 100 ) % 100, ver % 100 ) ) );
	}
	dfSrvPassed = qtrue;
}

static int	dfValStartSeen, dfValLastGravity, dfValLastSpeed, dfValHasteMemo;		// 0x11d790 / 0x11d784 / 0x11d780 / 0x11d788
static int	dfSnapDelta;			// serverTime of the new snapshot minus the previous one (set by CG_DF_ScriptEvents)

// 0x6237: the per-snapshot checker.  The event bits are the toggles derived by 0x1c8b, which are not produced in a snapshot carrying a
// respawn (team / spawn count change).
static void CG_DF_Validate( const playerState_t *ops, const playerState_t *ps ) {
	qboolean	respawn = ( ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT] || ps->persistant[PERS_TEAM] != ops->persistant[PERS_TEAM] ) ? qtrue : qfalse;
	int			changed = respawn ? 0 : ( ops->stats[STAT_DEFRAG_FLAGS] ^ ps->stats[STAT_DEFRAG_FLAGS] );
	int			expected;
	char		info[MAX_INFO_STRING];

	if ( !df_validityChecks.integer ) {
		return;
	}
	CG_DF_CheckServerSettings();

	if ( !CG_DF_Multiplayer() ) {
		if ( dfSnapDelta != 0 && dfSnapDelta < 7 ) {		// 0x6262: snapshots closer than 7 ms
			CG_DF_Violation( 9, va( dfViolations[9].format, 1000 / dfSnapDelta ) );
		}
		if ( ps->stats[STAT_DEFRAG_FLAGS] & 0x20 ) {
			CG_DF_Violation( 10, dfViolations[10].format );
		}
		if ( ps->stats[STAT_DEFRAG_FLAGS] & 0x40 ) {
			CG_DF_Violation( 11, dfViolations[11].format );
		}
		// a finish without ever having seen the start (joined mid-run)
		if ( changed & 4 ) {
			dfValStartSeen = 1;
		} else if ( ( changed & 8 ) && !dfValStartSeen ) {
			CG_DF_Violation( 4, dfViolations[4].format );
		}
	}

	if ( ps->stats[STAT_HEALTH] > 0 && ps->viewangles[ROLL] != 0 ) {
		CG_DF_Violation( 17, dfViolations[17].format );
	}
	if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR || ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0 ) {
		return;
	}
	if ( !ps->gravity && !ps->speed && !ps->velocity[0] && !ps->velocity[1] && !ps->velocity[2] ) {
		return;		// frozen player state
	}
	if ( !dfValLastGravity ) {
		dfValLastGravity = 800;
	}
	if ( ps->gravity != 800 && ps->gravity != dfValLastGravity ) {
		CG_DF_Violation( 1, va( dfViolations[1].format, ps->gravity, 800 ) );
		dfValLastGravity = ps->gravity;		// only remembered when it was reported (0x6364)
	}

	if ( ps->powerups[PW_HASTE] ) {
		dfValHasteMemo = ps->powerups[PW_HASTE];
	}
	expected = ( ( dfDecodeTime ? dfDecodeTime : ( cg.snap ? cg.snap->serverTime : 0 ) ) < dfValHasteMemo + 250 && ( dfDecodeTime || cg.snap ) ) ? 416 : 320;
	if ( ps->speed != 320 && ps->speed != expected && ps->speed != dfValLastSpeed ) {
		CG_DF_Violation( 0, va( dfViolations[0].format, ps->speed, expected ) );
	}
	dfValLastSpeed = ps->speed;

	// pm_flags promode bit must match the reported df_promode
	Q_strncpyz( info, CG_ConfigString( CS_SERVERINFO ), sizeof( info ) );
	if ( ( ( ps->pm_flags & PMF_PROMODE ) != 0 ) != ( atoi( Info_ValueForKey( info, "df_promode" ) ) != 0 ) ) {
		CG_DF_Violation( 16, dfViolations[16].format );
	}
}


// ---- end-of-run statistics (cgame 0x4388 dump, trackers 0x3a4a / 0x4677) ----
// Not ported: the per-axis counters (0x11d238/3c/40).
vmCvar_t	df_noEndOfRunStats;

static qboolean	runActive;			// 0x11d288
static qboolean	dfFinishPrint;		// a finish was seen at snapshot read time: print the stats block when the snapshot becomes current
static int			dfFinishPrintMs;
static float	statMaxSpeed;		// 0x11d230
static float	statDistance;		// 0x11d234
static int		statHealthSpent, statArmorSpent, statShots;		// 0x11d238 / 3c / 40 (cgame 0x4677)
static int		statStartTime;		// race time at the last start (ms)
// jump tracker (cgame 0x458e), globals 0x11d244..0x11d264
static float	jumpDist, jumpMaxRise, jumpLongest, jumpHighest;	// 0x11d244 / 248 / 24c / 250
static vec3_t	jumpTakeoff;		// 0x11d258
static int		jumpTracking;		// 0x11d254
static int		jumpTeleBit;		// 0x11d264

static float CG_DF_HorizontalLength( const vec3_t v ) {
	return sqrt( v[0] * v[0] + v[1] * v[1] );
}

static void CG_DF_ResetStats( void ) {
	statMaxSpeed = 0;
	statDistance = 0;
	statHealthSpent = statArmorSpent = statShots = 0;
	jumpLongest = 0;		// 0x4370 clears 0x11d24c / 0x11d250 (not 0x11d244 / 0x11d248)
	jumpHighest = 0;
}

// cgame 0x4545: box trace 5 units below the snapshot origin (mins -15 -15 -24, maxs 15 15 32,
// mask SOLID|PLAYERCLIP|BODY); returns qtrue and the contact point when it hits something
static qboolean CG_DF_GroundContact( const playerState_t *ps, vec3_t out ) {
	static const vec3_t mins = { -15, -15, -24 }, maxs = { 15, 15, 32 };
	trace_t	tr;
	vec3_t	end;

	VectorCopy( ps->origin, end );
	end[2] -= 5;
	trap_CM_BoxTrace( &tr, ps->origin, end, mins, maxs, 0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY );
	if ( tr.fraction >= 1.0f ) {
		return qfalse;
	}
	VectorCopy( tr.endpos, out );
	return qtrue;
}

// cgame 0x458e, every snapshot: horizontal distance / rise since take-off; "Highest Jump" = max rise
// (the original also accumulates while 0xcfcd0 = cg.demoPlayback is set)
static void CG_DF_TrackJump( const playerState_t *ps ) {
	vec3_t	pos, d;
	int		onGround, tele;
	float	rise;

	onGround = CG_DF_GroundContact( ps, pos );
	tele = ps->eFlags & EF_TELEPORT_BIT;
	if ( tele != jumpTeleBit ) {
		if ( !onGround ) {
			VectorCopy( ps->origin, jumpTakeoff );
		}
		jumpTracking = onGround;
		jumpTeleBit = tele;
	}
	if ( jumpTracking ) {
		if ( onGround ) {
			VectorCopy( pos, jumpTakeoff );
		} else {
			jumpDist = jumpMaxRise = 0;
			jumpTracking = 0;
		}
	}
	if ( jumpTracking || ps->stats[STAT_HEALTH] <= 0 ) {		// 0x45d5: only computed while airborne (0x11d254 == 0)
		return;
	}
	VectorSubtract( onGround ? pos : ps->origin, jumpTakeoff, d );
	rise = d[2];
	d[2] = 0;
	jumpDist = VectorLength( d );
	if ( rise > jumpMaxRise || rise < -100.0f - jumpMaxRise / 4.0f ) {
		jumpMaxRise = rise;
	}
	if ( runActive || cg.demoPlayback ) {
		if ( jumpDist > jumpLongest ) {
			jumpLongest = jumpDist;
		}
		if ( jumpMaxRise > jumpHighest ) {
			jumpHighest = jumpMaxRise;
		}
	}
	if ( onGround ) {
		jumpTracking = 1;
	}
}

// DeFRaG 0x3a4a (called every frame from CG_DrawActiveFrame after the prediction): the horizontal speed of the PREDICTED player state,
// truncated to an integer (0x11d... 967152), is what the HUD speed widget shows; while a run is active the largest value is the 'Max Speed' statistic
int		dfSpeedInt;

void CG_DF_SpeedFrame( void ) {
	vec3_t	v;

	VectorSet( v, cg.predictedPlayerState.velocity[0], cg.predictedPlayerState.velocity[1], 0 );
	dfSpeedInt = (int)VectorLength( v );
	if ( runActive && statMaxSpeed < (float)dfSpeedInt ) {
		statMaxSpeed = (float)dfSpeedInt;
	}
}

// per snapshot while a run is active: horizontal distance travelled
static void CG_DF_TrackStats( const playerState_t *ops, const playerState_t *ps, int changed ) {
	vec3_t	d;

	CG_DF_TrackJump( ps );
	if ( !runActive ) {
		return;
	}
	if ( ps->stats[STAT_HEALTH] <= 0 || ( changed & 1 ) ) {		// dead, or a target_init event (0x20)
		return;
	}
	VectorSubtract( ps->origin, ops->origin, d );
	statDistance += CG_DF_HorizontalLength( d );
	// 0x46dd: health lost between the two snapshots; the armor counter (0x11d23c) is DeFRaG's own bug: it adds (prev - prev) = 0
	if ( ops->stats[STAT_HEALTH] > ps->stats[STAT_HEALTH] ) {
		statHealthSpent += ops->stats[STAT_HEALTH] - ps->stats[STAT_HEALTH];
	}
	// 0x4731: same weapon, ammo dropped by exactly 1 -> one shot fired
	if ( ops->weapon == ps->weapon && ops->ammo[ops->weapon] == ps->ammo[ps->weapon] + 1 ) {
		statShots++;
	}
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

// prints the "Stats" block; runMs = elapsed run time for the average speed
void CG_DF_PrintStats( int runMs ) {
	char	sys[BIG_INFO_STRING];
	const char *type;
	int		msec, svfps = 0, clfps = 0, ver;
	float	avg = runMs ? ( statDistance * 1000.0f ) / (float)runMs : 0;

	if ( df_noEndOfRunStats.integer ) {
		return;
	}
	// every value comes from the cache the original fills in 0x3a70 (systeminfo) / 0x3b5e (serverinfo): physics from df_promode, pmove_msec only
	// while pmove_fixed is set (0 otherwise), sv_fps / com_maxFps from the server mirrors defrag_svfps / defrag_clfps (versions >= 18100 only)
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	msec = atoi( Info_ValueForKey( sys, "pmove_fixed" ) ) ? atoi( Info_ValueForKey( sys, "pmove_msec" ) ) : 0;
	ver = atoi( CG_DF_Serverinfo( "defrag_vers" ) );
	if ( ver < 1000 ) {
		ver *= 100;
	}
	if ( ver >= 18100 ) {
		svfps = atoi( Info_ValueForKey( sys, "defrag_svfps" ) );
		clfps = atoi( Info_ValueForKey( sys, "defrag_clfps" ) );
	}
	type = atoi( CG_DF_Serverinfo( "df_promode" ) ) ? "cpm" : "vq3";

	CG_Printf( "----- ^2Stats^7 -----\nPhysics\n" );
	CG_Printf( " type           : %s\n", type );
	CG_Printf( " pmove_fixed    : %i\n", msec != 0 );
	CG_Printf( " pmove_msec     : %i\n", msec );
	CG_Printf( " sv_fps         : %i\n", svfps );
	CG_Printf( " g_synch        : %i\n", atoi( Info_ValueForKey( sys, "g_synchronousClients" ) ) );
	CG_Printf( " com_maxFps     : %i\n", clfps );
	CG_Printf( "\nPerformance\n" );
	CG_Printf( " Max Speed      : %i ups%s\n", (int)statMaxSpeed, CG_DF_StatSuffixSpeed( statMaxSpeed ) );
	CG_Printf( " Average Speed  : %i ups%s\n", (int)avg, CG_DF_StatSuffixSpeed( (float)(int)avg ) );		// 0x43a0: the km/h and mph values are derived from the truncated integer
	CG_Printf( " Distance Run   : %i units", (int)statDistance );
	if ( df_unit.integer == 1 ) {
		CG_Printf( " (%i meters)", (int)CG_ConvertDistance( statDistance ) );
	} else if ( df_unit.integer == 2 ) {
		CG_Printf( " (%i miles)", (int)CG_ConvertDistance( statDistance ) );
	}
	CG_Printf( "\n" );
	CG_Printf( " Highest Jump   : %i units", (int)jumpHighest );
	if ( df_unit.integer == 1 ) {
		CG_Printf( " (%i meters)", (int)CG_ConvertDistance( jumpHighest ) );
	} else if ( df_unit.integer == 2 ) {
		CG_Printf( " (%i miles)", (int)CG_ConvertDistance( jumpHighest ) );
	}
	CG_Printf( "\n" );
	CG_Printf( " Farthest Jump  : %i units", (int)jumpLongest );		// 0x7e4c, value 0x11d24c
	if ( df_unit.integer == 1 ) {
		CG_Printf( " (%i meters)", (int)CG_ConvertDistance( jumpLongest ) );
	} else if ( df_unit.integer == 2 ) {
		CG_Printf( " (%i miles)", (int)CG_ConvertDistance( jumpLongest ) );
	}
	CG_Printf( "\n" );
	CG_Printf( " Health Used    : %i\n", statHealthSpent );
	CG_Printf( " Armor Used     : %i\n", statArmorSpent );		// always 0 in DeFRaG (see CG_DF_TrackStats)
	CG_Printf( " Shots Fired    : %i\n", statShots );
	CG_Printf( "-----------------\n" );
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


// ---- jump meters (cgame 0x2e7c..0x2fcc, flags df_drawJumpmeters 0x11c210: 1 height row, 2 distance row, 4 auto-hide) ----
vmCvar_t	df_drawJumpmeters;
static int	meterH, meterD;			// 0x11ccdc / 0x11cce0 latched values
static int	meterExpiry, meterShow;	// 0x11cce4 / 0x11cce8

static const char *CG_DF_MeterText( float value ) {
	// 0x4247(value, df_unit == 1): metres or plain units
	if ( df_unit.integer == 1 ) {
		return va( "%im", (int)CG_ConvertDistance( value ) );
	}
	return va( "%iu", (int)value );
}

static void CG_DF_MeterRow( float *y, const char *label, int labelX, int value ) {
	vec4_t		color;
	const char	*s;

	color[0] = color[1] = color[2] = color[3] = 1.0f;
	CG_DrawStringExt( labelX, (int)( *y + 2 ), label, color, qfalse, qtrue, 16, 16, 0 );
	*y += 16;
	s = CG_DF_MeterText( (float)value );
	CG_DrawStringExt( 635 - 16 * (int)strlen( s ), (int)( *y + 2 ), s, color, qfalse, qtrue, 16, 16, 0 );
	*y += 20;
}

void CG_DF_DrawJumpMeters( float y ) {
	int		flags = df_drawJumpmeters.integer;
	int		big = jumpMaxRise > 100.0f || jumpMaxRise < -100.0f || jumpDist > 400.0f;

	if ( !flags ) {
		return;
	}
	if ( !( flags & 4 ) ) {
		meterH = (int)jumpMaxRise;
		meterD = (int)jumpDist;
	} else if ( !meterShow ) {
		if ( !jumpTracking && big ) {
			meterShow = 1;
		}
	} else if ( meterExpiry ) {
		if ( !jumpTracking && big ) {
			meterExpiry = 0;
		}
		if ( meterExpiry < cg.time ) {
			meterExpiry = 0;
			meterShow = 0;
		}
	} else if ( jumpTracking || ( jumpMaxRise < 100.0f && jumpMaxRise > -100.0f && jumpDist < 400.0f ) ) {
		meterExpiry = cg.time + 1000;
	} else {
		meterH = (int)jumpMaxRise;
		meterD = (int)jumpDist;
	}
	if ( ( flags & 1 ) && ( !( flags & 4 ) || meterShow ) ) {
		CG_DF_MeterRow( &y, "Height", 539, meterH );
	}
	if ( ( flags & 2 ) && ( !( flags & 4 ) || meterShow ) ) {
		CG_DF_MeterRow( &y, "Distance", 507, meterD );
	}
}

// ---- keymaps (cgame 0xa0be): while df_keymaps is 1, every frame the console command stored in the cvar of each pressed movement
// direction is executed (df_keymaps_forward/back/moveleft/moveright/moveup/movedown).  The right/left mapping is DeFRaG's literal one:
// rightmove > 0 runs df_keymaps_moveleft, rightmove < 0 runs df_keymaps_moveright.
vmCvar_t	df_keymaps;
vmCvar_t	df_keymaps_dir[6];

void CG_DF_Keymaps( void ) {
	usercmd_t	cmd;
	char		buf[1024];
	int			i, v;

	if ( df_keymaps.integer != 1 ) {
		return;
	}
	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	for ( i = 0; i < 6; i++ ) {
		switch ( i ) {
		case 0: v = cmd.forwardmove > 0; break;
		case 1: v = cmd.forwardmove < 0; break;
		case 2: v = cmd.rightmove > 0; break;
		case 3: v = cmd.rightmove < 0; break;
		case 4: v = cmd.upmove > 0; break;
		default: v = cmd.upmove < 0; break;
		}
		if ( !v ) {
			continue;
		}
		Q_strncpyz( buf, df_keymaps_dir[i].string, sizeof( buf ) );
		if ( buf[0] ) {
			trap_SendConsoleCommand( va( "%s\n", buf ) );
		}
	}
}

// ---- cvar transitions (cgame 0x9d7f, cvar df_transitions): smoothly animate cvars -------------------------------------------------
// df_transitions is a list (COM_Parse tokens, at most 5) of cvar NAMES; each of those cvars holds a six-token definition
//   <targetCvar> <from> <to> <rate> <mode> <progress>
// from/to/rate/mode accept "$cvar" references.  Every frame progress advances by |rate|/100 * dt (rate < 0: units per second, scaled by
// 1000/|to-from|), the target cvar is set to from + progress*(to-from) (clamped between the two) and the definition cvar is rewritten
// with the new progress.  mode 0 = once, 1 = restart when finished, 2 = ping-pong.
// DeFRaG sets the target through its `varMath` console command; oDFs sets the cvar directly.
vmCvar_t	df_transitions;
static int	trLastTime;			// 0x1203c8, used by 0x8e4a

static float CG_DF_TrToken( const char **p, char *raw, int rawSize ) {
	const char	*t = COM_Parse( p );
	char		buf[256];

	Q_strncpyz( raw, t, rawSize );
	Q_strncpyz( buf, t, sizeof( buf ) );
	CG_DF_ExpandVar( buf, sizeof( buf ) );
	return atof( buf );
}

void CG_DF_Transitions( void ) {
	char		list[1024], entry[1024], def[1024], target[256], raw[4][256], out[1024];
	const char	*lp, *dp, *tok;
	float		dt, a, b, rate, mode, prog, rem, v, lo, hi, per;
	int			count, i, bad;

	dt = 0.001f * ( cg.time - trLastTime );
	trLastTime = cg.time;
	Q_strncpyz( list, df_transitions.string, sizeof( list ) );
	if ( !list[0] || !strcmp( list, "0" ) ) {
		return;
	}
	lp = list;
	for ( count = 0; count < 5; count++ ) {
		tok = COM_Parse( &lp );
		if ( !tok[0] ) {
			break;
		}
		Q_strncpyz( entry, tok, sizeof( entry ) );
		trap_Cvar_VariableStringBuffer( entry, def, sizeof( def ) );
		if ( !def[0] ) {
			break;
		}
		dp = def;
		bad = 0;
		tok = COM_Parse( &dp );
		if ( !tok[0] ) bad = 1;
		Q_strncpyz( target, tok, sizeof( target ) );
		a = CG_DF_TrToken( &dp, raw[0], sizeof( raw[0] ) );
		if ( !raw[0][0] ) bad = 1;
		b = CG_DF_TrToken( &dp, raw[1], sizeof( raw[1] ) );
		if ( !raw[1][0] ) bad = 1;
		rate = CG_DF_TrToken( &dp, raw[2], sizeof( raw[2] ) );
		if ( !raw[2][0] ) bad = 1;
		mode = CG_DF_TrToken( &dp, raw[3], sizeof( raw[3] ) );
		if ( !raw[3][0] ) bad = 1;
		tok = COM_Parse( &dp );
		if ( !tok[0] ) bad = 1;
		prog = atof( tok );

		per = fabs( b - a );
		rem = per >= 1.0f ? 1000.0f / (float)(int)per : 1000.0f;		// (the original divides by zero when from == to)
		if ( prog >= 1.0f ) {
			if ( mode == 1.0f ) {
				prog = 0;
			} else if ( mode == 2.0f ) {
				v = a; a = b; b = v;
				Q_strncpyz( out, raw[0], sizeof( out ) );
				Q_strncpyz( raw[0], raw[1], sizeof( raw[0] ) );
				Q_strncpyz( raw[1], out, sizeof( raw[1] ) );
				prog = 0;
			}
		}
		if ( prog >= 1.0f || bad ) {
			continue;
		}
		v = ( fabs( rate ) / 100.0f ) * dt;
		prog += rate >= 0 ? v : v * rem;
		if ( prog < 0 ) prog = 0;
		if ( prog > 1.0f ) prog = 1.0f;
		v = a + prog * ( b - a );
		if ( mode == 0 ) {
			if ( prog <= 0 ) v = a;
			if ( prog >= 1.0f ) v = b;
		}
		lo = a < b ? a : b;
		hi = a < b ? b : a;
		if ( v < lo ) v = lo;
		if ( v > hi ) v = hi;
		trap_Cvar_Set( target, va( "%f", v ) );
		Com_sprintf( out, sizeof( out ), "%s %s %s %s %s %f", target, raw[0], raw[1], raw[2], raw[3], prog );
		trap_Cvar_Set( entry, out );
		(void)i;
	}
}

// ---- console commands savepos (0xae76), displaymessage (0xaf72), resetjumpmeter (0x16282) ----------------------------------------

// savepos [option]: stores a `placeplayer x y z pitch yaw roll vx vy vz` command line in the cvar whose name is held by the cvar
// `saveposname` (set it first: `set saveposname mypos`); option 1 zeroes the velocity, option 2 also the pitch.  Replay it with `vstr mypos`.
void CG_DF_SavePos_f( void ) {
	char	arg[64], name[1024], line[512];
	vec3_t	pos, vel, ang;
	int		opt;

	trap_Argv( 1, arg, sizeof( arg ) );
	opt = atoi( arg );
	if ( !strcmp( arg, "?" ) ) {
		CG_Printf( "\n^1savepos: ^3Allows saving and restoring player position, angles, and velocity.\n^5Usage:   ^2savepos [ <option> ]\n^5Example: ^2see savepos documentation for details.\n\n" );
		return;
	}
	trap_Cvar_VariableStringBuffer( "saveposname", name, sizeof( name ) );
	if ( !name[0] ) {
		CG_Printf( "%s is empty. use: set %s <myposname> <options>\n", "saveposname", "saveposname" );
		return;
	}
	if ( !cg.snap ) {
		return;
	}
	VectorCopy( cg.snap->ps.origin, pos );
	VectorCopy( cg.snap->ps.velocity, vel );
	VectorCopy( cg.snap->ps.viewangles, ang );
	if ( opt == 1 ) {
		VectorClear( vel );
	} else if ( opt == 2 ) {
		VectorClear( vel );
		ang[0] = 0;
	}
	Com_sprintf( line, sizeof( line ), "placeplayer %f %f %f %f %f %f %f %f %f", pos[0], pos[1], pos[2], ang[0], ang[1], ang[2], vel[0], vel[1], vel[2] );
	trap_SendConsoleCommand( va( "set %s %s\n", name, line ) );
	CG_Printf( "^2\"%s\" Saved:^7 ( %.3f %.3f %.3f ) %.3f %.3f ( %i %i %i )\n", name, pos[0], pos[1], pos[2], ang[0], ang[1], (int)vel[0], (int)vel[1], (int)vel[2] );
}

// displaymessage <vertical_position> <char_size> <message...> : centerprint (the vanilla CG_CenterPrint)
void CG_DF_DisplayMessage_f( void ) {
	char	a1[64], a2[64], msg[1024], tmp[1024];
	int		i, n = 0, argc = trap_Argc();

	trap_Argv( 1, a1, sizeof( a1 ) );
	trap_Argv( 2, a2, sizeof( a2 ) );
	if ( !strcmp( a1, "?" ) || !a1[0] ) {
		CG_Printf( "\n^1displaymessage: ^3Centerprints a message on screen.\n^5Usage:          ^2displaymessage vertical_position char_size message\n^5Example:        ^2displaymessage 100 20 Hello There\n\n" );
		return;
	}
	msg[0] = 0;
	for ( i = 3; i < argc; i++ ) {
		int len;

		trap_Argv( i, tmp, sizeof( tmp ) );
		len = strlen( tmp );
		if ( n + len >= 1023 ) {
			break;
		}
		memcpy( msg + n, tmp, len );
		n += len;
		if ( i != argc - 1 ) {
			msg[n++] = ' ';
		}
	}
	msg[n] = 0;
	CG_CenterPrint( msg, atoi( a1 ), atoi( a2 ) );
}

// resetjumpmeter (0x16282): clears the jump history logger record only; the older jump / distance tracker behind the HUD meters and the stats block is untouched
void CG_DF_ResetJumpMeter_f( void ) {
	char	arg[64];

	trap_Argv( 1, arg, sizeof( arg ) );
	if ( !strcmp( arg, "?" ) || arg[0] ) {
		CG_Printf( "\n^2resetjumpmeter: ^3Resets jump histories and maximums.\n^5Usage:          ^2resetjumpmeter\n\n" );
		return;
	}
	CG_DF_JumpLogReset();
	CG_Printf( "Jumpmeter histories and maximums stats reset.\n" );
}

// ---- demo seek (cgame 0x7a4e): during demo playback  set df_demos_Seek M.SS  fast-forwards (timescale 100, sound muted) to the
// given level time (minutes.seconds, e.g. 1.30 = 1:30; the sign selects the comparison direction) and then restores timescale 1 and
// the volumes (saved in df_user_s_volume / df_user_s_musicvolume).  With M.00 the speed is 1000.
vmCvar_t	df_demos_Seek;
static int	seekActive;			// 0x161c

void CG_DF_DemoSeek( void ) {
	char	buf[64];
	float	a, seek = df_demos_Seek.value;
	int		cur, minutes, secs, target, speed, needMove;

	if ( !cg.demoPlayback ) {
		seekActive = 0;
		return;
	}
	if ( seek == 0 ) {
		if ( seekActive ) {
			trap_Cvar_Set( "timescale", "1" );
		}
		seekActive = 0;
		return;
	}
	cur = cg.time - cgs.levelStartTime;
	a = fabs( seek );
	minutes = (int)a;
	secs = (int)( 100.0f * ( a - (float)(int)a ) );
	target = 1000 * ( 60 * minutes + secs );
	needMove = ( seek < 0 ) ? ( target < cur ) : ( target > cur );
	speed = secs == 0 ? 1000 : 100;
	if ( needMove ) {
		if ( !seekActive ) {
			trap_Cvar_VariableStringBuffer( "s_volume", buf, sizeof( buf ) );
			trap_Cvar_Set( "df_user_s_volume", buf );
			trap_Cvar_VariableStringBuffer( "s_musicvolume", buf, sizeof( buf ) );
			trap_Cvar_Set( "df_user_s_musicvolume", buf );
			trap_Cvar_Set( "s_volume", "0.0" );
			trap_Cvar_Set( "s_musicvolume", "0.0" );
			trap_Cvar_Set( "timescale", va( "%i", speed ) );
			CG_Printf( "^1DEMOSEEK:^7 ^3Seeking To Target:^7 ^1%i^7 ^3Current:^7 ^1%i^7 ^3Speed:^7 ^1%i^7\n", target, cur, speed );
			seekActive = 1;
		}
		return;
	}
	if ( seekActive ) {
		trap_Cvar_Set( "timescale", "1" );
		trap_Cvar_VariableStringBuffer( "df_user_s_volume", buf, sizeof( buf ) );
		trap_Cvar_Set( "s_volume", buf );
		trap_Cvar_VariableStringBuffer( "df_user_s_musicvolume", buf, sizeof( buf ) );
		trap_Cvar_Set( "s_musicvolume", buf );
		CG_Printf( "^1DEMOSEEK:^7 ^3Target Reached:^7 ^1%i^7\n", cur );
	} else {
		CG_Printf( "^1DEMOSEEK:^7 ^3Target:^7 ^1%i^7 ^3Already Past:^7 ^1%i^7\n", target, cur );
	}
	trap_Cvar_Set( "df_demos_Seek", "0.00" );
	seekActive = 0;
}

// accessors for the CHS panels (items 210-217)
void CG_DF_GetStats( dfStats_t *o ) {
	o->jumpHeight = jumpMaxRise;
	o->jumpDist = jumpDist;
	o->tallest = jumpHighest;
	o->longest = jumpLongest;
	o->maxSpeed = statMaxSpeed;
	o->distance = statDistance;
	o->healthSpent = statHealthSpent;
	o->armorSpent = statArmorSpent;
	o->shotsFired = statShots;
}

// The "dfn" key of a client's configstring (the server's copy of its df_name) as DeFRaG's CG_NewClientInfo keeps it (cgame 0x253c6 -> 0xf48): at most 19
// characters, colour codes dropped, every character that is not valid in a file name removed, "UnnamedPlayer" when nothing is left. Read by the
// autorecord `$pl` token and by the player line of svinfo_report / the summary. `out` holds at least 20 bytes.
void CG_DF_ClientDfName( int clientNum, char *out ) {
	char		raw[20];
	const char	*s = raw;
	char		*d = out;

	Q_strncpyz( raw, Info_ValueForKey( CG_ConfigString( CS_PLAYERS + clientNum ), "dfn" ), sizeof( raw ) );
	for ( ; *s; s++ ) {
		int c = (byte)*s;

		if ( c == '^' && s[1] && s[1] != '^' ) {
			s++;
			continue;
		}
		if ( c > 32 && c < 126 && !strchr( ":*?\"<>|\\/.()[]", c ) ) {
			*d++ = c;
		}
	}
	*d = 0;
	if ( !out[0] ) {
		Q_strncpyz( out, "UnnamedPlayer", 20 );
	}
}

// ---- svinfo_report (cgame 0x4795): writes a server/run information report to system/reports/<name>.txt ----
static const char *dfGametypeNames[8] = {
	"Auto-config mode", "Defrag", "Tricks Mode", "Fast Caps", "Reserved", "MP Defrag", "MP Tricks Mode", "MP Fast Caps"
};

// 0x11d2ac (set by 0x3b5e): the "multiplayer" variant gametypes, version >= 18100 and defrag_gametype >= 5
qboolean CG_DF_Multiplayer( void ) {
	int v = atoi( CG_DF_Serverinfo( "defrag_vers" ) );

	if ( v < 1000 ) {
		v *= 100;
	}
	return ( v >= 18100 && atoi( CG_DF_Serverinfo( "defrag_gametype" ) ) >= 5 ) ? qtrue : qfalse;
}

// 0x153d: removes "^x" colour pairs (a '^' followed by a non-zero character other than '^'); every other byte, control characters included, is kept
static void CG_DF_StripColors( char *s ) {
	const char	*p = s;
	char		*o = s;

	while ( *p ) {
		if ( p[0] == '^' && p[1] && p[1] != '^' ) {
			p += 2;
			continue;
		}
		*o++ = *p++;
	}
	*o = 0;
}

// server facts the original caches in 0x3a70 / 0x3b5e / 0x3c33: detected flag 0x11d298, version 0x11d29c, gametype 0x11d2a4, mode 0x11d2a8
// (-1 below 18100), overbounces 0x11d2b4 (systeminfo defrag_obs, -1 below 18100), physics 0x11d2b0 (serverinfo df_promode)
typedef struct {
	qboolean	detected;
	int			ver, gt, mode, obs, promode;
} dfSvFacts_t;

static void CG_DF_SvFacts( const char *si, const char *sys, dfSvFacts_t *o ) {
	const char *vers = Info_ValueForKey( si, "defrag_vers" ), *legacy = Info_ValueForKey( si, "defrag_version" );

	o->detected = ( vers[0] || legacy[0] ) ? qtrue : qfalse;
	o->ver = 0;
	if ( vers[0] ) {
		o->ver = atoi( vers );
		if ( o->ver < 1000 ) {
			o->ver *= 100;
		}
	} else if ( legacy[0] ) {
		o->ver = 100 * atoi( legacy );
		if ( o->ver != 16100 ) {
			o->ver = 0;		// 0x3cbb: only 16100 is accepted, anything else is a violation (code 7)
		}
	}
	o->gt = atoi( Info_ValueForKey( si, "defrag_gametype" ) );
	o->mode = o->ver >= 18100 ? atoi( Info_ValueForKey( si, "defrag_mode" ) ) : -1;
	o->obs = o->ver >= 18100 ? atoi( Info_ValueForKey( sys, "defrag_obs" ) ) : -1;
	o->promode = atoi( Info_ValueForKey( si, "df_promode" ) ) != 0;
}

static void CG_DF_ReportPrintf( fileHandle_t f, const char *fmt, ... ) Q_PRINTF_FUNC(2,3);
static void CG_DF_ReportPrintf( fileHandle_t f, const char *fmt, ... ) {
	va_list	argptr;
	char	text[4096];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	CG_DF_StripColors( text );		// 0x15ab runs every line through the colour-code filter 0x153d
	trap_FS_Write( text, strlen( text ), f );
}

// "\key\value\key\value" -> "%-24s %s" lines
static void CG_DF_ReportInfo( fileHandle_t f, const char *info ) {
	char	key[BIG_INFO_KEY], value[BIG_INFO_VALUE];

	if ( *info == '\\' ) {
		info++;
	}
	while ( *info ) {
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

// 0x4a65..0x4b52: "*** <label>" block pairing the checksum list with the name list; the header is only written when both lists are non-empty, and
// the loop ends as soon as either side runs out.  (The first token is copied: COM_Parse returns a static buffer.)
static void CG_DF_ReportPaks( fileHandle_t f, const char *label, const char *sums, const char *names ) {
	const char	*s = sums, *n = names;
	char		cs[256];
	const char	*nm;

	if ( !sums[0] || !names[0] ) {
		return;
	}
	CG_DF_ReportPrintf( f, "\r\n*** %s\r\n", label );
	for ( ;; ) {
		Q_strncpyz( cs, COM_Parse( &s ), sizeof( cs ) );
		nm = COM_Parse( &n );
		if ( !cs[0] || !nm[0] ) {
			break;
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", cs, nm );
	}
}

// 0x153d + 0x31e65 + 0x31e0b: report file name = argument without ^x colour codes, ".txt" added when it has no extension, directories dropped
// (oDFs additionally maps '\' and ':' to '_')
static void CG_DF_ReportFileName( const char *arg, char *out, int size ) {
	char		tmp[256];
	const char	*skip, *c;
	qboolean	hasExt = qfalse;
	int			i;

	Q_strncpyz( tmp, arg, sizeof( tmp ) - 16 );
	CG_DF_StripColors( tmp );
	for ( i = 0; tmp[i]; i++ ) {		// not in DeFRaG: Windows separators / drive letters would escape system/reports/
		if ( tmp[i] == '\\' || tmp[i] == ':' ) {
			tmp[i] = '_';
		}
	}
	for ( i = strlen( tmp ) - 1; i >= 0 && tmp[i] != '/'; i-- ) {		// 0x31e65 COM_DefaultExtension
		if ( tmp[i] == '.' ) {
			hasExt = qtrue;
			break;
		}
	}
	if ( !hasExt ) {
		Q_strcat( tmp, sizeof( tmp ), ".txt" );
	}
	skip = tmp;
	for ( c = tmp; *c; c++ ) {		// 0x31e0b COM_SkipPath: only the last path component is used
		if ( *c == '/' ) {
			skip = c + 1;
		}
	}
	Q_strncpyz( out, skip, size );
}

// the "defrag_servername" client cvar (cvar read by 0xff3, cached in 0x11d390)
static void CG_DF_ServerNameCvar( char *out, int size ) {
	trap_Cvar_VariableStringBuffer( "defrag_servername", out, size );
}

// worker of 0x4795(name, timeMs, usage): timeMs != 0 adds the "time" line (0x4846), usage prints the usage text on an empty name
static void CG_DF_WriteReport( const char *arg, int runMs, qboolean usage ) {
	char		name[MAX_QPATH], path[MAX_QPATH + 16], si[BIG_INFO_STRING], sys[BIG_INFO_STRING];
	char		vers[32], servername[64], comp[64];
	fileHandle_t f;
	qtime_t		tm;
	dfSvFacts_t	sv;
	int			i;

	if ( !arg[0] ) {
		if ( usage ) {
			CG_Printf( "usage: svinfo_report [file]\n" );
		}
		return;
	}
	CG_DF_ReportFileName( arg, name, sizeof( name ) );
	Com_sprintf( path, sizeof( path ), "system/reports/%s", name );
	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		CG_Printf( "SVI_WriteReport(): failed to open file for writing.\n" );
		return;
	}

	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	CG_DF_ServerNameCvar( servername, sizeof( servername ) );
	Q_strncpyz( comp, Info_ValueForKey( si, "df_competition" ), sizeof( comp ) );
	trap_RealTime( &tm );
	CG_DF_SvFacts( si, sys, &sv );

	CG_DF_ReportPrintf( f, "=\r\n= Report for %s (%04i-%02i-%02i)\r\n=\r\n\r\n", servername, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday );
	CG_DF_ReportPrintf( f, "*** Info\r\n" );
	if ( sv.detected ) {
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "validation", dfViolationCount ? "failed" : "passed" );
		if ( runMs ) {
			CG_DF_ReportPrintf( f, "%-24s %s\r\n", "time", CG_DF_FormatTime( runMs ) );
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "map", Info_ValueForKey( si, "mapname" ) );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "gametype", dfGametypeNames[sv.gt >= 0 && sv.gt < 8 ? sv.gt : 0] );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "physics", sv.promode ? "cpm" : "vq3" );
		if ( sv.mode != -1 ) {
			CG_DF_ReportPrintf( f, "%-24s %i\r\n", "mode", sv.mode );
		}
		if ( sv.obs != -1 ) {
			CG_DF_ReportPrintf( f, "%-24s %i\r\n", "overbounces", sv.obs );
		}
		{		// 0x48c7: a DeFRaG server reports the client's df_name
			char dfn[20];

			dfn[0] = 0;
			if ( cg.snap ) {
				CG_DF_ClientDfName( cg.snap->ps.clientNum, dfn );
			}
			CG_DF_ReportPrintf( f, "%-24s %s\r\n", "player", dfn );
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "comp", comp );
		Com_sprintf( vers, sizeof( vers ), "%i.%02i.%02i", sv.ver / 10000, ( sv.ver / 100 ) % 100, sv.ver % 100 );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "version", vers );
	} else {
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "validation", "failed" );		// 0x4910 tests a constant address, i.e. always "failed" here
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "map", Info_ValueForKey( si, "mapname" ) );
		CG_DF_ReportPrintf( f, "%-24s %i\r\n", "gametype", cgs.gametype );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "player", cg.snap ? cgs.clientinfo[cg.snap->ps.clientNum].name : "" );
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "gamename", Info_ValueForKey( si, "gamename" ) );
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
	for ( i = 0; i < MAX_CLIENTS; i++ ) {		// 0x4a12: the CS_PLAYERS strings that exist
		const char *ci = CG_ConfigString( CS_PLAYERS + i );

		if ( ci[0] ) {
			CG_DF_ReportPrintf( f, "\r\n*** Client Info %i\r\n", i );
			CG_DF_ReportInfo( f, ci );
		}
	}
	CG_DF_ReportPaks( f, "sv_referencedPaks", Info_ValueForKey( sys, "sv_referencedPaks" ), Info_ValueForKey( sys, "sv_referencedPakNames" ) );
	CG_DF_ReportPaks( f, "sv_paks", Info_ValueForKey( sys, "sv_paks" ), Info_ValueForKey( sys, "sv_pakNames" ) );

	trap_FS_FCloseFile( f );
	CG_Printf( "report written to %s\n", path );
}

void CG_DF_SvinfoReport_f( void ) {
	char arg[MAX_QPATH];

	trap_Argv( 1, arg, sizeof( arg ) );
	CG_DF_WriteReport( arg, 0, qtrue );
}



// svinfo_summary [file] (cgame 0x4b67): brace-structured short report  system/reports/<file>.txt
//   <servername> {  / time map gametype physics mode overbounces player comp version  / validation { warnNN "text" } / }
static void CG_DF_SummaryLine( fileHandle_t f, int indent, const char *key, const char *value ) {
	CG_DF_ReportPrintf( f, "%s%-12s \"%s\"\r\n", indent ? "  " : "", key, value );
}

// worker of 0x4b67(name, timeMs, usage)
static void CG_DF_WriteSummary( const char *arg, int runMs, qboolean usage ) {
	char		name[MAX_QPATH], path[MAX_QPATH + 16], si[BIG_INFO_STRING], sys[BIG_INFO_STRING], tmp[64], servername[64];
	fileHandle_t f;
	dfSvFacts_t	sv;
	int			i, v, gt, mode, obs, promode;

	if ( !arg[0] ) {
		if ( usage ) {
			CG_Printf( "usage: svinfo_summary [file]\n" );
		}
		return;
	}
	CG_DF_ReportFileName( arg, name, sizeof( name ) );
	Com_sprintf( path, sizeof( path ), "system/reports/%s", name );
	trap_FS_FOpenFile( path, &f, FS_APPEND );		// 0x4bbb opens the summary with mode 2
	if ( !f ) {
		CG_Printf( "SVI_WriteSummary(): failed to open \"%s\" for writing.\n", name );
		return;
	}
	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	CG_DF_SvFacts( si, sys, &sv );
	v = sv.detected ? 1 : 0;
	gt = sv.gt;
	mode = sv.mode;
	obs = sv.obs;
	promode = sv.promode;
	CG_DF_ServerNameCvar( servername, sizeof( servername ) );

	CG_DF_ReportPrintf( f, "%s {\r\n", servername );
	if ( v ) {
		if ( runMs ) {
			CG_DF_SummaryLine( f, 1, "time", CG_DF_FormatTime( runMs ) );
		}
		CG_DF_SummaryLine( f, 1, "map", Info_ValueForKey( si, "mapname" ) );
		CG_DF_SummaryLine( f, 1, "gametype", dfGametypeNames[gt >= 0 && gt < 8 ? gt : 0] );
		CG_DF_SummaryLine( f, 1, "physics", promode ? "cpm" : "vq3" );
		if ( mode != -1 ) {
			Com_sprintf( tmp, sizeof( tmp ), "%i", mode );
			CG_DF_SummaryLine( f, 1, "mode", tmp );
		}
		if ( obs != -1 ) {
			Com_sprintf( tmp, sizeof( tmp ), "%i", obs );
			CG_DF_SummaryLine( f, 1, "overbounces", tmp );
		}
		if ( cg.snap ) {
			char dfn[20];

			CG_DF_ClientDfName( cg.snap->ps.clientNum, dfn );		// 0x4c6b
			CG_DF_SummaryLine( f, 1, "player", dfn );
		}
		if ( Info_ValueForKey( si, "df_competition" )[0] ) {
			CG_DF_SummaryLine( f, 1, "comp", Info_ValueForKey( si, "df_competition" ) );
		}
		Com_sprintf( tmp, sizeof( tmp ), "%i.%02i.%02i", sv.ver / 10000, ( sv.ver / 100 ) % 100, sv.ver % 100 );
		CG_DF_SummaryLine( f, 1, "version", tmp );
	} else {
		Com_sprintf( tmp, sizeof( tmp ), "%i", cgs.gametype );		// 0x4cbb: cgs.gametype
		CG_DF_SummaryLine( f, 1, "gametype", tmp );
		CG_DF_SummaryLine( f, 1, "map", Info_ValueForKey( si, "mapname" ) );
		CG_DF_SummaryLine( f, 1, "player", cg.snap ? cgs.clientinfo[cg.snap->ps.clientNum].name : "" );
		CG_DF_SummaryLine( f, 1, "gamename", Info_ValueForKey( si, "gamename" ) );
		CG_DF_SummaryLine( f, 1, "fs_game", Info_ValueForKey( sys, "fs_game" ) );
	}
	if ( dfViolationCount ) {
		CG_DF_ReportPrintf( f, "\r\n  validation {\r\n" );
		for ( i = 0; i < dfViolationCount; i++ ) {
			CG_DF_ReportPrintf( f, "    warn%02i \"%s\"\r\n", i + 1, dfViolationLog[i] );
		}
	}
	CG_DF_ReportPrintf( f, "  }\r\n" );		// DeFRaG writes this closing line unconditionally (0x4d56)
	CG_DF_ReportPrintf( f, "}\r\n\r\n" );
	trap_FS_FCloseFile( f );
	CG_Printf( "report written to %s%s\n", "system/reports/", name );
}

void CG_DF_SvinfoSummary_f( void ) {
	char arg[MAX_QPATH];

	trap_Argv( 1, arg, sizeof( arg ) );
	CG_DF_WriteSummary( arg, 0, qtrue );
}

// ---- demo validation (cgame 0x4db6, called from the finish handler 0x1aec when cg.demoPlayback and defrag_vers >= 18100) ----
// df_demos_validation >= 1: <date>_summary report; if a violation was logged (0x5b02) also <date>_failed.txt (the server name);
// df_demos_validation >= 2: full <date>.txt report. <date> = 0x4775 = "YYYY-MM-DD" of the real-time clock.
vmCvar_t	df_demos_validation, df_demos_ModSupport;

// ---- df_demos_ModSupport (cgame 0xa191 / 0xa1a6; cvar flags TEMP) ----
// During demo playback with df_demos_ModSupport > 0, the "Bad item index / Bad entity type / Bad leType / Unknown event / Unknown
// client game command" failures are ignored and weapon numbers > 10 are drawn as weapon 1 (0xa191 callers: 0x1ea0c, 0x1f222, 0x1f7e0,
// 0x20dab, 0x28d70, 0x2ab1d). Not ported: level 3 entity-trajectory fix 0xa213 (adds a per-entity delta of unidentified fields, only for
// ModSupport 3) and the "MOD Demo detected" print of 0x7f2a.
qboolean CG_DF_ModSupport( void ) {
	return ( cg.demoPlayback && df_demos_ModSupport.integer > 0 ) ? qtrue : qfalse;
}

// 0xa174: the server counts as DeFRaG when o3j_version or defrag_vers (parsed or merely present) is set
qboolean CG_DF_IsDefragServer( void ) {
	char si[BIG_INFO_STRING];

	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	return ( atoi( Info_ValueForKey( si, "o3j_version" ) ) || atoi( Info_ValueForKey( si, "defrag_vers" ) ) || Info_ValueForKey( si, "defrag_vers" )[0] ) ? qtrue : qfalse;
}

// DeFRaG 0x1c0b-0x1c50 (per-frame driver 0x1bd3): while sv_cheats is off com_maxfps is kept inside 60..140 (0 = unlimited counts as
// too high), with the red notice; the original sends the notice to the server ("Notify") which echoes it, here it is printed directly.
// Only on DeFRaG servers and not in demos; opt-out df_enforceMaxFps 0.
void CG_DF_EnforceMaxFps( void ) {
	char	buf[32];
	int		v;

	if ( !df_enforceMaxFps.integer || cg.demoPlayback || !cg.snap || CG_DF_Cheats() || !CG_DF_IsDefragServer() ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "com_maxfps", buf, sizeof( buf ) );
	v = atoi( buf );
	if ( v == 0 || v > 140 ) {
		trap_Cvar_Set( "com_maxfps", "140" );
		CG_Printf( S_COLOR_RED "com_maxfps highest value restricted to 140." S_COLOR_WHITE "\n" );
	} else if ( v < 60 ) {
		trap_Cvar_Set( "com_maxfps", "60" );
		CG_Printf( S_COLOR_RED "com_maxfps lowest value restricted to 60." S_COLOR_WHITE "\n" );
	}
}

// 0xa1a6, run after cg.clientFrame++ in CG_DrawActiveFrame: on a non-DeFRaG demo clear the stats the race code would misread
void CG_DF_ModSupportSnapshot( void ) {
	int i;

	if ( !CG_DF_ModSupport() || !cg.snap || CG_DF_IsDefragServer() ) {
		return;
	}
	for ( i = 7; i < 16; i++ ) {
		cg.snap->ps.stats[i] = 0;
	}
	if ( df_demos_ModSupport.integer == 2 ) {
		for ( i = 10; i < 16; i++ ) {
			cg.snap->ps.powerups[i] = 0;
		}
		for ( i = 0; i < 16; i++ ) {
			cg.snap->ps.persistant[i] = 0;
		}
	}
}

// ---- cgame init cvar sequence (cgame 0x3985, called from CG_Init after the glconfig) ----
// defrag_demoPlayback = cg.demoPlayback ("1"/"0"; read by the DeFRaG ui, 0x1a92), df_name defaults to the player's name,
// defrag_version_server = parsed server defrag_vers as "%i.%02i.%02i" (0x3c33 result), defrag_version_client = DF_VERSION_STRING.
// (cg.demoPlayback is still 0 at this point of CG_Init - DeFRaG writes the cvar only here too.) The shader registrations of 0x3985
// (sprites/notAvailable, gfx/unavailableItem, df_gfx/scoreboard/*) belong to the features that use them.
void CG_DF_InitCvars( void ) {
	char	name[MAX_STRING_CHARS], si[BIG_INFO_STRING];
	int		v;

	CG_DF_LoadBspRecord();		// 0x63f5 -> 0x61c6
	trap_Cvar_Set( "defrag_demoPlayback", cg.demoPlayback ? "1" : "0" );
	trap_Cvar_VariableStringBuffer( "df_name", name, sizeof( name ) );
	if ( !name[0] ) {
		trap_Cvar_VariableStringBuffer( "name", name, sizeof( name ) );
		trap_Cvar_Set( "df_name", name );
	}
	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	v = atoi( Info_ValueForKey( si, "defrag_vers" ) );
	if ( v > 0 && v < 1000 ) {
		v *= 100;
	}
	trap_Cvar_Set( "defrag_version_server", va( "%i.%02i.%02i", v / 10000, ( v / 100 ) % 100, v % 100 ) );
	trap_Cvar_Set( "defrag_version_client", DF_VERSION_STRING );
}

void CG_DF_DemoValidation( int ms ) {
	char		date[32], file[MAX_QPATH + 16], si[BIG_INFO_STRING];
	fileHandle_t f;
	qtime_t		tm;
	int			v;

	if ( !cg.demoPlayback || df_demos_validation.integer <= 0 ) {
		return;
	}
	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	v = atoi( Info_ValueForKey( si, "defrag_vers" ) );
	if ( v > 0 && v < 1000 ) {
		v *= 100;
	}
	if ( v < 18100 ) {
		return;
	}
	trap_RealTime( &tm );
	Com_sprintf( date, sizeof( date ), "%04i-%02i-%02i", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday );
	Com_sprintf( file, sizeof( file ), "%s_summary", date );
	CG_DF_WriteSummary( file, ms, qtrue );
	if ( dfViolationCount ) {
		Com_sprintf( file, sizeof( file ), "system/reports/%s_failed.txt", date );
		trap_FS_FOpenFile( file, &f, FS_WRITE );
		if ( f ) {
			CG_DF_ReportPrintf( f, "%s\r\n", CG_DF_Serverinfo( "defrag_servername" ) );
			trap_FS_FCloseFile( f );
		} else {
			CG_Printf( "SVI_ReportBadDemo(): failed to open \"%s\" for writing.\n", file );
		}
	}
	if ( df_demos_validation.integer > 1 ) {
		CG_DF_WriteReport( date, ms, qtrue );
	}
}

// ---- authentication reply (cgame 0x4337 / hash 0x11f9), see disasm/G_CLIENT/auth_handshake.md ----
// The server (df_auth 1, non-dedicated DeFRaG-style servers) sends "auth_request <token> <mapChecksum>"; the client
// answers "auth_reply <hash><random decoy digit>" or is dropped. The second argument is compared with the local bsp record
// (cgame 0x6222, see CG_DF_CheckBspCrc) right after the reply.
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

void CG_DF_AuthBspCrc( int serverCrc ) {		// 0x3d84: auth_request <token> <sv_mapChecksum>
	CG_DF_CheckBspCrc( serverCrc );
}

void CG_DF_AuthRequest( int token ) {
	int h = CG_DF_AuthHash( token, DF_PROTOCOL_VERSION_STRING );

	if ( h ) {
		trap_SendClientCommand( va( "auth_reply %i%i", h, rand() % 10 ) );
	}
}

void CG_DF_Reset( void ) {
	dfReported = 0;
	CG_DF_RecInit();
}

// called for every snapshot transition with the previous and the new player state
void CG_DF_SnapshotTransition( const playerState_t *ops, const playerState_t *ps ) {
	int	changed = ops->stats[STAT_DEFRAG_FLAGS] ^ ps->stats[STAT_DEFRAG_FLAGS];
	int	ms;

	if ( ops->clientNum != ps->clientNum ) {
		dfFollowStamp = 0;
		CG_DF_Reset();
		CG_DF_RecLoad();
		return;
	}
	CG_DF_Validate( ops, ps );
	{		// DeFRaG 0x5248 -> 0x11d284: the current race time, refreshed every snapshot (the 'last time' HUD widget shows it live)
		int live;

		// 0x1c8b -> 0x5248 returns the decoded race time (0 while the timer is not initialized); it is stored every snapshot, so the
		// last-time and checkpoint boxes disappear again once the player respawns (bit 2 clears) until a run is active
		CG_DF_SetLastRunTime( ( CG_DF_RaceTime( ps, &live ) && live > 0 ) ? live : 0 );
		CG_DF_RecTrack( ( CG_DF_RaceTime( ps, &live ) && live > 0 ) ? live : 0 );		// 0x5248 @0x52b8: B[N] = the running time, every snapshot
	}
	CG_DF_TrackStats( ops, ps, changed );
	// 0x1d49: the start / checkpoint / finish events are only derived when no respawn event (team or spawn count change) was seen
	if ( ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT] || ps->persistant[PERS_TEAM] != ops->persistant[PERS_TEAM] ) {
		changed = 0;
	} else if ( ops->stats[STAT_HEALTH] > 0 && ps->stats[STAT_HEALTH] <= 0 && ( ps->stats[STAT_DEFRAG_FLAGS] & 2 ) && CG_DF_Serverinfo( "defrag_vers" )[0] ) {
		// 0x1c8b event 8 + 0x1b10: dying while the timer is initialized prints the end-of-run stats (not on the multiplayer gametypes >= 5)
		int	sver = atoi( CG_DF_Serverinfo( "defrag_vers" ) ), dms;

		if ( sver < 1000 ) {
			sver *= 100;
		}
		if ( !( sver >= 18100 && atoi( CG_DF_Serverinfo( "defrag_gametype" ) ) >= 5 ) ) {
			CG_DF_RaceTime( ps, &dms );
			CG_DF_PrintStats( dms );
		}
	}
	// 0x1c56: event 0x20 (flags bit 1 toggled): a current weapon that is not owned is replaced by the machinegun
	if ( ( changed & 1 ) && CG_DF_Serverinfo( "defrag_vers" )[0] && !( ps->stats[STAT_WEAPONS] & ( 1 << ps->weapon ) ) ) {
		cg.weaponSelect = WP_MACHINEGUN;
	}
	// bit 0x4: run (re)started
	if ( ( changed & 4 ) && ( ps->stats[STAT_DEFRAG_FLAGS] & 4 ) ) {
		CG_DF_ResetStats();
		CG_DF_GhostRunStart();
		CG_DF_ArStartEvent();
		runActive = qtrue;
		CG_DF_RecStart();
	}
	// bit 0x10 toggles once per checkpoint (event 0x80)
	if ( ( changed & 0x10 ) && CG_DF_RaceTime( ps, &ms ) ) {
		CG_DF_RecCheckpoint( ms );
	}
	// bit 0x8 toggles at the finish: DeFRaG's own 0x1b10 prints the stats there on defrag_gametype >= 5 servers
	// (other servers get the "stats" command instead)
	if ( ( changed & 8 ) && CG_DF_RaceTime( ps, &ms ) ) {
		CG_DF_GhostRunEnd( ms );
		CG_DF_RecFinish( ms );
		CG_DF_ArFinishEvent( ms, CG_DF_RecKeep() );
		CG_DF_DemoValidation( ms );
		// the stats block is printed when the snapshot becomes current (CG_DF_FlushFinishPrint), after its server commands: the 'stats' command
		// of the finish clears runActive first, so the block is not printed twice
		dfFinishPrint = qtrue;
		dfFinishPrintMs = ms;
	}
}

// DeFRaG 0x1e4c: runs the event word / validity checks / per-frame work when a snapshot is READ (CG_ReadNextSnapshot, before it is the next
// snapshot), against the current one; snapshots flagged SNAPFLAG_NOT_ACTIVE are skipped.  The race time is decoded with the serverTime of
// the snapshot being read.
void CG_DF_ReadSnapshot( const snapshot_t *old, const snapshot_t *cur ) {
	if ( !old || ( cur->snapFlags & SNAPFLAG_NOT_ACTIVE ) ) {
		return;
	}
	dfDecodeTime = cur->serverTime;
	CG_DF_ScriptEvents( old, cur );
	CG_DF_SnapshotTransition( &old->ps, &cur->ps );
	dfDecodeTime = 0;
}

// called when the read snapshot becomes the current one, after its server commands were executed
void CG_DF_FlushFinishPrint( void ) {
	if ( !dfFinishPrint ) {
		return;
	}
	dfFinishPrint = qfalse;
	if ( runActive ) {
		CG_DF_PrintStats( dfFinishPrintMs );
		runActive = qfalse;
	}
}

// DeFRaG vote timing: on variant servers (defrag_gametype >= 5) with df_vote_system on, CS_VOTE_TIME holds the vote deadline
qboolean CG_DF_VoteIsDeadline( void ) {
	return ( atoi( CG_DF_Serverinfo( "defrag_gametype" ) ) >= 5 && atoi( CG_DF_Serverinfo( "df_vote_system" ) ) ) ? qtrue : qfalse;
}

// defrag_flags (0x11d2b8): serverinfo value, replaced by the "df_options" server command
int CG_DF_DefragFlags( void ) {
	return cg_dfOptionsSet ? cg_dfOptions : atoi( CG_DF_Serverinfo( "defrag_flags" ) );
}

// ---- DeFRaG event scripts (cgame 0x1b2e, fed by the event word of 0x1c8b): df_script_onMapLoad / onJoinTeam / onRespawn ----
// Event bits (0x1c8b): 0x1 = first snapshot or new server count (map load), 0x10 = respawn: set together with 0x1, and whenever
// PERS_TEAM or PERS_SPAWN_COUNT change between two snapshots. The handler runs `exec <mapname>.cfg`, `exec team_<red|blue|spect|free>.cfg`
// (first spawn, not following) and `exec respawn.cfg` (not following, not a spectator). Not ported from 0x1b2e: the stats-accumulator
// reset it also performs (our trackers reset on their own events).
vmCvar_t	df_script_onMapLoad, df_script_onJoinTeam, df_script_onRespawn;
vmCvar_t	df_player_Sprites;	// cgame 0x26191 (CG_PlayerSprites)
vmCvar_t	df_sound_flagGrabbing, df_sound_leadChanges;	// cgame 0x26e03 (CG_CheckLocalSounds)

void CG_DF_ScriptEvents( const snapshot_t *old, const snapshot_t *cur ) {
	int ev = 0;
	const char *team;

	dfSnapDelta = old ? cur->serverTime - old->serverTime : 0;

	if ( !old ) {
		ev = 0x11;
	} else if ( ( cur->snapFlags ^ old->snapFlags ) & SNAPFLAG_SERVERCOUNT ) {
		ev = 0x13;
	} else {
		if ( cur->ps.persistant[PERS_TEAM] != old->ps.persistant[PERS_TEAM] ) {
			ev |= 0x14;
		}
		if ( cur->ps.persistant[PERS_SPAWN_COUNT] != old->ps.persistant[PERS_SPAWN_COUNT] ) {
			ev |= 0x10;
		}
	}
	if ( ev & 1 ) {
		dfValHasteMemo = 0;		// 0x62d2: a map load clears the haste memo
	}
	if ( ev & 0x10 ) {
		CG_DF_RecReset( qfalse );		// 0x5273: a respawn clears the checkpoint list of the finished / aborted run (also in demos)
	}
	if ( !( ev & 0x10 ) || cg.demoPlayback ) {
		return;
	}
	CG_DF_ArRespawnEvent();
	if ( ( ev & 1 ) && df_script_onMapLoad.integer ) {
		trap_SendConsoleCommand( va( "exec %s.cfg\n", Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ) ) );
	}
	if ( cur->ps.pm_flags & PMF_FOLLOW ) {
		return;
	}
	if ( df_script_onJoinTeam.integer && cur->ps.persistant[PERS_SPAWN_COUNT] == 1 ) {
		switch ( cur->ps.persistant[PERS_TEAM] ) {
		case TEAM_RED: team = "team_red"; break;
		case TEAM_BLUE: team = "team_blue"; break;
		case TEAM_SPECTATOR: team = "team_spect"; break;
		default: team = "team_free"; break;
		}
		trap_SendConsoleCommand( va( "exec %s.cfg\n", team ) );
	}
	if ( df_script_onRespawn.integer && cur->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		trap_SendConsoleCommand( "exec respawn.cfg\n" );
	}
}

// ---- df_mp_NoDrawRadius (cgame 0x9c47 / 0x9bdd / 0x9c12, called from CG_AddCEntity 0x1f222 for ET_PLAYER and ET_MISSILE) ----
// With player interaction disabled by the server (ps.stats[STAT_INTERFERENCE]: bit 0 = players, bit 1 = missiles; bit 2 = only other-team
// clients), other clients' players / missiles closer than |df_mp_NoDrawRadius| to the viewer are not drawn; a NEGATIVE radius draws such players as
// translucent (powerup bit 16 = invisibility effect) instead. The viewer's own entities are never affected.
vmCvar_t	df_mp_NoDrawRadius;

qboolean CG_DF_HideForInterference( centity_t *cent ) {
	int			self = cg.predictedPlayerState.clientNum, owner = cent->currentState.clientNum;
	int			st = cg.predictedPlayerState.stats[STAT_INTERFERENCE], radius = df_mp_NoDrawRadius.integer, hide = 0;
	qboolean	player = cent->currentState.eType == ET_PLAYER;
	vec3_t		d;

	if ( owner == self || owner < 0 || owner >= MAX_CLIENTS ) {
		return qfalse;
	}
	VectorSubtract( cg.predictedPlayerState.origin, cent->lerpOrigin, d );
	if ( VectorLength( d ) >= ( radius < 0 ? -radius : radius ) ) {
		return qfalse;
	}
	if ( st & ( player ? 1 : 2 ) ) {
		hide = ( st & 4 ) ? ( cgs.clientinfo[self].team != cgs.clientinfo[owner].team ) : 1;
	}
	if ( hide && radius < 0 && player ) {
		cent->currentState.powerups |= 16;
		return qfalse;
	}
	return hide ? qtrue : qfalse;
}

// ---- df_player_SmokeTrails (cgame 0x9082 per frame, 0x9038 puff, 0x9154 fragment pre-step; LE loop 0x20dab) ----
// value = N.MM: N (integer part) = seconds the viewer's own smoke trail lasts (puff dropped wherever the viewer moved, tag 903); MM (hundredths)
// = seconds the trails of every player entity in the snapshot (tag 901) and every playing ghost (tag 902) last. The puffs are smoke-puff sprites
// turned into stationary LE_FRAGMENTs (so they stay until the last second, then sink like gibs) and tagged in color[0]; 0x9154 expires tagged
// puffs at once when their part of the value becomes 0. Limited to one update per 25 ms.
vmCvar_t	df_player_SmokeTrails;

static void CG_DF_SmokePuff( const vec3_t origin, int seconds, int tag ) {		// 0x9038
	static const vec3_t	vel = { 0, 0, 0 };
	localEntity_t		*le;

	le = CG_SmokePuff( origin, vel, 4, 1, 1, 1, 0.66f, 1000.0f * seconds, cg.time, cg.time, LEF_PUFF_DONT_SCALE, cgs.media.smokePuffShader );
	le->leType = LE_FRAGMENT;
	le->color[0] = tag;
}

void CG_DF_SmokeTrails( void ) {		// 0x9082
	static int		nextTime;
	static vec3_t	lastPos;
	float			x = df_player_SmokeTrails.value, ax = x < 0 ? -x : x;
	int				own, others, i;
	vec3_t			d, p;

	if ( x == 0 || cg.time < nextTime ) {
		return;
	}
	nextTime = cg.time + 25;
	own = (int)ax;
	others = (int)( 100.0f * ( ax - (float)own ) );
	if ( own ) {
		VectorSubtract( cg.predictedPlayerState.origin, lastPos, d );
		if ( VectorLength( d ) > 0 ) {
			VectorCopy( cg.predictedPlayerState.origin, lastPos );
			CG_DF_SmokePuff( lastPos, own, 903 );
		}
	}
	if ( !others ) {
		return;
	}
	for ( i = 0; i < 5; i++ ) {
		if ( CG_DF_GhostPos( i, p ) ) {
			CG_DF_SmokePuff( p, others, 902 );
		}
	}
	for ( i = 0; i < cg.snap->numEntities; i++ ) {
		const centity_t *cent = &cg_entities[cg.snap->entities[i].number];

		if ( cent->currentState.eType == ET_PLAYER ) {
			CG_DF_SmokePuff( cent->lerpOrigin, others, 901 );
		}
	}
}

void CG_DF_SmokeFragmentPre( localEntity_t *le ) {		// 0x9154
	int tag = (int)le->color[0], own, others;
	float ax = df_player_SmokeTrails.value;

	if ( tag <= 900 || tag >= 904 ) {
		return;
	}
	if ( ax < 0 ) {
		ax = -ax;
	}
	own = (int)ax;
	others = (int)( 100.0f * ( ax - (float)own ) );
	if ( ( tag == 903 && own == 0 ) || ( tag != 903 && others == 0 ) ) {
		le->endTime = cg.time;
	}
}
qboolean CG_DF_RunActive( void ) {		// 0x11d288: written by 0x1c8b as (snapshot stats[12] & 2) -- the timer 'initialized' bit, not our run flag
	return ( cg.snap && ( cg.snap->ps.stats[STAT_DEFRAG_FLAGS] & 2 ) ) ? qtrue : qfalse;
}

// 0x9cf3: explosions (LE_EXPLOSION / LE_SPRITE_EXPLOSION) tagged with their owner in color[0] follow the missile rule of df_mp_NoDrawRadius
qboolean CG_DF_HideLocalForInterference( localEntity_t *le ) {
	int			owner = (int)le->color[0], st = cg.predictedPlayerState.stats[STAT_INTERFERENCE], radius = df_mp_NoDrawRadius.integer;
	vec3_t		d;

	if ( owner == cg.predictedPlayerState.clientNum || owner < 0 || owner >= MAX_CLIENTS ) {
		return qfalse;
	}
	VectorSubtract( cg.predictedPlayerState.origin, le->refEntity.origin, d );
	if ( VectorLength( d ) >= ( radius < 0 ? -radius : radius ) ) {
		return qfalse;
	}
	if ( !( st & 2 ) ) {
		return qfalse;
	}
	return ( st & 4 ) ? ( cgs.clientinfo[cg.predictedPlayerState.clientNum].team != cgs.clientinfo[owner].team ) : qtrue;
}
