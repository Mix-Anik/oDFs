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
/*
=======================================================================

DEMOS MENU

=======================================================================
*/


#include "ui_local.h"

/*
DeFRaG demo browser (ui.qvm 0xe999..0xf46c, decoded in iteration 134).

Differences to the stock q3_ui demo menu:
 - the list is directory aware: sub folders of demos/ are listed first as "/name" (and "/.." inside a folder);
   activating one enters it.  The current folder is kept in df_demos_CurrentPath and the list scroll/cursor in
   df_demos_CurrentListVars ("top cursor") so that the menu comes back where it was after a demo ended
 - layout: the list is 65x21 at (10,75), a read-only FOLDER: field, vertical scroll arrows (PgUp/PgDn) and four
   option spinners (Replay / Mod Support / Validation / Time Demo) bound to df_demos_ReplayMode,
   df_demos_ModSupport, df_demos_validation and timedemo
 - double click on a list entry, Enter / Right arrow, middle mouse and Backspace / Left arrow (up one folder)
 - starting a demo also hands "#1 demo <name>" to the cgame ghost system through df_ghosts_Command
 - the demo name is lower-cased
 (The DeFRaG flag word at ui 0x12390, bit 0 = keep case, bit 1 = flat list, is never written in the qvm: both bits are 0.)
*/

#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"
#define ART_GO0				"menu/art/play_0"
#define ART_GO1				"menu/art/play_1"
#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define ART_ARROWS			"menu/art/arrows_vert_0"
#define ART_ARROWTOP		"menu/art/arrows_vert_top"
#define ART_ARROWBOT		"menu/art/arrows_vert_bot"

#define MAX_DEMOS			1024
#define MAX_DEMODIRS		64
#define DIRBUFSIZE			4096
#define FILEBUFSIZE			131072
#define NAMEBUFSIZE			( DIRBUFSIZE + FILEBUFSIZE )
#define DEMOPATH_SIZE		64
#define DEMONAME_SIZE		128
#define DOUBLECLICK_MSEC	600

#define ID_BACK				10
#define ID_GO				11
#define ID_LIST				12
#define ID_UP				13
#define ID_DOWN				14

typedef struct {
	int		listId;			// the id a double click is valid for (ID_LIST)
	int		lastId;			// id of the item that sent the last menu event
	int		lastIndex;
	int		lastTime;
} demoClick_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	arrows;
	menubitmap_s	up;
	menubitmap_s	down;
	menubitmap_s	back;
	menubitmap_s	go;

	menulist_s		list;
	menufield_s		folder;

	menulist_s		replay;
	menulist_s		modsupport;
	menulist_s		validation;
	menulist_s		timedemo;

	demoClick_t		click;

	int				numDirs;
	char			path[DEMOPATH_SIZE];
	char			names[NAMEBUFSIZE];
	char			*demolist[MAX_DEMOS];
} demos_t;

static demos_t	s_demos;

static const char *replay_names[] = { "Off", "On", "D1-Output", "D2-Objects", "D3-No Act", 0 };
static const char *modsupport_names[] = { "Off", "General 1", "General 2", "OSP", 0 };
static const char *validation_names[] = { "Off", "On", "On (verbose)", 0 };
static const char *onoff_names[] = { "Off", "On", 0 };


// 0x4188 / 0x4173 / 0x4197: double click tracker
static void Demos_ClickReset( void ) {
	s_demos.click.lastIndex = -1;
	s_demos.click.lastTime = -1;
}

static qboolean Demos_DoubleClick( int index ) {
	int now = trap_Milliseconds();

	if ( now - s_demos.click.lastTime <= DOUBLECLICK_MSEC && s_demos.click.lastIndex == index
		&& s_demos.click.lastId == s_demos.click.listId && s_demos.click.listId != 0 ) {
		Demos_ClickReset();
		return qtrue;
	}
	s_demos.click.lastIndex = index;
	s_demos.click.lastTime = now;
	return qfalse;
}

// 0xe999: case insensitive compare
static int Demos_Compare( const char *a, const char *b ) {
	while ( ( *a == *b || ( *a ^ 32 ) == *b ) && *a ) {
		a++;
		b++;
	}
	return tolower( *(const unsigned char *)a ) - tolower( *(const unsigned char *)b );
}

// 0xe9e0: folders ("/name") first, then alphabetical
static void Demos_Sort( void ) {
	int		pass, i;
	char	*t;
	char	**l = s_demos.demolist;

	for ( pass = 0; pass < s_demos.list.numitems; pass++ ) {
		for ( i = 0; i < s_demos.list.numitems - 1; i++ ) {
			if ( l[i][0] == '/' && l[i + 1][0] != '/' ) {
				continue;
			}
			if ( Demos_Compare( l[i], l[i + 1] ) > 0 ) {
				t = l[i];
				l[i] = l[i + 1];
				l[i + 1] = t;
			}
		}
	}
}

