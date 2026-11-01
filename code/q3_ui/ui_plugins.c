/*
oDFs: the Plugins page of the settings menu. Lists the client plugins of the oDFe engine (the native libraries in the plugins folder
of the home path), says which run, and switches them on and off. The engine does the work and remembers the choice: the
extension syscalls trap_ODF_Plugin* (docs/plugins.md of the engine), found by name, so an engine without them just has no
"Plugins" page in the settings rail (UI_PluginsSupported).
*/

#include "ui_local.h"

#define PL_MAX			64
#define PL_NAMELEN		256
#define PL_ROWS			15
#define PL_ROWH			SMALLCHAR_HEIGHT
#define PL_X			120			// right of the settings rail
#define PL_Y			84
#define PL_W			500			// pixels; the scroll bar is inside
#define PL_CHARS		( PL_W / SMALLCHAR_WIDTH )
#define PL_NAMECHARS	24
#define PL_VERSIONX		( PL_X + 8 + ( PL_NAMECHARS + 1 ) * SMALLCHAR_WIDTH )
#define PL_VERSIONCHARS	26
#define PL_STATEX		( PL_X + PL_W - 22 )	// the right edge of the state words
#define PL_SB_W			10
#define PL_REFRESH_MS	400
#define PL_DOUBLECLICK	600

#define PL_BUTTON_W		96
#define PL_BUTTON_H		30
#define PL_BUTTON_Y		438
#define PL_BUTTON_MARGIN 12
#define PL_NOTE_Y		( PL_Y + PL_ROWS * PL_ROWH + 12 )
#define PL_NOTE_CHARS	58
#define PL_NOTE_LINES	3

#define ID_PL_TOGGLE	2
#define ID_PL_LIST		3

// the states of trap_ODF_PluginInfo
#define PLS_RUNNING		0
#define PLS_STOPPED		1
#define PLS_FAILED		2
#define PLS_CRASHED		3
#define PLS_DISABLED	4

typedef struct {
	char		name[PL_NAMELEN];
	char		version[48];
	char		note[192];
	int			state;
	qboolean	off;		// switched off (a crashed plugin that is switched off still shows as crashed until the next start)
} plRow_t;

typedef struct {
	menuframework_s	menu;
	menulist_s		list;
	menutext_s		toggle;

	plRow_t			rows[PL_MAX];
	int				numRows;
	int				lastRefresh;
	int				clickIndex, clickTime;
	qboolean		keyNav;
	int				lastCursorX, lastCursorY;
	char			toggleLabel[12];
} plugins_t;

static plugins_t	s_pl;
static const char	*plBlank[PL_MAX];

