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
DeFRaG main menu (ui 0x1e6f build, 0x1d97 event, 0x1e2a draw): a list of text buttons (no 3D banner model, no Single Player /
Cinematics / Team Arena).  "Exit" quits at once.

oDFs: the same soft look as the Escape menu and the browsers.  The buttons are rows on the left that light up with an accent
bar, the right side is a short list of the servers that have players (ui_mainservers.c, df_ui_mainServers), and the bottom
strip has the version of the mod and of the engine as chips, which turn into update buttons when there is a newer release.

DeFRaG's four game type buttons (Defrag VQ3 / CPM / Tricks Mode / Fast Caps) are one "Maps" button here: it opens the
start-map menu (ui 0x3825, ui_startmap.c), whose mode chips are those four.
*/

#define ID_DF_MAPS				1
#define ID_DF_SERVERS			7
#define ID_DF_DEMOS				10
#define ID_DF_SETTINGS			11
#define ID_DF_CREDITS			13
#define ID_DF_MODS				14
#define ID_DF_EXIT				15

// the buttons
#define NAV_X					16
#define NAV_Y					96
#define NAV_W					224		// "SERVER BROWSER" is 205 pixels wide in the small proportional font
#define NAV_ROW_H				28
#define NAV_STEP				30
#define NAV_TEXT_X				12		// from the left edge of the row to the text

// the server panel
#define SRV_X					250
#define SRV_Y					30
#define SRV_W					374
#define SRV_HEAD_H				28
#define SRV_COLS_Y				32		// from the top of the panel
#define SRV_ROWS_Y				50
#define SRV_ROW_H				18
#define SRV_H					( SRV_ROWS_Y + MAIN_SERVERS_SHOWN * SRV_ROW_H + 36 )
#define SRV_PAD					12
#define SRV_COL_LOCK			4
#define SRV_COL_NAME			14		// 18 characters
#define SRV_COL_MAP				166		// 14 characters
#define SRV_COL_PLAYERS			326		// right edge
#define SRV_COL_PING			362		// right edge
#define SRV_NAME_CHARS			18
#define SRV_MAP_CHARS			14
#define SRV_BUTTON_H			20
#define SRV_DOUBLECLICK			600

// the bottom strip
#define FOOT_Y					448
#define FOOT_H					( SCREEN_HEIGHT - FOOT_Y )
#define CHIP_X					28
#define CHIP_H					18
#define CHIP_GAP				8

typedef struct {
	int			id;
	const char	*string;
	int			gap;		// extra pixels before the button
} mainButton_t;

static const mainButton_t mainButtons[] = {
	{ ID_DF_MAPS,		"Maps",				0 },
	{ ID_DF_SERVERS,	"Server Browser",	0 },
	{ ID_DF_DEMOS,		"Demos",			0 },
	{ ID_DF_SETTINGS,	"Settings",			10 },
	{ ID_DF_MODS,		"Mods",				0 },
	{ ID_DF_EXIT,		"Exit",				10 }
};

#define MAIN_BUTTONS	( sizeof( mainButtons ) / sizeof( mainButtons[0] ) )

typedef struct {
	menuframework_s	menu;
	menutext_s		button[MAIN_BUTTONS];
	menutext_s		credits;			// oDFs: not in the list, a small dim link at the bottom right
	qboolean		keyNav;				// the last input was a navigation key: the focused link is lit
	int				lastCursorX, lastCursorY;

	char			selAdr[64];			// the selected server of the panel
	qboolean		userSel;			// the player picked it: otherwise the first one is the selection
	char			clickAdr[64];
	int				clickTime;
} mainmenu_t;

static mainmenu_t s_main;