// 0xe972: the four spinners are stored when the menu is left or a demo is started
static void Demos_SaveSettings( void ) {
	trap_Cvar_SetValue( "df_demos_ReplayMode", s_demos.replay.curvalue );
	trap_Cvar_SetValue( "df_demos_ModSupport", s_demos.modsupport.curvalue );
	trap_Cvar_SetValue( "df_demos_validation", s_demos.validation.curvalue );
	trap_Cvar_SetValue( "timedemo", s_demos.timedemo.curvalue );
}

// returns the offset behind the last of n consecutive strings
static int Demos_SkipStrings( const char *p, int n ) {
	const char *start = p;

	while ( n-- > 0 ) {
		p += strlen( p ) + 1;
	}
	return p - start;
}

/*
===============
Demos_BuildList (0xeb9f)

Folders come from FS_GetFileList( "demos/<path>", "/" ), demos from "dm_*" (any protocol) or "dm_<protocol>" below
protocol 68.  The engine here lists exact extensions only, so when "dm_*" finds nothing the stock ioq3 lookup
(com_protocol, then com_legacyprotocol) is used instead.
===============
*/
static void Demos_BuildList( void ) {
	char	dirs[DIRBUFSIZE], dirpath[DEMOPATH_SIZE], ext[32], vars[32];
	const char	*p, *vp;
	char	*d;
	int		i, len, found, off, numFiles, protocol, legacy;

	memset( dirs, 0, sizeof( dirs ) );
	memset( s_demos.names, 0, sizeof( s_demos.names ) );
	s_demos.numDirs = 0;
	s_demos.list.numitems = 0;
	s_demos.list.curvalue = 0;
	s_demos.list.top = 0;

	Com_sprintf( dirpath, sizeof( dirpath ), "%s/%s", "demos", s_demos.path );
	found = trap_FS_GetFileList( dirpath, "/", dirs, sizeof( dirs ) );
	if ( found <= 0 ) {
		// the remembered folder is gone: back to the top
		s_demos.path[0] = 0;
		Com_sprintf( dirpath, sizeof( dirpath ), "%s/%s", "demos", s_demos.path );
		found = trap_FS_GetFileList( dirpath, "/", dirs, sizeof( dirs ) );
	}
	if ( found > MAX_DEMODIRS ) {
		found = MAX_DEMODIRS;
	}

	off = 0;
	if ( s_demos.path[0] ) {
		Q_strncpyz( s_demos.names, "/..", 4 );
		off = 4;
		s_demos.numDirs++;
	}
	d = dirs;
	for ( i = 0; i < found; i++, d += len + 1 ) {
		len = strlen( d );
		if ( !len || !strcmp( d, "." ) || !strcmp( d, ".." ) ) {
			continue;
		}
		Q_strncpyz( s_demos.names + off, "/", 2 );
		off += 1;
		Q_strlwr( d );
		Q_strncpyz( s_demos.names + off, d, len + 1 );
		off += len + 1;
		s_demos.numDirs++;
	}

	protocol = (int)trap_Cvar_VariableValue( "protocol" );
	if ( protocol >= 68 ) {
		Com_sprintf( ext, sizeof( ext ), "dm_*" );
	} else {
		Com_sprintf( ext, sizeof( ext ), "dm_%d", protocol );
	}
	numFiles = trap_FS_GetFileList( dirpath, ext, s_demos.names + off, FILEBUFSIZE );
	if ( numFiles <= 0 && protocol >= 68 ) {
		protocol = (int)trap_Cvar_VariableValue( "com_protocol" );
		legacy = (int)trap_Cvar_VariableValue( "com_legacyprotocol" );
		numFiles = 0;
		if ( protocol > 0 ) {
			Com_sprintf( ext, sizeof( ext ), "dm_%d", protocol );
			numFiles = trap_FS_GetFileList( dirpath, ext, s_demos.names + off, FILEBUFSIZE );
		}
		if ( legacy > 0 && legacy != protocol && numFiles >= 0 ) {
			int used = Demos_SkipStrings( s_demos.names + off, numFiles );
			Com_sprintf( ext, sizeof( ext ), "dm_%d", legacy );
			numFiles += trap_FS_GetFileList( dirpath, ext, s_demos.names + off + used, FILEBUFSIZE - used );
		}
	}
	if ( numFiles < 0 ) {
		numFiles = 0;
	}
	if ( numFiles > MAX_DEMOS - s_demos.numDirs ) {
		numFiles = MAX_DEMOS - s_demos.numDirs;
	}
	s_demos.list.numitems = numFiles + s_demos.numDirs;

	p = s_demos.names;
	for ( i = 0; i < s_demos.numDirs; i++ ) {
		s_demos.demolist[i] = (char *)p;
		p += strlen( p ) + 1;
	}
	d = s_demos.names + off;
	for ( ; i < s_demos.list.numitems; i++ ) {
		s_demos.demolist[i] = d;
		len = strlen( d );
		if ( len >= 4 && !strcmp( d + len - 4, ".dm3" ) ) {
			d[len - 4] = 0;
		}
		Q_strlwr( d );
		d += len + 1;
	}

	Demos_Sort();
	Demos_ClickReset();

	Com_sprintf( s_demos.folder.field.buffer, sizeof( s_demos.folder.field.buffer ), "%s", s_demos.path[0] ? s_demos.path : "/" );

	// back to the scroll position / selection the menu was left with
	trap_Cvar_VariableStringBuffer( "df_demos_CurrentListVars", vars, sizeof( vars ) );
	vp = vars;
	s_demos.list.top = Com_Clamp( 0, s_demos.list.numitems - 1, atoi( COM_Parse( &vp ) ) );
	s_demos.list.curvalue = Com_Clamp( 0, s_demos.list.numitems - 1, atoi( COM_Parse( &vp ) ) );
}

