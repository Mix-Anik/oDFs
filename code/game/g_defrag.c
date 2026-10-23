/*
===========================================================================
g_defrag.c — DeFRaG-specific game logic

Race timer system: per-player timer records, start/stop/checkpoint dispatch,
per-frame tick. Matches DeFRaG QVM functions 0x3933/0x3958/0x3978/0x37c7.
===========================================================================
*/
#include "g_local.h"

raceTimer_t  g_raceTimers[MAX_CLIENTS];
int          g_checkpointCount;
qboolean     g_startTimerReady;
qboolean     g_stopTimerReady;
qboolean     g_checkpointReady;
qboolean     g_hasTeamItem;
int          g_dfMode = 1;
qboolean     g_dfVariant;

// ─── helpers ─────────────────────────────────────────────────────────────────

raceTimer_t *G_GetTimerRecord( gclient_t *client ) {
	int clientNum = client - level.clients;
	return &g_raceTimers[clientNum];
}

/*
DeFRaG func_0x13bf (see disasm/G_TRIGGER/finish_handler.md): times under a minute print as "S:mmm", otherwise "M:SS:mmm";
negative times get a leading '-'.
*/
static void G_FormatTime( int ms, char *buf, int bufSize ) {
	const char *sign = "";

	if ( ms < 0 ) {
		ms = -ms;
		sign = "-";
	}
	if ( ms < 60000 ) {
		Com_sprintf( buf, bufSize, "%s%i:%03i", sign, ms / 1000, ms % 1000 );
	} else {
		Com_sprintf( buf, bufSize, "%s%i:%02i:%03i", sign, ms / 60000, ( ms % 60000 ) / 1000, ms % 1000 );
	}
}

// ─── speed award (qagame 0x131844 / 0x131848 / 0x13184c / 0x1318cc) ────────
// Highest horizontal speed above 800 ups; awarded 5 s after the last record, announced in the
// "scores" command (value, holder, name). See disasm/G_CLIENT/scoreboard.md.
int  g_speedAwardValue  = 0;
int  g_speedAwardClient = -1;
char g_speedAwardName[MAX_NETNAME];
static int g_speedAwardExpire = 0;

void G_SpeedAwardInit( void ) {
	g_speedAwardValue  = 0;
	g_speedAwardClient = -1;
	g_speedAwardName[0] = 0;
	g_speedAwardExpire = 0;
}

// DeFRaG ClientThink_real (0x1ac53..0x1ac83): horizontal speed after Pmove; new maximum above 800 ups
void G_SpeedAwardTrack( gentity_t *ent ) {
	vec3_t v;
	int    speed;

	VectorCopy( ent->client->ps.velocity, v );
	v[2] = 0;
	speed = (int)VectorLength( v );
	if ( speed > 800 && speed > g_speedAwardValue ) {
		g_speedAwardValue   = speed;
		g_speedAwardClient  = ent->client->ps.clientNum;
		g_speedAwardName[0] = 0;
		g_speedAwardExpire  = level.time + 5000;
	}
}

// DeFRaG per-frame rules (0x23f2): 5 s after the last record, log the award and latch the holder's name
void G_SpeedAwardFrame( void ) {
	int         clientNum;
	const char *name;

	if ( !g_dfVariant || !g_speedAwardExpire || level.time < g_speedAwardExpire ) {
		return;
	}
	if ( g_speedAwardName[0] ) {
		clientNum = -1;
		name = g_speedAwardName;
	} else {
		clientNum = g_speedAwardClient;
		name = ( clientNum >= 0 ) ? level.clients[clientNum].pers.netname : "";
	}
	g_speedAwardExpire = 0;
	G_LogPrintf( "ClientSpeedAward: %i %i \"%s\"\n", clientNum, g_speedAwardValue, name );
}

// DeFRaG ClientDisconnect (0x1d863): a holder who leaves keeps their name on the award, the client slot becomes -1
void G_SpeedAwardDisconnect( gentity_t *ent ) {
	if ( g_speedAwardClient == ent - g_entities ) {
		g_speedAwardClient = -1;
		Q_strncpyz( g_speedAwardName, ent->client->pers.netname, sizeof( g_speedAwardName ) );
	}
}

static int g_clockBadFrame;		// 0x1320f4: a frame outside 7..16 ms happened
static int g_clockBadRatio;		// 0x1320f8: game clock drifted >20% from the wall clock over a 10 s window
static int g_clockInitTime;		// 0x1320fc
static int g_clockGameAcc, g_clockRealAcc, g_clockPrevReal;