typedef struct {
	menuframework_s menu;
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;

static vec4_t	mainLabel = { 0.92f, 0.86f, 0.84f, 1.00f };
static vec4_t	mainWhite = { 1.00f, 1.00f, 1.00f, 1.00f };
static vec4_t	mainExit = { 1.00f, 0.55f, 0.50f, 1.00f };
static vec4_t	mainHint = { 0.62f, 0.38f, 0.34f, 1.00f };
static vec4_t	mainHead = { 0.72f, 0.62f, 0.60f, 1.00f };
static vec4_t	mainOff = { 0.45f, 0.35f, 0.33f, 1.00f };
static vec4_t	mainGood = { 0.55f, 0.85f, 0.55f, 1.00f };
static vec4_t	mainWarn = { 1.00f, 0.85f, 0.25f, 1.00f };
static vec4_t	mainBad = { 1.00f, 0.35f, 0.25f, 1.00f };
static vec4_t	mainPlate = { 1.00f, 1.00f, 1.00f, 0.05f };
static vec4_t	mainPlateHover = { 1.00f, 1.00f, 1.00f, 0.12f };
static vec4_t	mainBand = { 0.00f, 0.00f, 0.00f, 0.32f };
static vec4_t	mainCreditsColor = { 0.45f, 0.35f, 0.33f, 0.70f };
static vec4_t	mainCreditsLit = { 1.00f, 1.00f, 1.00f, 0.90f };

#define CREDITS_CHARW	7
#define CREDITS_CHARH	14
#define CREDITS_X		( SRV_X + SRV_W - SRV_PAD - 7 * CREDITS_CHARW )
#define CREDITS_Y		( FOOT_Y + ( FOOT_H - CREDITS_CHARH ) / 2 )

static qboolean Main_Lit( menucommon_s *c ) {
	return ( c->flags & QMF_HASMOUSEFOCUS ) || ( s_main.keyNav && Menu_ItemAtCursor( c->parent ) == (void *)c );
}

// the Credits link: 7x14 capital letters, faint; lit while the pointer is on it, or after a navigation key moved the focus to it
static void Main_CreditsDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;

	UI_DrawString2( t->generic.x, t->generic.y, t->string, Main_Lit( &t->generic ) ? mainCreditsLit : mainCreditsColor, CREDITS_CHARW, CREDITS_CHARH );
}

// a button: a row that shows an accent bar and a light fill while it is lit
static void Main_ButtonDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	qboolean	lit = Main_Lit( &t->generic );
	int			x = t->generic.left, y = t->generic.top, w = t->generic.right - t->generic.left, h = t->generic.bottom - t->generic.top;

	UI_SoftRow( x, y, w, h, lit ? SOFT_HOVER : 0, t->generic.id == ID_DF_EXIT ? mainExit : NULL );
	UI_DrawProportionalString( x + NAV_TEXT_X, y + ( h - 20 ) / 2, t->string, UI_LEFT|UI_SMALLFONT|UI_DROPSHADOW,
		lit ? mainWhite : t->generic.id == ID_DF_EXIT ? mainExit : uiDfColorBig );
}

/*
=================
Main_MenuEvent (0x1d97)
=================
*/
void Main_MenuEvent (void* ptr, int event) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	// the ping queue is for the menu that opens
	MainServers_Stop();

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_DF_MAPS:
		UI_DF_MapsMenu();
		break;

	case ID_DF_SERVERS:
		UI_ArenaServersMenu();
		break;

	case ID_DF_DEMOS:
		UI_DemosMenu();
		break;

	case ID_DF_SETTINGS:
		UI_SettingsMenu();
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
Update chips

One chip for this mod (oDFs) and one for the engine (oDFe) in the bottom strip: a dot, the name and the version, driven by
the <prefix>_* cvars of the engine's updater (cl_update.c).  An engine without it never sets <prefix>_state, so there is
only the version.  A chip with an update ("available" and "error") is a button.
===============
*/
typedef struct {
	const char	*name;
	const char	*prefix;
	const char	*command;
	const char	*versionCvar;	// where the version is, NULL: version
	const char	*version;
} mainUpdater_t;

static const mainUpdater_t main_updaters[] = {
	{ "oDFs", "odfs_update", "odfs_update\n", NULL, DF_VERSION_STRING DF_VERSION_TAG },
	{ "oDFe", "odfe_update", "odfe_update\n", "odfe_version", NULL },
};

#define NUM_UPDATERS ( sizeof( main_updaters ) / sizeof( main_updaters[0] ) )

typedef struct {
	char		name[16];
	char		detail[64];
	vec4_t		dot;
	vec4_t		detailColor;
	vec4_t		tint;			// the plate of a chip that is a button (alpha 0: the plain plate)
	int			percent;		// the download, -1: none
	qboolean	clickable;
	int			x, y, w;
} mainChip_t;

