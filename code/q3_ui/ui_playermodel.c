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
#include "ui_local.h"

/*
=======================================================================

PLAYER MODEL MENU

oDFs: the model picker in the look of the map browser.  A search box and a scrolling grid of the portraits (icon_*.tga of
models/players/<model>/) on the left, the 3D preview on the right.  A click picks a model (the "model", "headmodel",
"team_model" and "team_headmodel" cvars are set at once), a double click or Enter picks it and goes back.  The preview
follows the pointer (or the keyboard cursor) after a short pause, and goes back to the picked model when it leaves the grid.

The portraits are registered when they come into view, and the folders are listed without the size limits of the old
menu, so a big collection shows completely.

=======================================================================
*/

#define LOW_MEMORY			(5 * 1024 * 1024)

#define MAX_PLAYERMODELS	1024
#define PM_NAME				48
#define PM_SEARCHMAX		32
#define PM_MAXWORDS			4
#define PM_PREVIEW_DELAY	120
#define PM_DOUBLECLICK		600

// layout
#define PM_FIELD_X			10
#define PM_FIELD_Y			9
#define PM_FIELD_W			372
#define PM_COLS				5
#define PM_ROWS				5
#define PM_CELL				64
#define PM_GAP				6
#define PM_PITCH			( PM_CELL + PM_GAP )
#define PM_GX				14
#define PM_GY				40
#define PM_GRID_H			( PM_ROWS * PM_PITCH - PM_GAP )
#define PM_SB_X				( PM_GX + PM_COLS * PM_PITCH - PM_GAP + 8 )
#define PM_SB_W				12
#define PM_SB_MINTHUMB		20
#define PM_INFO_Y			( PM_GY + PM_GRID_H + 8 )
#define PM_RX				410
#define PM_PREVIEW_X		410
#define PM_PREVIEW_Y		36
#define PM_PREVIEW_W		220
#define PM_PREVIEW_H		350
#define PM_BACK_X			14
#define PM_BACK_Y			432
#define PM_BACK_W			88
#define PM_BACK_H			24

#define ID_BACK				100
#define ID_SEARCH			101

typedef struct
{
	char		model[PM_NAME];
	char		skin[PM_NAME];
	qhandle_t	pic;
} pmEntry_t;

typedef struct
{
	menuframework_s	menu;
	menufield_s		search;
	menutext_s		back;
	playerInfo_t	playerinfo;
	uiPlayerView_t	turn;

	pmEntry_t		entries[MAX_PLAYERMODELS];
	int				numentries;
	int				view[MAX_PLAYERMODELS];		// the entries that match the search
	int				numview;

	int				top;						// the first row shown
	int				cur;						// keyboard cursor (an index of view), -1: none
	int				selected;					// the entry of the model that is set, -1: none of the list
	int				shown;						// the entry the preview shows, -1: the model that is set
	int				target;						// the entry the preview should show
	int				targetTime;
	char			current[2 * PM_NAME + 2];	// the "model" cvar as it was when the menu opened

	char			applied[PM_SEARCHMAX + 1];
	char			word[PM_MAXWORDS][PM_SEARCHMAX + 1];
	int				numwords;

	qboolean		keyNav;
	int				lastCursorX, lastCursorY;
	qboolean		sbDrag;
	int				sbGrab;
	int				clickIndex;
	int				clickTime;
} playermodel_t;

static playermodel_t	s_pm;

