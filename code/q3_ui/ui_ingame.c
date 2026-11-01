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

INGAME MENU

=======================================================================
*/


#include "ui_local.h"

/*
oDFs: the menu of the escape key is a small see-through panel over the dimmed game: the map and what the player is doing,
then the rows.  The first row is what "Start" was: it changes the team of the player in one click (Join game / Spectate, with
Join red / Join blue in the team game types) instead of opening a menu of its own.  Settings opens the Settings menu,
Disconnect leaves the server.  Escape closes the menu.  The panel fades in when it opens.
*/

#define PANEL_W					320
#define PANEL_HEAD_H			70
#define PANEL_PAD				24		// from the panel edge to the text
#define ROW_MARGIN				8		// from the panel edge to the highlight of a row
#define ROW_H					32
#define ROW_STEP				36
#define LEAVE_GAP				10		// extra space above Disconnect
#define FADE_MS					180

#define ID_JOIN					10
#define ID_JOINRED				11
#define ID_JOINBLUE				12
#define ID_SPECTATE				13
#define ID_SETTINGS				14
#define ID_DISCONNECT			15

#define MAX_ROWS				6

typedef struct {
	menuframework_s	menu;

	menutext_s		rows[MAX_ROWS];
	int				numRows;
	int				panelY;
	int				panelH;
	int				openTime;
	float			fade;

	char			map[MAX_QPATH];
	int				team;
} ingamemenu_t;

static ingamemenu_t	s_ingame;

static vec4_t	igDim = { 0.0f, 0.0f, 0.0f, 0.35f };
static vec4_t	igLabel = { 0.92f, 0.86f, 0.84f, 1.0f };
static vec4_t	igLeave = { 1.0f, 0.55f, 0.50f, 1.0f };
static vec4_t	igWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	igRed = { 1.0f, 0.35f, 0.30f, 1.0f };
static vec4_t	igBlue = { 0.45f, 0.65f, 1.0f, 1.0f };
static vec4_t	igGreen = { 0.55f, 1.0f, 0.55f, 1.0f };


/*
=================
InGame_Fade

The colour with the alpha of the fade-in applied.
=================
*/
static float *InGame_Fade( const vec4_t c, vec4_t out ) {
	out[0] = c[0];
	out[1] = c[1];
	out[2] = c[2];
	out[3] = c[3] * s_ingame.fade;
	return out;
}


/*
=================
InGame_Event
=================
*/
static void InGame_Event( void *ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_JOIN:
		trap_Cmd_ExecuteText( EXEC_APPEND, "cmd team free\n" );
		UI_ForceMenuOff();
		break;

	case ID_JOINRED:
		trap_Cmd_ExecuteText( EXEC_APPEND, "cmd team red\n" );
		UI_ForceMenuOff();
		break;

	case ID_JOINBLUE:
		trap_Cmd_ExecuteText( EXEC_APPEND, "cmd team blue\n" );
		UI_ForceMenuOff();
		break;

	case ID_SPECTATE:
		trap_Cmd_ExecuteText( EXEC_APPEND, "cmd team spectator\n" );
		UI_ForceMenuOff();
		break;

	case ID_SETTINGS:
		UI_SettingsMenu();
		break;

	case ID_DISCONNECT:
		trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
		break;
	}
}


/*
=================
InGame_RowDraw
=================
*/
static void InGame_RowDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	qboolean	lit = ( t->generic.flags & QMF_HASMOUSEFOCUS ) || Menu_ItemAtCursor( t->generic.parent ) == self;
	qboolean	leave = t->generic.id == ID_DISCONNECT;
	int			x = t->generic.left, y = t->generic.top, w = t->generic.right - t->generic.left, h = t->generic.bottom - t->generic.top;
	vec4_t		c;

	UI_SoftRow( x, y, w, h, lit ? SOFT_HOVER|SOFT_BOTHBARS : 0, leave ? igLeave : NULL );
	UI_DrawProportionalString( 320, y + ( h - 20 ) / 2, t->string, UI_CENTER|UI_SMALLFONT,
		InGame_Fade( lit ? igWhite : leave ? igLeave : igLabel, c ) );
}


/*
=================
InGame_AddRow
=================
*/
static void InGame_AddRow( int id, const char *label, int y ) {
	menutext_s	*t = &s_ingame.rows[s_ingame.numRows++];

	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_CENTER_JUSTIFY;
	t->generic.x		= 320;
	t->generic.y		= y;
	t->generic.id		= id;
	t->generic.callback	= InGame_Event;
	t->generic.ownerdraw = InGame_RowDraw;
	t->string			= (char *)label;
	Menu_AddItem( &s_ingame.menu, t );
	// the widget worked out its mouse area from the text: use the row
	t->generic.left		= 320 - PANEL_W / 2 + ROW_MARGIN;
	t->generic.right	= 320 + PANEL_W / 2 - ROW_MARGIN;
	t->generic.top		= y;
	t->generic.bottom	= y + ROW_H;
}