static vec4_t	plWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	plLabel = { 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	plHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	plOn = { 0.55f, 0.85f, 0.55f, 1.0f };
static vec4_t	plOff = { 0.55f, 0.50f, 0.48f, 1.0f };
static vec4_t	plBad = { 1.0f, 0.35f, 0.30f, 1.0f };
static vec4_t	plTrack = { 0.15f, 0.0f, 0.0f, 0.6f };
static vec4_t	plThumb = { 0.70f, 0.08f, 0.08f, 0.95f };
static vec4_t	plBorder = { 0.80f, 0.14f, 0.10f, 1.0f };
static vec4_t	plBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	plFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	plFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	plChipOff = { 0.45f, 0.35f, 0.33f, 1.0f };

/*
===============
UI_PluginsSupported

oDFe extension (not in DeFRaG): does this engine have the plugin syscalls? The engine publishes the number of
trap_GetValue in the read-only cvar "//trap_GetValue"; on engines without it the extension syscalls do not exist and calling
them would be a fatal "bad ui system trap", so they are only used after this has said yes.
===============
*/
qboolean UI_PluginsSupported( void ) {
	static int state;		// 0 not asked yet, 1 yes, -1 no
	char value[16];

	if ( state == 0 ) {
		state = -1;
		trap_Cvar_VariableStringBuffer( "//trap_GetValue", value, sizeof( value ) );
		if ( atoi( value ) == UI_TRAP_GETVALUE
			&& trap_GetValue( value, sizeof( value ), "trap_ODF_PluginCount" ) && atoi( value ) == UI_ODF_PLUGIN_COUNT
			&& trap_GetValue( value, sizeof( value ), "trap_ODF_PluginInfo" ) && atoi( value ) == UI_ODF_PLUGIN_INFO
			&& trap_GetValue( value, sizeof( value ), "trap_ODF_PluginSetEnabled" ) && atoi( value ) == UI_ODF_PLUGIN_SET_ENABLED
			&& trap_GetValue( value, sizeof( value ), "trap_ODF_PluginRescan" ) && atoi( value ) == UI_ODF_PLUGIN_RESCAN ) {
			state = 1;
		}
	}
	return state > 0;
}

static const plRow_t *PL_Selected( void ) {
	if ( s_pl.list.curvalue < 0 || s_pl.list.curvalue >= s_pl.numRows ) {
		return NULL;
	}
	return &s_pl.rows[s_pl.list.curvalue];
}

static void PL_ClampTop( void ) {
	int max = s_pl.list.numitems - PL_ROWS;

	if ( max < 0 ) {
		max = 0;
	}
	if ( s_pl.list.curvalue < s_pl.list.top ) {
		s_pl.list.top = s_pl.list.curvalue;
	}
	if ( s_pl.list.curvalue >= s_pl.list.top + PL_ROWS ) {
		s_pl.list.top = s_pl.list.curvalue - PL_ROWS + 1;
	}
	s_pl.list.top = Com_Clamp( 0, max, s_pl.list.top );
}

// the label of the button says what it does to the selected plugin
static void PL_UpdateButton( void ) {
	Q_strncpyz( s_pl.toggleLabel, ( PL_Selected() && PL_Selected()->off ) ? "ENABLE" : "DISABLE", sizeof( s_pl.toggleLabel ) );
	if ( PL_Selected() ) {
		s_pl.toggle.generic.flags &= ~QMF_GRAYED;
	} else {
		s_pl.toggle.generic.flags |= QMF_GRAYED;
	}
}

/*
===============
PL_Refresh

Reads the list from the engine again (it changes when a plugin is switched, starts late or crashes), keeping the selection
on the same plugin.
===============
*/
static void PL_Refresh( void ) {
	char	keep[PL_NAMELEN];
	char	buf[PL_NAMELEN];
	int		i, count;

	keep[0] = '\0';
	if ( PL_Selected() ) {
		Q_strncpyz( keep, PL_Selected()->name, sizeof( keep ) );
	}

	count = trap_ODF_PluginCount();
	if ( count > PL_MAX ) {
		count = PL_MAX;
	}
	s_pl.numRows = 0;
	for ( i = 0; i < count; i++ ) {
		plRow_t *r = &s_pl.rows[s_pl.numRows];

		r->state = trap_ODF_PluginInfo( i, 0, r->name, sizeof( r->name ) );
		if ( r->state < 0 ) {
			continue;
		}
		trap_ODF_PluginInfo( i, 1, r->version, sizeof( r->version ) );
		trap_ODF_PluginInfo( i, 2, r->note, sizeof( r->note ) );
		trap_ODF_PluginInfo( i, 3, buf, sizeof( buf ) );
		r->off = buf[0] == '1';
		s_pl.numRows++;
	}
	s_pl.list.numitems = s_pl.numRows;
	s_pl.lastRefresh = uis.realtime;

	for ( i = 0; i < s_pl.numRows; i++ ) {
		if ( !Q_stricmp( s_pl.rows[i].name, keep ) ) {
			s_pl.list.curvalue = i;
			break;
		}
	}
	if ( s_pl.list.curvalue >= s_pl.numRows ) {
		s_pl.list.curvalue = s_pl.numRows ? s_pl.numRows - 1 : 0;
	}
	PL_ClampTop();

	PL_UpdateButton();
}

static void PL_Toggle( void ) {
	const plRow_t *r = PL_Selected();

	if ( !r ) {
		return;
	}
	trap_ODF_PluginSetEnabled( r->name, r->off ? 1 : 0 );
	PL_Refresh();
}

static void PL_DrawOutline( int x, int y, int w, int h, const float *c ) {
	UI_FillRect( x, y, w, 1, c );
	UI_FillRect( x, y + h - 1, w, 1, c );
	UI_FillRect( x, y, 1, h, c );
	UI_FillRect( x + w - 1, y, 1, h, c );
}

static qboolean PL_Lit( menucommon_s *c ) {
	return ( c->flags & QMF_HASMOUSEFOCUS ) || ( s_pl.keyNav && Menu_ItemAtCursor( c->parent ) == (void *)c );
}

static void PL_ButtonDraw( void *self ) {
	menutext_s	*b = (menutext_s *)self;
	qboolean	off = ( b->generic.flags & QMF_GRAYED ) != 0;
	qboolean	lit = !off && PL_Lit( &b->generic );
	int			x = b->generic.left, y = b->generic.top;
	vec4_t		fill;
	const float	*border = lit ? plBorderFocus : plBorder;
	const char	*text = b->generic.id == ID_PL_TOGGLE ? s_pl.toggleLabel : b->string;

	Vector4Copy( lit ? plFillLit : plFill, fill );
	if ( lit ) {
		fill[3] *= 0.85f + 0.15f * sin( uis.realtime * 0.006f );
	}
	UI_FillRect( x, y, PL_BUTTON_W, PL_BUTTON_H, fill );
	PL_DrawOutline( x, y, PL_BUTTON_W, PL_BUTTON_H, border );
	PL_DrawOutline( x + 1, y + 1, PL_BUTTON_W - 2, PL_BUTTON_H - 2, border );
	UI_DrawProportionalString( x + PL_BUTTON_W / 2, y + ( PL_BUTTON_H - 20 ) / 2, text, UI_CENTER | UI_SMALLFONT, off ? plChipOff : lit ? color_white : plLabel );
}

static void PL_ListEvent( void *ptr, int event ) {
	// the selection is read from the list when it is needed
}

static void PL_ButtonEvent( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( ( (menucommon_s *)ptr )->id ) {
	case ID_PL_TOGGLE:
		PL_Toggle();
		break;
	}
}

static void PL_StateText( const plRow_t *r, const char **text, float **color ) {
	switch ( r->state ) {
	case PLS_RUNNING:	*text = "ON"; *color = plOn; break;
	case PLS_DISABLED:	*text = "OFF"; *color = plOff; break;
	case PLS_FAILED:	*text = "FAILED"; *color = plBad; break;
	case PLS_CRASHED:	*text = "CRASHED"; *color = plBad; break;
	default:			*text = "STOPPED"; *color = plOff; break;
	}
}

static void PL_ListDraw( void *self ) {
	menulist_s	*l = (menulist_s *)self;
	qboolean	hasfocus = ( l->generic.parent->cursor == l->generic.menuPosition );
	int			i, x = l->generic.x, y = l->generic.y;
	char		text[PL_NAMELEN];

	// the headings
	UI_DrawString( x + 8, y - PL_ROWH - 2, "PLUGIN", UI_LEFT | UI_SMALLFONT, plHint );
	UI_DrawString( PL_VERSIONX, y - PL_ROWH - 2, "VERSION", UI_LEFT | UI_SMALLFONT, plHint );
	UI_DrawString( PL_STATEX, y - PL_ROWH - 2, "STATE", UI_RIGHT | UI_SMALLFONT, plHint );

	for ( i = l->top; i < l->top + PL_ROWS && i < l->numitems; i++, y += PL_ROWH ) {
		const plRow_t	*r = &s_pl.rows[i];
		const char		*state;
		float			*stateColor;
		int				style = UI_LEFT | UI_SMALLFONT;
		float			*color = text_color_normal;

		if ( i == l->curvalue ) {
			UI_FillRect( x - 2, y, PL_W - PL_SB_W - 2, PL_ROWH + 2, listbar_color );
			color = text_color_highlight;
			if ( hasfocus ) {
				style |= UI_PULSE;
			}
		}
		if ( r->state != PLS_RUNNING && i != l->curvalue ) {
			color = plOff;
		}
		Q_strncpyz( text, r->name, PL_NAMECHARS + 1 );
		UI_DrawString( x + 8, y, text, style, color );
		Q_strncpyz( text, r->version, PL_VERSIONCHARS + 1 );
		UI_DrawString( PL_VERSIONX, y, text, style, color );
		PL_StateText( r, &state, &stateColor );
		UI_DrawString( PL_STATEX, y, state, UI_RIGHT | UI_SMALLFONT, stateColor );
	}
}

static void PL_ScrollbarDraw( void ) {
	int total = s_pl.numRows;
	int trackY = PL_Y, trackH = PL_ROWS * PL_ROWH;
	int thumbH, thumbY, range;

	if ( total <= PL_ROWS ) {
		return;
	}
	range = total - PL_ROWS;
	thumbH = trackH * PL_ROWS / total;
	if ( thumbH < 16 ) {
		thumbH = 16;
	}
	thumbY = trackY + ( trackH - thumbH ) * Com_Clamp( 0, range, s_pl.list.top ) / range;
	UI_FillRect( PL_X + PL_W - PL_SB_W, trackY, PL_SB_W, trackH, plTrack );
	UI_FillRect( PL_X + PL_W - PL_SB_W + 1, thumbY, PL_SB_W - 2, thumbH, plThumb );
}

// what is said about the selected plugin
static const char *PL_Detail( const plRow_t *r ) {
	switch ( r->state ) {
	case PLS_RUNNING:
		return "Running. DISABLE (Enter) stops it and removes its commands.";
	case PLS_DISABLED:
		return "Switched off. ENABLE (Enter) starts it, also in a game that is running.";
	case PLS_FAILED:
		return r->note[0] ? r->note : "It did not start.";
	case PLS_CRASHED:
		return va( "%s  It does not run again until the game is restarted%s.", r->note,
			r->off ? "; it stays off after that" : "" );
	}
	return "";
}

// the text, broken into lines of at most PL_NOTE_CHARS characters at the spaces
static void PL_DrawWrapped( int x, int y, const char *text ) {
	char	line[PL_NOTE_CHARS + 2];
	int		lines, len, cut;

	for ( lines = 0; lines < PL_NOTE_LINES && *text; lines++ ) {
		len = strlen( text );
		cut = len;
		if ( len > PL_NOTE_CHARS ) {
			cut = PL_NOTE_CHARS;
			while ( cut > PL_NOTE_CHARS / 2 && text[cut] != ' ' ) {
				cut--;
			}
			if ( text[cut] != ' ' ) {
				cut = PL_NOTE_CHARS;
			}
		}
		Q_strncpyz( line, text, cut + 1 );
		UI_DrawString( x, y, line, UI_LEFT | UI_SMALLFONT, plWhite );
		text += cut;
		while ( *text == ' ' ) {
			text++;
		}
		y += PL_ROWH;
	}
}

static void PL_MenuDraw( void ) {
	const plRow_t *r;

	if ( uis.cursorx != s_pl.lastCursorX || uis.cursory != s_pl.lastCursorY ) {
		s_pl.lastCursorX = uis.cursorx;
		s_pl.lastCursorY = uis.cursory;
		s_pl.keyNav = qfalse;		// the mouse is in charge again
	}
	if ( uis.realtime - s_pl.lastRefresh >= PL_REFRESH_MS || uis.realtime < s_pl.lastRefresh ) {
		PL_Refresh();
	}

	PL_UpdateButton();
	Menu_Draw( &s_pl.menu );
	PL_ScrollbarDraw();

	r = PL_Selected();
	if ( r ) {
		PL_DrawWrapped( PL_X + 8, PL_NOTE_Y, PL_Detail( r ) );
	} else {
		UI_DrawString( PL_X + PL_W / 2, PL_Y + 40, "No plugins found", UI_CENTER | UI_SMALLFONT, plWhite );
		UI_DrawString( PL_X + PL_W / 2, PL_Y + 40 + PL_ROWH + 4, "Put them (.dll or .so) into the plugins folder of your home path;", UI_CENTER | UI_SMALLFONT, plHint );
		UI_DrawString( PL_X + PL_W / 2, PL_Y + 40 + 2 * PL_ROWH + 4, "they start with the game, or when this menu is opened.", UI_CENTER | UI_SMALLFONT, plHint );
	}
}

static sfxHandle_t PL_Key( int key ) {
	sfxHandle_t	sound;
	qboolean	onList = Menu_ItemAtCursor( &s_pl.menu ) == (void *)&s_pl.list;

	if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW || key == K_ENTER || key == K_KP_ENTER || key == K_SPACE ) {
		s_pl.keyNav = qtrue;
	}

	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		if ( s_pl.numRows ) {
			ScrollList_Key( &s_pl.list, key );
		}
		return menu_null_sound;
	}

	if ( onList && ( key == K_ENTER || key == K_KP_ENTER || key == K_SPACE ) ) {
		PL_Toggle();
		return menu_move_sound;
	}

	sound = Menu_DefaultKey( &s_pl.menu, key );

	// a click on the state of a plugin switches it, a double click on the plugin does too
	if ( key == K_MOUSE1 && UI_CursorInRect( PL_X, PL_Y, PL_W - PL_SB_W, PL_ROWS * PL_ROWH ) ) {
		int row = s_pl.list.top + ( uis.cursory - PL_Y ) / PL_ROWH;

		if ( row >= 0 && row < s_pl.numRows ) {
			s_pl.list.curvalue = row;
			if ( uis.cursorx >= PL_STATEX - 7 * SMALLCHAR_WIDTH - 4 ) {
				PL_Toggle();
				s_pl.clickIndex = -1;
			} else {
				int now = trap_Milliseconds();

				if ( s_pl.clickIndex == row && now - s_pl.clickTime <= PL_DOUBLECLICK ) {
					s_pl.clickIndex = -1;
					PL_Toggle();
				} else {
					s_pl.clickIndex = row;
					s_pl.clickTime = now;
				}
			}
		}
	}
	return sound;
}

