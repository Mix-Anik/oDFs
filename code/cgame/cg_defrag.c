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
static qboolean CG_DF_RaceTime( const playerState_t *ps, int *ms ) {
	unsigned int	e, x, r, a, t;
	static byte		mapSalt;
	static char		saltMap[MAX_QPATH];
	char			map[MAX_QPATH];
	int				i, v;

	*ms = 0;
	if ( !( ps->stats[STAT_DEFRAG_FLAGS] & 2 ) ) {
		return qfalse;
	}
	e = ( (unsigned int)ps->stats[STAT_RACE_TIME_HI] << 16 ) | ( (unsigned int)ps->stats[STAT_RACE_TIME_LO] & 0xffff );
	if ( !( e >> 22 ) ) {		// plain
		*ms = (int)e;
		return qtrue;
	}
	// obfuscated
	e ^= (unsigned int)( ps->movementDir & 0xf ) << 28;
	v = ps->stats[STAT_HEALTH];
	e ^= ( v > 0 ) ? (unsigned int)( v & 0xff ) : 0x96u;
	v = (int)floor( ps->velocity[0] );
	e ^= (unsigned int)( v < 0 ? -v : v ) << 16;
	v = (int)floor( ps->origin[0] );
	e ^= (unsigned int)( v < 0 ? -v : v ) & 0xffff;
	for ( i = 2; i >= 0; i-- ) {
		( (byte *)&e )[i + 1] ^= ( (byte *)&e )[i];
	}
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
	t = (unsigned int)cg.snap->serverTime;
	x = t << 2;
	x += ( (unsigned int)mapSalt + 19131u ) << 8;
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
// Not ported: the per-axis counters (0x11d238/3c/40).
vmCvar_t	df_noEndOfRunStats;

static qboolean	runActive;			// 0x11d288
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

// per snapshot while a run is active: max horizontal speed, horizontal distance travelled
static void CG_DF_TrackStats( const playerState_t *ops, const playerState_t *ps, int changed ) {
	vec3_t	d;
	float	speed;

	CG_DF_TrackJump( ps );
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
	CG_Printf( " Highest Jump   : %i units", (int)jumpHighest );
	if ( df_unit.integer == 1 ) {
		CG_Printf( " (%i meters)", (int)CG_ConvertDistance( jumpHighest ) );
	} else if ( df_unit.integer == 2 ) {
		CG_Printf( " (%i miles)", (int)CG_ConvertDistance( jumpHighest ) );
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

// resetjumpmeter: clears the jump maxima (DeFRaG also clears its jump history logger, not ported)
void CG_DF_ResetJumpMeter_f( void ) {
	char	arg[64];

	trap_Argv( 1, arg, sizeof( arg ) );
	if ( !strcmp( arg, "?" ) || arg[0] ) {
		CG_Printf( "\n^2resetjumpmeter: ^3Resets jump histories and maximums.\n^5Usage:          ^2resetjumpmeter\n\n" );
		return;
	}
	jumpDist = jumpMaxRise = jumpLongest = jumpHighest = 0;
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
	const char	*cs, *nm;

	for ( ;; ) {
		cs = COM_Parse( &s );
		nm = COM_Parse( &n );
		if ( !cs[0] && !nm[0] ) {
			break;
		}
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", cs, nm );
	}
}

// 0x153d + 0x31e65: report file name = argument with ^x colour codes removed (path characters are replaced by '_': oDFs keeps the files
// inside system/reports/, DeFRaG does not filter them)
static void CG_DF_ReportFileName( const char *arg, char *out, int size ) {
	int i;

	Q_strncpyz( out, arg, size );
	Q_CleanStr( out );
	for ( i = 0; out[i]; i++ ) {
		if ( out[i] == '/' || out[i] == '\\' || out[i] == ':' || out[i] == '.' ) {
			out[i] = '_';
		}
	}
}

// worker of 0x4795(name, timeMs, usage): timeMs != 0 adds the "time" line (0x4846), usage prints the usage text on an empty name
static void CG_DF_WriteReport( const char *arg, int runMs, qboolean usage ) {
	char		name[MAX_QPATH], path[MAX_QPATH + 16], si[BIG_INFO_STRING], sys[BIG_INFO_STRING];
	char		vers[32], servername[64];
	fileHandle_t f;
	qtime_t		tm;
	int			i, v, gt, mode, obs;
	const char	*physics;

	if ( !arg[0] ) {
		if ( usage ) {
			CG_Printf( "usage: svinfo_report [file]\n" );
		}
		return;
	}
	CG_DF_ReportFileName( arg, name, sizeof( name ) );
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
	if ( runMs ) {
		CG_DF_ReportPrintf( f, "%-24s %s\r\n", "time", CG_DF_FormatTime( runMs ) );
	}
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
	char		name[MAX_QPATH], path[MAX_QPATH + 16], si[BIG_INFO_STRING], sys[BIG_INFO_STRING], tmp[64];
	fileHandle_t f;
	int			i, v, gt, mode, obs, promode;

	if ( !arg[0] ) {
		if ( usage ) {
			CG_Printf( "usage: svinfo_summary [file]\n" );
		}
		return;
	}
	CG_DF_ReportFileName( arg, name, sizeof( name ) );
	Com_sprintf( path, sizeof( path ), "system/reports/%s.txt", name );
	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		CG_Printf( "SVI_WriteSummary(): failed to open \"%s\" for writing.\n", name );
		return;
	}
	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	v = atoi( Info_ValueForKey( si, "defrag_vers" ) );
	if ( v > 0 && v < 1000 ) {
		v *= 100;
	}
	gt = atoi( Info_ValueForKey( si, "defrag_gametype" ) );
	mode = Info_ValueForKey( si, "defrag_mode" )[0] ? atoi( Info_ValueForKey( si, "defrag_mode" ) ) : -1;
	obs = Info_ValueForKey( si, "defrag_obs" )[0] ? atoi( Info_ValueForKey( si, "defrag_obs" ) ) : -1;
	promode = atoi( Info_ValueForKey( si, "df_promode" ) ) != 0;

	CG_DF_ReportPrintf( f, "%s {\r\n", Info_ValueForKey( si, "defrag_servername" ) );
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
			char clean[MAX_QPATH];

			Q_strncpyz( clean, cgs.clientinfo[cg.snap->ps.clientNum].name, sizeof( clean ) );
			Q_CleanStr( clean );
			CG_DF_SummaryLine( f, 1, "player", clean );
		}
		if ( Info_ValueForKey( si, "df_competition" )[0] ) {
			CG_DF_SummaryLine( f, 1, "comp", Info_ValueForKey( si, "df_competition" ) );
		}
		Com_sprintf( tmp, sizeof( tmp ), "%i.%02i.%02i", v / 10000, ( v / 100 ) % 100, v % 100 );
		CG_DF_SummaryLine( f, 1, "version", tmp );
	} else {
		Com_sprintf( tmp, sizeof( tmp ), "%i", gt );
		CG_DF_SummaryLine( f, 1, "gametype", tmp );
		CG_DF_SummaryLine( f, 1, "map", Info_ValueForKey( si, "mapname" ) );
		CG_DF_SummaryLine( f, 1, "player", cg.snap ? cgs.clientinfo[cg.snap->ps.clientNum].name : "" );
		CG_DF_SummaryLine( f, 1, "gamename", Info_ValueForKey( sys, "gamename" ) );
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
static qboolean CG_DF_IsDefragServer( void ) {
	char si[BIG_INFO_STRING];

	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	return ( atoi( Info_ValueForKey( si, "o3j_version" ) ) || atoi( Info_ValueForKey( si, "defrag_vers" ) ) || Info_ValueForKey( si, "defrag_vers" )[0] ) ? qtrue : qfalse;
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
// defrag_version_server = parsed server defrag_vers as "%i.%02i.%02i" (0x3c33 result), defrag_version_client = "1.91.31".
// (cg.demoPlayback is still 0 at this point of CG_Init - DeFRaG writes the cvar only here too.) The shader registrations of 0x3985
// (sprites/notAvailable, gfx/unavailableItem, df_gfx/scoreboard/*) belong to the features that use them.
void CG_DF_InitCvars( void ) {
	char	name[MAX_STRING_CHARS], si[BIG_INFO_STRING];
	int		v;

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
	trap_Cvar_Set( "defrag_version_client", va( "%i.%02i.%02i", 1, 91, 31 ) );
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

		if ( CG_DF_RaceTime( ps, &live ) && live > 0 ) {
			CG_DF_SetLastRunTime( live );
		}
	}
	CG_DF_TrackStats( ops, ps, changed );
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
		if ( runActive ) {
			CG_DF_PrintStats( ms );
			runActive = qfalse;
		}
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
qboolean CG_DF_RunActive( void ) {		// 0x11d288
	return runActive;
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
