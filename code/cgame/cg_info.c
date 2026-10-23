/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_info.c -- display information while data is being loading

#include "cg_local.h"

// DeFRaG loading screen (cgame 0x34f5 draw, 0x3425 / 0x3491 message ring, 0x3909 setup, 0x34a8 / 0x34bf item and client lines): a black screen with
// the level shot in a frame, the last 25 loading messages on the right, the server description lines below and a progress bar at the bottom.

#define LOAD_RING_LINES		25
#define LOAD_RING_CHARS		128

static char		loadRing[LOAD_RING_LINES][LOAD_RING_CHARS];		// 0x11965c-ish ring at 1149400
static int		loadRingPos;		// 1152600: next slot
static int		loadTotal;			// 1152604: expected number of messages
static int		loadCount;			// 1152608: messages so far

static const vec4_t	loadBlack = { 0, 0, 0, 1 };			// 0x4084
static const vec4_t	loadWhite = { 1, 1, 1, 1 };			// 0x40f4
static const vec4_t	loadBarFill = { 0.4f, 0.4f, 0.7f, 0.7f };	// 0x1354
static const vec4_t	loadBarBack = { 0.4f, 0.4f, 0.7f, 0.7f };	// 0x1364
static const vec4_t	loadFrame = { 0.4f, 0.4f, 1.0f, 0.7f };		// 0x1374

// data table 0x1260: defrag_gametype -> name
static const char *loadGametypes[24] = {
	"Auto-config mode", "Defrag", "Tricks Mode", "Fast Caps", "Reserved", "MP Defrag", "MP Tricks Mode", "MP Fast Caps",
	"ac", "df", "tm", "fc", "res", "mdf", "mtm", "mfc", "AC", "DF", "TM", "FC", "RES", "DF", "TM", "FC"
};

// 0x3425: a line goes into the ring (127 characters)
void CG_DF_LoadingLine( const char *s ) {
	Q_strncpyz( loadRing[loadRingPos], s, LOAD_RING_CHARS );
	loadRingPos = ( loadRingPos + 1 ) % LOAD_RING_LINES;
}

// 0x3909 (from 0x3985): the ring is cleared and the number of messages to expect is worked out: the 6 fixed ones, one per client and one per item
void CG_DF_LoadingInit( void ) {
	const char	*items;
	int			i;

	memset( loadRing, 0, sizeof( loadRing ) );
	loadRingPos = loadTotal = loadCount = 0;
	CG_DF_LoadingLine( va( "Loading map %s", Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ) ) );
	loadTotal = 6;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( CG_ConfigString( CS_PLAYERS + i )[0] ) {
			loadTotal++;
		}
	}
	items = CG_ConfigString( CS_ITEMS );
	for ( i = 1; i < bg_numItems; i++ ) {
		if ( items[i] == '1' || cg_buildScript.integer ) {
			loadTotal++;
		}
	}
}

