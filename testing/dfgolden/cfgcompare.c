/*
 * cfgcompare.c - the server settings the game published against the demo's
 *
 * The game derives some published values from other settings (game mode, flags, overbounce ...). A
 * difference to what the recorded server published means the game was not given the right input.
 */
#include <stdio.h>
#include <string.h>

#include "demo.h"
#include "../host/gamehost.h"

void CompareConfig( gamehost_t *gh, const demo_t *d ) {
	static const char *skip[] = { "sv_serverid", "sv_referencedPaks", "sv_referencedPakNames", "sv_pakNames", "sv_paks",
		"sv_hostname", "version", "com_protocol", "protocol", "sv_voip", "mapname", "sv_maxPing", "sv_minPing",
		"sv_floodProtect", "sv_allowDownload", "sv_maxRate", "sv_minRate", "sv_dlRate", "sv_privateClients",
		"com_gamename", "gamename", "o3j_version", "fs_game", "sv_cheats", "sv_pure", "timescale", NULL };
	int pass, n = 0;

	for ( pass = 0; pass < 2; pass++ ) {
		const char *info = pass ? d->systeminfo : d->serverinfo;
		const char *ours = GH_Configstring( gh, pass ? CS_SYSTEMINFO : CS_SERVERINFO );

		while ( *info == '\\' ) {
			char key[BIG_INFO_STRING], val[BIG_INFO_STRING], mine[BIG_INFO_STRING];
			int k = 0, s;

			info++;
			while ( *info && *info != '\\' && k < (int)sizeof( key ) - 1 ) {
				key[k++] = *info++;
			}
			key[k] = '\0';
			if ( *info != '\\' ) {
				break;
			}
			info++;
			k = 0;
			while ( *info && *info != '\\' && k < (int)sizeof( val ) - 1 ) {
				val[k++] = *info++;
			}
			val[k] = '\0';
			for ( s = 0; skip[s]; s++ ) {
				if ( !Q_stricmp( key, skip[s] ) ) {
					break;
				}
			}
			if ( skip[s] ) {
				continue;
			}
			Q_strncpyz( mine, Info_ValueForKey( ours, key ), sizeof( mine ) );
			if ( strcmp( mine, val ) && n++ < 12 ) {
				printf( "  config: %s %s = demo '%.40s', game '%.40s'\n", pass ? "systeminfo" : "serverinfo", key, val, mine );
			}
		}
	}
}
