/*
 * gamesteps.c - per-step replay of a demo through the real qagame.qvm
 *
 *   dfgolden gamesteps --game qagame.qvm --qvm pmove.qvm (--bsp map.bsp | --maps dir) [--ulps N] [--max N]
 *                      [--times FILE] <demo> ...
 *
 * The game runs on the demo's map and settings. For every step A -> B of the demo the player's state
 * is set to the recorded state A, the usercmd recovered for the step is fed to the game's ClientThink,
 * the server frame runs, and the player state the game produced is compared with the recorded state B.
 * Everything the game does on its own (weapons, projectiles, explosions and their knockback, damage,
 * triggers, the race timer) is therefore tested, while the player cannot drift away from the recording.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "steps.h"
#include "psdiff.h"
#include "../host/gamehost.h"

#define EF_FIRING			0x00000100
#define WEAPON_DROPPING		2

void GameRun_ApplyInfo( gamehost_t *gh, const char *info );
void GameRun_ApplyDerived( gamehost_t *gh, const char *serverinfo, const char *systeminfo );

/* what the replayed steps exercised, and how many of those matched exactly (all demos of a run) */
static int covFire[16], covFireExact[16];
static int covKnock, covKnockExact, covPad, covPadExact, covDamage, covDamageExact, covFiringHold, covFiringHoldExact;

typedef struct {
	const char	*game, *qvm, *bsp, *mapsDir, *timesFile, *offsets;
	int			ulps, maxReport;
	int			listFire;	/* print the first fired steps */
	int			dumpFrom, dumpTo;	/* print the missile entities of these snapshots, recorded and simulated */
	int			lateThink;	/* run the server frame before the client command (other engine ordering) */
} gsOpts_t;

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
			return d->snaps[j].ps.weapon;
		}
	}
	return d->snaps[i].ps.weapon;
}

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

void CompareConfig( gamehost_t *gh, const demo_t *d );

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

