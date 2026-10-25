/*
 * gamerun.c - boots the real qagame.qvm in the test host
 *
 *   dfgolden gamerun --game qagame.qvm (--bsp map.bsp | --maps dir) [--frames N] <demo>
 *
 * Applies the demo's server settings, starts its map, connects a client and runs N empty server
 * frames, then prints what the game did. First milestone of the full-game host.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "demo.h"
#include "../host/gamehost.h"

#define EF_TELEPORT_BIT		0x00000004

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

/* the demo's serverinfo / systeminfo as cvars; engine-owned and file lists are left out */
void GameRun_ApplyInfo( gamehost_t *gh, const char *info ) {
	while ( *info == '\\' ) {
		char key[BIG_INFO_STRING], val[BIG_INFO_STRING];
		int n = 0;

		info++;
		while ( *info && *info != '\\' && n < (int)sizeof( key ) - 1 ) {
			key[n++] = *info++;
		}
		key[n] = '\0';
		if ( *info != '\\' ) {
			break;
		}
		info++;
		n = 0;
		while ( *info && *info != '\\' && n < (int)sizeof( val ) - 1 ) {
			val[n++] = *info++;
		}
		val[n] = '\0';
		if ( !Q_stricmpn( key, "sv_referenced", 13 ) || !Q_stricmpn( key, "sv_pak", 6 ) || !Q_stricmp( key, "sv_serverid" ) ||
			!Q_stricmp( key, "mapname" ) || !Q_stricmp( key, "sv_mapChecksum" ) || !Q_stricmp( key, "version" ) ||
			!Q_stricmp( key, "com_protocol" ) || !Q_stricmp( key, "protocol" ) ) {
			continue;
		}
		GH_SetCvar( gh, key, val );
	}
}

/* the port publishes some values it derives from other settings; hand the game the settings, not
   the derived values */
void GameRun_ApplyDerived( gamehost_t *gh, const char *serverinfo, const char *systeminfo ) {
	char obs[64], svfps[64], clfps[64];

	/* Info_ValueForKey returns one of two alternating static buffers: copy every result */
	Q_strncpyz( obs, Info_ValueForKey( systeminfo, "defrag_obs" ), sizeof( obs ) );
	Q_strncpyz( svfps, Info_ValueForKey( systeminfo, "defrag_svfps" ), sizeof( svfps ) );
	Q_strncpyz( clfps, Info_ValueForKey( systeminfo, "defrag_clfps" ), sizeof( clfps ) );
	(void)serverinfo;
	/* engine cvars the game mirrors into defrag_svfps / defrag_clfps (G_FrameRules) */
	if ( svfps[0] ) {
		GH_SetCvar( gh, "sv_fps", svfps );
	}
	if ( clfps[0] ) {
		GH_SetCvar( gh, "com_maxfps", clfps );
	}
	/* defrag_obs 1 = overbounces on, i.e. df_ob_KillOBs 0 (G_ResolveDfMode) */
	if ( obs[0] ) {
		GH_SetCvar( gh, "df_ob_KillOBs", atoi( obs ) ? "0" : "1" );
	}
}