static vec4_t	pmWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	pmPlate = { 0.14f, 0.02f, 0.02f, 0.90f };
static vec4_t	pmPlateFocus = { 0.26f, 0.04f, 0.03f, 0.95f };
static vec4_t	pmBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	pmBorderSel = { 1.0f, 0.38f, 0.22f, 1.0f };
static vec4_t	pmHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	pmLabel = { 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	pmFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	pmFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	pmRule = { 0.62f, 0.10f, 0.08f, 1.0f };

static void PM_Outline( int x, int y, int w, int h, int t, const float *color ) {
	UI_FillRect( x, y, w, t, color );
	UI_FillRect( x, y + h - t, w, t, color );
	UI_FillRect( x, y, t, h, color );
	UI_FillRect( x + w - t, y, t, h, color );
}

static void PM_EntryName( const pmEntry_t *e, char *out, int size ) {
	Com_sprintf( out, size, "%s/%s", e->model, e->skin );
}

static int PM_TotalRows( void ) {
	return ( s_pm.numview + PM_COLS - 1 ) / PM_COLS;
}

static int PM_MaxTop( void ) {
	int	max = PM_TotalRows() - PM_ROWS;

	return max > 0 ? max : 0;
}

static void PM_ScrollTo( int idx ) {
	int	row = idx / PM_COLS;

	if ( row < s_pm.top ) {
		s_pm.top = row;
	} else if ( row >= s_pm.top + PM_ROWS ) {
		s_pm.top = row - PM_ROWS + 1;
	}
	s_pm.top = Com_Clamp( 0, PM_MaxTop(), s_pm.top );
}

/*
=================
The search: words, all of them in "model/skin"
=================
*/
static void PM_ParseSearch( void ) {
	const char	*p = s_pm.search.field.buffer;
	int			n;

	Q_strncpyz( s_pm.applied, p, sizeof( s_pm.applied ) );
	s_pm.numwords = 0;
	while ( *p && s_pm.numwords < PM_MAXWORDS ) {
		while ( *p == ' ' ) {
			p++;
		}
		if ( !*p ) {
			break;
		}
		n = 0;
		while ( *p && *p != ' ' ) {
			if ( n < PM_SEARCHMAX ) {
				s_pm.word[s_pm.numwords][n++] = *p;
			}
			p++;
		}
		s_pm.word[s_pm.numwords][n] = '\0';
		s_pm.numwords++;
	}
}

static qboolean PM_Matches( const pmEntry_t *e ) {
	char	name[2 * PM_NAME + 2];
	int		i;

	PM_EntryName( e, name, sizeof( name ) );
	for ( i = 0; i < s_pm.numwords; i++ ) {
		if ( !Q_stristr( name, s_pm.word[i] ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void PM_UpdateView( void ) {
	int	i;

	s_pm.numview = 0;
	s_pm.cur = -1;
	for ( i = 0; i < s_pm.numentries; i++ ) {
		if ( PM_Matches( &s_pm.entries[i] ) ) {
			if ( i == s_pm.selected ) {
				s_pm.cur = s_pm.numview;
			}
			s_pm.view[s_pm.numview++] = i;
		}
	}
	if ( s_pm.cur < 0 && s_pm.numview ) {
		s_pm.cur = 0;
	}
	s_pm.top = 0;
	if ( s_pm.cur >= 0 && !s_pm.numwords ) {
		s_pm.top = Com_Clamp( 0, PM_MaxTop(), s_pm.cur / PM_COLS - PM_ROWS / 2 );
	}
}

/*
=================
The preview
=================
*/
static void PM_SetPreview( const char *modelskin ) {
	vec3_t	viewangles;
	vec3_t	moveangles;

	memset( &s_pm.playerinfo, 0, sizeof( playerInfo_t ) );

	viewangles[YAW]   = 180 - 30;
	viewangles[PITCH] = 0;
	viewangles[ROLL]  = 0;
	VectorClear( moveangles );

	UI_PlayerInfo_SetModel( &s_pm.playerinfo, modelskin );
	UI_PlayerInfo_SetInfo( &s_pm.playerinfo, LEGS_IDLE, TORSO_STAND, viewangles, moveangles, WP_MACHINEGUN, qfalse );
}

static void PM_ShowEntry( int entry ) {
	char	name[2 * PM_NAME + 2];

	if ( entry >= 0 ) {
		PM_EntryName( &s_pm.entries[entry], name, sizeof( name ) );
		PM_SetPreview( name );
	} else {
		PM_SetPreview( s_pm.current );
	}
	s_pm.shown = entry;
}

static void PM_Select( int entry ) {
	char	name[2 * PM_NAME + 2];

	s_pm.selected = entry;
	PM_EntryName( &s_pm.entries[entry], name, sizeof( name ) );
	trap_Cvar_Set( "model", name );
	trap_Cvar_Set( "headmodel", name );
	trap_Cvar_Set( "team_model", name );
	trap_Cvar_Set( "team_headmodel", name );
	Q_strncpyz( s_pm.current, name, sizeof( s_pm.current ) );
}

static int PM_CellAt( int x, int y ) {
	int	col, row, idx;

	x -= PM_GX;
	y -= PM_GY;
	if ( x < 0 || y < 0 ) {
		return -1;
	}
	col = x / PM_PITCH;
	row = y / PM_PITCH;
	if ( col >= PM_COLS || row >= PM_ROWS || x % PM_PITCH >= PM_CELL || y % PM_PITCH >= PM_CELL ) {
		return -1;
	}
	idx = ( s_pm.top + row ) * PM_COLS + col;
	return idx < s_pm.numview ? idx : -1;
}

/*
=================
Scroll bar
=================
*/
static qboolean PM_ScrollbarGeometry( int *thumbY, int *thumbH ) {
	int	total = PM_TotalRows();
	int	range, top;

	if ( total <= PM_ROWS ) {
		*thumbY = PM_GY;
		*thumbH = PM_GRID_H;
		return qfalse;
	}
	range = total - PM_ROWS;
	*thumbH = PM_GRID_H * PM_ROWS / total;
	if ( *thumbH < PM_SB_MINTHUMB ) {
		*thumbH = PM_SB_MINTHUMB;
	}
	top = Com_Clamp( 0, range, s_pm.top );
	*thumbY = PM_GY + ( PM_GRID_H - *thumbH ) * top / range;
	return qtrue;
}

static void PM_ScrollbarDrag( void ) {
	int	thumbY, thumbH, range;

	if ( !PM_ScrollbarGeometry( &thumbY, &thumbH ) ) {
		return;
	}
	range = PM_TotalRows() - PM_ROWS;
	s_pm.top = Com_Clamp( 0, range, ( uis.cursory - s_pm.sbGrab - PM_GY ) * range / ( PM_GRID_H - thumbH ) );
}

static void PM_ScrollbarDraw( void ) {
	static vec4_t	trackColor = { 0.15f, 0.0f, 0.0f, 0.6f };
	static vec4_t	thumbColor = { 0.70f, 0.08f, 0.08f, 0.95f };
	static vec4_t	hotColor = { 1.0f, 0.30f, 0.15f, 1.0f };
	int				thumbY, thumbH;
	qboolean		hot;

	if ( !PM_ScrollbarGeometry( &thumbY, &thumbH ) ) {
		return;
	}
	hot = s_pm.sbDrag || UI_CursorInRect( PM_SB_X, PM_GY, PM_SB_W, PM_GRID_H );
	UI_FillRect( PM_SB_X, PM_GY, PM_SB_W, PM_GRID_H, trackColor );
	UI_FillRect( PM_SB_X + 1, thumbY, PM_SB_W - 2, thumbH, hot ? hotColor : thumbColor );
}

/*
=================
Widgets
=================
*/
static void PM_FieldDraw( void *self ) {
	menufield_s	*f = (menufield_s *)self;
	qboolean	focus = ( Menu_ItemAtCursor( f->generic.parent ) == self );
	int			x = f->generic.left, y = f->generic.y;
	int			w = f->generic.right - x + 1;

	UI_FillRect( x, y - 2, w, SMALLCHAR_HEIGHT + 4, focus ? pmPlateFocus : pmPlate );
	if ( focus ) {
		PM_Outline( x, y - 2, w, SMALLCHAR_HEIGHT + 4, 1, pmBorderFocus );
	}
	if ( !f->field.buffer[0] && !focus ) {
		UI_DrawString( x + 4, y, "search models / skins", UI_SMALLFONT, pmHint );
	} else {
		MField_Draw( &f->field, x + 4, y, UI_SMALLFONT | ( focus ? UI_PULSE : 0 ), pmWhite );
	}
}

static void PM_BackDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	qboolean	lit = ( t->generic.flags & QMF_HASMOUSEFOCUS ) || Menu_ItemAtCursor( t->generic.parent ) == self;

	UI_FillRect( PM_BACK_X, PM_BACK_Y, PM_BACK_W, PM_BACK_H, lit ? pmFillLit : pmFill );
	if ( lit ) {
		PM_Outline( PM_BACK_X, PM_BACK_Y, PM_BACK_W, PM_BACK_H, 1, pmBorderFocus );
	}
	UI_DrawString( PM_BACK_X + PM_BACK_W / 2, PM_BACK_Y + ( PM_BACK_H - SMALLCHAR_HEIGHT ) / 2, "BACK", UI_CENTER | UI_SMALLFONT, lit ? pmWhite : pmLabel );
}

static void PM_Event( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( ((menucommon_s *)ptr)->id == ID_BACK ) {
		UI_PopMenu();
	}
}

static void PM_DrawCell( int idx, int x, int y ) {
	pmEntry_t	*e = &s_pm.entries[s_pm.view[idx]];
	qboolean	sel = s_pm.view[idx] == s_pm.selected;
	qboolean	lit = s_pm.keyNav ? idx == s_pm.cur : PM_CellAt( uis.cursorx, uis.cursory ) == idx;

	if ( !e->pic ) {
		e->pic = trap_R_RegisterShaderNoMip( va( "models/players/%s/icon_%s", e->model, e->skin ) );
	}
	UI_FillRect( x, y, PM_CELL, PM_CELL, pmPlate );
	if ( e->pic ) {
		UI_DrawHandlePic( x, y, PM_CELL, PM_CELL, e->pic );
	}
	if ( sel ) {
		PM_Outline( x - 2, y - 2, PM_CELL + 4, PM_CELL + 4, 2, pmBorderSel );
	}
	if ( lit ) {
		PM_Outline( x - 1, y - 1, PM_CELL + 2, PM_CELL + 2, 1, pmBorderFocus );
	}
}

/*
=================
PM_MenuDraw
=================
*/
static void PM_MenuDraw( void ) {
	int		i, idx, row, hover, want;
	char	name[2 * PM_NAME + 2];
	const pmEntry_t	*e;

	if ( uis.cursorx != s_pm.lastCursorX || uis.cursory != s_pm.lastCursorY ) {
		s_pm.lastCursorX = uis.cursorx;
		s_pm.lastCursorY = uis.cursory;
		s_pm.keyNav = qfalse;		// the mouse is in charge again
	}
	if ( s_pm.sbDrag ) {
		if ( trap_Key_IsDown( K_MOUSE1 ) ) {
			PM_ScrollbarDrag();
		} else {
			s_pm.sbDrag = qfalse;
		}
	}
	if ( strcmp( s_pm.applied, s_pm.search.field.buffer ) ) {
		PM_ParseSearch();
		PM_UpdateView();
	}

	// which model the preview should show: the one under the pointer / keyboard cursor, else the picked one
	hover = -1;
	if ( s_pm.keyNav ) {
		if ( s_pm.cur >= 0 && s_pm.cur < s_pm.numview ) {
			hover = s_pm.view[s_pm.cur];
		}
	} else if ( !s_pm.sbDrag ) {
		idx = PM_CellAt( uis.cursorx, uis.cursory );
		if ( idx >= 0 ) {
			hover = s_pm.view[idx];
		}
	}
	want = hover >= 0 ? hover : s_pm.selected;
	if ( want != s_pm.target ) {
		s_pm.target = want;
		s_pm.targetTime = uis.realtime;
	}
	if ( s_pm.target != s_pm.shown && uis.realtime - s_pm.targetTime >= PM_PREVIEW_DELAY && trap_MemoryRemaining() > LOW_MEMORY ) {
		PM_ShowEntry( s_pm.target );
	}

	// the portraits
	for ( row = 0; row < PM_ROWS; row++ ) {
		for ( i = 0; i < PM_COLS; i++ ) {
			idx = ( s_pm.top + row ) * PM_COLS + i;
			if ( idx >= s_pm.numview ) {
				break;
			}
			PM_DrawCell( idx, PM_GX + i * PM_PITCH, PM_GY + row * PM_PITCH );
		}
	}
	if ( !s_pm.numview ) {
		UI_DrawString( PM_GX + ( PM_COLS * PM_PITCH - PM_GAP ) / 2, PM_GY + 40, s_pm.numentries ? "No model matches" : "No models", UI_CENTER | UI_SMALLFONT, pmWhite );
	}
	PM_ScrollbarDraw();

	// under the grid: how many there are
	UI_DrawString( PM_SB_X + PM_SB_W, PM_INFO_Y, s_pm.numwords ? va( "%d of %d", s_pm.numview, s_pm.numentries ) : va( "%d models", s_pm.numentries ), UI_RIGHT | UI_SMALLFONT, pmHint );

	// the preview and the names of the model it shows
	if ( trap_MemoryRemaining() <= LOW_MEMORY ) {
		UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H / 2, "LOW MEMORY", UI_CENTER | UI_SMALLFONT, color_red );
	} else {
		UI_PlayerView_Draw( &s_pm.turn, PM_PREVIEW_X, PM_PREVIEW_Y, PM_PREVIEW_W, PM_PREVIEW_H, &s_pm.playerinfo, uis.realtime / 2 );
	}
	UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H + 6 + 3 * SMALLCHAR_HEIGHT, "drag to turn, wheel to zoom", UI_CENTER | UI_SMALLFONT, pmHint );
	if ( s_pm.target >= 0 ) {
		e = &s_pm.entries[s_pm.target];
		Q_strncpyz( name, e->model, sizeof( name ) );
		Q_strupr( name );
		UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H + 6, name, UI_CENTER | UI_SMALLFONT, pmWhite );
		UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H + 6 + SMALLCHAR_HEIGHT, e->skin, UI_CENTER | UI_SMALLFONT, pmLabel );
		if ( s_pm.target != s_pm.selected ) {
			UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H + 6 + 2 * SMALLCHAR_HEIGHT, "click to pick", UI_CENTER | UI_SMALLFONT, pmHint );
		}
	} else {
		Q_strncpyz( name, s_pm.current, sizeof( name ) );
		Q_strupr( name );
		UI_DrawString( PM_PREVIEW_X + PM_PREVIEW_W / 2, PM_PREVIEW_Y + PM_PREVIEW_H + 6, name, UI_CENTER | UI_SMALLFONT, pmWhite );
	}

	UI_DrawProportionalString( PM_RX, 5, "PLAYER MODEL", UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW, pmLabel );

	Menu_Draw( &s_pm.menu );
}

