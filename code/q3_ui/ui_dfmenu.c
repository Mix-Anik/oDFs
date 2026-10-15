/*
===========================================================================
DeFRaG settings menus (ui.qvm 0x46e4 / 0x474c / 0x468c / 0xb4fe .. 0xb59f, decoded in iteration 135).

All DeFRaG settings pages share a column of buttons on the left (BACK, HUD, CHS, GHOST, DEMOS, MISC, MP, Q3), ids 10 and
21..27 (UI_DF_NavInit / UI_DF_NavEvent).  The ported pages are HUD (0x521f) and Q3 ("Quake 3 Extras", 0xb4fe); the other
buttons stay grayed until their page exists (see uiDfPageOpen[]).

Every page has a spinner (id 100) that switches between pages of cvar widgets (Q3: Set 1 = ids 201..213, Set 2 = ids 101..136,
Menu Display = ids 301..312; HUD: Hud / Misc / Video Capture).  Widget layout, cvar binding, value ranges and status bar
help come out of the qvm (ui_dfmenu_data.h and ui_dfpage_*.h are generated from it).  Every field / spinner is
loaded from its cvar when the menu opens or the page spinner changes and is written back when a spinner changes, a
button is pressed or the menu is left with Esc / right mouse.  Menu Display widgets are written on every focus change.
===========================================================================
*/
#include "ui_local.h"

extern vmCvar_t	df_ui_CustomMenu;
extern vmCvar_t	df_ui_DisplayCVarName;
extern vec4_t	uiDfColorMedium, uiDfColorBig, text_color_status;

#define UI_DF_NAVITEMS		8
#define UI_DF_MODPAGES		3
#define UI_DF_MAXITEMS		96

#define ID_NAV_BACK			10
#define ID_NAV_FIRST		21
#define ID_NAV_LAST			27
#define ID_MOD_PAGE			100

#define DF_APPLY_NOEXIST_STR	1		// cvar missing: write "0.000000" instead of the field text  (0x4375)
#define DF_APPLY_NOEXIST_ZERO	2		// cvar missing: write 0 instead of the spinner value        (0x4397)
#define DF_APPLY_CHEATS			4		// only with sv_cheats on and a running local server
#define DF_APPLY_CMD			8		// written through a console command, only when different (0x43b9 / 0x43e7); second apply pass
#define DF_APPLY_NOCMD			16		// changing this widget does not trigger the command pass

#define DF_LOAD_INT		1
#define DF_LOAD_FLOAT	2
#define DF_LOAD_SPIN	3
#define DF_LOAD_STRING	4
#define DF_LOAD_STRTAB	6		// spinner = index of the item whose name equals the cvar string (0x4563)
#define DF_LOAD_BIT		5		// spinner = one bit (min) of an integer cvar (0x45b5); the group is written back as a sum

typedef struct {
	int				pg, kind;			// kind 0 = heading, 1 = field, 2 = spinner (int only: q3asm has no 16 bit initialised data)
	int				id, x, y;
	const char		*label, *cvar;
	int				load;
	float			min, max;
	int				apply;
	int				width, maxchars;	// field width / max chars
	const char		**items;
	const char		*help, *help2;
} uiDfModItem_t;

static const char *uiDfOffOn[] = { "Off", "On", 0 };
static const char *uiDfColorNames[] = { "Black", "Red", "Green", "Yellow", "Blue", "Cyan", "Magenta", "White", 0 };
static const char *uiDfShadowNames[] = { "Off", "Disc", "Stencil1", "Stencil2", 0 };
static const char *uiDfGunNames[] = { "Off", "On", "Static", 0 };
static const char *uiDfCustomMenuNames[] = { "Off", "On", "On with status hint colors", 0 };
static const char *uiDfPageNames[] = { "Set 1", "Set 2", "Menu Display", 0 };

typedef struct {
	const uiDfModItem_t	*items;
	int					numItems;
	const int			*show;			// spinner value -> bit mask of table pages
	const char			*spinLabel, *spinHelp;
	const char			**pageNames;
	int					navIndex;		// button that opens this page
	qboolean			liveTheme;		// write the pg 2 widgets on every focus change (Q3 Menu Display)
} uiDfPageDef_t;