static qboolean Main_ChipFill( const mainUpdater_t *up, mainChip_t *c ) {
	char	state[32], value[64], version[48];

	memset( c, 0, sizeof( *c ) );
	c->percent = -1;
	Q_strncpyz( c->name, up->name, sizeof( c->name ) );
	if ( up->versionCvar ) {
		trap_Cvar_VariableStringBuffer( up->versionCvar, version, sizeof( version ) );
	} else {
		Q_strncpyz( version, up->version, sizeof( version ) );
	}
	trap_Cvar_VariableStringBuffer( va( "%s_state", up->prefix ), state, sizeof( state ) );
	if ( !state[0] && !version[0] ) {
		return qfalse;
	}

	Vector4Copy( mainOff, c->dot );
	Vector4Copy( mainHint, c->detailColor );
	Q_strncpyz( c->detail, version, sizeof( c->detail ) );

	if ( !Q_stricmp( state, "checking" ) ) {
		Q_strcat( c->detail, sizeof( c->detail ), version[0] ? " checking..." : "checking..." );
	} else if ( !Q_stricmp( state, "uptodate" ) ) {
		Vector4Copy( mainGood, c->dot );
	} else if ( !Q_stricmp( state, "available" ) ) {
		trap_Cvar_VariableStringBuffer( va( "%s_latest", up->prefix ), value, sizeof( value ) );
		if ( version[0] ) {
			Com_sprintf( c->detail, sizeof( c->detail ), "%s > %s", version, value );
		} else {
			Com_sprintf( c->detail, sizeof( c->detail ), "update %s", value );
		}
		Vector4Copy( mainWarn, c->dot );
		Vector4Copy( mainWarn, c->detailColor );
		Vector4Copy( mainWarn, c->tint );
		c->clickable = qtrue;
	} else if ( !Q_stricmp( state, "downloading" ) ) {
		c->percent = trap_Cvar_VariableValue( va( "%s_progress", up->prefix ) );
		Com_sprintf( c->detail, sizeof( c->detail ), "downloading %i%%", c->percent );
		Vector4Copy( mainWarn, c->dot );
		Vector4Copy( mainWarn, c->detailColor );
		Vector4Copy( mainWarn, c->tint );
	} else if ( !Q_stricmp( state, "ready" ) ) {
		Q_strncpyz( c->detail, "restart to apply", sizeof( c->detail ) );
		Vector4Copy( mainGood, c->dot );
		Vector4Copy( mainGood, c->detailColor );
		Vector4Copy( mainGood, c->tint );
	} else if ( !Q_stricmp( state, "error" ) ) {
		Q_strncpyz( c->detail, "update failed", sizeof( c->detail ) );
		Vector4Copy( mainBad, c->dot );
		Vector4Copy( mainBad, c->detailColor );
		Vector4Copy( mainBad, c->tint );
		c->clickable = qtrue;
	}
	if ( c->tint[3] != 0 ) {
		c->tint[3] = 0.16f;
	}

	c->y = FOOT_Y + ( FOOT_H - CHIP_H ) / 2;
	c->w = 8 + 6 + 6 + ( (int)strlen( c->name ) + ( c->detail[0] ? 1 + (int)strlen( c->detail ) : 0 ) ) * SMALLCHAR_WIDTH + 8;
	return qtrue;
}

// the chips that are shown, side by side from the left
static int Main_Chips( mainChip_t *chips ) {
	int		i, n = 0, x = CHIP_X;

	for ( i = 0; i < (int)NUM_UPDATERS; i++ ) {
		if ( Main_ChipFill( &main_updaters[i], &chips[n] ) ) {
			chips[n].x = x;
			x += chips[n].w + CHIP_GAP;
			n++;
		}
	}
	return n;
}

static qboolean Main_InRect( int x, int y, int w, int h ) {
	return uis.cursorx >= x && uis.cursorx < x + w && uis.cursory >= y && uis.cursory < y + h;
}