/*
=================
PM_MenuKey
=================
*/
static void PM_Move( int cur ) {
	if ( !s_pm.numview ) {
		return;
	}
	s_pm.cur = Com_Clamp( 0, s_pm.numview - 1, cur );
	s_pm.keyNav = qtrue;
	PM_ScrollTo( s_pm.cur );
}

static void PM_Pick( int idx ) {
	int	now = trap_Milliseconds();

	s_pm.cur = idx;
	PM_Select( s_pm.view[idx] );
	if ( s_pm.clickIndex == idx && now - s_pm.clickTime <= PM_DOUBLECLICK ) {
		UI_PopMenu();
		return;
	}
	s_pm.clickIndex = idx;
	s_pm.clickTime = now;
}

static sfxHandle_t PM_MenuKey( int key ) {
	void		*item;
	qboolean	typing, onBack, emptyBox;
	int			idx, thumbY, thumbH;

	// drag in the preview turns the model, the wheel there zooms
	if ( ( key == K_MOUSE1 || key == K_MWHEELUP || key == K_MWHEELDOWN )
		&& UI_PlayerView_Key( &s_pm.turn, key, PM_PREVIEW_X, PM_PREVIEW_Y, PM_PREVIEW_W, PM_PREVIEW_H ) ) {
		return menu_null_sound;
	}

	// left click on the scroll bar: grab the thumb (a click on the track puts the middle of the thumb there)
	if ( key == K_MOUSE1 && PM_ScrollbarGeometry( &thumbY, &thumbH ) && UI_CursorInRect( PM_SB_X, PM_GY, PM_SB_W, PM_GRID_H ) ) {
		if ( uis.cursory >= thumbY && uis.cursory < thumbY + thumbH ) {
			s_pm.sbGrab = uis.cursory - thumbY;
		} else {
			s_pm.sbGrab = thumbH / 2;
		}
		s_pm.sbDrag = qtrue;
		PM_ScrollbarDrag();
		return menu_null_sound;
	}

	// click on a portrait
	if ( key == K_MOUSE1 ) {
		idx = PM_CellAt( uis.cursorx, uis.cursory );
		if ( idx >= 0 ) {
			PM_Pick( idx );
			return menu_move_sound;
		}
	}

	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		s_pm.top = Com_Clamp( 0, PM_MaxTop(), s_pm.top + ( key == K_MWHEELUP ? -2 : 2 ) );
		return menu_null_sound;
	}

	// a printable character outside of the box starts a search (the key press itself is swallowed, the character
	// event that follows is typed into the box)
	if ( Menu_ItemAtCursor( &s_pm.menu ) != (void *)&s_pm.search ) {
		int ch = key & ~K_CHAR_FLAG;

		if ( ch > ' ' && ch < 127 ) {
			Menu_SetCursorToItem( &s_pm.menu, &s_pm.search );
			if ( !( key & K_CHAR_FLAG ) ) {
				return menu_null_sound;
			}
		}
	}

	item = Menu_ItemAtCursor( &s_pm.menu );
	typing = item == (void *)&s_pm.search;
	onBack = item == (void *)&s_pm.back;
	emptyBox = !s_pm.search.field.buffer[0];

	// Escape with a search typed in clears it (the first press); the next one leaves the menu
	if ( key == K_ESCAPE && typing && !emptyBox ) {
		MField_Clear( &s_pm.search.field );
		return menu_null_sound;
	}

	if ( !onBack ) {
		switch ( key ) {
		case K_LEFTARROW: case K_KP_LEFTARROW: case K_RIGHTARROW: case K_KP_RIGHTARROW:
			if ( typing && !emptyBox ) {
				break;		// the cursor of the text keeps them
			}
			if ( s_pm.cur < 0 ) {
				return menu_buzz_sound;
			}
			idx = s_pm.cur;
			if ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) {
				idx--;
			} else {
				idx++;
			}
			if ( idx < 0 || idx >= s_pm.numview ) {
				return menu_buzz_sound;
			}
			PM_Move( idx );
			return menu_move_sound;
		case K_UPARROW: case K_KP_UPARROW:
			if ( s_pm.cur < 0 ) {
				return menu_buzz_sound;
			}
			if ( s_pm.cur - PM_COLS < 0 ) {
				return menu_buzz_sound;
			}
			PM_Move( s_pm.cur - PM_COLS );
			return menu_move_sound;
		case K_DOWNARROW: case K_KP_DOWNARROW:
			if ( s_pm.cur < 0 ) {
				return menu_buzz_sound;
			}
			if ( s_pm.cur + PM_COLS >= s_pm.numview ) {
				// the last row may be short: go to its last portrait
				if ( s_pm.cur / PM_COLS < ( s_pm.numview - 1 ) / PM_COLS ) {
					PM_Move( s_pm.numview - 1 );
					return menu_move_sound;
				}
				return menu_buzz_sound;
			}
			PM_Move( s_pm.cur + PM_COLS );
			return menu_move_sound;
		case K_PGUP: case K_KP_PGUP:
			PM_Move( s_pm.cur - PM_ROWS * PM_COLS );
			return menu_move_sound;
		case K_PGDN: case K_KP_PGDN:
			PM_Move( s_pm.cur + PM_ROWS * PM_COLS );
			return menu_move_sound;
		case K_HOME: case K_KP_HOME:
			if ( !typing || emptyBox ) {
				PM_Move( 0 );
				return menu_move_sound;
			}
			break;
		case K_END: case K_KP_END:
			if ( !typing || emptyBox ) {
				PM_Move( s_pm.numview - 1 );
				return menu_move_sound;
			}
			break;
		case K_ENTER: case K_KP_ENTER:
			if ( s_pm.cur >= 0 && s_pm.cur < s_pm.numview ) {
				PM_Select( s_pm.view[s_pm.cur] );
				UI_PopMenu();
				return menu_move_sound;
			}
			return menu_buzz_sound;
		}
	}

	return Menu_DefaultKey( &s_pm.menu, key );
}