/*
======================
CG_LoadingString

0x3491: add a line, count it and show it
======================
*/
void CG_LoadingString( const char *s ) {
	Q_strncpyz( cg.infoScreenText, s, sizeof( cg.infoScreenText ) );
	if ( s[0] ) {
		CG_DF_LoadingLine( s );
		loadCount++;
	}

	trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem( int itemNum ) {		// 0x34a8
	CG_LoadingString( va( "Item : %s", bg_itemlist[itemNum].pickup_name ) );
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient( int clientNum ) {		// 0x34bf
	char	name[256];

	Q_strncpyz( name, Info_ValueForKey( CG_ConfigString( CS_PLAYERS + clientNum ), "n" ), sizeof( name ) );
	Q_CleanStr( name );
	CG_LoadingString( va( "Client : %s", name ) );
}

// 0x53dd: text centred on x
static void CG_DF_LoadingCentered( const char *s, int x, int y ) {
	CG_DrawStringExt( x - ( (int)strlen( s ) * 8 ) / 2, y, s, loadWhite, qfalse, qtrue, 8, 12, 0 );
}

// one "label : value" row of the server description (x 59, 16 units apart)
static void CG_DF_LoadingRow( int *y, const char *fmt, const char *label, const char *value ) {
	CG_DrawStringExt( 59, *y, va( fmt, label, value ), loadWhite, qfalse, qtrue, 8, 12, 0 );
	*y += 16;
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
====================
*/
void CG_DrawInformation( void ) {
	const char	*info, *sysInfo, *s;
	char		buf[256], map[MAX_QPATH], fsGame[MAX_QPATH];
	qhandle_t	levelshot, shader;
	int			y, i, version = 0, gametype, flags;
	qboolean	defrag = qfalse;
	float		frac, x, yy, w, h;

	info = CG_ConfigString( CS_SERVERINFO );
	sysInfo = CG_ConfigString( CS_SYSTEMINFO );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof( map ) );
	Q_strncpyz( fsGame, Info_ValueForKey( sysInfo, "fs_game" ), sizeof( fsGame ) );

	// black background, framed level shot (a missing one is replaced by the unknown map picture)
	trap_R_SetColor( loadBlack );
	CG_DrawPic( 0, 0, 640, 480, cgs.media.whiteShader );
	trap_R_SetColor( loadFrame );
	CG_DrawPic( 58, 49, 254, 191, cgs.media.whiteShader );
	levelshot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", map ) );
	if ( !levelshot ) {
		levelshot = trap_R_RegisterShaderNoMip( "menu/art/unknownmap" );
	}
	trap_R_SetColor( NULL );
	CG_DrawPic( 59, 50, 252, 189, levelshot );

	// the messages so far, oldest first
	y = 50;
	for ( i = 0; i < LOAD_RING_LINES; i++ ) {
		const char *line = loadRing[( loadRingPos + i ) % LOAD_RING_LINES];

		if ( line[0] ) {
			CG_DrawStringExt( 370, y, line, loadWhite, qtrue, qtrue, 8, 12, 0 );
			y += 14;
		}
	}

	// map name and long map name under the picture
	y = 50 + 194;
	CG_DF_LoadingCentered( map, 59 + 128, y );
	s = CG_ConfigString( CS_MESSAGE );
	if ( s[0] ) {
		CG_DF_LoadingCentered( s, 59 + 128, y + 14 );
	}
	y += 60;

	// the server (the name comes from the client cvar defrag_servername, as in the original)
	trap_Cvar_VariableStringBuffer( "defrag_servername", buf, sizeof( buf ) );
	CG_DF_LoadingCentered( va( cg.demoPlayback ? "Playing demo %s" : "Connected to %s", buf ), 320, 10 );

	// DeFRaG server? (0x3c33: defrag_vers, else legacy defrag_version, else a game directory called defrag*)
	s = Info_ValueForKey( info, "defrag_vers" );
	if ( s[0] ) {
		defrag = qtrue;
		version = atoi( s );
		if ( version < 1000 ) {
			version *= 100;
		}
	} else {
		s = Info_ValueForKey( info, "defrag_version" );
		if ( s[0] ) {
			defrag = qtrue;
			version = 100 * atoi( s );
			if ( version != 16100 ) {
				version = 0;
			}
		} else if ( !Q_stricmpn( fsGame, "defrag", 6 ) ) {
			defrag = qtrue;
		}
	}

	if ( defrag ) {
		gametype = atoi( Info_ValueForKey( info, "defrag_gametype" ) );
		flags = atoi( Info_ValueForKey( info, "defrag_flags" ) );
		CG_DF_LoadingRow( &y, "%-12s : %.20s", "gametype", gametype >= 0 && gametype < 24 ? loadGametypes[gametype] : "" );
		CG_DF_LoadingRow( &y, "%-12s : %.20s", "physics", atoi( Info_ValueForKey( info, "df_promode" ) ) ? "cpm" : "vq3" );
		if ( version >= 18100 ) {		// defrag_mode is only reported by 1.81 and later
			CG_DF_LoadingRow( &y, "%-12s : %s", "mode", va( "%i", atoi( Info_ValueForKey( info, "defrag_mode" ) ) ) );
		}
		CG_DF_LoadingRow( &y, "%-12s : %.20s", "version", va( "%s%s", va( "%i.%02i.%02i", version / 10000, ( version / 100 ) % 100, version % 100 ),
			( flags & 8 ) ? " (beta)" : "" ) );
		s = Info_ValueForKey( info, "df_competition" );
		if ( s[0] ) {
			CG_DF_LoadingRow( &y, "%-12s : %.20s", "competition", s );
		}
		CG_DF_LoadingRow( &y, "%-12s : %s", "cheats", atoi( Info_ValueForKey( sysInfo, "sv_cheats" ) ) ? "On" : "Off" );
	} else {
		CG_DF_LoadingRow( &y, "%-12s : %.20s", "gametype", va( "%i", cgs.gametype ) );
		CG_DF_LoadingRow( &y, "%-12s : %.20s", "gamename", Info_ValueForKey( info, "gamename" ) );
		if ( fsGame[0] ) {
			CG_DF_LoadingRow( &y, "%-12s : %.20s", "fs_game", fsGame );
		}
		CG_DF_LoadingRow( &y, "%-12s : %s", "cheats", atoi( Info_ValueForKey( sysInfo, "sv_cheats" ) ) ? "On" : "Off" );
		CG_DF_LoadingRow( &y, "%-12s : %s", "pure server", atoi( Info_ValueForKey( sysInfo, "sv_pure" ) ) ? "On" : "Off" );
	}

	// progress bar: background, then the part that is done
	frac = (float)loadCount / (float)loadTotal;
	x = 10; yy = 445; w = 620; h = 25;
	CG_AdjustFrom640( &x, &yy, &w, &h );
	trap_R_SetColor( loadBarBack );
	shader = trap_R_RegisterShaderNoMip( "df_gfx/ldscr/progressbar_bg.tga" );
	trap_R_DrawStretchPic( x, yy, w, h, 0, 0, 1, 1, shader );
	x = 10; yy = 445; w = 620 * frac; h = 25;
	CG_AdjustFrom640( &x, &yy, &w, &h );
	trap_R_SetColor( loadBarFill );
	shader = trap_R_RegisterShaderNoMip( "df_gfx/ldscr/progressbar.tga" );
	trap_R_DrawStretchPic( x, yy, w, h, 0, 0, frac, 1, shader );
	trap_R_SetColor( NULL );
}
