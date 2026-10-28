/*
 * goldrun.c - replays a golden file through the real qagame.qvm
 *
 * The game runs on the file's map and settings. For every step A -> B the player's state is set to the
 * recorded state A, the usercmd of the step is fed to the game's ClientThink, the server frame runs, and the
 * player state the game produced is compared with the recorded state B. Everything the game does on its own
 * (weapons, projectiles, explosions and their knockback, damage, triggers, the race timer) is therefore
 * tested, while the player cannot drift away from the recording.
 *
 * Nothing but the golden file is read: no demo, no engine, no game installation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goldrun.h"
#include "psdiff.h"
#include "steps.h"
#include "../host/gamehost.h"

void GameRun_ApplyInfo( gamehost_t *gh, const char *info );
void GameRun_ApplyDerived( gamehost_t *gh, const char *serverinfo, const char *systeminfo );

/* copies the networked part of a recorded state into the game's player state; the fields that exist
   only on the server keep their value */
static void ResyncPlayer( playerState_t *dst, const playerState_t *src ) {
	int ping = dst->ping, framecount = dst->pmove_framecount, jpFrame = dst->jumppad_frame;
	int entEventSeq = dst->entityEventSequence, extEventTime = dst->externalEventTime, clientNum = dst->clientNum;

	*dst = *src;
	dst->ping = ping;
	dst->pmove_framecount = framecount;
	dst->jumppad_frame = jpFrame;
	dst->entityEventSequence = entEventSeq;
	dst->externalEventTime = extEventTime;
	dst->clientNum = clientNum;
}

static void TimeText( char *out, int size, int ms ) {
	if ( ms == GOLD_NO_TIME ) {
		Q_strncpyz( out, "--", size );
	} else {
		FormatSignedTime( out, size, ms );
	}
}