// DeFRaG finish handler (func_0x35ce): the whole-millisecond time plus the sub-millisecond-corrected estimate ("est.")
static void G_RaceFinished( gentity_t *ent ) {
	raceTimer_t *t         = G_GetTimerRecord( ent->client );
	char         timeBuf[32], estBuf[32], route[MAX_CVAR_VALUE_STRING];
	int          clientNum = ent - g_entities;

	// DeFRaG 0x35ce: with sv_cheats on no time is announced, logged or ranked
	if ( trap_Cvar_VariableIntegerValue( "sv_cheats" ) ) {
		trap_SendServerCommand( clientNum, "print \"^1Cheats are ENABLED^7\n\"" );
		return;
	}

	// best finish time of this connection (client+0x2b0); ranks the scoreboard (SortRanks)
	if ( t->bestTime > 0 && ( !ent->client->pers.bestFinishTime || ent->client->pers.bestFinishTime > t->bestTime ) ) {
		ent->client->pers.bestFinishTime = t->bestTime;
		CalculateRanks();
		ClientUserinfoChanged( clientNum );		// 0x36d3: republishes the configstring (dfscore)
	}

	G_FormatTime( level.time - t->startTime, timeBuf, sizeof(timeBuf) );
	G_FormatTime( t->bestTime, estBuf, sizeof(estBuf) );

	if ( g_dfVariant ) {
		trap_SendServerCommand( -1, va( "print \"%s^7 reached the finish line in ^2%s^7 (est. %s)\n\"",
		                                ent->client->pers.netname, timeBuf, estBuf ) );
		// DeFRaG 0x3662: "ClientTimerStop" log line (server log parsed by external tools)
		{
			char		mapname[MAX_QPATH];
			qtime_t		qt;

			trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
			trap_RealTime( &qt );
			G_LogPrintf( "ClientTimerStop: %i %i \"%s\" \"%s\" %i %i %i %i %i %i %s\n", clientNum,
			             level.time - t->startTime, mapname, ent->client->pers.netname,
			             g_dfMode, df_promode.integer, g_dfSubMode, df_mp_InterferenceOff.integer,
			             df_ob_killobs.integer == 0, 19131,
			             va( "%04i-%02d-%02d", qt.tm_year + 1900, qt.tm_mon + 1, qt.tm_mday ) );
		}
	} else {
		qtime_t		qt;
		char		comp[MAX_CVAR_VALUE_STRING];

		trap_SendServerCommand( clientNum, "stats" );	// DeFRaG: triggers the cgame end-of-run stats dump
		// DeFRaG 0x35ce (non-dedicated branch): date, competition, then the result line; the 'on route' form is only used when
		// defrag_route is set (it is empty in run and fastcaps mode), and the line carries the DeFRaG version tag
		trap_RealTime( &qt );
		trap_SendServerCommand( clientNum, va( "print \"Date: %02i-%02i-%02i %02i:%02i\n\"", qt.tm_mon + 1, qt.tm_mday, qt.tm_year % 100,
		                                qt.tm_hour, qt.tm_min ) );
		trap_Cvar_VariableStringBuffer( "df_competition", comp, sizeof( comp ) );
		trap_SendServerCommand( clientNum, va( "print \"Competition: %s\n\"", comp ) );
		trap_Cvar_VariableStringBuffer( "defrag_route", route, sizeof( route ) );
		if ( route[0] ) {
			trap_SendServerCommand( clientNum, va( "print \"Time performed by ^2%s^7 on route ^2%s^7 : ^3%s^7 (v1.91.31 beta) (est. %s)\n\"",
			                                ent->client->pers.netname, route, timeBuf, estBuf ) );
		} else {
			trap_SendServerCommand( clientNum, va( "print \"Time performed by ^2%s^7 : ^3%s^7 (v1.91.31 beta) (est. %s)\n\"",
			                                ent->client->pers.netname, timeBuf, estBuf ) );
		}
	}
	trap_SendServerCommand( clientNum, va( "cp \"^2Finished: %s\"", estBuf ) );
}


// ─── DeFRaG client authentication handshake (disasm/G_CLIENT/auth_handshake.md) ─────────────────────────────
// Only active on non-dedicated servers (g_dfVariant == qfalse) and when df_auth is set; bots are never asked.
// A faithful port, including DeFRaG's quirk: the per-client record is picked with ps.clientNum at ClientConnect
// time (func_0x271a), which is still 0 right after the client memset, so every client shares record 0, only the
// first word of the record is cleared and the request always goes to client 0.
#define AUTH_TIMEOUT_MS		1500
#define AUTH_DELAY_MS		2500

typedef struct authRec_s {
	int		target;			// +0x0 client the request is sent to
	int		token;			// +0x4
	int		deadline;		// +0x8 authentication deadline, 0 = none outstanding
	int		sendTime;		// +0xc time at which the request is sent, 0 = none pending
} authRec_t;

static authRec_t g_authRecs[MAX_CLIENTS];		// 0x1313bc, 16-byte stride

extern vmCvar_t df_auth;