/*
=================
PM_BuildList

Every icon_<skin>.tga of every models/players/<model>/ folder.  The lists are in static buffers, bigger than the old
2048 characters.
=================
*/
static void PM_BuildList( void )
{
	static char	dirlist[16384];
	static char	filelist[16384];
	char		skinname[MAX_QPATH];
	char		*dirptr, *fileptr;
	int			numdirs, numfiles;
	int			i, j, dirlen, filelen;
	pmEntry_t	*e;

	s_pm.numentries = 0;

	numdirs = trap_FS_GetFileList( "models/players", "/", dirlist, sizeof( dirlist ) );
	dirptr = dirlist;
	for ( i = 0; i < numdirs && s_pm.numentries < MAX_PLAYERMODELS; i++, dirptr += dirlen + 1 )
	{
		dirlen = strlen( dirptr );

		if ( dirlen && dirptr[dirlen-1] == '/' )
			dirptr[dirlen-1] = '\0';

		if ( !strcmp( dirptr, "." ) || !strcmp( dirptr, ".." ) || strlen( dirptr ) >= PM_NAME )
			continue;

		numfiles = trap_FS_GetFileList( va( "models/players/%s", dirptr ), "tga", filelist, sizeof( filelist ) );
		fileptr = filelist;
		for ( j = 0; j < numfiles && s_pm.numentries < MAX_PLAYERMODELS; j++, fileptr += filelen + 1 )
		{
			filelen = strlen( fileptr );

			COM_StripExtension( fileptr, skinname, sizeof( skinname ) );

			if ( Q_stricmpn( skinname, "icon_", 5 ) || strlen( skinname + 5 ) >= PM_NAME )
				continue;

			e = &s_pm.entries[s_pm.numentries++];
			Q_strncpyz( e->model, dirptr, sizeof( e->model ) );
			Q_strncpyz( e->skin, skinname + 5, sizeof( e->skin ) );
			e->pic = 0;
		}
	}
}

