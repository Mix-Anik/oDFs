/*
 * goldmain.c - the golden file commands
 *
 *   dfgolden make --game qagame.qvm --qvm pmove.qvm --offsets offsets.qvm (--bsp map.bsp | --maps dir)
 *                 (-o file.dfg | --out-dir dir) <demo> ...
 *       turns demos into golden files (needs the demo and its map; run by hand)
 *   dfgolden test --game qagame.qvm --offsets offsets.qvm [--max N] <file.dfg | dir> ...
 *       replays golden files through the game; fails when a step that matched when the file was made
 *       does not match any more (needs nothing but the files; run by the CI)
 *   dfgolden show <file.dfg>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "goldrun.h"
#include "steps.h"

static int IsDir( const char *p ) {
	struct stat st;

	return stat( p, &st ) == 0 && ( st.st_mode & S_IFDIR );
}

/* demo name -> a file name that is safe everywhere */
static void OutName( const char *demoName, char *out, int size ) {
	int n = 0;
	const char *c, *dot = strrchr( demoName, '.' );
	const char *end = dot && !Q_stricmp( dot, ".dm_68" ) ? dot : demoName + strlen( demoName );

	for ( c = demoName; c < end && n < size - 5; c++ ) {
		unsigned char ch = (unsigned char)*c;

		out[n++] = ( ch >= '0' && ch <= '9' ) || ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ch == '.' || ch == '-' || ch == '_' ? ch : '_';
	}
	out[n] = '\0';
	Q_strcat( out, size, ".dfg" );
}

static void Summary( const golden_t *g, const goldRunResult_t *r ) {
	char finish[64] = "";

	if ( r->finishWant >= 0 ) {
		char a[32], b[32];

		FormatSignedTime( a, sizeof( a ), r->finishWant );
		if ( r->finishGot >= 0 ) {
			FormatSignedTime( b, sizeof( b ), r->finishGot );
		} else {
			Q_strncpyz( b, "none", sizeof( b ) );
		}
		Com_sprintf( finish, sizeof( finish ), " finish %s/%s%s", a, b, r->finishGot != r->finishWant ? ( r->finishBad ? " WRONG" : " (known)" ) : "" );
		if ( r->eventBad ) {
			Q_strcat( finish, sizeof( finish ), " EVENT-WRONG" );
		}
	}
	printf( "%-58.58s steps=%-6d exact=%-6d (%.2f%%) known=%-4d NEW=%-4d fixed=%-3d race=%d/%d%s\n", g->name, r->steps, r->exact,
		r->steps ? 100.0 * r->exact / r->steps : 0.0, r->knownMiss, r->regressions, r->fixed, r->raceChecked - r->raceBad,
		r->raceChecked, finish );
}