// DeFRaG 0x1594: checks the token range and derives the expected reply
static int G_AuthHash( int token, const char *str ) {
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

// DeFRaG 0x271a, called from ClientConnect right after the client struct was cleared
void G_AuthConnect( gentity_t *ent ) {
	authRec_t *rec;

	if ( !df_auth.integer ) {
		ent->authRec = NULL;
		return;
	}
	rec = &g_authRecs[ent->client->ps.clientNum];
	ent->authRec = rec;
	rec->target = 0;		// memset( rec, 0, 4 )
}

// DeFRaG 0x26e8, from ClientBegin
void G_AuthBegin( gentity_t *ent ) {
	authRec_t *rec = ent->authRec;

	if ( rec && !rec->deadline && !rec->sendTime ) {
		rec->sendTime = level.time + AUTH_DELAY_MS;
	}
}

// DeFRaG 0x2c3f
static void G_AuthSendRequest( gentity_t *ent ) {
	authRec_t *rec = ent->authRec;

	if ( g_dfVariant || !rec ) {
		return;
	}
	if ( ent->r.svFlags & SVF_BOT ) {
		return;
	}
	if ( rec->deadline ) {
		return;
	}
	rec->token = rand() % 0xf3e58 + 0x14d;
	rec->deadline = level.time + AUTH_TIMEOUT_MS;
	trap_SendServerCommand( rec->target, va( "auth_request %i %i", rec->token, trap_Cvar_VariableIntegerValue( "sv_mapChecksum" ) ) );
}

// DeFRaG 0x259a (head): returns qtrue when the client was dropped this frame
qboolean G_AuthFrame( gentity_t *ent ) {
	authRec_t *rec = ent->authRec;

	if ( !rec ) {
		return qfalse;
	}
	if ( rec->deadline && rec->deadline <= level.time ) {
		rec->deadline = 0;
		trap_DropClient( ent->client->ps.clientNum, "dropped: Authentication timed out\n" );
		return qtrue;
	}
	if ( rec->sendTime && rec->sendTime <= level.time ) {
		rec->sendTime = 0;
		G_AuthSendRequest( ent );
	}
	return qfalse;
}

// DeFRaG 0x2cab: verify a reply
static void G_AuthReply( const char *reply, int clientNum ) {
	authRec_t	*rec = &g_authRecs[clientNum];
	int			expected;

	if ( !rec->deadline ) {
		return;
	}
	expected = G_AuthHash( rec->token, va( "%i.%02i.%02i", 1, 91, 31 ) );
	if ( expected == 0 || atoi( reply ) != expected ) {
		trap_DropClient( clientNum, "dropped: Authentication failed\n" );
	}
	rec->deadline = 0;
}

// DeFRaG 0x2d45: pre-dispatch of "auth_reply <n>" and "notify <text>"; returns qtrue when the command was consumed
qboolean G_AuthCommand( int clientNum, const char *cmd ) {
	char	buf[0x400];
	int		len;

	if ( !Q_stricmp( cmd, "auth_reply" ) ) {
		trap_Argv( 1, buf, 0xff );
		// the client appends one random decoy digit to the number
		len = strlen( buf );
		if ( len > 0 ) {
			buf[len - 1] = 0;
		}
		G_AuthReply( buf, clientNum );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "notify" ) ) {
		trap_Argv( 1, buf, sizeof( buf ) );
		trap_SendServerCommand( clientNum, va( "print \"%s\"", buf ) );
		return qtrue;
	}
	return qfalse;
}

// ─── init ────────────────────────────────────────────────────────────────────

void G_InitRaceMode( void ) {
	memset( g_raceTimers, 0, sizeof(g_raceTimers) );
	g_clockBadFrame = g_clockBadRatio = g_clockInitTime = g_clockGameAcc = g_clockRealAcc = g_clockPrevReal = 0;
	g_checkpointCount  = 0;
	g_startTimerReady  = qfalse;
	g_stopTimerReady   = qfalse;
	g_checkpointReady  = qfalse;
	g_hasTeamItem      = qfalse;
	G_FragsFilterInit();
	G_SpeedAwardInit();
	// The timer cvars (df_ndm_timer_*) are advertised by SP_target_* ("df") when still empty and chosen by the player otherwise;
	// they are cleared on a map change, not on a map_restart (G_NdmClearTimerCvars, DeFRaG 0x24825 / G_WriteSessionData).
	memset( g_ndmTimer, 0, sizeof( g_ndmTimer ) );
	trap_Cvar_Set( "defrag_route",             "" );
}

// --- timers bound to items / respawn / flags (non-defrag maps and fastcaps) -------------------------------------------------
// DeFRaG func_0x1de9 (parse), 0x1e35 (name), 0x3adb (alias lookup, data 2684 = aliases, 2872 = bg_itemlist), 0x3b96 (pickup hook),
// 0x3c23 (is a timer item), 0x274b (respawn start). See disasm/G_TRIGGER/ndm_item_timers.md.

ndmTimer_t g_ndmTimer[3];

static const struct { const char *alias, *classname; } ndmAliases[] = {
	{ "as", "item_armor_shard" }, { "ya", "item_armor_combat" }, { "ra", "item_armor_body" },
	{ "h5", "item_health_small" }, { "h25", "item_health" }, { "h50", "item_health_large" }, { "mh", "item_health_mega" },
	{ "gt", "weapon_gauntlet" }, { "sg", "weapon_shotgun" }, { "mg", "weapon_machinegun" }, { "gl", "weapon_grenadelauncher" },
	{ "rl", "weapon_rocketlauncher" }, { "lg", "weapon_lightning" }, { "rg", "weapon_railgun" }, { "pg", "weapon_plasmagun" },
	{ "bfg", "weapon_bfg" }, { "hook", "weapon_grapplinghook" },
	{ "asg", "ammo_shells" }, { "amg", "ammo_bullets" }, { "agl", "ammo_grenades" }, { "apg", "ammo_cells" }, { "alg", "ammo_lightning" },
	{ "arl", "ammo_rockets" }, { "arg", "ammo_slugs" }, { "abfg", "ammo_bfg" },
	{ "tp", "holdable_teleporter" }, { "med", "holdable_medkit" },
	{ "quad", "item_quad" }, { "bs", "item_enviro" }, { "hast", "item_haste" }, { "inv", "item_invis" }, { "reg", "item_regen" },
	{ "fli", "item_flight" },
};

