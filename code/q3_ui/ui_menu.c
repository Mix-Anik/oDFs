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

MAIN MENU

=======================================================================
*/


#include "ui_local.h"

/*
DeFRaG main menu (ui 0x1e6f build, 0x1d97 event, 0x1e2a draw): a plain left-aligned list of small proportional text
buttons (no 3D banner model, no Single Player / Cinematics / Team Arena), drawn in the light blue 0.53 0.77 1.00 1.00 and
turning white while focused (UI_FOCUSCOLOR).  "Exit" quits at once.  The right bottom corner shows the DeFRaG client version.

The four game type buttons (Defrag VQ3 / CPM / Tricks Mode / Fast Caps) open DeFRaG's start-map menu (ui 0x3825, ui_startmap.c).
*/

#define ID_DF_VQ3				1
#define ID_DF_CPM				2
#define ID_DF_TRICKS			3
#define ID_DF_FASTCAPS			5
#define ID_DF_SERVERS			7
#define ID_DF_DEMOS				10
#define ID_DF_SETUP				11
#define ID_DF_DEFRAGSETUP		12
#define ID_DF_CREDITS			13
#define ID_DF_MODS				14
#define ID_DF_EXIT				15

#define MAIN_MENU_X				170
#define MAIN_MENU_Y				100
#define MAIN_MENU_STEP			24

typedef struct {
	int			id;
	const char	*string;
	int			gap;		// extra pixels before the button
} mainButton_t;

static const mainButton_t mainButtons[] = {
	{ ID_DF_VQ3,		"Defrag VQ3",		0 },
	{ ID_DF_CPM,		"Defrag CPM",		0 },
	{ ID_DF_TRICKS,		"Tricks Mode",		0 },
	{ ID_DF_FASTCAPS,	"Fast Caps",		0 },
	{ ID_DF_SERVERS,	"Server Browser",	0 },
	{ ID_DF_DEMOS,		"Demos",			8 },
	{ ID_DF_SETUP,		"Setup",			0 },
	{ ID_DF_DEFRAGSETUP,"Defrag Setup",		0 },
	{ ID_DF_CREDITS,	"Credits",			0 },
	{ ID_DF_MODS,		"Mods",				0 },
	{ ID_DF_EXIT,		"Exit",				8 }
};

#define MAIN_BUTTONS	( sizeof( mainButtons ) / sizeof( mainButtons[0] ) )

typedef struct {
	menuframework_s	menu;
	menutext_s		button[MAIN_BUTTONS];
} mainmenu_t;

static mainmenu_t s_main;

typedef struct {
	menuframework_s menu;
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;

static vec4_t	mainButtonColor = { 0.53f, 0.77f, 1.00f, 1.00f };

/*
=================
Main_MenuEvent (0x1d97)
=================
*/
void Main_MenuEvent (void* ptr, int event) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_DF_VQ3:
		UI_DF_StartMapMenu( 1, 0 );
		break;

	case ID_DF_CPM:
		UI_DF_StartMapMenu( 1, 1 );
		break;

	case ID_DF_TRICKS:
		UI_DF_StartMapMenu( 2, 0 );
		break;

	case ID_DF_FASTCAPS:
		UI_DF_StartMapMenu( 3, 0 );
		break;

	case ID_DF_SERVERS:
		UI_ArenaServersMenu();
		break;

	case ID_DF_DEMOS:
		UI_DemosMenu();
		break;

	case ID_DF_SETUP:
		UI_SetupMenu();
		break;

	case ID_DF_DEFRAGSETUP:
		UI_DF_HudMenu();
		break;

	case ID_DF_CREDITS:
		UI_CreditMenu();
		break;

	case ID_DF_MODS:
		UI_ModsMenu();
		break;

	case ID_DF_EXIT:
		UI_PopMenu();
		trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
		break;
	}
}


/*
===============
MainMenu_Cache
===============
*/
void MainMenu_Cache( void ) {
}