/*
===============
Demos_EnterFolder (0xea70): the selected entry is a folder ("/name" or "/..")
===============
*/
static void Demos_EnterFolder( void ) {
	char	sel[DEMOPATH_SIZE], cur[DEMOPATH_SIZE], up[DEMOPATH_SIZE];
	int		i, n, slashes, seen;

	Q_strncpyz( sel, s_demos.demolist[s_demos.list.curvalue], sizeof( sel ) );
	Q_strncpyz( cur, s_demos.path, sizeof( cur ) );

	if ( !strcmp( sel, "/.." ) ) {
		// drop the last path component
		memset( up, 0, sizeof( up ) );
		slashes = 0;
		for ( i = 0; cur[i]; i++ ) {
			if ( cur[i] == '/' ) {
				slashes++;
			}
		}
		n = 0;
		seen = 0;
		for ( i = 0; i < (int)strlen( cur ); i++ ) {
			if ( seen == slashes ) {
				break;
			}
			if ( cur[i] == '/' ) {
				seen++;
				if ( seen < slashes ) {
					up[n++] = cur[i];
				}
			} else {
				up[n++] = cur[i];
			}
		}
		Com_sprintf( s_demos.path, sizeof( s_demos.path ), "%s", up );
	} else if ( strlen( cur ) + strlen( sel ) + 1 < sizeof( s_demos.path ) ) {
		Com_sprintf( s_demos.path, sizeof( s_demos.path ), "%s%s", cur, sel );
	}

	s_demos.list.top = 0;
	s_demos.list.curvalue = 0;
	trap_Cvar_Set( "df_demos_CurrentPath", s_demos.path );
	trap_Cvar_Set( "df_demos_CurrentListVars", va( "%i %i", s_demos.list.top, s_demos.list.curvalue ) );
}

/*
===============
Demos_Play (0xee6e)
===============
*/
static void Demos_Play( void ) {
	char	name[DEMONAME_SIZE];
	const char	*item = s_demos.demolist[s_demos.list.curvalue];
	int		len = strlen( item );

	if ( len >= DEMONAME_SIZE ) {
		Com_Printf( "^1ERROR:^7 Demo filename too long, maximum length = %i, filename length = %i.", DEMONAME_SIZE, len );
		return;
	}
	Q_strncpyz( name, item, sizeof( name ) );
	len = strlen( name );
	if ( len > 6 ) {
		name[len - 6] = 0;		// ".dm_NN"
	}
	trap_Cvar_Set( "df_ghosts_Command", va( "#1 demo %s", name ) );

	if ( s_demos.path[0] ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s/%s\"\n", s_demos.path, name ) );
	} else {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s\"\n", name ) );
	}
}

/*
===============
Demos_Activate (0xef16): enter the folder or play the demo
===============
*/
static void Demos_Activate( void ) {
	if ( s_demos.list.numitems <= 0 ) {
		return;
	}
	trap_Cvar_Set( "df_demos_CurrentListVars", va( "%i %i", s_demos.list.top, s_demos.list.curvalue ) );
	if ( s_demos.list.curvalue < s_demos.numDirs ) {
		Demos_EnterFolder();
		Demos_BuildList();
	} else {
		UI_ForceMenuOff();
		Demos_SaveSettings();
		Demos_Play();
	}
}