int Gold_Run( golden_t *g, const goldRunOpts_t *o, goldRunResult_t *res ) {
	gamehost_t *gh;
	char err[256];
	playerState_t *ps;
	int client, i, reported = 0, f, plain, finishIdx = -1;
	unsigned int salt;
	int *simMs, lastRaceDiff = 0;

	memset( res, 0, sizeof( *res ) );
	gHostColl = ( g->knownFlags & GOLD_KNOWN_X87 ) ? 1 : ( g->knownFlags & GOLD_KNOWN_F32 ) ? 2 : 0;
	res->finishWant = res->finishGot = -1;
	res->eventTime = -1;
	PSDiffUlps = g->ulps;

	gh = GH_Create( o->game, err, sizeof( err ) );
	if ( !gh ) {
		printf( "%s: cannot load the game: %s\n", g->name, err );
		res->error = 1;
		return -1;
	}
	GameRun_ApplyInfo( gh, g->serverinfo );
	GameRun_ApplyInfo( gh, g->systeminfo );
	GameRun_ApplyDerived( gh, g->serverinfo, g->systeminfo );
	GH_SetCvar( gh, "dedicated", g->dedicated ? "1" : "0" );
	GH_SetCvar( gh, "mapname", g->map );
	GH_SetCvar( gh, "df_hook_Type", va( "%d", g->hookType ) );
	GH_SetCvar( gh, "df_lg_Knockback", g->lgKnockback ? "1" : "0" );

	if ( GH_StartMap( gh, g->bsp, g->bspLen, g->steps[0].serverTime - g->startOffset, err, sizeof( err ) ) ) {
		printf( "%s: %s (unhandled syscalls:%s)\n", g->name, err, GH_Unhandled( gh ) );
		GH_Destroy( gh );
		res->error = 1;
		return -1;
	}
	client = GH_ConnectClient( gh, g->userinfo, err, sizeof( err ) );
	if ( client < 0 ) {
		printf( "%s: %s\n", g->name, err );
		GH_Destroy( gh );
		res->error = 1;
		return -1;
	}
	ps = GH_PlayerState( gh, client );
	if ( o->offsets && GH_LoadOffsets( gh, o->offsets, err, sizeof( err ) ) ) {
		printf( "%s: %s\n", g->name, err );
		GH_Destroy( gh );
		res->error = 1;
		return -1;
	}
	if ( GH_RunFrame( gh, g->steps[0].serverTime ) ) {
		printf( "%s: first frame failed: %s\n", g->name, GH_Error( gh ) );
		GH_Destroy( gh );
		res->error = 1;
		return -1;
	}
	/* residual + 8 ms per step reaches 1000 in step j (set after the first frame: it runs the connected client's
	   stale command once, which adds up to 200 ms) */
	if ( o->offsets && g->decayStep > 0 ) {
		GH_SetTimeResidual( gh, client, ( 1000 - 8 * ( g->decayStep % 125 ) ) % 1000 );
	}
	ResyncPlayer( ps, &g->states[0] );
	/* an event that is on the player when the recording starts: the game clears it 300 ms after it was added, and the
	   recording shows when it was cleared */
	if ( o->offsets && g->states[0].externalEvent ) {
		for ( i = 1; i < g->numSteps; i++ ) {
			if ( g->states[i].externalEvent != g->states[0].externalEvent ) {
				GH_SetEventTime( gh, client, g->steps[i].serverTime - 301 );
				break;
			}
		}
	}

	plain = Gold_RacePlain( g->serverinfo, g->systeminfo );
	salt = Gold_MapSalt( g->map ) + 19131u;		/* the version number this port scrambles with */
	simMs = (int *)malloc( g->numSteps * sizeof( int ) );
	for ( i = 0; i < g->numSteps; i++ ) {
		simMs[i] = -1;
	}

	for ( i = 1; i < g->numSteps; i++ ) {
		const playerState_t *A = &g->states[i - 1], *B = &g->states[i];
		goldStep_t *s = &g->steps[i];
		unsigned long long mask;
		playerState_t got;
		int n, race = 1;

		ResyncPlayer( ps, A );
		if ( o->offsets ) {
			GH_SetHealth( gh, client, A->stats[0] );		/* the game's own copy of the health */
		}
		if ( s->flags & GS_RAN ) {
			GH_SetUsercmd( gh, client, &s->cmd );
			if ( GH_ClientThink( gh, client ) ) {
				printf( "%s: ClientThink failed at step %d: %s\n", g->name, i, GH_Error( gh ) );
				res->error = 1;
				break;
			}
		}
		if ( getenv( "DFG_DEBUG_STEP" ) && atoi( getenv( "DFG_DEBUG_STEP" ) ) == i ) {
			printf( "    P (after ClientThink): origin %.9g %.9g %.9g vel %.9g %.9g %.9g\n", ps->origin[0], ps->origin[1], ps->origin[2],
				ps->velocity[0], ps->velocity[1], ps->velocity[2] );
		}
		if ( GH_RunFrame( gh, s->serverTime ) ) {
			printf( "%s: RunFrame failed at step %d: %s\n", g->name, i, GH_Error( gh ) );
			res->error = 1;
			break;
		}
		if ( !( s->flags & GS_REGULAR ) ) {
			ResyncPlayer( ps, B );		/* respawn / teleport / irregular step: continue from the recording */
			continue;
		}
		res->steps++;
		if ( B->weaponTime > A->weaponTime ) {
			res->fire[B->weapon & 15]++;
		}
		res->knock += ( B->pm_flags & 64 ) && !( A->pm_flags & 64 );
		res->pad += B->jumppad_ent != A->jumppad_ent;
		res->damage += B->damageEvent != A->damageEvent;

		/* the race time the game publishes, read the way the client reads it */
		if ( ps->stats[12] & 2 ) {
			unsigned int e = ( (unsigned int)ps->stats[7] << 16 ) | ( (unsigned int)ps->stats[8] & 0xffff );
			int ms;

			if ( !e ) {
				simMs[i] = 0;
			} else if ( plain ) {
				simMs[i] = (int)e;
			} else if ( Gold_DecodeRaceTime( e, (unsigned int)s->serverTime, ps->origin[0], ps->velocity[0], ps->stats[0],
					ps->movementDir, salt, &ms ) ) {
				simMs[i] = ms;
			}
		}
		if ( s->raceMs >= 0 ) {
			res->raceChecked++;
			if ( getenv( "DFG_DEBUG_RACE" ) && ( res->raceChecked < 8 || ( simMs[i] >= 0 && s->raceMs - simMs[i] != lastRaceDiff ) ) ) {
				lastRaceDiff = simMs[i] >= 0 ? s->raceMs - simMs[i] : lastRaceDiff;
				printf( "  race step %d: want %d, sim %d (sim words %d %d flags 0x%x, recorded words %d %d, t %d, health %d move %d ox %g vx %g)\n", i, s->raceMs, simMs[i], ps->stats[7], ps->stats[8], ps->stats[12], B->stats[7], B->stats[8], s->serverTime, ps->stats[0], ps->movementDir, ps->origin[0], ps->velocity[0] );
			}
			if ( simMs[i] != s->raceMs ) {
				res->raceBad++;
				race = 0;
			}
		}

		if ( getenv( "DFG_DEBUG_STEP" ) && atoi( getenv( "DFG_DEBUG_STEP" ) ) == i ) {
			printf( "  DEBUG step %d serverTime %d cmd: time %d angles %d %d %d buttons %d weapon %d move %d %d %d\n", i, s->serverTime,
				s->cmd.serverTime, s->cmd.angles[0], s->cmd.angles[1], s->cmd.angles[2], s->cmd.buttons, s->cmd.weapon,
				s->cmd.forwardmove, s->cmd.rightmove, s->cmd.upmove );
			printf( "    A: origin %.9g %.9g %.9g vel %.9g %.9g %.9g pmf 0x%x pmt %d ground %d cmdTime %d weaponTime %d health %d\n",
				A->origin[0], A->origin[1], A->origin[2], A->velocity[0], A->velocity[1], A->velocity[2], A->pm_flags, A->pm_time,
				A->groundEntityNum, A->commandTime, A->weaponTime, A->stats[0] );
			printf( "    B: origin %.9g %.9g %.9g vel %.9g %.9g %.9g pmf 0x%x pmt %d ground %d cmdTime %d weaponTime %d health %d dmgEv %d dmgCnt %d jp %d\n",
				B->origin[0], B->origin[1], B->origin[2], B->velocity[0], B->velocity[1], B->velocity[2], B->pm_flags, B->pm_time,
				B->groundEntityNum, B->commandTime, B->weaponTime, B->stats[0], B->damageEvent, B->damageCount, B->jumppad_ent );
			printf( "    W: A weapon %d state %d wt %d stats2 0x%x | B weapon %d state %d wt %d stats2 0x%x | S weapon %d state %d wt %d | cmd weapon %d\n",
				A->weapon, A->weaponstate, A->weaponTime, A->stats[2], B->weapon, B->weaponstate, B->weaponTime, B->stats[2],
				ps->weapon, ps->weaponstate, ps->weaponTime, s->cmd.weapon );
			printf( "    S: origin %.9g %.9g %.9g vel %.9g %.9g %.9g pmf 0x%x pmt %d ground %d cmdTime %d weaponTime %d health %d dmgEv %d dmgCnt %d jp %d\n",
				ps->origin[0], ps->origin[1], ps->origin[2], ps->velocity[0], ps->velocity[1], ps->velocity[2], ps->pm_flags, ps->pm_time,
				ps->groundEntityNum, ps->commandTime, ps->weaponTime, ps->stats[0], ps->damageEvent, ps->damageCount, ps->jumppad_ent );
		}
		got = *ps;
		got.clientNum = B->clientNum;
		/* the race time words are scrambled with the recording server's version and clock: compared decoded above */
		if ( o->ignoreHealth ) {
			got.stats[0] = B->stats[0];
		}
		got.stats[7] = B->stats[7];
		got.stats[8] = B->stats[8];
		/* bits 2 and 4 of the flags word: the port keeps a 'finished' latch in bit 2 and clears bit 4 at the
		   finish on purpose (g_defrag.c), the original does the opposite; not compared */
		got.stats[12] = ( got.stats[12] & ~6 ) | ( B->stats[12] & 6 );
		n = PSDiff( &got, B, &mask ) + ( race ? 0 : 1 );

		if ( !n ) {
			res->exact++;
			if ( s->flags & GS_KNOWN_MISS ) {
				res->fixed++;
			}
			if ( o->markKnown ) {
				s->flags &= ~GS_KNOWN_MISS;
			}
			continue;
		}
		if ( o->miss && res->missCount < o->missCap ) {
			o->miss[res->missCount].step = i;
			VectorCopy( got.velocity, o->miss[res->missCount].simVel );
			res->missCount++;
		}
		if ( o->markKnown ) {
			s->flags |= GS_KNOWN_MISS;
		}
		if ( s->flags & GS_KNOWN_MISS ) {
			res->knownMiss++;
		} else {
			res->regressions++;
		}
		if ( ( o->listAll || !( s->flags & GS_KNOWN_MISS ) ) && reported < o->maxReport && !o->quiet ) {
			char text[500] = "", one[120], tbuf[32];
			int shown = 0;

			for ( f = 0; f < PSDIFF_FIELDS && shown < 6; f++ ) {
				if ( mask & ( 1ULL << f ) ) {
					PSDiffDescribe( &got, B, f, one, sizeof( one ) );
					Q_strcat( text, sizeof( text ), " " );
					Q_strcat( text, sizeof( text ), one );
					Q_strcat( text, sizeof( text ), ";" );
					shown++;
				}
			}
			if ( !race ) {
				Q_strcat( text, sizeof( text ), va( " race time want %d got %d;", s->raceMs, simMs[i] ) );
			}
			TimeText( tbuf, sizeof( tbuf ), s->timerMs );
			printf( "  %s step %d timer %s (%d fields):%s\n", ( s->flags & GS_KNOWN_MISS ) ? "known" : "NEW MISMATCH", i, tbuf, n, text );
			if ( o->listAll ) {
				printf( "      [A: ground %d pmf 0x%x pmt %d mdir %d speed %.0f | cmd f%d r%d u%d btn %d | B: ground %d pmf 0x%x]\n", A->groundEntityNum,
					A->pm_flags, A->pm_time, A->movementDir, sqrt( A->velocity[0] * A->velocity[0] + A->velocity[1] * A->velocity[1] ),
					s->cmd.forwardmove, s->cmd.rightmove, s->cmd.upmove, s->cmd.buttons, B->groundEntityNum, B->pm_flags );
			}
			reported++;
		}
	}

	/* the race time at the finish: the step where the finished bit toggles */
	for ( i = 1; i < g->numSteps; i++ ) {
		if ( ( g->states[i].stats[12] ^ g->states[i - 1].stats[12] ) & 8 ) {
			finishIdx = i;
			break;
		}
	}
	if ( finishIdx > 0 ) {
		int j, best = -1;

		/* the game publishes the final time at the finish step and keeps it afterwards (the latch) */
		for ( j = finishIdx - 1; j <= finishIdx + 3 && j < g->numSteps; j++ ) {
			if ( simMs[j] > best ) {
				best = simMs[j];
			}
		}
		res->finishGot = best;
		j = g->steps[finishIdx].raceMs >= 0 ? finishIdx : finishIdx - 1;
		res->finishWant = g->nameMs >= 0 ? g->nameMs : g->steps[j].raceMs;
		res->finishFrom = g->nameMs >= 0;
		if ( res->finishWant >= 0 && res->finishGot != res->finishWant ) {
			if ( o->markKnown ) {
				g->knownFlags |= 1;
			} else if ( !( g->knownFlags & 1 ) ) {
				res->finishBad = 1;
				res->regressions++;
			}
		} else if ( o->markKnown ) {
			g->knownFlags &= ~1;
		}
	}

	/* what the server plugins are told (online records): when the game finished with the demo's time, the
	   "player_finish" event has to carry exactly that time */
	for ( i = 0; i < GH_NumEvents( gh ); i++ ) {
		const char *data;

		if ( !strcmp( GH_Event( gh, i, &data ), "player_finish" ) ) {
			res->eventTime = atoi( Info_ValueForKey( data, "time" ) );
			break;
		}
	}
	if ( !o->markKnown && res->finishGot >= 0 && res->finishGot == res->finishWant && res->eventTime != res->finishGot ) {
		res->eventBad = 1;
		res->regressions++;
	}

	if ( GH_Unhandled( gh )[0] && !o->quiet ) {
		printf( "      unhandled syscalls:%s\n", GH_Unhandled( gh ) );
	}
	free( simMs );
	GH_Destroy( gh );
	return res->error ? -1 : 0;
}