#include "ui_dfmenu_data.h"
#include "ui_dfpage_hud.h"
#include "ui_dfpage_chs.h"
#include "ui_dfpage_ghost.h"
#include "ui_dfpage_demos.h"
#include "ui_dfpage_misc.h"
#include "ui_dfpage_mp.h"

// Q3: Set 1 shows ids 2xx (table page 1), Set 2 ids 1xx (0), Menu Display 3xx (2)
static const int uiDfQ3Show[UI_DF_MODPAGES] = { 1 << 1, 1 << 0, 1 << 2 };

static const uiDfPageDef_t uiDfQ3Page = {
	uiDfQ3Items, sizeof( uiDfQ3Items ) / sizeof( uiDfQ3Items[0] ), uiDfQ3Show,
	"> Quake 3 Extras:", "^1Click for ^7Quake 3 Extras^1 choices", uiDfPageNames, 7, qtrue
};
static const uiDfPageDef_t uiDfChsPage = {
	uiDfChsItems, sizeof( uiDfChsItems ) / sizeof( uiDfChsItems[0] ), uiDfChsShow,
	UIDF_CHS_SPINLABEL, UIDF_CHS_SPINHELP, uiDfChsPageNames, 2, qfalse
};
static const uiDfPageDef_t uiDfGhostPage = {
	uiDfGhostItems, sizeof( uiDfGhostItems ) / sizeof( uiDfGhostItems[0] ), uiDfGhostShow,
	UIDF_GHOST_SPINLABEL, UIDF_GHOST_SPINHELP, uiDfGhostPageNames, 3, qfalse
};
static const uiDfPageDef_t uiDfDemosPage = {
	uiDfDemosItems, sizeof( uiDfDemosItems ) / sizeof( uiDfDemosItems[0] ), uiDfDemosShow,
	UIDF_DEMOS_SPINLABEL, UIDF_DEMOS_SPINHELP, uiDfDemosPageNames, 4, qfalse
};
static const uiDfPageDef_t uiDfMiscPage = {
	uiDfMiscItems, sizeof( uiDfMiscItems ) / sizeof( uiDfMiscItems[0] ), uiDfMiscShow,
	UIDF_MISC_SPINLABEL, UIDF_MISC_SPINHELP, uiDfMiscPageNames, 5, qfalse
};
static const uiDfPageDef_t uiDfMpPage = {
	uiDfMpItems, sizeof( uiDfMpItems ) / sizeof( uiDfMpItems[0] ), uiDfMpShow,
	UIDF_MP_SPINLABEL, UIDF_MP_SPINHELP, uiDfMpPageNames, 6, qfalse
};
static const uiDfPageDef_t uiDfHudPage = {
	uiDfHudItems, sizeof( uiDfHudItems ) / sizeof( uiDfHudItems[0] ), uiDfHudShow,
	UIDF_HUD_SPINLABEL, UIDF_HUD_SPINHELP, uiDfHudPageNames, 1, qfalse
};

typedef union {
	menutext_s	text;
	menufield_s	field;
	menulist_s	list;
} uiDfModWidget_t;

typedef struct {
	menuframework_s	menu;
	menutext_s		nav[UI_DF_NAVITEMS];
	menulist_s		pageSpin;
	uiDfModWidget_t	w[UI_DF_MAXITEMS];
	const uiDfPageDef_t	*def;
} uiDfMod_t;

static uiDfMod_t	s_dfmod;

// opening a settings page from the nav column; pages that are not ported yet stay NULL (button grayed)
static void ( *uiDfPageOpen[UI_DF_NAVITEMS] )( void ) = { 0, UI_DF_HudMenu, UI_DF_ChsMenu, UI_DF_GhostMenu, UI_DF_DemosMenu, UI_DF_MiscMenu, UI_DF_MpMenu, UI_DF_Q3ExtrasMenu };