static void Main_DrawChips( void ) {
	mainChip_t	chips[NUM_UPDATERS];
	int			i, n = Main_Chips( chips );

	for ( i = 0; i < n; i++ ) {
		mainChip_t	*c = &chips[i];
		qboolean	hover = c->clickable && Main_InRect( c->x, c->y, c->w, CHIP_H );
		int			x = c->x + 8 + 6 + 6;
		vec4_t		plate;

		if ( c->tint[3] != 0 ) {
			Vector4Copy( c->tint, plate );
			plate[3] = hover ? 0.30f : c->tint[3];
		} else {
			Vector4Copy( mainPlate, plate );
		}
		UI_FillRect( c->x, c->y, c->w, CHIP_H, plate );
		UI_FillRect( c->x + 8, c->y + ( CHIP_H - 6 ) / 2, 6, 6, c->dot );
		UI_DrawString( x, c->y + 1, c->name, UI_LEFT|UI_SMALLFONT, hover ? mainWhite : mainLabel );
		if ( c->detail[0] ) {
			UI_DrawString( x + ( (int)strlen( c->name ) + 1 ) * SMALLCHAR_WIDTH, c->y + 1, c->detail, UI_LEFT|UI_SMALLFONT, hover ? mainWhite : c->detailColor );
		}
		if ( c->percent >= 0 ) {
			int		percent = c->percent > 100 ? 100 : c->percent;

			UI_FillRect( c->x, c->y + CHIP_H - 2, c->w, 2, mainPlateHover );
			UI_FillRect( c->x, c->y + CHIP_H - 2, c->w * percent / 100, 2, c->dot );
		}
	}
}

// a click on a chip with an update starts it
static qboolean Main_ChipClick( void ) {
	mainChip_t	chips[NUM_UPDATERS];
	int			i, n = Main_Chips( chips );

	for ( i = 0; i < n; i++ ) {
		if ( chips[i].clickable && Main_InRect( chips[i].x, chips[i].y, chips[i].w, CHIP_H ) ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, main_updaters[i].command );
			return qtrue;
		}
	}
	return qfalse;
}


/*
===============
The server panel
===============
*/
static int Main_ServerShown( void ) {
	int		n = MainServers_Count();

	return n > MAIN_SERVERS_SHOWN ? MAIN_SERVERS_SHOWN : n;
}

static int Main_ServerRowAt( void ) {
	int		row;

	if ( uis.cursorx < SRV_X + 4 || uis.cursorx >= SRV_X + SRV_W - 4 || uis.cursory < SRV_Y + SRV_ROWS_Y ) {
		return -1;
	}
	row = ( uis.cursory - SRV_Y - SRV_ROWS_Y ) / SRV_ROW_H;
	return row < Main_ServerShown() ? row : -1;
}

static const mainServer_t *Main_ServerSelected( void ) {
	int		i;

	for ( i = 0; i < Main_ServerShown(); i++ ) {
		if ( !Q_stricmp( MainServers_Get( i )->adr, s_main.selAdr ) ) {
			return MainServers_Get( i );
		}
	}
	return NULL;
}

// the buttons below the list: 0 Refresh, 1 Join
static void Main_ServerButton( int which, const char **label, int *x, int *y, int *w ) {
	static const char	*labels[2] = { "REFRESH", "JOIN" };
	int		joinW = ( (int)strlen( labels[1] ) * SMALLCHAR_WIDTH ) + 24;
	int		refreshW = ( (int)strlen( labels[0] ) * SMALLCHAR_WIDTH ) + 24;

	*label = labels[which];
	*y = SRV_Y + SRV_H - SRV_BUTTON_H - 8;
	if ( which == 1 ) {
		*w = joinW;
		*x = SRV_X + SRV_W - SRV_PAD - joinW;
	} else {
		*w = refreshW;
		*x = SRV_X + SRV_W - SRV_PAD - joinW - 6 - refreshW;
	}
}

static void Main_ServerJoin( void ) {
	const mainServer_t	*s = Main_ServerSelected();

	if ( s ) {
		MainServers_Stop();
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", s->adr ) );
	}
}

static void Main_ServerPick( const mainServer_t *s ) {
	Q_strncpyz( s_main.selAdr, s->adr, sizeof( s_main.selAdr ) );
	s_main.userSel = qtrue;
}