sfxHandle_t ErrorMessage_Key(int key)
{
	trap_Cvar_Set( "com_errorMessage", "" );
	UI_MainMenu();
	return (menu_null_sound);
}

/*
===============
Main_DrawUpdate / Main_MenuKey

oDFs update line above the version, driven by the odfs_update_* cvars of the engine's mod updater (cl_update.c).  An engine
without it never sets odfs_update_state, so nothing is drawn.  "available" and "error" are clickable.
===============
*/
#define UPDATE_LINE_Y		440
#define UPDATE_BAR_WIDTH	120

static qboolean Main_UpdateLine( char *text, int size, vec4_t color, int *percent ) {
	char	state[32], value[64];

	*percent = -1;
	trap_Cvar_VariableStringBuffer( "odfs_update_state", state, sizeof( state ) );

	if ( !Q_stricmp( state, "checking" ) ) {
		Q_strncpyz( text, "checking for updates...", size );
		Vector4Copy( colorMdGrey, color );
	} else if ( !Q_stricmp( state, "uptodate" ) ) {
		Q_strncpyz( text, "up to date", size );
		Vector4Set( color, 0.35f, 0.65f, 0.35f, 1.0f );
	} else if ( !Q_stricmp( state, "available" ) ) {
		trap_Cvar_VariableStringBuffer( "odfs_update_latest", value, sizeof( value ) );
		Com_sprintf( text, size, "Update available: %s (click to install)", value );
		Vector4Copy( colorYellow, color );
		return qtrue;
	} else if ( !Q_stricmp( state, "downloading" ) ) {
		*percent = trap_Cvar_VariableValue( "odfs_update_progress" );
		Com_sprintf( text, size, "downloading update %i%%", *percent );
		Vector4Copy( colorYellow, color );
	} else if ( !Q_stricmp( state, "ready" ) ) {
		Q_strncpyz( text, "Update downloaded, restart the game to install it", size );
		Vector4Copy( colorGreen, color );
	} else if ( !Q_stricmp( state, "error" ) ) {
		trap_Cvar_VariableStringBuffer( "odfs_update_error", value, sizeof( value ) );
		Com_sprintf( text, size, "Update failed: %s (click to retry)", value );
		Vector4Copy( colorRed, color );
		return qtrue;
	} else {
		text[0] = '\0';
	}
	return qfalse;
}

static qboolean Main_UpdateHover( const char *text ) {
	int	w = (int)strlen( text ) * SMALLCHAR_WIDTH;

	return uis.cursorx >= 640 - w && uis.cursory >= UPDATE_LINE_Y && uis.cursory < UPDATE_LINE_Y + SMALLCHAR_HEIGHT;
}

static void Main_DrawUpdate( void ) {
	char		text[128];
	vec4_t		color;
	int			percent;
	qboolean	clickable;

	clickable = Main_UpdateLine( text, sizeof( text ), color, &percent );
	if ( !text[0] ) {
		return;
	}

	if ( clickable && Main_UpdateHover( text ) ) {
		Vector4Copy( colorWhite, color );
	}
	UI_DrawString( 640, UPDATE_LINE_Y, text, UI_RIGHT|UI_SMALLFONT, color );

	if ( percent >= 0 ) {
		vec4_t	back = { 0.2f, 0.2f, 0.2f, 0.8f };

		if ( percent > 100 ) {
			percent = 100;
		}
		UI_FillRect( 640 - UPDATE_BAR_WIDTH, UPDATE_LINE_Y + SMALLCHAR_HEIGHT, UPDATE_BAR_WIDTH, 3, back );
		UI_FillRect( 640 - UPDATE_BAR_WIDTH, UPDATE_LINE_Y + SMALLCHAR_HEIGHT, UPDATE_BAR_WIDTH * percent / 100, 3, color );
	}
}