/*
===============
Demos_MenuEvent (0xef43)
===============
*/
static void Demos_MenuEvent( void *ptr, int event ) {
	s_demos.click.lastId = ((menucommon_s*)ptr)->id;
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GO:
		Demos_Activate();
		break;

	case ID_BACK:
		UI_PopMenu();
		break;

	case ID_UP:
		ScrollList_Key( &s_demos.list, K_PGUP );
		break;

	case ID_DOWN:
		ScrollList_Key( &s_demos.list, K_PGDN );
		break;
	}
}

/*
===============
Demos_MenuKey (0xef84)
===============
*/
static sfxHandle_t Demos_MenuKey( int key ) {
	sfxHandle_t	sound = Menu_DefaultKey( &s_demos.menu, key );

	if ( key == K_ESCAPE || key == K_MOUSE2 ) {
		Demos_SaveSettings();
	}
	if ( key == K_MOUSE1 && s_demos.click.lastId == ID_LIST ) {
		if ( Demos_DoubleClick( s_demos.list.curvalue ) ) {
			Demos_Activate();
		}
	}
	if ( key == K_MWHEELUP ) {
		ScrollList_Key( &s_demos.list, K_UPARROW );
	}
	if ( key == K_MWHEELDOWN ) {
		ScrollList_Key( &s_demos.list, K_DOWNARROW );
	}
	if ( key == K_MOUSE3 && s_demos.path[0] ) {
		s_demos.list.curvalue = 0;
		Demos_Activate();
	}
	if ( ( key == K_LEFTARROW || key == K_BACKSPACE ) && s_demos.path[0] ) {
		s_demos.list.curvalue = 0;
		Demos_Activate();
	}
	if ( key == K_RIGHTARROW || key == K_ENTER || key == K_KP_ENTER ) {
		Demos_Activate();
	}
	return sound;
}

static void Demos_InitSpin( menulist_s *s, const char *name, int x, int y, const char **items, int numitems, const char *cvar ) {
	s->generic.type		= MTYPE_SPINCONTROL;
	s->generic.name		= name;
	s->generic.flags	= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.callback	= Demos_MenuEvent;
	s->generic.x		= x;
	s->generic.y		= y;
	s->itemnames		= items;
	s->numitems			= numitems;
	s->curvalue			= Com_Clamp( 0, numitems - 1, trap_Cvar_VariableValue( cvar ) );
}