static const char *uiDfNavText[UI_DF_NAVITEMS] = { "BACK", "HUD", "CHS", "GHOST", "DEMOS", "MISC", "MP", "Q3" };
static const char *uiDfNavHelp[UI_DF_NAVITEMS] = {
	"^1Click to save and return to game",
	"^4Click for ^7HUD^4 options",
	"^4Click for ^7Crosshair^4 options",
	"^4Click for ^7Ghost^4 options",
	"^4Click for ^7Demos^4 options",
	"^4Click for ^7Misc^4 options",
	"^4Click for ^7Multiplayer^4 options",
	"^4Click for ^7Quake 3 Extras^4 options"
};

// 0x48ac: status line at the bottom; df_ui_CustomMenu 2 keeps the colour codes
static void UI_DF_DrawStatus( const char *text ) {
	char	buf[512];

	if ( !text || !text[0] ) {
		return;
	}
	if ( df_ui_CustomMenu.integer == 2 ) {
		UI_DrawString( 320, 450, text, UI_CENTER|UI_SMALLFONT, text_color_status );
	} else {
		Q_strncpyz( buf, text, sizeof( buf ) );
		Q_CleanStr( buf );
		UI_DrawString( 320, 450, buf, UI_CENTER|UI_SMALLFONT, text_color_status );
	}
}

// 0x48ec: cvar name in the top left corner
static void UI_DF_DrawCvarName( const char *name ) {
	if ( name && name[0] && df_ui_DisplayCVarName.integer ) {
		UI_DrawString( 5, 10, name, UI_SMALLFONT, text_color_status );
	}
}

// 0x468c
static void UI_DF_NavStatus( void *ptr ) {
	int id = ( (menucommon_s *)ptr )->id;

	if ( id == ID_NAV_BACK ) {
		UI_DF_DrawStatus( uiDfNavHelp[0] );
	} else if ( id >= ID_NAV_FIRST && id <= ID_NAV_LAST ) {
		UI_DF_DrawStatus( uiDfNavHelp[id - ID_NAV_FIRST + 1] );
	}
}

// 0x46e4: the part of the event handler every settings page shares
static void UI_DF_NavEvent( void *ptr, int event ) {
	int id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ( (menucommon_s *)ptr )->id;
	if ( id == ID_NAV_BACK ) {
		UI_PopMenu();
	} else if ( id >= ID_NAV_FIRST && id <= ID_NAV_LAST && uiDfPageOpen[id - ID_NAV_FIRST + 1] ) {
		UI_PopMenu();
		uiDfPageOpen[id - ID_NAV_FIRST + 1]();
	}
}

// 0x474c: BACK + seven page buttons down the left side
static void UI_DF_NavInit( menuframework_s *menu, menutext_s *nav, void ( *callback )( void *, int ) ) {
	int i, y = 30;

	for ( i = 0; i < UI_DF_NAVITEMS; i++ ) {
		nav[i].generic.type		= MTYPE_PTEXT;
		nav[i].generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
		nav[i].generic.id		= i ? ID_NAV_FIRST + i - 1 : ID_NAV_BACK;
		nav[i].generic.x		= 10;
		nav[i].generic.y		= y;
		nav[i].generic.callback	= callback;
		nav[i].generic.statusbar = UI_DF_NavStatus;
		nav[i].string			= (char *)uiDfNavText[i];
		nav[i].style			= 0;
		nav[i].color			= uiDfColorBig;
		if ( i && !uiDfPageOpen[i] ) {
			nav[i].generic.flags |= QMF_GRAYED;
		}
		Menu_AddItem( menu, &nav[i] );
		y += ( i ? 30 : 60 );
	}
}

// ---- Q3 Extras page -------------------------------------------------------------------------------------------

static qboolean UI_DF_CvarExists( const char *cvar ) {
	char buf[256];

	trap_Cvar_VariableStringBuffer( cvar, buf, sizeof( buf ) );
	return buf[0] != 0;
}

// 0x423a
static qboolean UI_DF_CheatsOn( void ) {
	char buf[1024];

	trap_GetConfigString( 1, buf, sizeof( buf ) );
	return strcmp( Info_ValueForKey( buf, "sv_cheats" ), "0" ) != 0;
}