// the mouse: a row is picked with a click and joined with a second one, Refresh and Join are the buttons
static qboolean Main_ServerClick( void ) {
	const char	*label;
	int			x, y, w, row, i;

	if ( !df_ui_mainServers.integer ) {
		return qfalse;
	}
	for ( i = 0; i < 2; i++ ) {
		Main_ServerButton( i, &label, &x, &y, &w );
		if ( Main_InRect( x, y, w, SRV_BUTTON_H ) ) {
			if ( i == 0 ) {
				MainServers_Refresh();
			} else {
				Main_ServerJoin();
			}
			return qtrue;
		}
	}
	row = Main_ServerRowAt();
	if ( row >= 0 ) {
		const mainServer_t	*s = MainServers_Get( row );

		if ( !Q_stricmp( s_main.clickAdr, s->adr ) && uis.realtime - s_main.clickTime < SRV_DOUBLECLICK ) {
			Main_ServerPick( s );
			Main_ServerJoin();
			s_main.clickAdr[0] = '\0';
		} else {
			Main_ServerPick( s );
			Q_strncpyz( s_main.clickAdr, s->adr, sizeof( s_main.clickAdr ) );
			s_main.clickTime = uis.realtime;
		}
		return qtrue;
	}
	return qfalse;
}

static void Main_ServerButtonDraw( int which, qboolean enabled ) {
	const char	*label;
	int			x, y, w, h = SRV_BUTTON_H;
	qboolean	hover;
	vec4_t		c;

	Main_ServerButton( which, &label, &x, &y, &w );
	hover = enabled && Main_InRect( x, y, w, h );
	if ( which == 1 ) {
		Vector4Copy( uiSoftAccent, c );
		c[3] = enabled ? ( hover ? 1.0f : 0.80f ) : 0.25f;
	} else {
		Vector4Copy( hover ? mainPlateHover : mainPlate, c );
	}
	UI_FillRect( x, y, w, h, c );
	UI_DrawString( x + w / 2, y + ( h - SMALLCHAR_HEIGHT ) / 2, label, UI_CENTER|UI_SMALLFONT, enabled ? ( hover || which == 1 ? mainWhite : mainLabel ) : mainOff );
}