/*
===============
Demos_MenuInit (0xf023)
===============
*/
static void Demos_MenuInit( void ) {
	memset( &s_demos, 0 ,sizeof(demos_t) );

	Demos_Cache();

	s_demos.menu.key = Demos_MenuKey;
	s_demos.menu.wrapAround = qtrue;
	s_demos.menu.fullscreen = qtrue;

	s_demos.banner.generic.type		= MTYPE_BTEXT;
	s_demos.banner.generic.x		= 320;
	s_demos.banner.generic.y		= 16;
	s_demos.banner.string			= "DEMOS";
	s_demos.banner.color			= color_white;
	s_demos.banner.style			= UI_CENTER;

	s_demos.arrows.generic.type		= MTYPE_BITMAP;
	s_demos.arrows.generic.name		= ART_ARROWS;
	s_demos.arrows.generic.flags	= QMF_INACTIVE;
	s_demos.arrows.generic.x		= 568;
	s_demos.arrows.generic.y		= 180;
	s_demos.arrows.width			= 64;
	s_demos.arrows.height			= 128;

	s_demos.up.generic.type			= MTYPE_BITMAP;
	s_demos.up.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	s_demos.up.generic.x			= 568;
	s_demos.up.generic.y			= 180;
	s_demos.up.generic.id			= ID_UP;
	s_demos.up.generic.callback		= Demos_MenuEvent;
	s_demos.up.width				= 64;
	s_demos.up.height				= 64;
	s_demos.up.focuspic				= ART_ARROWTOP;

	s_demos.down.generic.type		= MTYPE_BITMAP;
	s_demos.down.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	s_demos.down.generic.x			= 568;
	s_demos.down.generic.y			= 244;
	s_demos.down.generic.id			= ID_DOWN;
	s_demos.down.generic.callback	= Demos_MenuEvent;
	s_demos.down.width				= 64;
	s_demos.down.height				= 64;
	s_demos.down.focuspic			= ART_ARROWBOT;

	s_demos.back.generic.type		= MTYPE_BITMAP;
	s_demos.back.generic.name		= ART_BACK0;
	s_demos.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.back.generic.id			= ID_BACK;
	s_demos.back.generic.callback	= Demos_MenuEvent;
	s_demos.back.generic.x			= 0;
	s_demos.back.generic.y			= 480-64;
	s_demos.back.width				= 128;
	s_demos.back.height				= 64;
	s_demos.back.focuspic			= ART_BACK1;

	s_demos.go.generic.type			= MTYPE_BITMAP;
	s_demos.go.generic.name			= ART_GO0;
	s_demos.go.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.go.generic.id			= ID_GO;
	s_demos.go.generic.callback		= Demos_MenuEvent;
	s_demos.go.generic.x			= 640;
	s_demos.go.generic.y			= 480-64;
	s_demos.go.width				= 128;
	s_demos.go.height				= 64;
	s_demos.go.focuspic				= ART_GO1;

	s_demos.list.generic.type		= MTYPE_SCROLLLIST;
	s_demos.list.generic.flags		= QMF_PULSEIFFOCUS;
	s_demos.list.generic.callback	= Demos_MenuEvent;
	s_demos.list.generic.id			= ID_LIST;
	s_demos.list.generic.x			= 10;
	s_demos.list.generic.y			= 75;
	s_demos.list.width				= 65;
	s_demos.list.height				= 21;
	s_demos.list.columns			= 1;
	s_demos.list.itemnames			= (const char **)s_demos.demolist;

	s_demos.folder.generic.type		= MTYPE_FIELD;
	s_demos.folder.generic.name		= "FOLDER:";
	s_demos.folder.generic.flags	= QMF_SMALLFONT|QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	s_demos.folder.generic.callback	= Demos_MenuEvent;
	s_demos.folder.generic.x		= 75;
	s_demos.folder.generic.y		= 50;
	s_demos.folder.field.widthInChars = 64;
	s_demos.folder.field.maxchars	= 64;

	Demos_InitSpin( &s_demos.replay, "Replay:", 230, 425, replay_names, 5, "df_demos_ReplayMode" );
	Demos_InitSpin( &s_demos.modsupport, "Mod Support:", 230, 445, modsupport_names, 4, "df_demos_ModSupport" );
	Demos_InitSpin( &s_demos.validation, "Validation:", 420, 425, validation_names, 3, "df_demos_validation" );
	Demos_InitSpin( &s_demos.timedemo, "Time Demo:", 420, 445, onoff_names, 2, "timedemo" );

	s_demos.click.listId = ID_LIST;
	s_demos.click.lastId = ID_LIST;
	Demos_ClickReset();

	trap_Cvar_VariableStringBuffer( "df_demos_CurrentPath", s_demos.path, sizeof( s_demos.path ) );
	Demos_BuildList();

	if ( !s_demos.list.numitems ) {
		// nothing at all (the stock "No Demos Found." entry; DeFRaG sets it before its list rebuild wipes it again)
		Q_strncpyz( s_demos.names, "No Demos Found.", 16 );
		s_demos.demolist[0] = s_demos.names;
		s_demos.list.numitems = 1;
		s_demos.list.curvalue = 0;
		s_demos.go.generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
	}

	Menu_AddItem( &s_demos.menu, &s_demos.banner );
	Menu_AddItem( &s_demos.menu, &s_demos.list );
	Menu_AddItem( &s_demos.menu, &s_demos.arrows );
	Menu_AddItem( &s_demos.menu, &s_demos.up );
	Menu_AddItem( &s_demos.menu, &s_demos.down );
	Menu_AddItem( &s_demos.menu, &s_demos.back );
	Menu_AddItem( &s_demos.menu, &s_demos.go );
	Menu_AddItem( &s_demos.menu, &s_demos.folder );
	Menu_AddItem( &s_demos.menu, &s_demos.replay );
	Menu_AddItem( &s_demos.menu, &s_demos.modsupport );
	Menu_AddItem( &s_demos.menu, &s_demos.validation );
	Menu_AddItem( &s_demos.menu, &s_demos.timedemo );
}

/*
=================
Demos_Cache (0xf2e2)
=================
*/
void Demos_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_GO0 );
	trap_R_RegisterShaderNoMip( ART_GO1 );
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_ARROWS );
	trap_R_RegisterShaderNoMip( ART_ARROWTOP );
	trap_R_RegisterShaderNoMip( ART_ARROWBOT );
}

/*
===============
UI_DemosMenu
===============
*/
void UI_DemosMenu( void ) {
	Demos_MenuInit();
	UI_PushMenu( &s_demos.menu );
}
