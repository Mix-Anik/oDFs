/*
 * goldmake.c - turns a demo into a golden file
 *
 * The demo only holds the player states the server sent, not the commands the player typed, so the commands
 * come from the per-step search of steps.c (a command that makes Pmove() reproduce the step) plus what the
 * states say about the keys that Pmove() does not use (walk, chat, use item).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "steps.h"
#include "golden.h"
#include "goldrun.h"

#define WEAPON_DROPPING		2

static unsigned char *ReadAll( const char *path, int *length ) {
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

/* the weapon the player asked for in the step into snapshot i: weapon changes complete a few frames later,
   so look ahead to the first snapshot that is not lowering the old weapon */
static int TargetWeapon( const demo_t *d, int i ) {
	int j;

	for ( j = i; j < d->numSnaps && j < i + 60; j++ ) {
		if ( d->snaps[j].ps.weaponstate != WEAPON_DROPPING ) {
			int w = d->snaps[j].ps.weapon;

			/* the change ended with no weapon: the one asked for was taken away while the old one was lowering
			   (PM_FinishWeaponChange then falls back to nothing); ask for a weapon that was owned at the start
			   and is gone at the end */
			if ( !w && i > 0 && d->snaps[i].ps.weaponstate == WEAPON_DROPPING && d->snaps[i].ps.weapon ) {
				int k;

				for ( k = 1; k < 16; k++ ) {
					if ( ( d->snaps[i - 1].ps.stats[2] >> k & 1 ) && !( d->snaps[j].ps.stats[2] >> k & 1 ) &&
							k != d->snaps[i].ps.weapon ) {
						return k;
					}
				}
			}
			return w;
		}
	}
	return d->snaps[i].ps.weapon;
}

/* the game takes one point of health per second while it is above the maximum, counted from the
   spawn. The demo shows the phase: the first step in which health fell by one without damage */
static int FindDecayStep( const demo_t *d ) {
	int i;

	for ( i = 1; i < d->numSnaps && i < 400; i++ ) {
		const playerState_t *A = &d->snaps[i - 1].ps, *B = &d->snaps[i].ps;

		if ( A->stats[0] > A->stats[6] && B->stats[0] == A->stats[0] - 1 && B->damageEvent == A->damageEvent ) {
			return i;
		}
	}
	return -1;
}

/* does the recording publish the race time plainly (variant servers, cheats) or scrambled? */
int Gold_RacePlain( const char *serverinfo, const char *systeminfo ) {
	return atoi( Info_ValueForKey( serverinfo, "defrag_gametype" ) ) >= 5 || atoi( Info_ValueForKey( systeminfo, "sv_cheats" ) );
}

static int DecodeAt( const demo_t *d, int i, int off, unsigned int salt, int *ms ) {
	const playerState_t *ps = &d->snaps[i].ps;
	unsigned int e = ( (unsigned int)ps->stats[7] << 16 ) | ( (unsigned int)ps->stats[8] & 0xffff );

	return Gold_DecodeRaceTime( e, (unsigned int)( d->snaps[i].serverTime + off ), ps->origin[0], ps->velocity[0], ps->stats[0],
		ps->movementDir, salt, ms );
}

/* the clock shift the recording's race time was scrambled with: 0 for most, a multiple of 1024 ms or a few ms for
   some (see the cgame); the shift that decodes the most snapshots */
static int FindRaceOffset( const demo_t *d, unsigned int salt ) {
	int i, k, off, best = 0, bestScore = -1, samples[512], numSamples = 0, numPairs = 0, ms0, ms1;

	for ( i = 0; i < d->numSnaps && numSamples < 512; i++ ) {
		const playerState_t *ps = &d->snaps[i].ps;

		if ( ( ps->stats[12] & 2 ) && ( ps->stats[7] || ps->stats[8] ) ) {
			samples[numSamples++] = i;
		}
	}
	for ( i = 1; i < numSamples; i++ ) {
		numPairs += samples[i] == samples[i - 1] + 1;
	}
	if ( !numPairs ) {
		return 0;
	}
	/* a wrong shift can decode a value now and then, but only the right one decodes snapshot after snapshot with
	   the time advancing as the snapshot clock does: score the pairs of consecutive snapshots that do */
	for ( k = -64; k <= 16384 + 16384 + 1; k++ ) {
		int score = 0;

		/* 0, then multiples of 1024, then single milliseconds within +-8 s */
		if ( k == -64 ) {
			off = 0;
		} else if ( k <= 16384 ) {
			off = k * 1024;
		} else {
			off = k - 16384 - 8193;
		}
		for ( i = 1; i < numSamples; i++ ) {
			if ( samples[i] == samples[i - 1] + 1 && DecodeAt( d, samples[i - 1], off, salt, &ms0 ) &&
					DecodeAt( d, samples[i], off, salt, &ms1 ) &&
					ms1 - ms0 == d->snaps[samples[i]].serverTime - d->snaps[samples[i - 1]].serverTime ) {
				score++;
			}
		}
		if ( score > bestScore ) {
			bestScore = score;
			best = off;
		}
		if ( off == 0 && score * 10 >= numPairs * 9 ) {
			return 0;		/* the usual case */
		}
	}
	return best;
}