static void Main_DrawServers( void ) {
	int					x = SRV_X, y = SRV_Y, i, n = Main_ServerShown(), hoverRow = Main_ServerRowAt();
	const mainServer_t	*sel;
	char				text[64];

	// the first one is the selection until the player picks one
	if ( n > 0 && ( !s_main.userSel || !Main_ServerSelected() ) ) {
		Q_strncpyz( s_main.selAdr, MainServers_Get( 0 )->adr, sizeof( s_main.selAdr ) );
		s_main.userSel = qfalse;
	}
	sel = Main_ServerSelected();

	UI_SoftPanel( x, y, SRV_W, SRV_H, 0, qtrue );

	// heading, and how many there are
	UI_DrawString( x + SRV_PAD, y + 9, "FAVORITES", UI_LEFT|UI_SMALLFONT, uiDfColorMedium );
	if ( MainServers_Busy() && n == 0 ) {
		Q_strncpyz( text, "searching...", sizeof( text ) );
	} else {
		Com_sprintf( text, sizeof( text ), "%d server%s, %d player%s", MainServers_Count(), MainServers_Count() == 1 ? "" : "s",
			MainServers_Players(), MainServers_Players() == 1 ? "" : "s" );
		UI_FillRect( x + SRV_W - SRV_PAD - (int)strlen( text ) * SMALLCHAR_WIDTH - 12, y + 15, 6, 6, mainGood );
	}
	UI_DrawString( x + SRV_W - SRV_PAD, y + 9, text, UI_RIGHT|UI_SMALLFONT, mainHint );
	UI_SoftRule( x + SRV_PAD, y + SRV_HEAD_H, SRV_W - 2 * SRV_PAD );

	// column headings
	UI_DrawString( x + SRV_COL_NAME, y + SRV_COLS_Y, "NAME", UI_LEFT|UI_SMALLFONT, mainHead );
	UI_DrawString( x + SRV_COL_MAP, y + SRV_COLS_Y, "MAP", UI_LEFT|UI_SMALLFONT, mainHead );
	UI_DrawString( x + SRV_COL_PLAYERS, y + SRV_COLS_Y, "PLAYERS", UI_RIGHT|UI_SMALLFONT, mainHead );
	UI_DrawString( x + SRV_COL_PING, y + SRV_COLS_Y, "PING", UI_RIGHT|UI_SMALLFONT, mainHead );

	for ( i = 0; i < n; i++ ) {
		const mainServer_t	*s = MainServers_Get( i );
		qboolean	isSel = s == sel;
		int			ry = y + SRV_ROWS_Y + i * SRV_ROW_H, ty = ry + ( SRV_ROW_H - SMALLCHAR_HEIGHT ) / 2;
		float		*pingColor = s->ping < 200 ? mainGood : s->ping < 400 ? mainWarn : mainBad;
		char		name[SRV_NAME_CHARS + 1], map[SRV_MAP_CHARS + 1];

		if ( isSel || i == hoverRow ) {
			UI_SoftRow( x + 4, ry, SRV_W - 8, SRV_ROW_H, isSel ? SOFT_SELECTED : SOFT_HOVER, NULL );
		}
		if ( s->needpass ) {
			UI_DrawString( x + SRV_COL_LOCK, ty, "!", UI_LEFT|UI_SMALLFONT, mainBad );
		}
		Q_strncpyz( name, s->host, sizeof( name ) );
		UI_DrawString( x + SRV_COL_NAME, ty, name, UI_LEFT|UI_SMALLFONT, isSel ? mainWhite : mainLabel );
		Q_strncpyz( map, s->map, sizeof( map ) );
		UI_DrawString( x + SRV_COL_MAP, ty, map, UI_LEFT|UI_SMALLFONT, isSel ? mainWhite : mainHead );
		UI_DrawString( x + SRV_COL_PLAYERS, ty, va( "%i/%i", s->clients, s->maxclients ), UI_RIGHT|UI_SMALLFONT,
			( s->maxclients && s->clients >= s->maxclients ) ? mainBad : isSel ? mainWhite : mainLabel );
		UI_DrawString( x + SRV_COL_PING, ty, va( "%i", s->ping ), UI_RIGHT|UI_SMALLFONT, pingColor );
	}

	if ( n == 0 ) {
		if ( MainServers_Busy() ) {
			UI_DrawString( x + SRV_W / 2, y + SRV_ROWS_Y + 40, "Looking for servers...", UI_CENTER|UI_SMALLFONT, mainHint );
		} else if ( !MainServers_Favorites() ) {
			UI_DrawString( x + SRV_W / 2, y + SRV_ROWS_Y + 40, "No favorite servers yet.", UI_CENTER|UI_SMALLFONT, mainHint );
			UI_DrawString( x + SRV_W / 2, y + SRV_ROWS_Y + 60, "Add them in the Server Browser.", UI_CENTER|UI_SMALLFONT, mainHint );
		} else {
			UI_DrawString( x + SRV_W / 2, y + SRV_ROWS_Y + 40, "No favorite server answered.", UI_CENTER|UI_SMALLFONT, mainHint );
		}
	}

	// below: how old the list is, and the buttons
	UI_SoftRule( x + SRV_PAD, y + SRV_H - SRV_BUTTON_H - 14, SRV_W - 2 * SRV_PAD );
	if ( MainServers_Busy() ) {
		Q_strncpyz( text, n ? "refreshing..." : "pinging the servers", sizeof( text ) );
	} else if ( MainServers_Age() < 60 ) {
		Com_sprintf( text, sizeof( text ), "updated %i s ago", MainServers_Age() );
	} else {
		Com_sprintf( text, sizeof( text ), "updated %i min ago", MainServers_Age() / 60 );
	}
	UI_DrawString( x + SRV_PAD, y + SRV_H - SRV_BUTTON_H - 8 + ( SRV_BUTTON_H - SMALLCHAR_HEIGHT ) / 2, text, UI_LEFT|UI_SMALLFONT, mainHint );
	Main_ServerButtonDraw( 0, qtrue );
	Main_ServerButtonDraw( 1, sel != NULL );
}