static void G_NdmParseTimer( int slot, const char *cvar ) {
	ndmTimer_t	*t = &g_ndmTimer[slot];
	char		buf[64];
	int			i;

	memset( t, 0, sizeof( *t ) );
	trap_Cvar_VariableStringBuffer( cvar, buf, sizeof( buf ) );
	if ( !buf[0] || !Q_stricmp( buf, "df" ) ) {
		return;				// "df" / unset: the map's target_* entities drive this timer event
	}
	if ( !Q_stricmp( buf, "resp" ) ) {
		if ( slot == 0 ) {	// the respawn only works as a start (the checkpoint / stop value 3 is discarded)
			t->mode = 3;
			Q_strncpyz( t->name, "resp", sizeof( t->name ) );
		}
		return;
	}
	for ( i = 0; i < ARRAY_LEN( ndmAliases ); i++ ) {
		if ( !Q_stricmp( ndmAliases[i].alias, buf ) ) {
			t->item = G_NdmItemByClassname( ndmAliases[i].classname );
			if ( t->item ) {
				t->mode = 4;
				Q_strncpyz( t->name, ndmAliases[i].alias, sizeof( t->name ) );
			}
			return;
		}
	}
}

// Called by G_ResolveDfMode once the mode is known. Run mode: only the entities. Fastcaps: the enemy flag starts, touching the own flag stops.
// Otherwise (tricks): the three df_ndm_timer_* cvars.
void G_NdmTimerInit( void ) {
	memset( g_ndmTimer, 0, sizeof( g_ndmTimer ) );
	if ( g_dfMode == 1 ) {
		return;
	}
	if ( g_dfMode == 3 ) {
		g_ndmTimer[0].mode = 2;
		g_ndmTimer[2].mode = 2;
	} else {
		G_NdmParseTimer( 0, "df_ndm_timer_start" );
		G_NdmParseTimer( 1, "df_ndm_timer_checkpoint" );
		G_NdmParseTimer( 2, "df_ndm_timer_stop" );
	}
	if ( g_ndmTimer[0].mode ) {
		g_startTimerReady = qtrue;
	}
	if ( g_ndmTimer[1].mode ) {
		g_checkpointReady = qtrue;
	}
	if ( g_ndmTimer[2].mode ) {
		g_stopTimerReady = qtrue;
	}
}

qboolean G_IsTimerItem( gitem_t *item ) {		// func_0x3c23: the item chosen as start / checkpoint / stop
	return item && ( ( g_ndmTimer[0].mode == 4 && item == g_ndmTimer[0].item ) || ( g_ndmTimer[1].mode == 4 && item == g_ndmTimer[1].item )
		|| ( g_ndmTimer[2].mode == 4 && item == g_ndmTimer[2].item ) );
}

static qboolean G_NdmFlagOf( gentity_t *player, gitem_t *item, qboolean enemy ) {		// func_0x3b22 (enemy) / 0x3b5c (own)
	int team = player->client->sess.sessionTeam;

	if ( item->giType != IT_TEAM ) {
		return qfalse;
	}
	if ( item->giTag == PW_REDFLAG ) {
		return team == ( enemy ? TEAM_BLUE : TEAM_RED );
	}
	if ( item->giTag == PW_BLUEFLAG ) {
		return team == ( enemy ? TEAM_RED : TEAM_BLUE );
	}
	return qfalse;
}

// func_0x3b96: runs on every item touch, after the pickup function and before the "was it picked up" test
void G_NdmItemHook( gentity_t *player, gentity_t *ent ) {
	gitem_t		*item = ent->item;
	raceTimer_t	*t;

	if ( !item || !player->client ) {
		return;
	}
	t = G_GetTimerRecord( player->client );
	if ( g_ndmTimer[0].mode == 2 && G_NdmFlagOf( player, item, qtrue ) ) {
		G_StartRace( player );
	} else if ( g_ndmTimer[2].mode == 2 && G_NdmFlagOf( player, item, qfalse ) ) {
		G_StopRace( player );
	} else if ( g_ndmTimer[0].mode == 4 && item == g_ndmTimer[0].item ) {
		if ( !t->initialized ) {		// the start item only starts a timer that is not running yet
			G_StartRace( player );
		}
	} else if ( g_ndmTimer[1].mode == 4 && item == g_ndmTimer[1].item ) {
		G_Checkpoint( player, 1 );
	} else if ( g_ndmTimer[2].mode == 4 && item == g_ndmTimer[2].item ) {
		if ( g_ndmTimer[1].mode != 4 || t->checkpointCount ) {		// with a checkpoint item set, the stop item counts after it
			G_StopRace( player );
		}
	}
}

void G_NdmClearTimerCvars( void ) {
	trap_Cvar_Set( "df_ndm_timer_start",      "" );
	trap_Cvar_Set( "df_ndm_timer_checkpoint", "" );
	trap_Cvar_Set( "df_ndm_timer_stop",       "" );
	trap_Cvar_Set( "defrag_route",            "" );
}