static sfxHandle_t Main_MenuKey( int key ) {
	if ( key == K_MOUSE1 ) {
		char		text[128];
		vec4_t		color;
		int			percent;

		if ( Main_UpdateLine( text, sizeof( text ), color, &percent ) && Main_UpdateHover( text ) ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, "odfs_update\n" );
			return menu_in_sound;
		}
	}
	return Menu_DefaultKey( &s_main.menu, key );
}

/*
===============
Main_MenuDraw (0x1e2a)
this function is common to the main menu and errorMessage menu
===============
*/
static void Main_MenuDraw( void ) {
	if (strlen(s_errorMessage.errorMessage))
	{
		UI_DrawProportionalString_AutoWrapped( 320, 192, 600, 20, s_errorMessage.errorMessage, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );
	}
	else
	{
		// standard menu drawing, version text bottom right
		UI_DrawString( 640, 460, DF_VERSION_STRING DF_VERSION_TAG, UI_RIGHT|UI_SMALLFONT, uiDfColorBig );
		Main_DrawUpdate();
		Menu_Draw( &s_main.menu );
	}
}

/*
===============
UI_MainMenu (0x1e6f)

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu( void ) {
	int		i, y;

	trap_Cvar_Set( "sv_killserver", "1" );

	// oDFs: DeFRaG needs no CD key; the ioq3 engine rejects an empty key, which would open the CD key menu at every start
	if( 0 && !uis.demoversion && !ui_cdkeychecked.integer ) {
		char	key[17];

		trap_GetCDKey( key, sizeof(key) );
		if( trap_VerifyCDKey( key, NULL ) == qfalse ) {
			UI_CDKeyMenu();
			return;
		}
	}

	memset( &s_main, 0 ,sizeof(mainmenu_t) );
	memset( &s_errorMessage, 0 ,sizeof(errorMessage_t) );

	MainMenu_Cache();

	trap_Cvar_VariableStringBuffer( "com_errorMessage", s_errorMessage.errorMessage, sizeof(s_errorMessage.errorMessage) );
	if (strlen(s_errorMessage.errorMessage))
	{
		s_errorMessage.menu.draw = Main_MenuDraw;
		s_errorMessage.menu.key = ErrorMessage_Key;
		s_errorMessage.menu.fullscreen = qtrue;
		s_errorMessage.menu.wrapAround = qtrue;
		s_errorMessage.menu.showlogo = qtrue;

		trap_Key_SetCatcher( KEYCATCH_UI );
		uis.menusp = 0;
		UI_PushMenu ( &s_errorMessage.menu );

		return;
	}

	s_main.menu.draw = Main_MenuDraw;
	s_main.menu.key = Main_MenuKey;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;

	y = MAIN_MENU_Y;
	for ( i = 0; i < (int)MAIN_BUTTONS; i++ ) {
		y += mainButtons[i].gap;
		s_main.button[i].generic.type		= MTYPE_PTEXT;
		s_main.button[i].generic.flags		= QMF_LEFT_JUSTIFY;
		s_main.button[i].generic.x			= MAIN_MENU_X;
		s_main.button[i].generic.y			= y;
		s_main.button[i].generic.id			= mainButtons[i].id;
		s_main.button[i].generic.callback	= Main_MenuEvent;
		s_main.button[i].string				= (char *)mainButtons[i].string;
		s_main.button[i].color				= mainButtonColor;
		s_main.button[i].focusColor			= color_white;
		s_main.button[i].style				= UI_SMALLFONT|UI_DROPSHADOW|UI_FOCUSCOLOR;
		Menu_AddItem( &s_main.menu, &s_main.button[i] );
		y += MAIN_MENU_STEP;
	}

	// oDFs: look for a newer release once per run (the engine ignores this when odfs_update_check is 0)
	{
		char	state[8];

		trap_Cvar_VariableStringBuffer( "odfs_update_state", state, sizeof( state ) );
		if ( !state[0] ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "odfs_checkupdate %i auto\n", DF_VERSION_NUMBER ) );
		}
	}

	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );
}