static qboolean UI_DF_CheatsAndRunning( void ) {
	return UI_DF_CheatsOn() && trap_Cvar_VariableValue( "sv_running" ) != 0;
}

// widgets are only touched while the table page is selected, but the data always follows the cvars
static void UI_DF_ModRefreshItem( int i ) {
	const uiDfModItem_t	*it = &s_dfmod.def->items[i];
	uiDfModWidget_t		*w = &s_dfmod.w[i];
	char	buf[256];
	float	v;

	if ( !it->cvar || !it->load || !UI_DF_CvarExists( it->cvar ) ) {
		return;
	}
	if ( ( it->apply & DF_APPLY_CHEATS ) && !UI_DF_CheatsAndRunning() ) {
		return;
	}
	switch ( it->load ) {
	case DF_LOAD_INT:		// 0x45eb
		v = (int)trap_Cvar_VariableValue( it->cvar );
		Com_sprintf( w->field.field.buffer, sizeof( w->field.field.buffer ), "%i", (int)Com_Clamp( it->min, it->max, v ) );
		break;
	case DF_LOAD_FLOAT:		// 0x462f
		v = trap_Cvar_VariableValue( it->cvar );
		Com_sprintf( w->field.field.buffer, sizeof( w->field.field.buffer ), "%.3f", Com_Clamp( it->min, it->max, v ) );
		break;
	case DF_LOAD_SPIN:		// 0x4527
		v = (int)trap_Cvar_VariableValue( it->cvar );
		w->list.curvalue = (int)Com_Clamp( it->min, it->max, v );
		break;
	case DF_LOAD_STRTAB:	// 0x4563
		w->list.curvalue = 0;
		trap_Cvar_VariableStringBuffer( it->cvar, buf, sizeof( buf ) );
		for ( v = 0; v < it->max + 1 && it->items[(int)v]; v++ ) {
			if ( !strcmp( buf, it->items[(int)v] ) ) {
				w->list.curvalue = (int)v;
				break;
			}
		}
		break;
	case DF_LOAD_BIT:		// 0x45b5
		w->list.curvalue = ( (int)trap_Cvar_VariableValue( it->cvar ) & (int)it->min ) != 0;
		break;
	case DF_LOAD_STRING:	// 0x4666
		trap_Cvar_VariableStringBuffer( it->cvar, buf, sizeof( buf ) );
		Com_sprintf( w->field.field.buffer, sizeof( w->field.field.buffer ), "%s", buf );
		break;
	}
}

static void UI_DF_ModApplyItem( int i, int cmdPass ) {
	const uiDfModItem_t	*it = &s_dfmod.def->items[i];
	uiDfModWidget_t		*w = &s_dfmod.w[i];

	if ( !it->cvar || !it->load || ( ( it->apply & DF_APPLY_CMD ) != 0 ) != cmdPass ) {
		return;
	}
	if ( cmdPass ) {
		// 0x43b9 / 0x43e7: console command, and only when the value really changed
		char cur[256];

		trap_Cvar_VariableStringBuffer( it->cvar, cur, sizeof( cur ) );
		if ( it->kind == 2 && it->load == DF_LOAD_SPIN ) {
			if ( (int)trap_Cvar_VariableValue( it->cvar ) != w->list.curvalue ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "set %s %i\n", it->cvar, w->list.curvalue ) );
			}
		} else {
			const char *val = it->kind == 2 ? it->items[w->list.curvalue] : w->field.field.buffer;

			if ( strcmp( cur, val ) ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "set %s \"%s\"\n", it->cvar, val ) );
			}
		}
		return;
	}
	if ( ( it->apply & DF_APPLY_CHEATS ) && !UI_DF_CheatsAndRunning() ) {
		return;
	}
	if ( it->load == DF_LOAD_BIT ) {
		// all spinners of the group write the same sum (0x4d21)
		int j, sum = 0;

		for ( j = 0; j < s_dfmod.def->numItems; j++ ) {
			const uiDfModItem_t *o = &s_dfmod.def->items[j];

			if ( o->load == DF_LOAD_BIT && o->cvar && !strcmp( o->cvar, it->cvar ) ) {
				sum += s_dfmod.w[j].list.curvalue << ( o->min == 1 ? 0 : o->min == 2 ? 1 : 2 );
			}
		}
		trap_Cvar_Set( it->cvar, va( "%i", sum ) );
		return;
	}
	if ( it->kind == 1 ) {
		if ( ( it->apply & DF_APPLY_NOEXIST_STR ) && !UI_DF_CvarExists( it->cvar ) ) {
			trap_Cvar_Set( it->cvar, "0.000000" );
		} else {
			trap_Cvar_Set( it->cvar, w->field.field.buffer );
		}
	} else if ( it->kind == 2 ) {
		if ( ( it->apply & DF_APPLY_NOEXIST_ZERO ) && !UI_DF_CvarExists( it->cvar ) ) {
			trap_Cvar_SetValue( it->cvar, 0 );
		} else {
			trap_Cvar_SetValue( it->cvar, w->list.curvalue );
		}
	}
}

