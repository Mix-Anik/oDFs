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

// ─── helpers ─────────────────────────────────────────────────────────────────

raceTimer_t *G_GetTimerRecord( gclient_t *client ) {
	int clientNum = client - level.clients;
	return &g_raceTimers[clientNum];
}

static void G_FormatTime( int ms, char *buf, int bufSize ) {
	int minutes = ms / 60000;
	int seconds = (ms % 60000) / 1000;
	int millis  = ms % 1000;
	Com_sprintf( buf, bufSize, "%d:%02d.%03d", minutes, seconds, millis );
}

static void G_RaceFinished( gentity_t *ent ) {
	raceTimer_t *t         = G_GetTimerRecord( ent->client );
	char         timeBuf[32];
	int          clientNum = ent - g_entities;

	G_FormatTime( t->bestTime, timeBuf, sizeof(timeBuf) );

	trap_SendServerCommand( -1, va( "print \"%s^7 reached the finish line in ^2%s^7\n\"",
	                                ent->client->pers.netname, timeBuf ) );
	trap_SendServerCommand( clientNum, va( "cp \"^2Finished: %s\"", timeBuf ) );
}

// ─── init ────────────────────────────────────────────────────────────────────

void G_InitRaceMode( void ) {
	memset( g_raceTimers, 0, sizeof(g_raceTimers) );
	g_checkpointCount  = 0;
	g_startTimerReady  = qfalse;
	g_stopTimerReady   = qfalse;
	g_checkpointReady  = qfalse;
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
		t->state     = TIMER_RUNNING;
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
void G_UpdateRacePs( gentity_t *ent ) {
	gclient_t *client    = ent->client;
	int        raceActive = g_startTimerReady ? qtrue : qfalse;
	int        i;

	// sentinel — always -1
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

void G_TimerTick( gentity_t *ent, int elapsed_ms ) {
	raceTimer_t  *t      = G_GetTimerRecord( ent->client );
	gclient_t    *client = ent->client;

	(void)elapsed_ms;  // not used; we derive elapsed from level.time

	if ( client->ps.stats[STAT_HEALTH] < 1 ) {
		t->state = TIMER_IDLE;
		return;
	}

	if ( t->initialized == 0 ) goto check_state;

	if ( t->state == TIMER_CHECKPOINT ) {
		// One-shot event: toggle the checkpoint bit for this one snapshot.
		client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x10;
	}

	if ( t->state == TIMER_FINISHED ) {
		t->bestTime    = level.time - t->startTime;
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
		t->bestTime = level.time - t->startTime;
		t->state    = TIMER_IDLE;
		// Timer not running — clear the running bit.
		client->ps.stats[STAT_DEFRAG_FLAGS] &= ~0x4;
	}
}