static int Make( int argc, char **argv ) {
	goldMakeOpts_t mo;
	goldRunOpts_t ro;
	const char *outFile = NULL, *outDir = NULL;
	int i, made = 0, failed = 0, refine = 0, pass;
	static goldMiss_t miss[4096];

	memset( &mo, 0, sizeof( mo ) );
	memset( &ro, 0, sizeof( ro ) );
	mo.ulps = 1;
	mo.recoverUlps = 1;
	ro.maxReport = 0;
	ro.markKnown = 1;
	ro.quiet = 1;
	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--game" ) && i + 1 < argc ) {
			ro.game = argv[++i];
		} else if ( !strcmp( argv[i], "--qvm" ) && i + 1 < argc ) {
			mo.qvm = argv[++i];
		} else if ( !strcmp( argv[i], "--offsets" ) && i + 1 < argc ) {
			ro.offsets = argv[++i];
		} else if ( !strcmp( argv[i], "--bsp" ) && i + 1 < argc ) {
			mo.bsp = argv[++i];
		} else if ( !strcmp( argv[i], "--maps" ) && i + 1 < argc ) {
			mo.mapsDir = argv[++i];
		} else if ( !strcmp( argv[i], "--ulps" ) && i + 1 < argc ) {
			mo.ulps = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--recover-ulps" ) && i + 1 < argc ) {
			mo.recoverUlps = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--deep" ) ) {
			mo.deep = 1;
		} else if ( !strcmp( argv[i], "--refine" ) ) {
			refine = 1;
		} else if ( !strcmp( argv[i], "--allow-map-mismatch" ) ) {
			/* the map file is not the version the demo was recorded on (competition maps that were fixed later);
			   the replay result shows whether the collision still agrees */
			putenv( "DFG_IGNORE_CRC=1" );
		} else if ( !strcmp( argv[i], "-o" ) && i + 1 < argc ) {
			outFile = argv[++i];
		} else if ( !strcmp( argv[i], "--out-dir" ) && i + 1 < argc ) {
			outDir = argv[++i];
		}
	}
	if ( !ro.game || !mo.qvm || !ro.offsets || ( !mo.bsp && !mo.mapsDir ) || ( !outFile && !outDir ) ) {
		fprintf( stderr, "usage: dfgolden make --game qagame.qvm --qvm pmove.qvm --offsets offsets.qvm (--bsp map.bsp | --maps dir) "
			"(-o file.dfg | --out-dir dir) <demo> ...\n" );
		return 2;
	}
	for ( i = 0; i < argc; i++ ) {
		golden_t g;
		goldRunResult_t res;
		char err[256], path[600], name[300];
		const char *base, *c;

		if ( argv[i][0] == '-' ) {
			if ( !strcmp( argv[i], "--allow-map-mismatch" ) || !strcmp( argv[i], "--deep" ) || !strcmp( argv[i], "--refine" ) ) {
				continue;
			}
			if ( strcmp( argv[i], "--recover-ulps" ) && strcmp( argv[i], "--ulps" ) && strcmp( argv[i], "--game" ) && strcmp( argv[i], "--qvm" ) && strcmp( argv[i], "--offsets" ) &&
					strcmp( argv[i], "--bsp" ) && strcmp( argv[i], "--maps" ) && strcmp( argv[i], "-o" ) && strcmp( argv[i], "--out-dir" ) ) {
				continue;
			}
			i++;
			continue;
		}
		base = argv[i];
		for ( c = argv[i]; *c; c++ ) {
			if ( *c == '/' || *c == '\\' ) {
				base = c + 1;
			}
		}
		if ( Gold_Build( argv[i], &mo, &g, err, sizeof( err ) ) ) {
			printf( "FAILED %s\n", err );
			failed++;
			continue;
		}
		if ( refine ) {
			ro.miss = miss;
			ro.missCap = 4096;
		}
		if ( Gold_Run( &g, &ro, &res ) ) {
			printf( "FAILED %s: the game does not run this demo\n", base );
			Gold_Free( &g );
			failed++;
			continue;
		}
		/* the steps the game replay does not reproduce: the server's part of the step (knockback) is known from the
		   replay, so the command can be chosen to give the recorded velocity, not only the recorded position */
		for ( pass = 0; refine && pass < 2 && res.missCount; pass++ ) {
			refineReq_t *reqs = (refineReq_t *)calloc( res.missCount, sizeof( refineReq_t ) );
			int k, changed = 0, n = res.missCount;

			for ( k = 0; k < n; k++ ) {
				const goldStep_t *s = &g.steps[miss[k].step];

				reqs[k].step = miss[k].step;
				reqs[k].fm = s->cmd.forwardmove;
				reqs[k].rm = s->cmd.rightmove;
				reqs[k].um = s->cmd.upmove;
				VectorCopy( miss[k].simVel, reqs[k].simVel );
			}
			Steps_Refine( argv[i], mo.qvm, mo.bsp, mo.mapsDir, reqs, n );
			for ( k = 0; k < n; k++ ) {
				goldStep_t *s = &g.steps[reqs[k].step];

				if ( reqs[k].found && ( reqs[k].nfm != s->cmd.forwardmove || reqs[k].nrm != s->cmd.rightmove || reqs[k].num != s->cmd.upmove ) ) {
					s->cmd.forwardmove = reqs[k].nfm;
					s->cmd.rightmove = reqs[k].nrm;
					s->cmd.upmove = reqs[k].num;
					changed++;
				}
			}
			free( reqs );
			if ( !changed ) {
				break;
			}
			if ( Gold_Run( &g, &ro, &res ) ) {
				break;
			}
		}
		if ( outFile ) {
			Q_strncpyz( path, outFile, sizeof( path ) );
		} else {
			OutName( base, name, sizeof( name ) );
			Com_sprintf( path, sizeof( path ), "%s/%s", outDir, name );
		}
		if ( Gold_Write( &g, path, err, sizeof( err ) ) ) {
			printf( "FAILED %s\n", err );
			failed++;
		} else {
			FILE *f = fopen( path, "rb" );
			long size = 0;

			if ( f ) {
				fseek( f, 0, SEEK_END );
				size = ftell( f );
				fclose( f );
			}
			Summary( &g, &res );
			{
				static const char *wn[16] = { "-", "gt", "mg", "sg", "gl", "rl", "lg", "rg", "pg", "bfg", "hook", "?", "?", "?", "?", "?" };
				char cov[200] = "";
				int w;

				for ( w = 1; w < 16; w++ ) {
					if ( res.fire[w] ) {
						Q_strcat( cov, sizeof( cov ), va( " %s:%d", wn[w], res.fire[w] ) );
					}
				}
				printf( "    -> %s (%.1f KB) fired:%s knock:%d pads:%d damage:%d\n", path, size / 1024.0, cov, res.knock, res.pad, res.damage );
			}
			made++;
		}
		Gold_Free( &g );
	}
	printf( "\n%d golden files made, %d failed\n", made, failed );
	return failed ? 1 : 0;
}