/*
=================
InGame_MenuDraw
=================
*/
static void InGame_MenuDraw( void ) {
	int			x = 320 - PANEL_W / 2, y = s_ingame.panelY, sx;
	const char	*status;
	float		*color;
	vec4_t		c;

	s_ingame.fade = ( uis.realtime - s_ingame.openTime ) / (float)FADE_MS;
	if ( s_ingame.fade > 1.0f ) {
		s_ingame.fade = 1.0f;
	} else if ( s_ingame.fade < 0.0f ) {
		s_ingame.fade = 0.0f;
	}

	UI_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, InGame_Fade( igDim, c ) );
	uiSoftFade = s_ingame.fade;
	UI_SoftPanel( x, y, PANEL_W, s_ingame.panelH, 0, qtrue );

	if ( s_ingame.map[0] ) {
		UI_DrawProportionalString( 320, y + 14, s_ingame.map, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, InGame_Fade( igWhite, c ) );
	}
	switch ( s_ingame.team ) {
	case TEAM_RED:			status = "Red team";	color = igRed;		break;
	case TEAM_BLUE:			status = "Blue team";	color = igBlue;		break;
	case TEAM_SPECTATOR:	status = "Spectating";	color = uiSoftAccent;	break;
	default:				status = "Playing";		color = igGreen;	break;
	}
	// the dot and the status together in the middle
	sx = 320 - ( 14 + (int)strlen( status ) * SMALLCHAR_WIDTH ) / 2;
	UI_FillRect( sx, y + 44, 6, 6, InGame_Fade( color, c ) );
	UI_DrawString( sx + 14, y + 40, status, UI_LEFT|UI_SMALLFONT, InGame_Fade( color, c ) );
	UI_SoftRule( x + PANEL_PAD, y + PANEL_HEAD_H - 8, PANEL_W - 2 * PANEL_PAD );

	Menu_Draw( &s_ingame.menu );
	uiSoftFade = 1.0f;
}


/*
=================
InGame_MenuInit
=================
*/
static void InGame_MenuInit( void ) {
	int		y, i, n = 0, gametype;
	int		ids[MAX_ROWS];
	const char	*labels[MAX_ROWS];
	uiClientState_t	cs;
	char	info[MAX_INFO_STRING];

	memset( &s_ingame, 0 ,sizeof(ingamemenu_t) );

	s_ingame.menu.wrapAround = qtrue;
	s_ingame.menu.fullscreen = qfalse;
	s_ingame.menu.draw = InGame_MenuDraw;
	s_ingame.openTime = uis.realtime;

	// the map, and the team of the player
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	gametype = atoi( Info_ValueForKey( info, "g_gametype" ) );
	Q_strncpyz( s_ingame.map, Info_ValueForKey( info, "mapname" ), sizeof( s_ingame.map ) );

	trap_GetClientState( &cs );
	trap_GetConfigString( CS_PLAYERS + cs.clientNum, info, sizeof( info ) );
	s_ingame.team = atoi( Info_ValueForKey( info, "t" ) );

	// what the first rows offer, then the panel is as high as the rows
	if ( gametype >= GT_TEAM ) {
		if ( s_ingame.team != TEAM_RED ) {
			ids[n] = ID_JOINRED;	labels[n++] = "Join red";
		}
		if ( s_ingame.team != TEAM_BLUE ) {
			ids[n] = ID_JOINBLUE;	labels[n++] = "Join blue";
		}
		if ( s_ingame.team != TEAM_SPECTATOR ) {
			ids[n] = ID_SPECTATE;	labels[n++] = "Spectate";
		}
	} else if ( s_ingame.team == TEAM_SPECTATOR ) {
		ids[n] = ID_JOIN;		labels[n++] = "Join game";
	} else {
		ids[n] = ID_SPECTATE;	labels[n++] = "Spectate";
	}
	ids[n] = ID_SETTINGS;		labels[n++] = "Settings";

	// Disconnect below the rest
	s_ingame.panelH = PANEL_HEAD_H + n * ROW_STEP + LEAVE_GAP + ROW_H + PANEL_PAD;
	s_ingame.panelY = ( SCREEN_HEIGHT - s_ingame.panelH ) / 2;

	y = s_ingame.panelY + PANEL_HEAD_H;
	for ( i = 0; i < n; i++ ) {
		InGame_AddRow( ids[i], labels[i], y );
		y += ROW_STEP;
	}
	y += LEAVE_GAP;

	InGame_AddRow( ID_DISCONNECT, "Disconnect", y );
}


/*
=================
InGame_Cache
=================
*/
void InGame_Cache( void ) {
}


/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu( void ) {
	// force as top level menu
	uis.menusp = 0;

	InGame_MenuInit();

	// set menu cursor to a nice location: on the first row
	uis.cursorx = 320;
	uis.cursory = s_ingame.rows[0].generic.top + ROW_H / 2;

	UI_PushMenu( &s_ingame.menu );
}