static void PL_InitButton( menutext_s *b, int id, const char *label, int x ) {
	Com_Memset( b, 0, sizeof( *b ) );
	b->generic.type = MTYPE_PTEXT;
	b->generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	b->generic.id = id;
	b->generic.x = x;
	b->generic.y = PL_BUTTON_Y;
	b->generic.callback = PL_ButtonEvent;
	b->generic.ownerdraw = PL_ButtonDraw;
	b->string = (char *)label;
	Menu_AddItem( &s_pl.menu, b );
	// the widget worked out its mouse area from the text: use the plate
	b->generic.left = x;
	b->generic.right = x + PL_BUTTON_W - 1;
	b->generic.top = PL_BUTTON_Y;
	b->generic.bottom = PL_BUTTON_Y + PL_BUTTON_H - 1;
}

/*
===============
UI_PluginsMenu
===============
*/
void UI_PluginsMenu( void ) {
	Com_Memset( &s_pl, 0, sizeof( s_pl ) );
	s_pl.menu.wrapAround = qtrue;
	s_pl.menu.fullscreen = qtrue;
	s_pl.menu.key = PL_Key;
	s_pl.menu.draw = PL_MenuDraw;
	s_pl.clickIndex = -1;

	// libraries put into the folder since the game started
	trap_ODF_PluginRescan();

	s_pl.list.generic.type = MTYPE_SCROLLLIST;
	s_pl.list.generic.flags = QMF_PULSEIFFOCUS;
	s_pl.list.generic.id = ID_PL_LIST;
	s_pl.list.generic.callback = PL_ListEvent;
	s_pl.list.generic.ownerdraw = PL_ListDraw;
	s_pl.list.generic.x = PL_X;
	s_pl.list.generic.y = PL_Y;
	s_pl.list.width = PL_CHARS;
	s_pl.list.height = PL_ROWS;
	s_pl.list.columns = 1;
	s_pl.list.itemnames = plBlank;

	Menu_AddItem( &s_pl.menu, &s_pl.list );
	PL_InitButton( &s_pl.toggle, ID_PL_TOGGLE, "DISABLE", 640 - PL_BUTTON_MARGIN - PL_BUTTON_W );
	UI_Settings_AddRail( &s_pl.menu, SETP_PLUGINS, NULL );

	PL_Refresh();
	UI_PushMenu( &s_pl.menu );
}