// defrag_route for the tricks mode (func_0x1e60 tail): "<start>-<checkpoint>-<stop>" ("df" = the map's entity, "resp", or the item alias)
void G_NdmBuildRoute( void ) {
	const char	*name[3];
	int			i;

	for ( i = 0; i < 3; i++ ) {
		qboolean ready = i == 0 ? g_startTimerReady : ( i == 1 ? g_checkpointReady : g_stopTimerReady );

		name[i] = g_ndmTimer[i].mode ? g_ndmTimer[i].name : ( ready ? "df" : "" );
	}
	if ( !name[0][0] ) {
		return;
	}
	trap_Cvar_Set( "defrag_route", va( "%s-%s%s%s", name[0], name[1], name[1][0] ? "-" : "", name[2] ) );
}

// ─── timer state transitions (called from USE callbacks in g_trigger.c) ──────

void G_StartRace( gentity_t *ent ) {
	raceTimer_t *t       = G_GetTimerRecord( ent->client );
	int          elapsed = t->initialized ? level.time - t->startTime : 0;
	// Allow start if never run before, or timer has been running >= 750 ms.
	// The 750 ms gate prevents instant restart on re-entry into the start zone.
	if ( t->initialized == 0 || elapsed >= 750 ) {
		t->startTime = level.time;
		t->ms        = 0;
		t->state     = TIMER_RUNNING;
		t->fracAccum = 0;	// sub-frame offset is filled in by G_TimerSubFrame() when the trigger crossing can be solved
		t->fracDelta = 0;
	}
}

void G_StopRace( gentity_t *ent ) {
	raceTimer_t *t = G_GetTimerRecord( ent->client );
	if ( t->initialized != 0 ) {
		t->state = TIMER_FINISHED;
	}
}

void G_Checkpoint( gentity_t *ent, int idx ) {
	raceTimer_t *t = G_GetTimerRecord( ent->client );
	if ( t->initialized == 0 ) return;
	if ( t->checkpointBits & (1 << idx) ) return;  // already hit
	t->checkpointBits  |= (1 << idx);
	t->checkpointCount++;
	t->state = TIMER_CHECKPOINT;
}

// ─── per-frame race HUD update ───────────────────────────────────────────────
//
// CORRECTION: this does NOT mirror qagame's func_0x274b as originally claimed
// here — 0x274b is confirmed to be a one-time spawn-time weapon/ammo/health
// loadout initializer (DeFRaG's ClientSpawn-equivalent), not a per-frame
// race-HUD toggle; see disasm/G_TRIGGER/func_0x274b.md. The field VALUES
// below were instead independently derived from the actual cgame.qvm
// disassembly's HUD-draw consumption logic — see
// disasm/CGAME/cgame_timer_hud.md's "What oDFs must set in server ps" table,
// which they match exactly. Being called every frame (rather than once at
// spawn like 0x274b) is intentional and believed harmless: DeFRaG removed
// ammo[] from its ps layout for these slots (repurposed as the race-HUD
// communication area), so oDFs repurposing the same byte offsets and
// rewriting them idempotently every frame has no observed downside.
//
//   ps.ammo[1]   (ps+0x17c) = -1            always (sentinel)
//   ps.ammo[2]   (ps+0x180) = 100/200       100 = race on this map
//   ps.ammo[3-10](ps+0x184) = weapon index  weapon tag for each slot
//   ps.stats[2]  (ps+0xc0)  = 6             race mode config
//   ps.stats[3]  (ps+0xc4)  = 0/200         0 = race active
/*
DeFRaG func_0x3f0c: obfuscated race time (see disasm/G_TRIGGER/func_0x38ba.md). Only used when the server is not dedicated/variant and sv_cheats is 0.
*/
static int G_EncodeRaceTime( int ms, gclient_t *cl ) {
	static byte	mapSalt;
	static qboolean	haveSalt;
	unsigned int	x, r, a, e;
	int				i, v;

	if ( ms == 0 ) {
		return 0;
	}
	if ( !haveSalt ) {
		char map[MAX_QPATH];
		char *p;

		trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
		Q_strlwr( map );
		mapSalt = 0;
		for ( p = map; *p; p++ ) {
			mapSalt += (byte)*p;
		}
		haveSalt = qtrue;
	}

	e = (unsigned int)ms & 0x3fffff;
	a = 0;
	for ( i = 0; i < 3; i++ ) {
		a += ( e >> ( 6 * i ) ) & 0x3f;
	}
	a += ( e >> 18 ) & 0xf;
	e |= ( a & 0x3f ) << 22;

	r = rand() % 15 + 1;
	r |= ( ~r << 4 ) & 0xff;
	r |= r << 8;
	r |= r << 16;
	e ^= r;
	e &= 0xfffffff;
	e |= r << 28;

	x = (unsigned int)level.time << 2;
	x += ( (unsigned int)mapSalt + 19131u ) << 8;
	x ^= (unsigned int)level.time << 24;
	e ^= x;

	for ( i = 0; i < 3; i++ ) {
		( (byte *)&e )[i + 1] ^= ( (byte *)&e )[i];
	}

	v = (int)floor( cl->ps.origin[0] );
	e ^= (unsigned int)( v < 0 ? -v : v ) & 0xffff;
	v = (int)floor( cl->ps.velocity[0] );
	e ^= (unsigned int)( v < 0 ? -v : v ) << 16;
	v = cl->ps.stats[STAT_HEALTH];
	e ^= ( v > 0 ) ? (unsigned int)( v & 0xff ) : 0x96u;
	e ^= (unsigned int)( cl->ps.movementDir & 0xf ) << 28;
	return (int)e;
}