static int TestOne( const char *path, const goldRunOpts_t *ro, int *total, int *bad ) {
	golden_t g;
	goldRunResult_t res;
	char err[256];

	if ( Gold_Read( &g, path, err, sizeof( err ) ) ) {
		printf("FAILED %s\n", err);
		( *bad )++;
		( *total )++;
		return -1;
	}
	Gold_Run( &g, ro, &res );
	Summary( &g, &res );
	( *total )++;
	if ( res.error || res.regressions ) {
		( *bad )++;
	}
	Gold_Free( &g );
	return 0;
}

static int Test( int argc, char **argv ) {
	goldRunOpts_t ro;
	int i, total = 0, bad = 0;

	memset( &ro, 0, sizeof( ro ) );
	ro.maxReport = 10;
	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--game" ) && i + 1 < argc ) {
			ro.game = argv[++i];
		} else if ( !strcmp( argv[i], "--offsets" ) && i + 1 < argc ) {
			ro.offsets = argv[++i];
		} else if ( !strcmp( argv[i], "--max" ) && i + 1 < argc ) {
			ro.maxReport = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "--list-known" ) ) {
			ro.listAll = 1;
		} else if ( !strcmp( argv[i], "--ignore-health" ) ) {
			ro.ignoreHealth = 1;
		}
	}
	if ( !ro.game ) {
		fprintf( stderr, "usage: dfgolden test --game qagame.qvm --offsets offsets.qvm [--max N] <file.dfg | dir> ...\n" );
		return 2;
	}
	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--list-known" ) || !strcmp( argv[i], "--ignore-health" ) ) {
			continue;
		}
		if ( argv[i][0] == '-' ) {
			i++;
			continue;
		}
		if ( IsDir( argv[i] ) ) {
			DIR *d = opendir( argv[i] );
			struct dirent *e;

			if ( !d ) {
				continue;
			}
			while ( ( e = readdir( d ) ) != NULL ) {
				size_t n = strlen( e->d_name );
				char path[1024];

				if ( n > 4 && !Q_stricmp( e->d_name + n - 4, ".dfg" ) ) {
					Com_sprintf( path, sizeof( path ), "%s/%s", argv[i], e->d_name );
					TestOne( path, &ro, &total, &bad );
				}
			}
			closedir( d );
		} else {
			TestOne( argv[i], &ro, &total, &bad );
		}
	}
	printf( "\n%d golden files, %d with new mismatches or errors\n", total, bad );
	if ( !total ) {
		printf( "no golden files found\n" );
		return 1;
	}
	return bad ? 1 : 0;
}

static int Show( int argc, char **argv ) {
	int i;

	for ( i = 0; i < argc; i++ ) {
		golden_t g;
		char err[256];
		int k, ran = 0, regular = 0, known = 0, race = 0;

		if ( Gold_Read( &g, argv[i], err, sizeof( err ) ) ) {
			printf( "%s\n", err );
			continue;
		}
		for ( k = 0; k < g.numSteps; k++ ) {
			ran += ( g.steps[k].flags & GS_RAN ) != 0;
			regular += ( g.steps[k].flags & GS_REGULAR ) != 0;
			known += ( g.steps[k].flags & GS_KNOWN_MISS ) != 0;
			race += g.steps[k].raceMs >= 0;
		}
		printf( "%s\n  made from: %s\n  map: %s (stripped %d bytes, original crc %d)\n  steps: %d (%d compared, %d with a command, %d known mismatches, %d with a race time)\n",
			argv[i], g.name, g.map, g.bspLen, g.bspCrc, g.numSteps, regular, ran, known, race );
		printf( "  hook mode %d, lg knockback %d, dedicated %d, decay step %d, race clock shift %d ms, float tolerance %d\n", g.hookType,
			g.lgKnockback, g.dedicated, g.decayStep, g.raceOffset, g.ulps );
		printf( "  recorded on: %s (defrag_vers %s, sv_fps %s)\n", Info_ValueForKey( g.serverinfo, "version" ), Info_ValueForKey( g.serverinfo, "defrag_vers" ),
			Info_ValueForKey( g.systeminfo, "sv_fps" ) );
		printf( "  userinfo: %s\n", g.userinfo );
		Gold_Free( &g );
	}
	return 0;
}

int Gold_Main( int argc, char **argv ) {
	if ( argc >= 1 && !strcmp( argv[0], "make" ) ) {
		return Make( argc - 1, argv + 1 );
	}
	if ( argc >= 1 && !strcmp( argv[0], "test" ) ) {
		return Test( argc - 1, argv + 1 );
	}
	if ( argc >= 1 && !strcmp( argv[0], "show" ) ) {
		return Show( argc - 1, argv + 1 );
	}
	return -1;
}
