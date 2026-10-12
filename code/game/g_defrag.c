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

// DeFRaG finish handler (func_0x35ce): the whole-millisecond time plus the sub-millisecond-corrected estimate ("est.")
static void G_RaceFinished( gentity_t *ent ) {
	raceTimer_t *t         = G_GetTimerRecord( ent->client );
	char         timeBuf[32], estBuf[32], route[MAX_CVAR_VALUE_STRING];
	int          clientNum = ent - g_entities;

	G_FormatTime( level.time - t->startTime, timeBuf, sizeof(timeBuf) );
	G_FormatTime( t->bestTime, estBuf, sizeof(estBuf) );

	if ( g_dfVariant ) {
		trap_SendServerCommand( -1, va( "print \"%s^7 reached the finish line in ^2%s^7 (est. %s)\n\"",
		                                ent->client->pers.netname, timeBuf, estBuf ) );
	} else {
		trap_Cvar_VariableStringBuffer( "defrag_route", route, sizeof( route ) );
		trap_SendServerCommand( clientNum, va( "print \"Time performed by ^2%s^7 on route ^2%s^7 : ^3%s^7 (est. %s)\n\"",
		                                ent->client->pers.netname, route, timeBuf, estBuf ) );
	}
	trap_SendServerCommand( clientNum, va( "cp \"^2Finished: %s\"", estBuf ) );
}

// ─── init ────────────────────────────────────────────────────────────────────

void G_InitRaceMode( void ) {
	memset( g_raceTimers, 0, sizeof(g_raceTimers) );
	g_checkpointCount  = 0;
	g_startTimerReady  = qfalse;
	g_stopTimerReady   = qfalse;
	g_checkpointReady  = qfalse;
	g_hasTeamItem      = qfalse;
	G_FragsFilterInit();
	// Clear the SERVERINFO cvars that cgame reads to detect timer entities.
	// SP_target_* will re-set them to "df" if entities exist on this map.
	trap_Cvar_Set( "df_ndm_timer_start",      "" );
	trap_Cvar_Set( "df_ndm_timer_stop",        "" );
	trap_Cvar_Set( "df_ndm_timer_checkpoint",  "" );
	trap_Cvar_Set( "defrag_route",             "" );
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

// DeFRaG func_0x38ba: publish the race time in ps.stats[7]/[8] and "timer initialized" in stats[12] bit 2
static void G_PublishRaceTime( gentity_t *ent ) {
	gclient_t	*cl = ent->client;
	raceTimer_t	*t = G_GetTimerRecord( cl );
	int			v;

	if ( t->initialized ) {
		cl->ps.stats[STAT_DEFRAG_FLAGS] |= 2;
	} else {
		cl->ps.stats[STAT_DEFRAG_FLAGS] &= ~2;
	}

	if ( g_dfVariant || trap_Cvar_VariableIntegerValue( "sv_cheats" ) ) {
		v = t->ms;
	} else {
		v = G_EncodeRaceTime( t->ms, cl );
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
		// One-shot event: toggle the finish bit; clear the running bit.
		client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x8;
		client->ps.stats[STAT_DEFRAG_FLAGS] &= ~0x4;
		G_RaceFinished( ent );
	}

check_state:
	if ( t->state == TIMER_RUNNING ) {
		if ( t->initialized == 0 ) {
			// First frame of running state — timer starting now
			G_Printf( "^2Race started for %s\n", client->pers.netname );
			G_Printf( "^3Setting stats[12] bit 0x4. Before: %d\n", client->ps.stats[STAT_DEFRAG_FLAGS] );
			client->ps.stats[STAT_DEFRAG_FLAGS] |= 0x4;
			G_Printf( "^3After: %d (bit 0x4 is %s)\n", client->ps.stats[STAT_DEFRAG_FLAGS],
				(client->ps.stats[STAT_DEFRAG_FLAGS] & 0x4) ? "SET" : "CLEAR" );
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