static int RunGameDemo( const char *path, const gsOpts_t *o, FILE *times ) {
	static demo_t d;
	recoveredCmd_t *rec = NULL;
	int numRec = 0, i, bspLen = 0, pairs = 0, exact = 0, steps = 0, reported = 0, f;
	static int fieldCount[PSDIFF_FIELDS];
	static int elemCount[4][16];
	unsigned char *bsp;
	gamehost_t *gh;
	char err[256], map[128], mapPath[512], val[128], userinfo[512];
	playerState_t *ps;
	int client, startTime, decayStep = -1;

	if ( Steps_Recover( path, o->qvm, o->bsp, o->mapsDir, o->ulps, &rec, &numRec ) ) {
		printf( "%s: input recovery failed\n", path );
		return -1;
	}
	if ( Demo_Load( path, &d ) || d.err[0] || d.numSnaps != numRec ) {
		printf( "%s: cannot read demo\n", path );
		free( rec );
		return -1;
	}

	Q_strncpyz( map, Info_ValueForKey( d.serverinfo, "mapname" ), sizeof( map ) );
	Q_strlwr( map );
	if ( o->bsp ) {
		Q_strncpyz( mapPath, o->bsp, sizeof( mapPath ) );
	} else {
		Com_sprintf( mapPath, sizeof( mapPath ), "%s/%s.bsp", o->mapsDir, map );
	}
	bsp = ReadAll( mapPath, &bspLen );
	if ( !bsp ) {
		printf( "%s: no map file %s\n", path, mapPath );
		free( rec );
		Demo_Free( &d );
		return -1;
	}

	gh = GH_Create( o->game, err, sizeof( err ) );
	if ( !gh ) {
		printf( "cannot load the game: %s\n", err );
		return -1;
	}
	GameRun_ApplyInfo( gh, d.serverinfo );
	GameRun_ApplyInfo( gh, d.systeminfo );
	GameRun_ApplyDerived( gh, d.serverinfo, d.systeminfo );
	/* variant servers (defrag_gametype >= 5) are dedicated; the other demos are listen-server games */
	GH_SetCvar( gh, "dedicated", atoi( Info_ValueForKey( d.serverinfo, "defrag_gametype" ) ) >= 5 ? "1" : "0" );

	/* settings the server does not publish are read from the flag word of the first snapshot (the game packs
	   them at the spawn): the grapple mode (df_hook_Type) and the lightning gun knockback */
	{
		const int cfgBits = 0x400 | 0x800 | 0x1000;
		int first = d.snaps[0].ps.stats[12] & cfgBits, hook = ( first >> 10 ) & 3;

		GH_SetCvar( gh, "df_hook_Type", va( "%d", hook ) );
		GH_SetCvar( gh, "df_lg_Knockback", ( first & 0x1000 ) ? "1" : "0" );
	}

	startTime = d.snaps[0].serverTime - 408;
	if ( GH_StartMap( gh, bsp, bspLen, startTime, err, sizeof( err ) ) ) {
		printf( "%s: %s (unhandled syscalls:%s)\n", path, err, GH_Unhandled( gh ) );
		return -1;
	}
	CompareConfig( gh, &d );
	{
		const char *pf = Info_ValueForKey( d.systeminfo, "pmove_fixed" );
		int predict, ev19 = 0, ext19 = 0, k;

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
		Com_sprintf( userinfo, sizeof( userinfo ),
			"\\name\\Player\\model\\sarge\\hmodel\\sarge\\rate\\25000\\snaps\\40\\handicap\\100\\ip\\localhost\\cg_pmove_fixed\\%d\\cg_predictItems\\%d",
			atoi( pf ) ? 1 : 0, predict );
	}
	client = GH_ConnectClient( gh, userinfo, err, sizeof( err ) );
	if ( client < 0 ) {
		printf( "%s: %s\n", path, err );
		return -1;
	}
	ps = GH_PlayerState( gh, client );
	if ( o->offsets ) {
		int j = FindDecayStep( &d );

		if ( GH_LoadOffsets( gh, o->offsets, err, sizeof( err ) ) ) {
			printf( "%s: %s\n", path, err );
			return -1;
		}
		decayStep = j;
	}
	if ( GH_RunFrame( gh, d.snaps[0].serverTime ) ) {
		printf( "%s: first frame failed: %s\n", path, GH_Error( gh ) );
		return -1;
	}
	/* residual + 8 ms per step reaches 1000 in step j (set after the first frame: it runs the connected client's
	   stale command once, which adds up to 200 ms) */
	if ( decayStep > 0 ) {
		GH_SetTimeResidual( gh, client, ( 1000 - 8 * ( decayStep % 125 ) ) % 1000 );
	}
	ResyncPlayer( ps, &d.snaps[0].ps );

	for ( i = 1; i < d.numSnaps; i++ ) {
		const playerState_t *A = &d.snaps[i - 1].ps, *B = &d.snaps[i].ps;
		unsigned long long mask;
		playerState_t got;
		usercmd_t cmd;
		int n;

		ResyncPlayer( ps, A );
		if ( o->offsets ) {
			GH_SetHealth( gh, client, A->stats[0] );		/* the game's own copy of the health */
		}
		if ( o->lateThink && GH_RunFrame( gh, d.snaps[i].serverTime ) ) {
			printf( "%s: RunFrame failed at snapshot %d: %s\n", path, i, GH_Error( gh ) );
			break;
		}
		if ( rec[i].regular || B->commandTime - A->commandTime == d.snaps[1].serverTime - d.snaps[0].serverTime ) {
			/* the game ran this step too (a respawn or teleport step has no recovered command; the keys of the
			   input word are enough because the state is taken from the recording afterwards) */
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
			/* PMF_RESPAWNED stays while the attack or the use-item button is down, and only attack is in the input
			   word */
			if ( B->eFlags & 4096 ) {
				buttons |= 2;		/* BUTTON_TALK: the player is typing (EF_TALK) */
			}
			if ( B->pm_flags & 1024 ) {
				buttons |= 4;		/* PMF_USE_ITEM_HELD: the use-item button is down */
			}
			if ( ( B->pm_flags & 512 ) && !( buttons & 1 ) ) {
				buttons |= 4;		/* BUTTON_USE_HOLDABLE */
			}
			Steps_BuildCmd( &cmd, A, B, buttons, fm, rm, um, TargetWeapon( &d, i ) );
			if ( getenv( "DFG_DEBUG_SNAP" ) && atoi( getenv( "DFG_DEBUG_SNAP" ) ) == i ) {
				printf( "  DEBUG snap %d: fm %d rm %d um %d buttons %d | A bob %d B bob %d | B pmf 0x%x ground %d input 0x%x vel (%g %g %g)\n", i,
					fm, rm, um, buttons, A->bobCycle, B->bobCycle, B->pm_flags, B->groundEntityNum, B->stats[13], B->velocity[0],
					B->velocity[1], B->velocity[2] );
				printf( "                 A weapon %d state %d time %d ammo %d | B weapon %d state %d time %d | cmd.weapon %d stats[2] 0x%x\n",
					A->weapon, A->weaponstate, A->weaponTime, A->ammo[A->weapon & 15], B->weapon, B->weaponstate, B->weaponTime,
					cmd.weapon, A->stats[2] );
			}
			GH_SetUsercmd( gh, client, &cmd );
			if ( GH_ClientThink( gh, client ) ) {
				printf( "%s: ClientThink failed at snapshot %d: %s\n", path, i, GH_Error( gh ) );
				break;
			}
		}
		if ( GH_RunFrame( gh, d.snaps[i].serverTime ) ) {
			printf( "%s: RunFrame failed at snapshot %d: %s\n", path, i, GH_Error( gh ) );
			break;
		}
		if ( !rec[i].regular ) {
			ResyncPlayer( ps, B );		/* respawn / teleport / irregular step: continue from the recording */
			continue;
		}
		steps++;
		pairs++;

		if ( getenv( "DFG_DEBUG_SNAP" ) && atoi( getenv( "DFG_DEBUG_SNAP" ) ) == i ) {
			printf( "  DEBUG snap %d: RECORDED A seq %d ev (%d %d) parm (%d %d) ext %d/%d | B seq %d ev (%d %d) parm (%d %d) ext %d/%d origin (%g %g %g)\n"
				"                  SIMULATED seq %d ev (%d %d) parm (%d %d) ext %d/%d origin (%g %g %g)\n", i,
				A->eventSequence, A->events[0], A->events[1], A->eventParms[0], A->eventParms[1], A->externalEvent, A->externalEventParm,
				B->eventSequence, B->events[0], B->events[1], B->eventParms[0], B->eventParms[1], B->externalEvent, B->externalEventParm,
				B->origin[0], B->origin[1], B->origin[2],
				ps->eventSequence, ps->events[0], ps->events[1], ps->eventParms[0], ps->eventParms[1], ps->externalEvent, ps->externalEventParm,
				ps->origin[0], ps->origin[1], ps->origin[2] );
		}
		if ( getenv( "DFG_SCAN_GAUNTLET" ) && A->weapon == 1 && ( B->stats[13] & 0x100 ) && A->weaponstate == 0 && A->weaponTime == 0 ) {
			printf( "  GAUNTLET step %d: attack held, A ready; B state %d time %d event %d/%d pmf 0x%x\n", i, B->weaponstate, B->weaponTime,
				B->events[0], B->events[1], B->pm_flags );
		}
		got = *ps;
		got.clientNum = B->clientNum;
		/* the race time words are scrambled with the recording server's version and clock: compared separately */
		got.stats[7] = B->stats[7];
		got.stats[8] = B->stats[8];
		/* bits 2 and 4 of the flags word: the port keeps a 'finished' latch in bit 2 and clears bit 4 at the
		   finish on purpose (g_defrag.c), the original does the opposite; not compared */
		got.stats[12] = ( got.stats[12] & ~6 ) | ( B->stats[12] & 6 );
		n = PSDiff( &got, B, &mask );
		if ( o->dumpTo > o->dumpFrom && i >= o->dumpFrom && i < o->dumpTo && d.keepStart ) {
			int ei, base = d.keepStart[i - gKeepFrom], cnt = d.keepCount[i - gKeepFrom];

			for ( ei = 0; ei < cnt; ei++ ) {
				const entityState_t *e = &d.keepEnts[base + ei];

				if ( i == o->dumpFrom + 1 ) {
					printf( "  entity #%d eType %d weapon %d solid %d\n", e->number, e->eType, e->weapon, e->solid );
				}
				if ( e->number >= MAX_CLIENTS && ( e->eType == 3 || e->eType >= 13 || ( e->eType == 0 && e->weapon ) ) ) {		/* ET_MISSILE, an event entity or a spent missile */
					printf( "  snap %d RECORDED #%d eType %d weapon %d trType %d trTime %d base (%g %g %g) delta (%g %g %g) eventParm %d\n", i, e->number,
						e->eType, e->weapon, e->pos.trType, e->pos.trTime, e->pos.trBase[0], e->pos.trBase[1], e->pos.trBase[2], e->pos.trDelta[0],
						e->pos.trDelta[1], e->pos.trDelta[2], e->eventParm );
				}
			}
			for ( ei = 0; ei < GH_NumEntities( gh ); ei++ ) {
				const sharedEntity_t *e = GH_Entity( gh, ei );

				if ( e->s.number == ei && ei >= MAX_CLIENTS && ( e->s.eType == 3 || e->s.eType >= 13 || ( e->s.eType == 0 && e->s.weapon ) ) ) {
					printf( "  snap %d SIMULATED #%d eType %d weapon %d trType %d trTime %d base (%g %g %g) delta (%g %g %g) eventParm %d\n", i, e->s.number,
						e->s.eType, e->s.weapon, e->s.pos.trType, e->s.pos.trTime, e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
						e->s.pos.trDelta[0], e->s.pos.trDelta[1], e->s.pos.trDelta[2], e->s.eventParm );
				}
			}
		}
		{
			int w = B->weapon & 15;
			int fired = B->weaponTime > A->weaponTime;
			int knock = ( B->pm_flags & 64 ) && !( A->pm_flags & 64 );
			int pad = B->jumppad_ent != A->jumppad_ent;
			int dmg = B->damageEvent != A->damageEvent;

			if ( fired ) {
				static int listed;

				covFire[w]++;
				if ( !n ) covFireExact[w]++;
				if ( o->listFire && listed++ < o->listFire ) {
					printf( "  FIRE at snapshot %d weapon %d (%s)\n", i, w, n ? "differs" : "exact" );
				}
			}
			if ( knock ) { covKnock++; if ( !n ) covKnockExact++; }
			if ( pad ) { covPad++; if ( !n ) covPadExact++; }
			if ( dmg ) { covDamage++; if ( !n ) covDamageExact++; }
			if ( ( B->eFlags & EF_FIRING ) && !fired ) { covFiringHold++; if ( !n ) covFiringHoldExact++; }
		}
		if ( !n ) {
			exact++;
			continue;
		}
		for ( f = 0; f < PSDIFF_FIELDS; f++ ) {
			if ( mask & ( 1ULL << f ) ) {
				fieldCount[f]++;
			}
		}
		if ( ( mask & 1 ) && !getenv( "DF_NODEBUG" ) ) {
			static int dbgCmd;

			if ( dbgCmd++ < 1 ) {
				int ff;

				for ( ff = 0; ff < PSDIFF_FIELDS; ff++ ) {
					if ( mask & ( 1ULL << ff ) ) {
						char one[160];

						PSDiffDescribe( &got, B, ff, one, sizeof( one ) );
						printf( "     %s\n", one );
					}
				}
				printf( "     flags got 0x%x want 0x%x, persistant[3..4] got %d %d want %d %d, eFlags got 0x%x want 0x%x\n", got.stats[12],
					B->stats[12], got.persistant[3], got.persistant[4], B->persistant[3], B->persistant[4], got.eFlags, B->eFlags );
			}
			if ( dbgCmd == 1 ) {
				int ci, nc = GH_NumCommands( gh );

				for ( ci = 0; ci < nc && ci < 40; ci++ ) {
					int cn;
					const char *ct = GH_Command( gh, ci, &cn );

					printf( "     cmd[%d] to %d: %.120s\n", ci, cn, ct );
				}
			}
			if ( dbgCmd < 3 ) {
				printf( "  commandTime mismatch at snapshot %d: pm_type %d/%d health %d/%d regular %d pm_flags 0x%x/0x%x cmd.serverTime %d level %d\n", i,
					got.pm_type, B->pm_type, got.stats[0], B->stats[0], rec[i].regular, got.pm_flags, B->pm_flags,
					cmd.serverTime, GH_Time( gh ) );
			}
		}
		{
			static int dbg;

			if ( dbg < 6 && ( (short)got.stats[7] != (short)B->stats[7] || (short)got.stats[8] != (short)B->stats[8] ) ) {
				printf( "  timer words at snapshot %d: want %d %d (flags 0x%x) got %d %d (flags 0x%x)\n", i, B->stats[7], B->stats[8],
					B->stats[12], got.stats[7], got.stats[8], got.stats[12] );
				dbg++;
			}
		}
		for ( f = 0; f < 16; f++ ) {		/* which element of stats / persistant / ammo / powerups */
			if ( (short)got.stats[f] != (short)B->stats[f] ) elemCount[0][f]++;
			if ( (short)got.persistant[f] != (short)B->persistant[f] ) elemCount[1][f]++;
			if ( (short)got.ammo[f] != (short)B->ammo[f] ) elemCount[2][f]++;
			if ( got.powerups[f] != B->powerups[f] ) elemCount[3][f]++;
		}
		if ( reported < o->maxReport || times ) {
			char text[400] = "", one[120];
			int shown = 0, rel = 0, hasRel;
			char tbuf[32];

			for ( f = 0; f < PSDIFF_FIELDS && shown < 4; f++ ) {
				if ( mask & ( 1ULL << f ) ) {
					PSDiffDescribe( &got, B, f, one, sizeof( one ) );
					Q_strcat( text, sizeof( text ), " " );
					Q_strcat( text, sizeof( text ), one );
					Q_strcat( text, sizeof( text ), ";" );
					shown++;
				}
			}
			hasRel = TimerRelative( &d, i, &rel );
			if ( hasRel ) {
				FormatSignedTime( tbuf, sizeof( tbuf ), rel );
			} else {
				Q_strncpyz( tbuf, "--", sizeof( tbuf ) );
			}
			if ( reported < o->maxReport ) {
				printf( "  DIFF snapshot %d timer %s (%d fields):%s\n", i, tbuf, n, text );
				reported++;
			}
			if ( times ) {
				fprintf( times, "  timer %-9s %d fields:%s\n", tbuf, n, text );
			}
		}
	}

	val[0] = '\0';
	printf( "%-60.60s %-16s steps=%-6d exact=%-6d (%.2f%%)\n", path, map, steps, exact, steps ? 100.0 * exact / steps : 0.0 );
	{
		int shown = 0;

		static const char *arrayName[4] = { "stats", "persistant", "ammo", "powerups" };
		int a2, e;

		for ( f = 0; f < 48; f++ ) {
			if ( fieldCount[f] ) {
				printf( "      %6d steps differ in %s\n", fieldCount[f], PSDiffFieldName( f ) );
				shown++;
			}
		}
		for ( a2 = 0; a2 < 4; a2++ ) {
			for ( e = 0; e < 16; e++ ) {
				if ( elemCount[a2][e] ) {
					printf( "      %6d steps differ in %s[%d]\n", elemCount[a2][e], arrayName[a2], e );
				}
			}
		}
		(void)shown;
	}
	if ( GH_Unhandled( gh )[0] ) {
		printf( "      unhandled syscalls:%s\n", GH_Unhandled( gh ) );
	}

	GH_Destroy( gh );
	free( bsp );
	free( rec );
	Demo_Free( &d );
	return 0;
}