int GameRun_Main( int argc, char **argv ) {
	const char *gameQvm = NULL, *bspPath = NULL, *mapsDir = NULL, *demoPath = NULL;
	int frames = 200, i, bspLen = 0, startTime = 100000, client;
	static demo_t d;
	unsigned char *bsp;
	gamehost_t *gh;
	char err[256], map[128], path[512];
	playerState_t *ps;

	for ( i = 0; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--game" ) && i + 1 < argc ) {
			gameQvm = argv[++i];
		} else if ( !strcmp( argv[i], "--bsp" ) && i + 1 < argc ) {
			bspPath = argv[++i];
		} else if ( !strcmp( argv[i], "--maps" ) && i + 1 < argc ) {
			mapsDir = argv[++i];
		} else if ( !strcmp( argv[i], "--frames" ) && i + 1 < argc ) {
			frames = atoi( argv[++i] );
		} else if ( argv[i][0] != '-' ) {
			demoPath = argv[i];
		}
	}
	if ( !gameQvm || !demoPath || ( !bspPath && !mapsDir ) ) {
		fprintf( stderr, "usage: dfgolden gamerun --game qagame.qvm (--bsp map.bsp | --maps dir) [--frames N] <demo>\n" );
		return 1;
	}
	if ( Demo_Load( demoPath, &d ) || d.err[0] || d.numSnaps < 2 ) {
		fprintf( stderr, "cannot read demo %s: %s\n", demoPath, d.err );
		return 1;
	}

	Q_strncpyz( map, Info_ValueForKey( d.serverinfo, "mapname" ), sizeof( map ) );
	Q_strlwr( map );
	if ( bspPath ) {
		Q_strncpyz( path, bspPath, sizeof( path ) );
	} else {
		Com_sprintf( path, sizeof( path ), "%s/%s.bsp", mapsDir, map );
	}
	bsp = ReadAll( path, &bspLen );
	if ( !bsp ) {
		fprintf( stderr, "no map file %s\n", path );
		return 1;
	}

	gh = GH_Create( gameQvm, err, sizeof( err ) );
	if ( !gh ) {
		fprintf( stderr, "cannot load the game: %s\n", err );
		return 1;
	}
	GameRun_ApplyInfo( gh, d.serverinfo );
	GameRun_ApplyInfo( gh, d.systeminfo );
	GH_SetCvar( gh, "dedicated", "1" );		/* the recorded games ran on dedicated servers */

	if ( GH_StartMap( gh, bsp, bspLen, startTime, err, sizeof( err ) ) ) {
		fprintf( stderr, "%s\n", err );
		fprintf( stderr, "unhandled syscalls:%s\n", GH_Unhandled( gh ) );
		return 1;
	}
	printf( "map %s started: %d entities, game time %d\n", map, GH_NumEntities( gh ), GH_Time( gh ) );

	client = GH_ConnectClient( gh,
		"\\name\\Player\\model\\sarge\\hmodel\\sarge\\rate\\25000\\snaps\\40\\handicap\\100\\ip\\127.0.0.1\\cg_pmove_fixed\\1",
		err, sizeof( err ) );
	if ( client < 0 ) {
		fprintf( stderr, "%s\n", err );
		fprintf( stderr, "unhandled syscalls:%s\n", GH_Unhandled( gh ) );
		return 1;
	}
	ps = GH_PlayerState( gh, client );
	printf( "client %d connected: origin (%g %g %g) angles (%g %g %g) pm_type %d health %d\n", client, ps->origin[0],
		ps->origin[1], ps->origin[2], ps->viewangles[0], ps->viewangles[1], ps->viewangles[2], ps->pm_type,
		ps->stats[0] );

	for ( i = 0; i < frames; i++ ) {
		usercmd_t cmd;

		memset( &cmd, 0, sizeof( cmd ) );
		cmd.serverTime = GH_Time( gh ) + 8;
		GH_SetUsercmd( gh, client, &cmd );
		if ( GH_ClientThink( gh, client ) || GH_RunFrame( gh, GH_Time( gh ) + 8 ) ) {
			fprintf( stderr, "frame %d failed: %s\n", i, GH_Error( gh ) );
			break;
		}
		if ( i % 50 == 0 || i == frames - 1 ) {
			printf( "frame %4d t=%d origin (%g %g %g) vel (%g %g %g) ground %d\n", i, GH_Time( gh ), ps->origin[0],
				ps->origin[1], ps->origin[2], ps->velocity[0], ps->velocity[1], ps->velocity[2], ps->groundEntityNum );
		}
	}
	printf( "server commands sent by the game: %d\n", GH_NumCommands( gh ) );
	for ( i = 0; i < GH_NumCommands( gh ) && i < 8; i++ ) {
		int cn = 0;
		const char *c = GH_Command( gh, i, &cn );

		printf( "  [%d] %.100s\n", cn, c );
	}
	printf( "unhandled syscalls:%s\n", GH_Unhandled( gh ) );
	printf( "first demo ps: origin (%g %g %g)\n", d.snaps[0].ps.origin[0], d.snaps[0].ps.origin[1], d.snaps[0].ps.origin[2] );

	GH_Destroy( gh );
	free( bsp );
	Demo_Free( &d );
	return 0;
}