// DeFRaG per-frame rules (func_0x23f2, see disasm/G_UTILS/func_0x23f2.md)
extern vmCvar_t df_enforceSvFps, df_sv_script_idleCfg, df_sv_script_idleDelay;
static int g_idleDeadline;		// 0x130854

extern vmCvar_t df_lockPromode;
static int g_promodeCache, g_promodeCacheStart = -1;	// 0x1317c4
static int g_mirrorSvFps = -1, g_mirrorClFps = -1;

void G_FrameRules( void ) {
	char buf[MAX_CVAR_VALUE_STRING];
	int  v;

	if ( !g_dfVariant ) {
		// defrag_svfps / defrag_clfps mirrors of sv_fps / com_maxfps, written only on change
		v = trap_Cvar_VariableIntegerValue( "sv_fps" );
		if ( v != g_mirrorSvFps ) {
			g_mirrorSvFps = v;
			trap_Cvar_Set( "defrag_svfps", va( "%i", v ) );
		}
		v = trap_Cvar_VariableIntegerValue( "com_maxfps" );
		if ( v != g_mirrorClFps ) {
			g_mirrorClFps = v;
			trap_Cvar_Set( "defrag_clfps", va( "%i", v ) );
		}
		// runtime df_promode lock (opt-in: it makes live changes by the operator impossible)
		if ( df_lockPromode.integer ) {
			if ( g_promodeCacheStart != level.startTime ) {
				g_promodeCacheStart = level.startTime;
				g_promodeCache = df_promode.integer;
			} else if ( df_promode.integer != g_promodeCache ) {
				trap_Cvar_Set( "df_promode", va( "%i", g_promodeCache ) );
			}
		}
		// non-dedicated path: sv_fps clamp 60..140 unless cheats (opt-in here, it overrides operator config)
		if ( df_enforceSvFps.integer && !trap_Cvar_VariableIntegerValue( "sv_cheats" ) ) {
			v = trap_Cvar_VariableIntegerValue( "sv_fps" );
			if ( v > 140 ) {
				trap_Cvar_Set( "sv_fps", "140" );
				trap_SendServerCommand( -1, "print \"^1sv_fps highest value restricted to 140.^7\n\"" );
			} else if ( v < 60 ) {
				trap_Cvar_Set( "sv_fps", "60" );
				trap_SendServerCommand( -1, "print \"^1sv_fps lowest value restricted to 60.^7\n\"" );
			}
		}
		return;
	}

	// dedicated path: run df_sv_script_idleCfg after df_sv_script_idleDelay seconds without clients
	if ( !df_sv_script_idleCfg.string[0] ) {
		return;
	}
	if ( level.numConnectedClients == 0 ) {
		if ( !g_idleDeadline ) {
			g_idleDeadline = level.time + 1000 * df_sv_script_idleDelay.integer;
		} else if ( level.time > g_idleDeadline ) {
			trap_Cvar_VariableStringBuffer( "defrag_idle", buf, sizeof( buf ) );
			if ( buf[0] != '1' ) {
				trap_SendConsoleCommand( EXEC_APPEND, va( "exec %s\n", df_sv_script_idleCfg.string ) );
				trap_Cvar_Set( "defrag_idle", "1" );
			}
		}
	} else {
		g_idleDeadline = 0;
		trap_Cvar_Set( "defrag_idle", "0" );
	}
}

// DeFRaG func_0x3ccd: sticky frame-time / clock-drift flags, published as stats[12] bits 0x40 / 0x20
extern vmCvar_t df_clockCheck;

void G_ClockSanityFrame( void ) {
	int   d, now;
	float r;

	if ( !df_clockCheck.integer ) {
		return;
	}
	if ( !g_clockInitTime ) {
		g_clockInitTime = level.time;
	}
	if ( level.time < g_clockInitTime + 2500 ) {
		return;
	}
	d = level.time - level.previousTime;
	if ( d < 7 || d > 16 ) {
		g_clockBadFrame = 1;
	}
	now = trap_Milliseconds();
	if ( g_clockPrevReal != 0 ) {
		g_clockGameAcc += d;
		g_clockRealAcc += now - g_clockPrevReal;
	}
	g_clockPrevReal = now;
	if ( g_clockRealAcc >= 10000 ) {
		r = (float)g_clockGameAcc / (float)g_clockRealAcc;
		if ( r <= 0.8f || r >= 1.2f ) {
			g_clockBadRatio = 1;
		}
		g_clockGameAcc = g_clockRealAcc = 0;
	}
}