int Gold_Build( const char *path, const goldMakeOpts_t *o, golden_t *g, char *err, int errSize ) {
	static demo_t d;
	recoveredCmd_t *rec = NULL;
	int numRec = 0, i, bspLen = 0, plain;
	unsigned char *bsp, *stripped;
	char mapPath[512];
	const char *pf;
	int predict, ev19 = 0, ext19 = 0, k, first, step;
	unsigned int salt;

	memset( g, 0, sizeof( *g ) );
	if ( ( gRecoverDeep = o->deep, Steps_Recover( path, o->qvm, o->bsp, o->mapsDir, o->recoverUlps, &rec, &numRec ) ) ) {
		Com_sprintf( err, errSize, "%s: input recovery failed", path );
		return -1;
	}
	if ( Demo_Load( path, &d ) || d.err[0] || d.numSnaps != numRec || d.numSnaps < 3 ) {
		Com_sprintf( err, errSize, "%s: cannot read the demo", path );
		free( rec );
		return -1;
	}
	Q_strncpyz( g->map, Info_ValueForKey( d.serverinfo, "mapname" ), sizeof( g->map ) );
	Q_strlwr( g->map );
	if ( o->bsp ) {
		Q_strncpyz( mapPath, o->bsp, sizeof( mapPath ) );
	} else {
		Com_sprintf( mapPath, sizeof( mapPath ), "%s/%s.bsp", o->mapsDir, g->map );
	}
	bsp = ReadAll( mapPath, &bspLen );
	if ( !bsp ) {
		Com_sprintf( err, errSize, "%s: no map file %s", path, mapPath );
		free( rec );
		Demo_Free( &d );
		return -1;
	}
	stripped = Gold_StripBsp( bsp, bspLen, &g->bspLen );
	free( bsp );
	if ( !stripped ) {
		Com_sprintf( err, errSize, "%s: %s is not a Quake 3 map", path, mapPath );
		free( rec );
		Demo_Free( &d );
		return -1;
	}
	g->bsp = stripped;

	{
		const char *name = path, *c;

		for ( c = path; *c; c++ ) {
			if ( *c == '/' || *c == '\\' ) {
				name = c + 1;
			}
		}
		Q_strncpyz( g->name, name, sizeof( g->name ) );

		/* the demo's file name carries the finish time: map[df.cpm]MM.SS.mmm(player.country).dm_68 */
		g->nameMs = -1;
		c = strchr( name, ']' );
		if ( c && strlen( c ) > 10 && c[3] == '.' && c[6] == '.' && c[10] == '(' ) {
			g->nameMs = ( atoi( c + 1 ) * 60 + atoi( c + 4 ) ) * 1000 + atoi( c + 7 );
		}
	}
	g->serverinfo = (char *)calloc( 1, BIG_INFO_STRING );
	g->systeminfo = (char *)calloc( 1, BIG_INFO_STRING );
	Q_strncpyz( g->serverinfo, d.serverinfo, BIG_INFO_STRING );
	Q_strncpyz( g->systeminfo, d.systeminfo, BIG_INFO_STRING );
	g->bspCrc = atoi( Info_ValueForKey( d.systeminfo, "defrag_bspcrc" ) );
	switch ( Steps_EngineColl( Info_ValueForKey( d.serverinfo, "version" ) ) ) {
	case 1:
		g->knownFlags |= GOLD_KNOWN_X87;
		break;
	case 2:
		g->knownFlags |= GOLD_KNOWN_F32;
		break;
	}
	g->ulps = o->ulps;
	g->startOffset = 408;
	g->dedicated = atoi( Info_ValueForKey( d.serverinfo, "defrag_gametype" ) ) >= 5;

	/* settings the server does not publish are read from the flag word of the first snapshot (the game packs
	   them at the spawn): the grapple mode (df_hook_Type) and the lightning gun knockback */
	first = d.snaps[0].ps.stats[12] & ( 0x400 | 0x800 | 0x1000 );
	g->hookType = ( first >> 10 ) & 3;
	g->lgKnockback = ( first & 0x1000 ) ? 1 : 0;

	/* the player's cg_predictItems shows in how the pickups arrive: as a predictable event in events[] or as the
	   external event */
	for ( k = 1; k < d.numSnaps; k++ ) {
		const playerState_t *a = &d.snaps[k - 1].ps, *b = &d.snaps[k].ps;

		if ( b->eventSequence != a->eventSequence && b->events[( b->eventSequence - 1 ) & 1] == 19 ) {
			ev19++;		/* EV_ITEM_PICKUP */
		}
		if ( b->externalEvent != a->externalEvent && ( b->externalEvent & 0xff ) == 19 ) {
			ext19++;
		}
	}
	predict = ev19 > 0 || ext19 == 0;
	pf = Info_ValueForKey( d.systeminfo, "pmove_fixed" );
	Com_sprintf( g->userinfo, sizeof( g->userinfo ),
		"\\name\\Player\\model\\sarge\\hmodel\\sarge\\rate\\25000\\snaps\\40\\handicap\\100\\ip\\localhost\\cg_pmove_fixed\\%d\\cg_predictItems\\%d",
		atoi( pf ) ? 1 : 0, predict );
	g->decayStep = FindDecayStep( &d );

	plain = Gold_RacePlain( d.serverinfo, d.systeminfo );
	/* the scrambling server adds its own DeFRaG version number */
	salt = Gold_MapSalt( g->map ) + (unsigned int)atoi( Info_ValueForKey( d.serverinfo, "defrag_vers" ) );
	g->raceOffset = plain ? 0 : FindRaceOffset( &d, salt );

	g->numSteps = d.numSnaps;
	g->steps = (goldStep_t *)calloc( g->numSteps, sizeof( goldStep_t ) );
	g->states = (playerState_t *)calloc( g->numSteps, sizeof( playerState_t ) );
	step = d.snaps[1].serverTime - d.snaps[0].serverTime;

	for ( i = 0; i < d.numSnaps; i++ ) {
		const playerState_t *A = i ? &d.snaps[i - 1].ps : NULL, *B = &d.snaps[i].ps;
		goldStep_t *s = &g->steps[i];
		int rel = 0;

		g->states[i] = *B;
		s->serverTime = d.snaps[i].serverTime;
		s->timerMs = TimerRelative( &d, i, &rel ) ? rel : GOLD_NO_TIME;
		s->raceMs = -1;
		if ( B->stats[12] & 2 ) {
			unsigned int e = ( (unsigned int)B->stats[7] << 16 ) | ( (unsigned int)B->stats[8] & 0xffff );
			int ms;

			if ( !e ) {
				s->raceMs = 0;
			} else if ( plain ) {
				s->raceMs = (int)e;
			} else if ( DecodeAt( &d, i, g->raceOffset, salt, &ms ) ) {
				s->raceMs = ms;
			}
		}
		if ( !i ) {
			continue;
		}
		if ( rec[i].regular ) {
			s->flags |= GS_REGULAR;
		}
		if ( rec[i].regular || B->commandTime - A->commandTime == step ||
				( B->commandTime - A->commandTime > step && B->commandTime - A->commandTime <= 8 * step &&
				( B->commandTime - A->commandTime ) % step == 0 ) ) {
			/* the game ran this step too (a respawn or teleport step has no recovered command; the keys of the
			   input word are enough because the state is taken from the recording afterwards). A command clock that
			   jumped by several steps (the server got two commands in one frame) is run once with the whole span: it
			   advances the game's own clocks (health decay) by what the server advanced them */
			int buttons = ( B->stats[13] & 0x100 ) ? 1 : 0;		/* DINPUT_ATTACK */
			int fm = rec[i].regular ? rec[i].fm : ( B->stats[13] & 1 ) ? 127 : ( B->stats[13] & 2 ) ? -127 : 0;
			int rm = rec[i].regular ? rec[i].rm : ( B->stats[13] & 16 ) ? 127 : ( B->stats[13] & 8 ) ? -127 : 0;
			int um = rec[i].regular ? rec[i].um : ( B->stats[13] & 32 ) ? 127 : ( B->stats[13] & 64 ) ? -127 : 0;

			/* the walk key is not in the input word: it shows in the step of the bob cycle (PM_Footsteps adds
			   0.3 * 8 = 2 per step walking, 0.4 * 8 = 3 running) */
			if ( ( fm || rm ) && !( B->pm_flags & 1 /* PMF_DUCKED */ ) && B->groundEntityNum != ENTITYNUM_NONE &&
					( ( B->bobCycle - A->bobCycle ) & 255 ) == 2 ) {
				buttons |= 16;		/* BUTTON_WALKING */
			}
			if ( B->eFlags & 4096 ) {
				buttons |= 2;		/* BUTTON_TALK: the player is typing (EF_TALK) */
			}
			if ( B->pm_flags & 1024 ) {
				buttons |= 4;		/* PMF_USE_ITEM_HELD: the use-item button is down */
			}
			/* PMF_RESPAWNED stays while the attack or the use-item button is down, and only attack is in the input word */
			if ( ( B->pm_flags & 512 ) && !( buttons & 1 ) ) {
				buttons |= 4;		/* BUTTON_USE_HOLDABLE */
			}
			Steps_BuildCmd( &s->cmd, A, B, buttons, fm, rm, um, TargetWeapon( &d, i ) );
			s->flags |= GS_RAN;
		}
	}
	free( rec );
	Demo_Free( &d );
	return 0;
}