static void UI_DF_ModRefreshAll( void ) {		// 0xb4b2
	int i;

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		UI_DF_ModRefreshItem( i );
	}
}

static void UI_DF_ModApplyAll( void ) {			// 0xb4be
	int i;

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		UI_DF_ModApplyItem( i, 0 );
	}
}

static void UI_DF_ModApplyCmd( void ) {			// 0x9768
	int i;

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		UI_DF_ModApplyItem( i, 1 );
	}
}

static void UI_DF_ModApplyTheme( void ) {		// 0xa847
	int i;

	if ( !s_dfmod.def->liveTheme ) {
		return;
	}

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		if ( s_dfmod.def->items[i].pg == 2 ) {
			UI_DF_ModApplyItem( i, 0 );
		}
	}
}

static void UI_DF_ModShowPage( int pageMask ) {	// 0xa3bd/0xa47b and friends: hide everything, show the pages in the mask
	int i;

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		if ( pageMask & ( 1 << s_dfmod.def->items[i].pg ) ) {
			s_dfmod.w[i].text.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		} else {
			s_dfmod.w[i].text.generic.flags |= ( QMF_HIDDEN|QMF_INACTIVE );
		}
	}
}

// 0xb59f
static void UI_DF_ModStatus( void *ptr ) {
	int id = ( (menucommon_s *)ptr )->id;
	int i;

	if ( id == ID_MOD_PAGE ) {
		UI_DF_DrawStatus( s_dfmod.def->spinHelp );
		return;
	}
	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		if ( s_dfmod.def->items[i].id == id && id ) {
			UI_DF_DrawStatus( s_dfmod.def->items[i].help );
			UI_DF_DrawCvarName( s_dfmod.def->items[i].help2 );
			return;
		}
	}
}

// 0xb54f
static void UI_DF_ModEvent( void *ptr, int event ) {
	if ( event != QM_LOSTFOCUS ) {
		UI_DF_ModApplyTheme();
	}
	if ( event != QM_ACTIVATED ) {
		return;
	}
	UI_DF_ModApplyAll();
	{
		// 0x97fc: the command pass runs for every widget except the map-restart ones
		int id = ( (menucommon_s *)ptr )->id, j, exempt = 0;

		for ( j = 0; j < s_dfmod.def->numItems; j++ ) {
			if ( id && s_dfmod.def->items[j].id == id && ( s_dfmod.def->items[j].apply & DF_APPLY_NOCMD ) ) {
				exempt = 1;
			}
		}
		if ( !exempt ) {
			UI_DF_ModApplyCmd();
		}
	}
	if ( ( (menucommon_s *)ptr )->id == ID_MOD_PAGE ) {
		UI_DF_ModRefreshAll();
		UI_DF_ModShowPage( s_dfmod.def->show[s_dfmod.pageSpin.curvalue] );
	} else {
		UI_DF_NavEvent( ptr, event );
	}
}

// 0xb531
static sfxHandle_t UI_DF_ModKey( int key ) {
	if ( key == K_ESCAPE || key == K_MOUSE2 ) {
		UI_DF_ModApplyAll();
		UI_DF_ModApplyCmd();
	}
	return Menu_DefaultKey( &s_dfmod.menu, key );
}