static sfxHandle_t Main_MenuKey( int key ) {
	if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW || key == K_ENTER || key == K_KP_ENTER ) {
		s_main.keyNav = qtrue;
	}
	if ( key == K_F5 && df_ui_mainServers.integer ) {
		MainServers_Refresh();
		return menu_in_sound;
	}
	if ( key == K_MOUSE1 && ( Main_ChipClick() || Main_ServerClick() ) ) {
		return menu_in_sound;
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
		if ( uis.cursorx != s_main.lastCursorX || uis.cursory != s_main.lastCursorY ) {
			s_main.lastCursorX = uis.cursorx;
			s_main.lastCursorY = uis.cursory;
			s_main.keyNav = qfalse;		// the mouse is in charge again
		}

		// the background art is drawn before this, darken it so the text stands out
		if ( df_ui_mainDim.value > 0.0f ) {
			vec4_t	dim;

			dim[0] = dim[1] = dim[2] = 0.0f;
			dim[3] = df_ui_mainDim.value > 1.0f ? 1.0f : df_ui_mainDim.value;
			UI_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, dim );
		}

		MainServers_Frame();

		if ( df_ui_mainServers.integer ) {
			Main_DrawServers();
		}

		// the bottom strip: the versions, and Credits
		UI_FillRect( 0, FOOT_Y, SCREEN_WIDTH, FOOT_H, mainBand );
		UI_SoftRule( 0, FOOT_Y, SCREEN_WIDTH );
		Main_DrawChips();

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

	y = NAV_Y;
	for ( i = 0; i < (int)MAIN_BUTTONS; i++ ) {
		if ( mainButtons[i].id == ID_DF_MODS && !df_ui_showMods.integer ) {
			continue;		// oDFs: hidden unless df_ui_showMods is set
		}
		y += mainButtons[i].gap;
		s_main.button[i].generic.type		= MTYPE_PTEXT;
		s_main.button[i].generic.flags		= QMF_LEFT_JUSTIFY;
		s_main.button[i].generic.x			= NAV_X;
		s_main.button[i].generic.y			= y;
		s_main.button[i].generic.id			= mainButtons[i].id;
		s_main.button[i].generic.callback	= Main_MenuEvent;
		s_main.button[i].generic.ownerdraw	= Main_ButtonDraw;
		s_main.button[i].string				= (char *)mainButtons[i].string;
		s_main.button[i].style				= UI_SMALLFONT;
		Menu_AddItem( &s_main.menu, &s_main.button[i] );
		// the widget worked out its mouse area from the text: use the row
		s_main.button[i].generic.left		= NAV_X;
		s_main.button[i].generic.right		= NAV_X + NAV_W;
		s_main.button[i].generic.top		= y;
		s_main.button[i].generic.bottom		= y + NAV_ROW_H;
		y += NAV_STEP;
	}

	// oDFs: Credits stays on the main menu but out of the way: a small dim link in the bottom strip, white when the pointer is on it
	s_main.credits.generic.type		= MTYPE_PTEXT;
	s_main.credits.generic.flags	= QMF_LEFT_JUSTIFY;
	s_main.credits.generic.x		= CREDITS_X;
	s_main.credits.generic.y		= CREDITS_Y;
	s_main.credits.generic.ownerdraw	= Main_CreditsDraw;
	s_main.credits.generic.id		= ID_DF_CREDITS;
	s_main.credits.generic.callback	= Main_MenuEvent;
	s_main.credits.string			= "CREDITS";
	s_main.credits.color			= mainCreditsColor;
	s_main.credits.focusColor		= color_white;
	s_main.credits.style			= UI_SMALLFONT|UI_FOCUSCOLOR;
	Menu_AddItem( &s_main.menu, &s_main.credits );
	// the widget worked out its mouse area from the proportional text: use the characters
	s_main.credits.generic.left		= s_main.credits.generic.x;
	s_main.credits.generic.right	= s_main.credits.generic.x + 7 * CREDITS_CHARW;
	s_main.credits.generic.top		= CREDITS_Y;
	s_main.credits.generic.bottom	= CREDITS_Y + CREDITS_CHARH;

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