// DeFRaG func_0x38ba: publish the race time in ps.stats[7]/[8] and "timer initialized" in stats[12] bit 2
static void G_PublishRaceTime( gentity_t *ent ) {
	gclient_t	*cl = ent->client;
	raceTimer_t	*t = G_GetTimerRecord( cl );
	int			v, ms;

	// The cgame decodes the race time (and so handles the finish transition: demo autorecord, records, ghosts, stats, and shows the
	// last-time / checkpoint boxes) only while bit 2 is set, so after the finish the bit and the final time stay published
	// ('finished' latch) until the next start or respawn; G_TimerTick itself clears 'initialized' at the finish.
	if ( t->initialized || t->finished ) {
		cl->ps.stats[STAT_DEFRAG_FLAGS] |= 2;
	} else {
		cl->ps.stats[STAT_DEFRAG_FLAGS] &= ~2;
	}

	if ( g_clockBadFrame ) {
		cl->ps.stats[STAT_DEFRAG_FLAGS] |= 0x40;
	}
	if ( g_clockBadRatio ) {
		cl->ps.stats[STAT_DEFRAG_FLAGS] |= 0x20;
	}

	ms = ( !t->initialized && t->finished ) ? t->finalMs : t->ms;
	if ( g_dfVariant || trap_Cvar_VariableIntegerValue( "sv_cheats" ) ) {
		v = ms;
	} else {
		v = G_EncodeRaceTime( ms, cl );
	}
	cl->ps.stats[STAT_RACE_TIME_LO] = v & 0xffff;
	cl->ps.stats[STAT_RACE_TIME_HI] = v >> 16;
}

void G_UpdateRacePs( gentity_t *ent ) {
	gclient_t *client    = ent->client;
	int        raceActive = g_startTimerReady ? qtrue : qfalse;
	int        i;

	G_PublishRaceTime( ent );

	// LEGACY "race HUD signal" (see disasm/CGAME/cgame_timer_hud.md and the correction in disasm/G_TRIGGER/func_0x38ba.md):
	// these writes clobber the real ammo[] / STAT_WEAPONS / STAT_ARMOR every frame. The offsets they were derived from are
	// snapshot-relative in the cgame (snap = ps + 0x2c): the cgame actually reads snap+0x17c/0x180/0x184 = ps.powerups[6..8]
	// (flight / red flag / blue flag), not ammo. Disabled by default; set df_racePsSignal 1 to restore the old behaviour.
	if ( !df_racePsSignal.integer ) {
		return;
	}

	client->ps.ammo[1] = -1;

	// race-active signal read by cgame to enable the timer HUD
	client->ps.ammo[2] = raceActive ? 100 : 200;

	// weapon tag for each slot: weapon index when race is configured, 200 = none
	for ( i = 3; i <= 10; i++ ) {
		client->ps.ammo[i] = raceActive ? i : 200;
	}

	// stats[2] = race mode config (6 means "standard race mode" to the cgame HUD)
	client->ps.stats[2] = raceActive ? 6 : 0;

	// stats[3] = 0 when race active, 200 when not
	client->ps.stats[3] = raceActive ? 0 : 200;
}

// ─── per-frame tick (called from ClientThink_real after Pmove) ───────────────
//
// NOTE on bit 0x4 (STAT_DEFRAG_FLAGS "running" bit): DeFRaG's real qagame
// (func_0x37c7) XOR-toggles this bit every single frame while running,
// producing a blink rather than a level signal, and never explicitly clears
// it when the race stops (see disasm/G_TRIGGER/func_0x37c7.md). Below,
// oDFs deliberately does NOT replicate that — it SETs the bit once and
// explicitly CLEARs it on stop/finish instead. This is intentional and
// confirmed correct against the actual cgame.qvm consumption logic (see
// disasm/CGAME/cgame_timer_hud.md: "the cgame detects STATE CHANGES... XOR
// toggling every frame was wrong — it caused startTime to reset each
// snapshot"). Do not "fix" this to match the qagame bytecode literally.

// ─── 1 ms (sub-frame) timer correction ───────────────────────────────────────
//
// Port of DeFRaG func_0x274db's numeric half, see disasm/G_TRIGGER/Touch_Multi_1mstimer.md.
// When a trigger_multiple changes the race timer state to RUNNING (start) or FINISHED (stop), the
// moment the player's bounding box crossed the trigger inside the last frame is solved from the pre-move
// position/velocity and the post-move velocity, and the remainder of the frame (frametime - t, ms)
// is stored in fracAccum: start -> fracAccum = offset, stop -> fracAccum -= offset. The reported time is
// (whole-ms elapsed) + round_half(fracAccum), exactly like func_0x37c7.

static int G_RoundHalf( float x ) {
	return (int)( x + 0.5f );
}

static void G_1msDebug( const char *msg, float value ) {
	if ( df_dbg_1mstimer.integer ) {
		G_Printf( "1mstimer: %s %.10f\n", msg, value );
	}
}

