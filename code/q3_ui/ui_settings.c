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

SETTINGS MENU

=======================================================================
*/


#include "ui_local.h"

/*
oDFs: one Settings menu in place of Setup, Defrag Setup and Plugins.  There is no screen of its own: every settings page
(the Q3 ones, the DeFRaG ones, the plugin list) adds the same category rail down its left side with UI_Settings_AddRail,
and a click on another category closes the page and opens that one.  The page that was open last is opened first next time.

AddRail also dresses the page: a dark panel behind the rail, the heading of its group, the page title and a line under it
at the top of the content (menu.draw is wrapped, the page's own draw function still runs).  Pages lay their widgets out
from SETTINGS_FORM_X (the column the labels end at) and SETTINGS_FORM_Y (the first row).

A page that has to save something when it is left (the controls, the player name, the DeFRaG cvars) gives the rail a
"leave" function, which runs before the page closes, however it is left through the rail.
*/

#define ID_BACK					101

#define RAIL_X					12		// headings; the pages are indented under them
#define RAIL_INDENT				8
#define RAIL_Y					14		// the first heading
#define RAIL_STEP				22
#define RAIL_HEAD_H				24		// from a heading to its first page
#define RAIL_GROUP_GAP			12		// between the last page of a group and the next heading
#define RAIL_ROW_L				6		// where the highlight of a row starts and ends
#define RAIL_ROW_R				( SETTINGS_PANEL_W - 6 )
#define RAIL_RULE_Y				424
#define RAIL_BACK_Y				436

#define TITLE_X					( SETTINGS_PANEL_W + 18 )
#define TITLE_Y					5

typedef struct {
	const char	*label;
	void		( *open )( void );
	const char	*group;		// heading drawn above this page, NULL: none
} settingsPage_t;

static const settingsPage_t settingsPages[SETP_COUNT] = {
	{ "Player",			UI_PlayerSettingsMenu,	"GENERAL" },
	{ "Controls",		UI_ControlsMenu,		NULL },
	{ "Video",			UI_GraphicsOptionsMenu,	NULL },
	{ "Audio",			UI_SoundOptionsMenu,	NULL },
	{ "Network",		UI_NetworkOptionsMenu,	NULL },
	{ "Game",			UI_PreferencesMenu,		NULL },
	{ "HUD",			UI_DF_HudMenu,			"DEFRAG" },
	{ "Crosshair",		UI_DF_ChsMenu,			NULL },
	{ "Replays",		UI_DF_ReplaysMenu,		NULL },
	{ "Gameplay",		UI_DF_GameplayMenu,		NULL },
	{ "Advanced",		UI_DF_Q3ExtrasMenu,		NULL },
	{ "Plugins",		UI_PluginsMenu,			"MISC" },
};

static menutext_s		s_rail[SETP_COUNT + 1];		// the pages, Back
static int				s_railY[SETP_COUNT];		// where the pages are (0: not on the rail)
static int				s_headY[SETP_COUNT];		// where the heading of the group a page starts is
static int				s_current;					// the page the rail is on
static int				s_last;						// the page the Settings menu opens
static void				( *s_leave )( void );
static void				( *s_pageDraw )( void );
static menuframework_s	*s_menu;

// the colours of the map and demo browsers (ui_startmap.c / ui_demo2.c)
static vec4_t	railLabel = { 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	railHead = { 1.0f, 0.38f, 0.22f, 1.0f };
static vec4_t	railWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	panelColor = { 0.14f, 0.02f, 0.02f, 0.88f };
static vec4_t	panelEdge = { 0.62f, 0.10f, 0.08f, 1.0f };
static vec4_t	rowFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	rowFillSel = { 0.85f, 0.12f, 0.06f, 0.60f };
static vec4_t	rowBorder = { 0.80f, 0.14f, 0.10f, 1.0f };
static vec4_t	rowBorderLit = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	rowBorderSel = { 1.0f, 0.38f, 0.22f, 1.0f };


static void Settings_Leave( void ) {
	if ( s_leave ) {
		s_leave();
	}
}


/*
===============
Settings_RailEvent
===============
*/
static void Settings_RailEvent( void *ptr, int event ) {
	int		id;

	if( event != QM_ACTIVATED ) {
		return;
	}

	id = ((menucommon_s*)ptr)->id;
	switch( id ) {
	case ID_BACK:
		Settings_Leave();
		UI_PopMenu();
		return;
	}

	if ( id == s_current ) {
		return;
	}
	Settings_Leave();
	UI_PopMenu();
	settingsPages[id].open();
}


static void Settings_RailDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	qboolean	lit = ( t->generic.flags & QMF_HASMOUSEFOCUS ) || Menu_ItemAtCursor( t->generic.parent ) == self;
	qboolean	current = t->generic.id < SETP_COUNT && t->generic.id == s_current;
	int			x = RAIL_ROW_L, y = t->generic.top, w = RAIL_ROW_R - RAIL_ROW_L, h = t->generic.bottom - t->generic.top;
	float		*color = railLabel;

	if ( current ) {
		UI_FillRect( x, y, w, h, rowFillSel );
		UI_DrawRect( x, y, w, h, rowBorderSel );
		color = railWhite;
	} else if ( lit ) {
		UI_FillRect( x, y, w, h, rowFillLit );
		UI_DrawRect( x, y, w, h, rowBorderLit );
		color = railWhite;
	}
	UI_DrawString( t->generic.x, t->generic.y, t->string, UI_LEFT|UI_SMALLFONT, color );
}