/*
=================
PM_MenuInit
=================
*/
static void PM_MenuInit( void )
{
	int			i;
	char		model[2 * PM_NAME + 2];

	memset( &s_pm, 0, sizeof( playermodel_t ) );

	s_pm.menu.key        = PM_MenuKey;
	s_pm.menu.draw       = PM_MenuDraw;
	s_pm.menu.wrapAround = qtrue;
	s_pm.menu.fullscreen = qtrue;
	s_pm.clickIndex      = -1;
	UI_PlayerView_Init( &s_pm.turn );

	s_pm.search.generic.type		= MTYPE_FIELD;
	s_pm.search.generic.flags		= QMF_SMALLFONT | QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_pm.search.generic.id			= ID_SEARCH;
	s_pm.search.generic.ownerdraw	= PM_FieldDraw;
	s_pm.search.generic.x			= PM_FIELD_X + 4;
	s_pm.search.generic.y			= PM_FIELD_Y;
	s_pm.search.field.widthInChars	= 44;
	s_pm.search.field.maxchars		= PM_SEARCHMAX;
	Menu_AddItem( &s_pm.menu, &s_pm.search );
	s_pm.search.generic.left		= PM_FIELD_X;
	s_pm.search.generic.right		= PM_FIELD_X + PM_FIELD_W - 1;
	s_pm.search.generic.top			= PM_FIELD_Y - 2;
	s_pm.search.generic.bottom		= PM_FIELD_Y - 2 + SMALLCHAR_HEIGHT + 3;

	s_pm.back.generic.type			= MTYPE_PTEXT;
	s_pm.back.generic.flags			= QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_pm.back.generic.id			= ID_BACK;
	s_pm.back.generic.x				= PM_BACK_X;
	s_pm.back.generic.y				= PM_BACK_Y;
	s_pm.back.generic.callback		= PM_Event;
	s_pm.back.generic.ownerdraw		= PM_BackDraw;
	s_pm.back.string				= "BACK";
	Menu_AddItem( &s_pm.menu, &s_pm.back );
	// the widget worked out its mouse area from the text: use the button
	s_pm.back.generic.left			= PM_BACK_X;
	s_pm.back.generic.right			= PM_BACK_X + PM_BACK_W - 1;
	s_pm.back.generic.top			= PM_BACK_Y;
	s_pm.back.generic.bottom		= PM_BACK_Y + PM_BACK_H - 1;

	PM_BuildList();

	// the model that is set (a model without a skin is the default skin)
	trap_Cvar_VariableStringBuffer( "model", s_pm.current, sizeof( s_pm.current ) );
	if ( !strchr( s_pm.current, '/' ) ) {
		Q_strcat( s_pm.current, sizeof( s_pm.current ), "/default" );
	}
	s_pm.selected = -1;
	for ( i = 0; i < s_pm.numentries; i++ ) {
		PM_EntryName( &s_pm.entries[i], model, sizeof( model ) );
		if ( !Q_stricmp( s_pm.current, model ) ) {
			s_pm.selected = i;
			break;
		}
	}
	s_pm.shown = s_pm.target = s_pm.selected;
	s_pm.cur = -1;

	PM_ParseSearch();
	PM_UpdateView();
	PM_ShowEntry( s_pm.selected );
}

/*
=================
UI_PlayerModelMenu
=================
*/
void UI_PlayerModelMenu(void)
{
	PM_MenuInit();

	UI_PushMenu( &s_pm.menu );
}