/*
===============
UI_DF_OpenPage (0xb4fe / 0x521f)
===============
*/
static void UI_DF_OpenPage( const uiDfPageDef_t *def ) {
	int i;

	memset( &s_dfmod, 0, sizeof( s_dfmod ) );
	s_dfmod.def = def;

	s_dfmod.menu.wrapAround = qtrue;
	s_dfmod.menu.fullscreen = qtrue;
	s_dfmod.menu.key = UI_DF_ModKey;

	UI_DF_NavInit( &s_dfmod.menu, s_dfmod.nav, UI_DF_ModEvent );

	s_dfmod.pageSpin.generic.type		= MTYPE_SPINCONTROL;
	s_dfmod.pageSpin.generic.name		= def->spinLabel;
	s_dfmod.pageSpin.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_dfmod.pageSpin.generic.id			= ID_MOD_PAGE;
	s_dfmod.pageSpin.generic.x			= 350;
	s_dfmod.pageSpin.generic.y			= 10;
	s_dfmod.pageSpin.generic.callback	= UI_DF_ModEvent;
	s_dfmod.pageSpin.generic.statusbar	= UI_DF_ModStatus;
	s_dfmod.pageSpin.itemnames			= def->pageNames;
	Menu_AddItem( &s_dfmod.menu, &s_dfmod.pageSpin );

	for ( i = 0; i < s_dfmod.def->numItems; i++ ) {
		const uiDfModItem_t	*it = &s_dfmod.def->items[i];
		uiDfModWidget_t		*w = &s_dfmod.w[i];

		switch ( it->kind ) {
		case 0:
			w->text.generic.type	= MTYPE_TEXT;
			w->text.generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
			w->text.string			= (char *)it->label;
			w->text.style			= 0;
			w->text.color			= uiDfColorMedium;
			break;
		case 1:
			w->field.generic.type	= MTYPE_FIELD;
			w->field.generic.flags	= QMF_SMALLFONT;
			w->field.field.widthInChars = it->width;
			w->field.field.maxchars	= it->maxchars;
			break;
		default:
			w->list.generic.type	= MTYPE_SPINCONTROL;
			w->list.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
			w->list.itemnames		= it->items;
			break;
		}
		w->text.generic.name		= it->kind ? it->label : 0;
		w->text.generic.id			= it->id;
		w->text.generic.x			= it->x;
		w->text.generic.y			= it->y;
		if ( it->kind ) {
			w->text.generic.callback	= UI_DF_ModEvent;
			w->text.generic.statusbar	= UI_DF_ModStatus;
		}
		Menu_AddItem( &s_dfmod.menu, w );
	}

	// 0xb4ca: hide the pages, load them, write them back once, load again
	UI_DF_ModShowPage( 0 );
	UI_DF_ModRefreshAll();
	UI_DF_ModApplyAll();
	UI_DF_ModRefreshAll();
	UI_DF_ModShowPage( s_dfmod.def->show[0] );

	Menu_SetCursorToItem( &s_dfmod.menu, &s_dfmod.nav[def->navIndex] );
	UI_PushMenu( &s_dfmod.menu );
}

void UI_DF_Q3ExtrasMenu( void ) {
	UI_DF_OpenPage( &uiDfQ3Page );
}

void UI_DF_MpMenu( void ) {
	UI_DF_OpenPage( &uiDfMpPage );
}

void UI_DF_MiscMenu( void ) {
	UI_DF_OpenPage( &uiDfMiscPage );
}

void UI_DF_DemosMenu( void ) {
	UI_DF_OpenPage( &uiDfDemosPage );
}

void UI_DF_GhostMenu( void ) {
	UI_DF_OpenPage( &uiDfGhostPage );
}

void UI_DF_ChsMenu( void ) {
	UI_DF_OpenPage( &uiDfChsPage );
}

void UI_DF_HudMenu( void ) {
	UI_DF_OpenPage( &uiDfHudPage );
}