static void Settings_RailItem( menuframework_s *menu, menutext_s *t, int id, const char *label, int y ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_LEFT_JUSTIFY;
	t->generic.x		= RAIL_X + RAIL_INDENT;
	t->generic.y		= y;
	t->generic.id		= id;
	t->generic.callback	= Settings_RailEvent;
	t->generic.ownerdraw = Settings_RailDraw;
	t->string			= (char *)label;
	Menu_AddItem( menu, t );
	// the widget worked out its mouse area from the proportional text: use the row
	t->generic.left		= RAIL_ROW_L;
	t->generic.right	= RAIL_ROW_R;
	t->generic.top		= y - 3;
	t->generic.bottom	= y + SMALLCHAR_HEIGHT + 2;
}


/*
===============
Settings_MenuDraw

The panel, the headings and the title go under the widgets of the page.
===============
*/
static void Settings_MenuDraw( void ) {
	int		i;

	UI_FillRect( 0, 0, SETTINGS_PANEL_W, SCREEN_HEIGHT, panelColor );
	UI_FillRect( SETTINGS_PANEL_W, 0, 2, SCREEN_HEIGHT, panelEdge );

	// a heading is in the colour of the selected things of the map browser, with a rule under it, the pages are indented
	for ( i = 0; i < SETP_COUNT; i++ ) {
		if ( s_railY[i] && settingsPages[i].group ) {
			UI_DrawString2( RAIL_X, s_headY[i], settingsPages[i].group, railHead, 7, 14 );
			UI_FillRect( RAIL_X, s_headY[i] + 17, SETTINGS_PANEL_W - 2 * RAIL_X, 1, rowBorder );
		}
	}
	UI_FillRect( RAIL_X, RAIL_RULE_Y, SETTINGS_PANEL_W - 2 * RAIL_X, 1, rowBorder );

	UI_DrawProportionalString( TITLE_X, TITLE_Y, settingsPages[s_current].label, UI_LEFT|UI_SMALLFONT|UI_DROPSHADOW, railWhite );
	UI_FillRect( TITLE_X, SETTINGS_TITLE_H - 4, SCREEN_WIDTH - TITLE_X - 10, 2, panelEdge );

	if ( s_pageDraw ) {
		s_pageDraw();
	} else {
		Menu_Draw( s_menu );
	}
}


/*
===============
UI_Settings_AddRail

Adds the category rail to a settings page and dresses it.  Add it after the page's own items (the first one of the menu
gets the focus when it opens) and before any item that covers the whole screen.  "leave" may be NULL.
===============
*/
void UI_Settings_AddRail( menuframework_s *menu, int current, void ( *leave )( void ) ) {
	int		i, y;

	memset( s_rail, 0, sizeof( s_rail ) );
	memset( s_railY, 0, sizeof( s_railY ) );
	memset( s_headY, 0, sizeof( s_headY ) );
	s_current = current;
	s_last = current;
	s_leave = leave;
	s_menu = menu;
	menu->settings = qtrue;
	s_pageDraw = menu->draw;
	menu->draw = Settings_MenuDraw;

	y = RAIL_Y;
	for ( i = 0; i < SETP_COUNT; i++ ) {
		if ( i == SETP_PLUGINS && !UI_PluginsSupported() ) {
			continue;		// the engine has no client plugins
		}
		if ( settingsPages[i].group ) {
			if ( i ) {
				y += RAIL_GROUP_GAP;
			}
			s_headY[i] = y;
			y += RAIL_HEAD_H;
		}
		s_railY[i] = y;
		Settings_RailItem( menu, &s_rail[i], i, settingsPages[i].label, y );
		y += RAIL_STEP;
	}
	Settings_RailItem( menu, &s_rail[SETP_COUNT], ID_BACK, "Back", RAIL_BACK_Y );
}


/*
=================
UI_SettingsMenu_Cache
=================
*/
void UI_SettingsMenu_Cache( void ) {
}


/*
===============
UI_SettingsMenu

Opens the page that was open last.
===============
*/
void UI_SettingsMenu( void ) {
	if ( s_last == SETP_PLUGINS && !UI_PluginsSupported() ) {
		s_last = SETP_PLAYER;
	}
	settingsPages[s_last].open();
}