// Returns qtrue and the offset (ms) when the crossing could be solved.
static qboolean G_TimerCrossingOffset( gentity_t *self, gentity_t *other, float *offsetOut ) {
	gclient_t	*cl = other->client;
	trace_t		tr;
	vec3_t		start, pos0, n, vel0, vel1, delta;
	float		F, up, v0, v1, d, a, t, disc;
	int			attempt, hop, passEnt;

	F = (float)( cl->ps.commandTime - cl->timerPrevCommandTime );
	if ( F <= 0 ) {
		return qfalse;
	}

	// find the point where the movement pos0 -> pos1 entered the trigger; hop over up to one other
	// trigger/solid; if the trace hits the world while the player stepped up (0..18 units), retry once from the raised start
	VectorCopy( cl->timerPos0, pos0 );
	for ( attempt = 0 ; attempt < 2 ; attempt++ ) {
		VectorCopy( pos0, start );
		passEnt = -1;
		for ( hop = 0 ; hop < 2 ; hop++ ) {
			trap_Trace( &tr, start, other->r.mins, other->r.maxs, cl->ps.origin, passEnt, CONTENTS_TRIGGER | CONTENTS_SOLID );
			if ( &g_entities[tr.entityNum] == self || tr.fraction == 1.0f || tr.entityNum == ENTITYNUM_WORLD ) {
				break;
			}
			passEnt = tr.entityNum;
			VectorCopy( tr.endpos, start );
		}
		if ( tr.entityNum != ENTITYNUM_WORLD ) {
			break;
		}
		up = cl->ps.origin[2] - pos0[2];
		if ( up < 0 || up > 18.0f ) {
			break;
		}
		pos0[2] = cl->ps.origin[2];
		cl->timerPos0[2] = cl->ps.origin[2];
		G_1msDebug( "stepping up", up );
	}
	if ( &g_entities[tr.entityNum] != self ) {
		G_1msDebug( "failed to trace for trigger", 0 );
		return qfalse;
	}
	VectorCopy( tr.plane.normal, n );
	if ( VectorLength( n ) < 1e-5f ) {
		G_1msDebug( "player inside the trigger, no adjustement", 0 );
		return qfalse;
	}

	VectorCopy( cl->timerVel0, vel0 );
	VectorCopy( cl->ps.velocity, vel1 );		// the touch runs after Pmove, so this is DeFRaG's saved post-move velocity
	v0 = -DotProduct( vel0, n );
	v1 = -DotProduct( vel1, n );
	VectorSubtract( tr.endpos, cl->timerPos0, delta );
	d = -DotProduct( delta, n );
	a = 1000.0f * ( v1 - v0 ) / F;
	if ( v1 <= 1e-5f ) {
		a = 0;									// obstructed move: cancel acceleration
	}

	if ( fabs( a ) < 1e-5f ) {
		if ( v0 <= 1e-5f ) {
			G_1msDebug( "not moving - defaulting to main timer", 0 );
			return qfalse;
		}
		t = 1000.0f * d / v0;					// linear: d = v0 * t
	} else {
		disc = v0 * v0 + 2.0f * a * d;
		if ( disc < 0 ) {
			G_1msDebug( "negative discriminant - bailing out", 0 );
			return qfalse;
		}
		t = 2000.0f * d / ( v0 + sqrt( disc ) );	// quadratic: d = v0*t + a*t*t/2
	}
	G_1msDebug( "t (ms)", t );
	*offsetOut = F - t;
	return qtrue;
}

void G_TimerSubFrame( gentity_t *self, gentity_t *other, timerState_t stateBefore ) {
	raceTimer_t	*t = G_GetTimerRecord( other->client );
	float		offset;

	if ( t->state == stateBefore ) {
		return;
	}
	if ( t->state != TIMER_RUNNING && t->state != TIMER_FINISHED ) {
		return;
	}
	if ( !G_TimerCrossingOffset( self, other, &offset ) ) {
		return;
	}
	if ( t->state == TIMER_RUNNING ) {
		t->fracAccum = offset;
		G_1msDebug( "timer offset +=", offset );
	} else {
		t->fracAccum -= offset;
		G_1msDebug( "timer offset -=", offset );
	}
}

void G_TimerTick( gentity_t *ent, int elapsed_ms ) {
	raceTimer_t  *t      = G_GetTimerRecord( ent->client );
	gclient_t    *client = ent->client;

	(void)elapsed_ms;  // not used; we derive elapsed from level.time

	if ( client->ps.stats[STAT_HEALTH] < 1 ) {
		t->state = TIMER_IDLE;
		return;
	}

	if ( t->initialized ) {
		t->ms = level.time - t->startTime;
	}

	if ( t->initialized == 0 ) goto check_state;

	if ( t->state == TIMER_CHECKPOINT ) {
		// One-shot event: toggle the checkpoint bit for this one snapshot.
		client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x10;
	}

	if ( t->state == TIMER_FINISHED ) {
		t->bestTime    = level.time - t->startTime + G_RoundHalf( t->fracAccum );
		t->initialized = 0;
		t->finalMs = t->ms;		// rec[0] of the original: the whole-frame time is what is published; only '(est. ...)' carries the sub-frame correction
		t->finished = 1;
		// One-shot event: toggle the finish bit; clear the running bit.
		client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x8;
		client->ps.stats[STAT_DEFRAG_FLAGS] &= ~0x4;
		G_RaceFinished( ent );
	}

check_state:
	if ( t->state == TIMER_RUNNING ) {
		if ( t->initialized == 0 ) {
			// First frame of running state — timer starting now
			t->finished = 0;
			client->ps.stats[STAT_DEFRAG_FLAGS] |= 0x4;
		}
		t->initialized = 1;
		// Keep bit SET (not XOR'd) so the cgame sees a stable RUNNING signal.
		client->ps.stats[STAT_DEFRAG_FLAGS] |= 0x4;
	} else {
		t->bestTime = level.time - t->startTime + G_RoundHalf( t->fracAccum );
		t->state    = TIMER_IDLE;
		// Timer not running — clear the running bit.
		client->ps.stats[STAT_DEFRAG_FLAGS] &= ~0x4;
	}
}