int GameSteps_Main( int argc, char **argv ) {
	gsOpts_t o;
	FILE *times = NULL;
	int i, any = 0;

	memset( &o, 0, sizeof( o ) );
	o.ulps = 1;
	PSDiffUlps = 1;
	o.maxReport = 12;
	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--game" ) && i + 1 < argc ) {
			o.game = argv[++i];
		} else if ( !strcmp( argv[i], "--qvm" ) && i + 1 < argc ) {
			o.qvm = argv[++i];
		} else if ( !strcmp( argv[i], "--bsp" ) && i + 1 < argc ) {
			o.bsp = argv[++i];
		} else if ( !strcmp( argv[i], "--maps" ) && i + 1 < argc ) {
			o.mapsDir = argv[++i];
		} else if ( !strcmp( argv[i], "--ulps" ) && i + 1 < argc ) {
			o.ulps = atoi( argv[++i] );
			PSDiffUlps = o.ulps;
		} else if ( !strcmp( argv[i], "--max" ) && i + 1 < argc ) {
			o.maxReport = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--list-fire" ) && i + 1 < argc ) {
			o.listFire = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--dump-shot" ) && i + 2 < argc ) {
			o.dumpFrom = atoi( argv[++i] );
			o.dumpTo = atoi( argv[++i] );
			gKeepFrom = o.dumpFrom;
			gKeepTo = o.dumpTo;
		} else if ( !strcmp( argv[i], "--late-think" ) ) {
			o.lateThink = 1;
		} else if ( !strcmp( argv[i], "--offsets" ) && i + 1 < argc ) {
			o.offsets = argv[++i];
		} else if ( !strcmp( argv[i], "--times" ) && i + 1 < argc ) {
			o.timesFile = argv[++i];
		}
	}
	if ( !o.game || !o.qvm || ( !o.bsp && !o.mapsDir ) ) {
		fprintf( stderr, "usage: dfgolden gamesteps --game qagame.qvm --qvm pmove.qvm (--bsp map.bsp | --maps dir) "
			"[--ulps N] [--max N] [--times FILE] <demo> ...\n" );
		return 1;
	}
	if ( o.timesFile ) {
		times = fopen( o.timesFile, "w" );
	}
	for ( i = 0; i < argc; i++ ) {
		if ( argv[i][0] == '-' ) {
			if ( !strcmp( argv[i], "--dump-shot" ) ) {
				i += 2;
			} else if ( strcmp( argv[i], "--late-think" ) ) {
				i++;			/* option with a value */
			}
			continue;
		}
		any = 1;
		if ( times ) {
			fprintf( times, "== %s\n", argv[i] );
		}
		RunGameDemo( argv[i], &o, times );
	}
	{
		static const char *wn[16] = { "none", "gauntlet", "machinegun", "shotgun", "grenade launcher", "rocket launcher",
			"lightning gun", "railgun", "plasma gun", "bfg", "grappling hook", "?", "?", "?", "?", "?" };
		int w;

		printf( "\ncoverage of the replayed steps (steps that exercised the feature / of those matching exactly):\n" );
		for ( w = 0; w < 16; w++ ) {
			if ( covFire[w] ) {
				printf( "  weapon fired: %-18s %6d / %6d\n", wn[w], covFire[w], covFireExact[w] );
			}
		}
		printf( "  beam / continuous fire held (no new shot): %6d / %6d\n", covFiringHold, covFiringHoldExact );
		printf( "  knockback applied by the server (explosion, hit): %6d / %6d\n", covKnock, covKnockExact );
		printf( "  jump pad touched:                                 %6d / %6d\n", covPad, covPadExact );
		printf( "  damage taken (health or armor event):             %6d / %6d\n", covDamage, covDamageExact );
	}
	if ( times ) {
		fclose( times );
	}
	return any ? 0 : 1;
}
