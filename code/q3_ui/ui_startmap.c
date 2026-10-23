// ui_startmap.c -- DeFRaG start-map menu (ui 0x3825 and the 0x222d..0x36fc cluster), see disasm/UI/start_map_menu.md.
// Map picker grid (50 buttons / page) over the map databases of ui_dfmaps.c, option panels
// (Map Info / Timer / Gameplay / Weapons) writing the server cvars, and the run-server handler.

#include "ui_local.h"

const struct uiDfMapEntry_s *UI_DF_MapNext( int type, int maskA, int maskB, const struct uiDfMapEntry_s *prev );

// view of the 104-byte entry of ui_dfmaps.c
typedef struct dfEntry_s {
	char				map[32];
	char				longname[32];
	char				author[32];
	int					flags;
	struct dfEntry_s	*next;
} dfEntry_t;

#define DFS_PAGE		50
#define DFS_ROWS		25
#define DFS_MAXPANEL	4
#define DFS_PANELITEMS	16
#define DFS_ALLFLAGS	0x7fffffff

#define ID_BUTTONBASE	0		// map button ids: idx + 50 * page (as DeFRaG)
#define ID_PREV			1
#define ID_NEXT			2
#define ID_DBSPIN		3
#define ID_FILTER		4
#define ID_GO			1
#define ID_BACK			2
#define ID_OPTIONS		100
#define ID_PANEL		200		// + panel index
#define ID_WNONE		300
#define ID_WALL			301
#define ID_TIMER		400		// + group 0..2

#define GO_ART0			"menu/art/fight_0"
#define GO_ART1			"menu/art/fight_1"
#define BACK_ART0		"menu/art/back_0"
#define BACK_ART1		"menu/art/back_1"
#define ARROWS_0		"menu/art/gs_arrows_0"
#define ARROWS_L		"menu/art/gs_arrows_l"
#define ARROWS_R		"menu/art/gs_arrows_r"

enum { PK_MAPINFO, PK_TIMER, PK_GAMEPLAY, PK_WEAPONS };

typedef struct {
	int				kind;
	const char		*title;
	menucommon_s	*items[DFS_PANELITEMS];
	int				count;
} dfPanel_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		map[DFS_PAGE];
	menubitmap_s	arrowBg, prev, next;
	menulist_s		dbSpin;
	menuradiobutton_s	rVq3, rCpm;
	menulist_s		typeSpin;
	menutext_s		title, info;
	menubitmap_s	preview;
	menubitmap_s	back, go;
	menulist_s		optionsSpin;

	// panels
	menulist_s		spin[24];
	int				nspin;
	menuradiobutton_s	check[10];
	menutext_s		wnone, wall;
	menulist_s		timer[9];
	menubitmap_s	previewDummy;
	dfPanel_t		panel[DFS_MAXPANEL];
	int				npanel;
	int				curPanel;
	const char		*panelTitles[DFS_MAXPANEL + 1];

	// state
	int				gameType;		// 1 vq3/cpm run, 2 tricks, 3 fastcaps (625328)
	int				fixedMask;		// 629396
	int				dbType;			// 629004
	int				dbTypes[4];		// 628964
	const char		*dbNames[5];	// 628980
	int				ndb;
	const dfEntry_t	*pageHead[128];	// 629252
	int				npages;
	int				page;			// 629384
	int				total;			// 629216
	int				selIndex;		// 629212
	const dfEntry_t	*sel;			// 629208
	const dfEntry_t	*slot[DFS_PAGE];// 629008
	int				maskA, maskB;	// 629388 / 629392
	char			titleBuf[16];	// 629220
	char			infoBuf[48];	// 629236
	int				cheats;			// 634032
	int				runServer;		// 634028
	char			nameBuf[DFS_PAGE][34];
	char			timerShadow[3];
	const dfEntry_t	*previewEntry;	// 634024
	qhandle_t		previewShader;
} dfStart_t;

static dfStart_t	s_ds;

static vec4_t	dfsColorNormal = { 1.0f, 0.43f, 0.0f, 1.0f };	// data 10772 / 10788
static vec4_t	dfsColorHover = { 0.0f, 1.0f, 0.0f, 1.0f };	// data 15048
static vec4_t	dfsColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };	// data 15128

static const char *dfsListOffOn[] = { "off", "on", 0 };
static const char *dfsListOnOff[] = { "on", "off", 0 };
static const char *dfsListRun[] = { "no", "listen", "local", "public", 0 };
static const char *dfsListMove[] = { "VQ3", "CPM", 0 };
static const char *dfsListOnOffKill[] = { "on", "off", "kill", 0 };
static const char *dfsListItems[] = { "off", "selective", "on", 0 };
static const char *dfsListHook[] = { "Quake3", "Swinging", "Pendulum", "Quake2", 0 };
static const char *dfsListNum8[] = { "0", "1", "2", "3", "4", "5", "6", "7", 0 };
static const char *dfsListType[] = { "any", "run", "training", "level", "accuracy", 0 };
static const char *dfsListT1[] = { "none", "Defrag Targets", "Item", "Respawn", 0 };
static const char *dfsListT2[] = { "none", "Defrag Targets", "Item", 0 };
static const char *dfsListGroup[] = { "Weapons", "Ammo", "Powerups", "Armor", "Health", "Holdables", 0 };
static const char *dfsListG0[] = { "Machinegun", "Shotgun", "Grenade Launcher", "Rocket Launcher", "Lightning", "Railgun", "Plasmagun", "BFG", "Hook", 0 };
static const char *dfsListG1[] = { "Machinegun", "Shotgun", "Grenade Launcher", "Rocket Launcher", "Lightning", "Railgun", "Plasmagun", "BFG", 0 };
static const char *dfsListG2[] = { "Quad", "Battle Suit", "Haste", "Invisibility", "Regen", "Flight", 0 };
static const char *dfsListG3[] = { "Armor Shard", "Yellow Armor", "Red Armor", 0 };
static const char *dfsListG4[] = { "5 health", "25 health", "50 health", "Mega Health", 0 };
static const char *dfsListG5[] = { "Teleporter", "Medkit", 0 };
static const char **dfsGroupLists[6] = { dfsListG0, dfsListG1, dfsListG2, dfsListG3, dfsListG4, dfsListG5 };
// ui data 1460: cvar token of group*10 + item
static const char *dfsTimerTokens[60] = {
	"mg", "sg", "gl", "rl", "lg", "rg", "pg", "bfg", "hook", 0,
	"amg", "asg", "agl", "arl", "alg", "arg", "apg", "abfg", 0, 0,
	"quad", "bs", "hast", "inv", "reg", "fli", 0, 0, 0, 0,
	"as", "ya", "ra", 0, 0, 0, 0, 0, 0, 0,
	"h5", "h25", "h50", "mh", 0, 0, 0, 0, 0, 0,
	"tp", "med", 0, 0, 0, 0, 0, 0, 0, 0
};
static const char *dfsWeaponNames[11] = { 0, "Gauntlet", "Machinegun", "Shotgun", "Grenade Launcher", "Rocket Launcher", "Lightning Gun", "Railgun", "Plasma Gun", "BFG", "Hook" };

static int DFS_Count( const char **list ) {
	int n = 0;
	while ( list[n] ) {
		n++;
	}
	return n;
}

static int DFS_ClampCvar( const char *name, int max ) {
	int v = (int)trap_Cvar_VariableValue( name );
	if ( v < 0 ) {
		v = 0;
	}
	if ( v > max ) {
		v = max;
	}
	return v;
}

static void DFS_SetInt( const char *cvar, int v ) {
	trap_Cvar_Set( cvar, va( "%i", v ) );
}

// ---------------------------------------------------------------------------------------------------------------
// picker (0x222d / 0x233c / 0x246d / 0x25c0 / 0x2551)

static int DFS_TextWidth( const char *s ) {		// 0xc527 (proportional width)
	return UI_ProportionalStringWidth( s );
}

static void DFS_FillPage( int page ) {			// 0x222d
	const dfEntry_t *e;
	int i;

	if ( page == 0 ) {
		s_ds.prev.generic.flags |= QMF_GRAYED;
	} else {
		s_ds.prev.generic.flags &= ~QMF_GRAYED;
	}
	if ( page + 1 >= s_ds.npages ) {
		s_ds.next.generic.flags |= QMF_GRAYED;
	} else {
		s_ds.next.generic.flags &= ~QMF_GRAYED;
	}
	e = s_ds.pageHead[page];
	for ( i = 0; i < DFS_PAGE; i++ ) {
		menutext_s *t = &s_ds.map[i];

		s_ds.slot[i] = e;
		t->generic.id = i + DFS_PAGE * page;
		if ( e ) {
			int w;

			Q_strncpyz( s_ds.nameBuf[i], e->map, sizeof( s_ds.nameBuf[i] ) );
			t->string = s_ds.nameBuf[i];
			t->generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
			w = (int)( DFS_TextWidth( t->string ) * PROP_SMALL_SIZE_SCALE );
			t->generic.left = t->generic.x;
			t->generic.right = t->generic.x + w;
			t->generic.top = t->generic.y;
			t->generic.bottom = t->generic.y + 13;
			t->color = dfsColorNormal;
			e = (const dfEntry_t *)UI_DF_MapNext( s_ds.dbType, s_ds.maskA, s_ds.maskB, (const struct uiDfMapEntry_s *)e );
		} else {
			s_ds.nameBuf[i][0] = 0;
			t->string = s_ds.nameBuf[i];
			t->generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
		}
	}
	s_ds.page = page;
}

static void DFS_SetPreview( const dfEntry_t *e ) {	// 0x2223 + preview shader reset
	s_ds.previewEntry = e;
	s_ds.previewShader = 0;
}

static void DFS_Select( int index ) {			// 0x233c
	int old, i;
	const dfEntry_t *e;

	if ( s_ds.total == 0 ) {
		Q_strncpyz( s_ds.titleBuf, "no map", sizeof( s_ds.titleBuf ) );
		s_ds.infoBuf[0] = 0;
		s_ds.sel = NULL;
		DFS_SetPreview( NULL );
		return;
	}
	if ( index < 0 ) {
		index = 0;
	} else if ( index >= s_ds.total ) {
		index = s_ds.total - 1;
	}
	if ( index / DFS_PAGE != s_ds.page ) {
		DFS_FillPage( index / DFS_PAGE );
	}
	old = s_ds.selIndex % DFS_PAGE;
	s_ds.map[old].generic.flags &= ~QMF_INACTIVE;
	s_ds.map[old].color = ( s_ds.map[old].generic.flags & QMF_HASMOUSEFOCUS ) ? dfsColorHover : dfsColorNormal;
	i = index % DFS_PAGE;
	s_ds.map[i].generic.flags |= QMF_INACTIVE;
	s_ds.map[i].color = dfsColorNormal;
	s_ds.selIndex = index;
	e = s_ds.slot[i];
	s_ds.sel = e;
	DFS_SetPreview( e );
	Com_sprintf( s_ds.titleBuf, sizeof( s_ds.titleBuf ), "%3i / %3i", index + 1, s_ds.total );
	if ( e && s_ds.dbType == 1 ) {
		const char *mode, *typ;

		if ( ( e->flags & 3 ) == 3 ) {
			mode = "VQ3/CPM ";
		} else if ( e->flags & 2 ) {
			mode = "CPM ";
		} else {
			mode = "VQ3 ";
		}
		if ( e->flags & 4 ) {
			typ = "Training";
		} else if ( e->flags & 16 ) {
			typ = "Level";
		} else if ( e->flags & 32 ) {
			typ = "Accuracy";
		} else {
			typ = "Run";
		}
		Com_sprintf( s_ds.infoBuf, sizeof( s_ds.infoBuf ), "%s%s", mode, typ );
	} else {
		Q_strncpyz( s_ds.infoBuf, "Quake 3", sizeof( s_ds.infoBuf ) );
	}
}

static void DFS_TypeAhead( int ch ) {			// 0x246d
	const dfEntry_t *e = s_ds.sel;
	int i, found = -1, c = tolower( ch );

	if ( !( ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ( ch >= '0' && ch <= '9' ) ) ) {
		return;
	}
	for ( i = 1; i <= s_ds.total; i++ ) {
		e = (const dfEntry_t *)UI_DF_MapNext( s_ds.dbType, s_ds.maskA, s_ds.maskB, (const struct uiDfMapEntry_s *)e );
		if ( !e ) {
			e = (const dfEntry_t *)UI_DF_MapNext( s_ds.dbType, s_ds.maskA, s_ds.maskB, NULL );
		}
		if ( e && tolower( e->map[0] ) == c ) {
			found = i;
			break;
		}
	}
	if ( found >= 0 ) {
		DFS_Select( ( found + s_ds.selIndex ) % s_ds.total );
	} else {
		DFS_Select( 0 );
	}
}

static void DFS_SetFilters( void ) {			// 0x25c0
	if ( s_ds.fixedMask ) {
		s_ds.maskA = s_ds.fixedMask;
		s_ds.maskB = DFS_ALLFLAGS;
		return;
	}
	s_ds.maskA = DFS_ALLFLAGS;
	s_ds.maskB = DFS_ALLFLAGS;
	if ( s_ds.dbType == 1 ) {
		s_ds.maskA = 0;
		s_ds.maskB = 0;
		if ( s_ds.rVq3.curvalue ) {
			s_ds.maskA |= 1;
		}
		if ( s_ds.rCpm.curvalue ) {
			s_ds.maskA |= 2;
		}
		switch ( s_ds.typeSpin.curvalue ) {
		case 1: s_ds.maskB |= 8; break;
		case 2: s_ds.maskB |= 4; break;
		case 3: s_ds.maskB |= 16; break;
		case 4: s_ds.maskB |= 32; break;
		default: s_ds.maskB |= 60; break;
		}
	}
}

static void DFS_Rebuild( void ) {				// 0x2551
	const dfEntry_t *e;
	int n = 0;

	Com_Memset( s_ds.pageHead, 0, sizeof( s_ds.pageHead ) );
	s_ds.npages = 0;
	e = (const dfEntry_t *)UI_DF_MapNext( s_ds.dbType, s_ds.maskA, s_ds.maskB, NULL );
	while ( e ) {
		if ( n % DFS_PAGE == 0 && s_ds.npages < 128 ) {
			s_ds.pageHead[s_ds.npages++] = e;
		}
		n++;
		e = (const dfEntry_t *)UI_DF_MapNext( s_ds.dbType, s_ds.maskA, s_ds.maskB, (const struct uiDfMapEntry_s *)e );
	}
	s_ds.total = n;
	s_ds.selIndex = 0;
	s_ds.sel = NULL;
	if ( s_ds.npages == 0 ) {
		int i;
		for ( i = 0; i < DFS_PAGE; i++ ) {
			s_ds.slot[i] = NULL;
			s_ds.map[i].generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
		}
		s_ds.prev.generic.flags |= QMF_GRAYED;
		s_ds.next.generic.flags |= QMF_GRAYED;
		s_ds.page = 0;
	} else {
		DFS_FillPage( 0 );
	}
	DFS_Select( 0 );
}

static void DFS_SetDb( int type ) {				// 0x2651
	int i;

	s_ds.dbType = type;
	if ( type == 1 ) {
		s_ds.rVq3.generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
		s_ds.rCpm.generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
		s_ds.typeSpin.generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
	} else {
		s_ds.rVq3.generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
		s_ds.rCpm.generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
		s_ds.typeSpin.generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
	}
	for ( i = 0; i < s_ds.ndb; i++ ) {
		if ( s_ds.dbTypes[i] == type ) {
			s_ds.dbSpin.curvalue = i;
			break;
		}
	}
}

static void DFS_PickerEvent( void *ptr, int event ) {	// 0x2701 / 0x2751
	menucommon_s *c = (menucommon_s *)ptr;
	menutext_s *t = (menutext_s *)ptr;

	if ( c->type == MTYPE_PTEXT ) {
		if ( event == QM_ACTIVATED ) {
			DFS_Select( c->id );
		} else if ( event == 101 ) {
			if ( t->color == dfsColorNormal && !( c->flags & QMF_INACTIVE ) ) {
				t->color = dfsColorHover;
			}
		} else if ( event == 102 ) {
			if ( t->color == dfsColorHover ) {
				t->color = dfsColorNormal;
			}
		}
		return;
	}
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( c->id ) {
	case ID_PREV:
		DFS_Select( s_ds.selIndex - DFS_PAGE );
		break;
	case ID_NEXT:
		DFS_Select( s_ds.selIndex + DFS_PAGE );
		break;
	case ID_DBSPIN:
		DFS_SetDb( s_ds.dbTypes[s_ds.dbSpin.curvalue] );
		DFS_SetFilters();
		DFS_Rebuild();
		break;
	case ID_FILTER:
		DFS_SetFilters();
		DFS_Rebuild();
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// preview (ui 0x20d6)

static void DFS_PreviewDraw( void *ptr ) {
	menubitmap_s *b = (menubitmap_s *)ptr;
	const dfEntry_t *e = s_ds.previewEntry;
	float x = b->generic.x, y = b->generic.y, w = b->width, h = b->height;
	int ty;

	if ( !s_ds.previewShader && e ) {
		s_ds.previewShader = trap_R_RegisterShaderNoMip( va( "levelshots/%s", e->map ) );
	}
	if ( !s_ds.previewShader ) {
		s_ds.previewShader = trap_R_RegisterShaderNoMip( "menu/art/unknownmap" );
	}
	UI_FillRect( x - w / 2 - 1, y - 1, w + 2, h + 2, dfsColorWhite );
	UI_DrawHandlePic( x - w / 2, y, w, h, s_ds.previewShader );
	ty = y + h + 6;
	if ( e && e->longname[0] ) {
		UI_DrawString( (int)x, ty, e->longname, UI_CENTER | UI_SMALLFONT, dfsColorWhite );
		ty += 16;
	}
	if ( e && e->author[0] ) {
		UI_DrawString( (int)x, ty, va( "by %s", e->author ), UI_CENTER | UI_SMALLFONT, dfsColorWhite );
	}
}

// ---------------------------------------------------------------------------------------------------------------
// panels

static void DFS_ShowPanel( int idx ) {			// 0xedc / 0xe0a
	int p, i;

	if ( idx < 0 || idx >= s_ds.npanel ) {
		return;
	}
	for ( p = 0; p < s_ds.npanel; p++ ) {
		for ( i = 0; i < s_ds.panel[p].count; i++ ) {
			if ( p == idx ) {
				s_ds.panel[p].items[i]->flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
			} else {
				s_ds.panel[p].items[i]->flags |= QMF_INACTIVE | QMF_HIDDEN;
			}
		}
	}
	s_ds.curPanel = idx;
	if ( s_ds.panel[idx].kind == PK_TIMER ) {
		for ( i = 0; i < 3; i++ ) {
			int on = s_ds.timer[i * 3].curvalue == 2;

			if ( !on ) {
				s_ds.timer[i * 3 + 1].generic.flags |= QMF_GRAYED;
				s_ds.timer[i * 3 + 2].generic.flags |= QMF_GRAYED;
			} else {
				s_ds.timer[i * 3 + 1].generic.flags &= ~QMF_GRAYED;
				s_ds.timer[i * 3 + 2].generic.flags &= ~QMF_GRAYED;
			}
		}
	}
}

static dfPanel_t *DFS_NewPanel( int kind, const char *title ) {
	dfPanel_t *p = &s_ds.panel[s_ds.npanel];

	p->kind = kind;
	p->title = title;
	p->count = 0;
	s_ds.panelTitles[s_ds.npanel] = title;
	s_ds.npanel++;
	return p;
}

static void DFS_PanelAdd( dfPanel_t *p, void *item ) {		// 0xe52
	menucommon_s *c = (menucommon_s *)item;

	if ( p->count >= DFS_PANELITEMS ) {
		return;
	}
	p->items[p->count++] = c;
	c->flags |= QMF_INACTIVE | QMF_HIDDEN;
	Menu_AddItem( &s_ds.menu, item );
}

static menulist_s *DFS_AddSpin( dfPanel_t *p, const char *label, int id, int x, int y, const char **list, int cur ) {
	menulist_s *s = &s_ds.spin[s_ds.nspin++];

	s->generic.type = MTYPE_SPINCONTROL;
	s->generic.name = label;
	s->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.callback = NULL;	// set by caller
	s->generic.id = id;
	s->generic.x = x;
	s->generic.y = y;
	s->itemnames = list;
	s->numitems = DFS_Count( list );
	s->curvalue = cur;
	DFS_PanelAdd( p, s );
	return s;
}

static void DFS_ApplyMapInfo( void ) {			// 0x30aa / 0x349a / 0x32da (items[0] is the preview)
	menucommon_s **it = s_ds.panel[0].items;
	menulist_s *w0 = (menulist_s *)it[1], *w1 = (menulist_s *)it[2];
	menulist_s *ch = (menulist_s *)it[3], *run = (menulist_s *)it[4];

	if ( s_ds.gameType == 1 ) {
		DFS_SetInt( "df_promode", w0->curvalue );
		DFS_SetInt( "df_ob_KillOBs", w1->curvalue == 0 ? 1 : 0 );
	} else {
		DFS_SetInt( s_ds.gameType == 3 ? "df_mode_fastcaps" : "df_mode_tricksmode", w0->curvalue );
		DFS_SetInt( "df_promode", w1->curvalue );
	}
	s_ds.cheats = ch->curvalue;
	s_ds.runServer = run->curvalue;
}

static dfPanel_t *DFS_FindPanel( int kind ) {
	int i;
	for ( i = 0; i < s_ds.npanel; i++ ) {
		if ( s_ds.panel[i].kind == kind ) {
			return &s_ds.panel[i];
		}
	}
	return NULL;
}

static void DFS_ApplyGameplay( void ) {			// 0x299c
	dfPanel_t *p = DFS_FindPanel( PK_GAMEPLAY );
	menulist_s **w;

	if ( !p ) {
		return;
	}
	w = (menulist_s **)p->items;
	DFS_SetInt( "df_ndm_damage", w[0]->curvalue );
	DFS_SetInt( "df_ndm_powerups", w[1]->curvalue );
	DFS_SetInt( "df_ndm_nopad", w[2]->curvalue );
	DFS_SetInt( "df_ndm_noteleport", w[3]->curvalue );
	DFS_SetInt( "df_ndm_nomover", w[4]->curvalue );
	DFS_SetInt( "df_ob_KillOBs", w[5]->curvalue == 0 ? 1 : 0 );
	DFS_SetInt( "df_ndm_itemsSpawn", w[6]->curvalue );
}

static void DFS_ApplyWeapons( void ) {			// 0x2cd1
	dfPanel_t *p = DFS_FindPanel( PK_WEAPONS );
	menulist_s **w;
	int i, mask = 0;

	if ( !p ) {
		return;
	}
	w = (menulist_s **)p->items;
	DFS_SetInt( "df_ndm_weaponsPickup", w[0]->curvalue );
	DFS_SetInt( "df_lg_Knockback", w[1]->curvalue );
	DFS_SetInt( "df_hook_Type", w[2]->curvalue );
	for ( i = 1; i <= 10; i++ ) {
		if ( s_ds.check[i - 1].curvalue ) {
			mask |= 1 << i;
		}
	}
	DFS_SetInt( "df_ndm_weaponsSet", mask );
}

static void DFS_TimerWrite( int g ) {			// 0xf1a
	menulist_s *t = &s_ds.timer[g * 3];
	static const char *cv[3] = { "df_ndm_timer_start", "df_ndm_timer_checkpoint", "df_ndm_timer_stop" };
	const char *val = "";

	if ( t[1].curvalue != s_ds.timerShadow[g] ) {	// item type group changed: reload the item list
		t[2].itemnames = dfsGroupLists[t[1].curvalue];
		t[2].numitems = DFS_Count( t[2].itemnames );
		t[2].curvalue = 0;
		s_ds.timerShadow[g] = t[1].curvalue;
	}
	switch ( t[0].curvalue ) {
	case 1: val = "df"; break;
	case 2: val = dfsTimerTokens[t[1].curvalue * 10 + t[2].curvalue]; if ( !val ) val = ""; break;
	case 3: val = "resp"; break;
	}
	trap_Cvar_Set( cv[g], val );
	DFS_ShowPanel( s_ds.curPanel );		// refresh graying
}

static void DFS_PanelEvent( void *ptr, int event ) {
	menucommon_s *c = (menucommon_s *)ptr;
	int i;

	if ( event == 101 || event == 102 ) {
		return;
	}
	if ( c->type == MTYPE_PTEXT || c->type == MTYPE_TEXT ) {
		if ( event == QM_GOTFOCUS || event == QM_LOSTFOCUS ) {
			return;
		}
	}
	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( c->id >= ID_TIMER && c->id < ID_TIMER + 3 ) {
		DFS_TimerWrite( c->id - ID_TIMER );
		return;
	}
	if ( c->id == ID_WNONE || c->id == ID_WALL ) {
		for ( i = 0; i < 10; i++ ) {
			s_ds.check[i].curvalue = ( c->id == ID_WALL );
		}
		DFS_ApplyWeapons();
		return;
	}
	switch ( s_ds.panel[c->id - ID_PANEL].kind ) {
	case PK_MAPINFO: DFS_ApplyMapInfo(); break;
	case PK_GAMEPLAY: DFS_ApplyGameplay(); break;
	case PK_WEAPONS: DFS_ApplyWeapons(); break;
	}
}

static menulist_s *DFS_PSpin( dfPanel_t *p, const char *label, int x, int y, const char **list, int cur ) {
	menulist_s *s = DFS_AddSpin( p, label, ID_PANEL + ( p - s_ds.panel ), x, y, list, cur );
	s->generic.callback = DFS_PanelEvent;
	return s;
}

// ui 0x32b9 (statusbar of the Game Mode spinner, table at data 0xc6c): while the spinner has focus its meaning is written at y 362
static const char *dfsModeCaptions[8] = {
	"(custom settings)", "(no weapon / no map object)", "(weapons / map objects)", "(no weapon / map objects)",
	"(weapons / no map object)", "(swinging hook)", "(quake3 hook)", "(original quake 3)"
};

static void DFS_ModeStatus( void *ptr ) {
	menulist_s *s = (menulist_s *)ptr;

	UI_DrawString( s->generic.x, 362, dfsModeCaptions[s->curvalue & 7], UI_CENTER|UI_SMALLFONT, color_white );
}

static void DFS_BuildMapInfo( int x, int y, int w ) {		// 0x30e9 / 0x34cd / 0x330d
	dfPanel_t *p = DFS_NewPanel( PK_MAPINFO, "Map Info" );
	int sx = x + w / 2 + 25;

	// preview (0x21f0)
	s_ds.preview.generic.type = MTYPE_BITMAP;
	s_ds.preview.generic.flags = QMF_INACTIVE | QMF_OWNERDRAW;
	s_ds.preview.generic.x = x + w / 2;
	s_ds.preview.generic.y = y;
	s_ds.preview.generic.ownerdraw = DFS_PreviewDraw;
	s_ds.preview.width = 200;
	s_ds.preview.height = 150;
	DFS_PanelAdd( p, &s_ds.preview );
	y += 230;

	if ( s_ds.gameType == 1 ) {
		DFS_PSpin( p, "Movements:", sx, y, dfsListMove, DFS_ClampCvar( "df_promode", 1 ) ); y += 20;
		DFS_PSpin( p, "Overbounces:", sx, y, dfsListOffOn, trap_Cvar_VariableValue( "df_ob_KillOBs" ) == 0 ? 1 : 0 ); y += 20;
	} else {
		DFS_PSpin( p, "Game Mode:", sx, y, dfsListNum8, DFS_ClampCvar( s_ds.gameType == 3 ? "df_mode_fastcaps" : "df_mode_tricksmode", 7 ) )->generic.statusbar = DFS_ModeStatus;
		y += 20;
		DFS_PSpin( p, "Movements:", sx, y, dfsListMove, trap_Cvar_VariableValue( "df_promode" ) != 0 ? 1 : 0 ); y += 20;
	}
	DFS_PSpin( p, "Server Cheats:", sx, y, dfsListOffOn, s_ds.cheats ? 1 : 0 ); y += 20;
	DFS_PSpin( p, "Run Server:", sx, y, dfsListRun, s_ds.runServer ); y += 20;
}

static void DFS_BuildTimer( int x, int y, int w ) {			// 0x326f / 0x12e5 / 0x1147
	dfPanel_t *p = DFS_NewPanel( PK_TIMER, "Timer" );
	static const char *labels[3] = { "Timer Start:", "Checkpoint:", "Timer Stop:" };
	static const char *cv[3] = { "df_ndm_timer_start", "df_ndm_timer_checkpoint", "df_ndm_timer_stop" };
	int sx = x + w / 2 + 25 - 10;
	int g, k;

	y += 90;
	for ( g = 0; g < 3; g++ ) {
		menulist_s *t = &s_ds.timer[g * 3];
		char cur[256];
		int type = 0, group = 0, item = 0;

		trap_Cvar_VariableStringBuffer( cv[g], cur, sizeof( cur ) );
		if ( !cur[0] ) {
			type = 0;
		} else if ( !Q_stricmp( cur, "df" ) ) {
			type = 1;
		} else if ( !Q_stricmp( cur, "resp" ) ) {
			type = g == 0 ? 3 : 0;
		} else {
			for ( k = 0; k < 60; k++ ) {
				if ( dfsTimerTokens[k] && !Q_stricmp( dfsTimerTokens[k], cur ) ) {
					type = 2; group = k / 10; item = k % 10;
					break;
				}
			}
		}
		for ( k = 0; k < 3; k++ ) {
			menulist_s *s = &t[k];
			static const char *lbl[3] = { 0, "Item Type:", "Item:" };

			s->generic.type = MTYPE_SPINCONTROL;
			s->generic.name = k == 0 ? labels[g] : lbl[k];
			s->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
			s->generic.callback = DFS_PanelEvent;
			s->generic.id = ID_TIMER + g;
			s->generic.x = sx;
			s->generic.y = y + 20 * k;
			DFS_PanelAdd( p, s );
		}
		t[0].itemnames = g == 0 ? dfsListT1 : dfsListT2;
		t[1].itemnames = dfsListGroup;
		t[2].itemnames = dfsGroupLists[group];
		for ( k = 0; k < 3; k++ ) {
			t[k].numitems = DFS_Count( t[k].itemnames );
		}
		t[0].curvalue = type;
		t[1].curvalue = group;
		t[2].curvalue = item;
		s_ds.timerShadow[g] = group;
		y += 80;
	}
}

static void DFS_BuildGameplay( int x, int y, int w ) {		// 0x2a28
	dfPanel_t *p = DFS_NewPanel( PK_GAMEPLAY, "Gameplay" );
	int sx = x + w / 2 + 25;

	y += 130;
	DFS_PSpin( p, "Damage:", sx, y, dfsListOffOn, DFS_ClampCvar( "df_ndm_damage", 1 ) ); y += 20;
	DFS_PSpin( p, "Powerups:", sx, y, dfsListOffOn, DFS_ClampCvar( "df_ndm_powerups", 1 ) ); y += 20;
	DFS_PSpin( p, "Jump-pads:", sx, y, dfsListOnOffKill, DFS_ClampCvar( "df_ndm_nopad", 2 ) ); y += 20;
	DFS_PSpin( p, "Teleporters:", sx, y, dfsListOnOffKill, DFS_ClampCvar( "df_ndm_noteleport", 2 ) ); y += 20;
	DFS_PSpin( p, "Movers:", sx, y, dfsListOnOff, DFS_ClampCvar( "df_ndm_nomover", 1 ) ); y += 40;
	DFS_PSpin( p, "OverBounces:", sx, y, dfsListOffOn, trap_Cvar_VariableValue( "df_ob_KillOBs" ) == 0 ? 1 : 0 ); y += 20;
	DFS_PSpin( p, "Items spawn:", sx, y, dfsListItems, DFS_ClampCvar( "df_ndm_itemsSpawn", 2 ) );
}

static void DFS_BuildWeapons( int x, int y, int w ) {		// 0x2db3
	dfPanel_t *p = DFS_NewPanel( PK_WEAPONS, "Weapons" );
	int sx = x + w / 2 + 25;
	int i, set;

	y += 50;
	DFS_PSpin( p, "Weapons pickup:", sx, y, dfsListOffOn, DFS_ClampCvar( "df_ndm_weaponsPickup", 1 ) ); y += 20;
	DFS_PSpin( p, "LG knockback:", sx, y, dfsListOffOn, DFS_ClampCvar( "df_lg_Knockback", 1 ) ); y += 20;
	DFS_PSpin( p, "Hook type:", sx, y, dfsListHook, DFS_ClampCvar( "df_hook_Type", 3 ) ); y += 40;
	set = (int)trap_Cvar_VariableValue( "df_ndm_weaponsSet" );
	for ( i = 1; i <= 10; i++ ) {
		menuradiobutton_s *c = &s_ds.check[i - 1];

		c->generic.type = MTYPE_RADIOBUTTON;
		c->generic.name = dfsWeaponNames[i];
		c->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
		c->generic.callback = DFS_PanelEvent;
		c->generic.id = ID_PANEL + ( p - s_ds.panel );
		c->generic.x = sx;
		c->generic.y = y;
		c->curvalue = ( set & ( 1 << i ) ) != 0;
		DFS_PanelAdd( p, c );
		y += 20;
	}
	y += 2;
	s_ds.wnone.generic.type = MTYPE_PTEXT;
	s_ds.wnone.generic.flags = QMF_PULSEIFFOCUS | QMF_RIGHT_JUSTIFY;
	s_ds.wnone.generic.callback = DFS_PanelEvent;
	s_ds.wnone.generic.id = ID_WNONE;
	s_ds.wnone.generic.x = sx - 26;
	s_ds.wnone.generic.y = y;
	s_ds.wnone.string = "None";
	s_ds.wnone.style = UI_RIGHT | UI_SMALLFONT;
	s_ds.wnone.color = text_color_normal;
	DFS_PanelAdd( p, &s_ds.wnone );
	s_ds.wall.generic.type = MTYPE_PTEXT;
	s_ds.wall.generic.flags = QMF_PULSEIFFOCUS | QMF_LEFT_JUSTIFY;
	s_ds.wall.generic.callback = DFS_PanelEvent;
	s_ds.wall.generic.id = ID_WALL;
	s_ds.wall.generic.x = sx + 26;
	s_ds.wall.generic.y = y;
	s_ds.wall.string = "All";
	s_ds.wall.style = UI_LEFT | UI_SMALLFONT;
	s_ds.wall.color = text_color_normal;
	DFS_PanelAdd( p, &s_ds.wall );
}

// ---------------------------------------------------------------------------------------------------------------
// run server (0x365a) and keys (0x36fc)

static void DFS_ButtonEvent( void *ptr, int event ) {
	menucommon_s *c = (menucommon_s *)ptr;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( c->id ) {
	case ID_GO:
		if ( s_ds.sel ) {
			int type = s_ds.gameType;

			if ( s_ds.runServer ) {
				type += 4;
			}
			trap_Cvar_Set( "df_gametype", va( "%i", type ) );
			trap_Cvar_Set( "sv_pure", "0" );
			if ( s_ds.runServer >= 1 ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "dedicated %i\n", s_ds.runServer - 1 ) );
			}
			trap_Cmd_ExecuteText( EXEC_APPEND, va( s_ds.cheats ? "wait ; wait ; devmap %s\n" : "wait ; wait ; map %s\n", s_ds.sel->map ) );
		}
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_OPTIONS:
		DFS_ShowPanel( s_ds.optionsSpin.curvalue );
		break;
	}
}

static sfxHandle_t DFS_Key( int key ) {
	int k = key & ~K_CHAR_FLAG;

	if ( key & K_CHAR_FLAG ) {
		DFS_TypeAhead( k );
		return menu_move_sound;
	}
	switch ( key ) {
	case K_UPARROW: DFS_Select( s_ds.selIndex - 1 ); return menu_move_sound;
	case K_DOWNARROW: DFS_Select( s_ds.selIndex + 1 ); return menu_move_sound;
	case K_LEFTARROW: DFS_Select( s_ds.selIndex - DFS_ROWS ); return menu_move_sound;
	case K_RIGHTARROW: DFS_Select( s_ds.selIndex + DFS_ROWS ); return menu_move_sound;
	case K_PGDN: DFS_Select( s_ds.selIndex + DFS_PAGE ); return menu_move_sound;
	case K_PGUP: DFS_Select( s_ds.selIndex - DFS_PAGE ); return menu_move_sound;
	case K_HOME: DFS_Select( 0 ); return menu_move_sound;
	case K_END: DFS_Select( s_ds.total - 1 ); return menu_move_sound;
	}
	return Menu_DefaultKey( &s_ds.menu, key );
}

// ---------------------------------------------------------------------------------------------------------------

static void DFS_InitBitmap( menubitmap_s *b, const char *name, const char *focus, int id, int x, int y, int w, int h, unsigned flags, void (*cb)( void *, int ) ) {
	Com_Memset( b, 0, sizeof( *b ) );
	b->generic.type = MTYPE_BITMAP;
	b->generic.name = name;
	b->generic.id = id;
	b->generic.x = x;
	b->generic.y = y;
	b->generic.flags = flags;
	b->generic.callback = cb;
	b->focuspic = (char *)focus;
	b->width = w;
	b->height = h;
}

/*
===============
UI_DF_StartMapMenu (ui 0x3825)

type: 1 = Defrag VQ3/CPM (DeFRaG maps), 2 = Tricks Mode, 3 = Fast Caps; variant: 0 = VQ3, 1 = CPM.
===============
*/
void UI_DF_StartMapMenu( int type, int variant ) {
	int i, x0 = 10, y0 = 10;
	int dbMask, firstDb;
	int px = 360, py = 10, pw = 270;

	Com_Memset( &s_ds, 0, sizeof( s_ds ) );
	s_ds.menu.wrapAround = qtrue;
	s_ds.menu.fullscreen = qtrue;
	s_ds.menu.key = DFS_Key;
	s_ds.gameType = type;
	s_ds.cheats = 0;
	s_ds.runServer = 0;

	trap_R_RegisterShaderNoMip( GO_ART0 );
	trap_R_RegisterShaderNoMip( GO_ART1 );
	trap_R_RegisterShaderNoMip( BACK_ART0 );
	trap_R_RegisterShaderNoMip( BACK_ART1 );
	trap_R_RegisterShaderNoMip( ARROWS_0 );
	trap_R_RegisterShaderNoMip( ARROWS_L );
	trap_R_RegisterShaderNoMip( ARROWS_R );

	// database list (0x26c0): type 1 -> defrag only, 3 -> quake3 (ctf) only, 2 -> both
	if ( type == 1 ) {
		dbMask = 1; firstDb = 1;
	} else if ( type == 3 ) {
		dbMask = 2; firstDb = 2; s_ds.fixedMask = UIDF_MAP_CTF;
	} else {
		dbMask = 3; firstDb = 2;
	}
	for ( i = 0; i < 2; i++ ) {
		if ( dbMask & ( 1 << i ) ) {
			s_ds.dbTypes[s_ds.ndb] = 1 << i;
			s_ds.dbNames[s_ds.ndb] = ( i == 0 ) ? "defrag" : "quake3";
			s_ds.ndb++;
		}
	}

	if ( type == 1 ) {
		trap_Cvar_Set( "df_promode", variant == 1 ? "1" : "0" );
	}

	// map buttons (0x279e)
	for ( i = 0; i < DFS_PAGE; i++ ) {
		menutext_s *t = &s_ds.map[i];

		t->generic.type = MTYPE_PTEXT;
		t->generic.flags = QMF_LEFT_JUSTIFY | QMF_SMALLFONT;
		t->generic.id = i;
		t->generic.x = x0 + 170 * ( i / DFS_ROWS );
		t->generic.y = y0 + 13 * ( i % DFS_ROWS );
		t->generic.callback = DFS_PickerEvent;
		t->string = s_ds.nameBuf[i];
		t->style = UI_SMALLFONT;
		t->color = dfsColorNormal;
		Menu_AddItem( &s_ds.menu, t );
	}
	DFS_InitBitmap( &s_ds.arrowBg, ARROWS_0, NULL, 0, x0 + 170 - 64, y0 + 340, 128, 16, QMF_INACTIVE, NULL );
	Menu_AddItem( &s_ds.menu, &s_ds.arrowBg );
	DFS_InitBitmap( &s_ds.prev, NULL, ARROWS_L, ID_PREV, x0 + 170 - 64, y0 + 340, 64, 16, QMF_PULSEIFFOCUS | QMF_LEFT_JUSTIFY, DFS_PickerEvent );
	Menu_AddItem( &s_ds.menu, &s_ds.prev );
	DFS_InitBitmap( &s_ds.next, NULL, ARROWS_R, ID_NEXT, x0 + 170 - 64 + 61, y0 + 340, 64, 16, QMF_PULSEIFFOCUS | QMF_LEFT_JUSTIFY, DFS_PickerEvent );
	Menu_AddItem( &s_ds.menu, &s_ds.next );

	if ( s_ds.ndb > 1 ) {
		s_ds.dbSpin.generic.type = MTYPE_SPINCONTROL;
		s_ds.dbSpin.generic.name = "Maps";
		s_ds.dbSpin.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
		s_ds.dbSpin.generic.id = ID_DBSPIN;
		s_ds.dbSpin.generic.x = x0 + 170;
		s_ds.dbSpin.generic.y = y0 + 340 + 16 + 12;
		s_ds.dbSpin.generic.callback = DFS_PickerEvent;
		s_ds.dbSpin.itemnames = s_ds.dbNames;
		s_ds.dbSpin.numitems = s_ds.ndb;
		Menu_AddItem( &s_ds.menu, &s_ds.dbSpin );
	}
	// VQ3 / CPM / type filters (only without a fixed mask)
	s_ds.rVq3.generic.type = MTYPE_RADIOBUTTON;
	s_ds.rVq3.generic.name = "VQ3 maps";
	s_ds.rVq3.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_ds.rVq3.generic.id = ID_FILTER;
	s_ds.rVq3.generic.x = x0 + 170;
	s_ds.rVq3.generic.y = y0 + 340 + 16 + 12 + 24;
	s_ds.rVq3.generic.callback = DFS_PickerEvent;
	s_ds.rVq3.curvalue = variant == 0;
	s_ds.rCpm = s_ds.rVq3;
	s_ds.rCpm.generic.name = "CPM maps";
	s_ds.rCpm.generic.y += 18;
	s_ds.rCpm.curvalue = variant == 1;
	s_ds.typeSpin.generic.type = MTYPE_SPINCONTROL;
	s_ds.typeSpin.generic.name = "Type";
	s_ds.typeSpin.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_ds.typeSpin.generic.id = ID_FILTER;
	s_ds.typeSpin.generic.x = x0 + 170;
	s_ds.typeSpin.generic.y = s_ds.rCpm.generic.y + 18;
	s_ds.typeSpin.generic.callback = DFS_PickerEvent;
	s_ds.typeSpin.itemnames = dfsListType;
	s_ds.typeSpin.numitems = 5;
	if ( !s_ds.fixedMask ) {
		Menu_AddItem( &s_ds.menu, &s_ds.rVq3 );
		Menu_AddItem( &s_ds.menu, &s_ds.rCpm );
		Menu_AddItem( &s_ds.menu, &s_ds.typeSpin );
	}

	s_ds.title.generic.type = MTYPE_TEXT;
	s_ds.title.generic.flags = QMF_INACTIVE | QMF_SMALLFONT;
	s_ds.title.generic.x = x0;
	s_ds.title.generic.y = y0 + 340;
	s_ds.title.string = s_ds.titleBuf;
	s_ds.title.style = UI_SMALLFONT;
	s_ds.title.color = dfsColorWhite;
	Menu_AddItem( &s_ds.menu, &s_ds.title );
	s_ds.info.generic.type = MTYPE_TEXT;
	s_ds.info.generic.flags = QMF_INACTIVE | QMF_SMALLFONT;
	s_ds.info.generic.x = x0 + 330;
	s_ds.info.generic.y = y0 + 340;
	s_ds.info.string = s_ds.infoBuf;
	s_ds.info.style = UI_RIGHT | UI_SMALLFONT;
	s_ds.info.color = dfsColorWhite;
	Menu_AddItem( &s_ds.menu, &s_ds.info );

	// panels
	if ( type == 1 ) {
		DFS_BuildMapInfo( px, py, pw );
	} else {
		DFS_BuildMapInfo( px, py, pw );
		if ( type == 2 ) {
			DFS_BuildTimer( px, py, pw );
		}
		DFS_BuildGameplay( px, py, pw );
		DFS_BuildWeapons( px, py, pw );
	}

	DFS_InitBitmap( &s_ds.back, BACK_ART0, BACK_ART1, ID_BACK, 360, 412, 96, 48, QMF_PULSEIFFOCUS | QMF_LEFT_JUSTIFY, DFS_ButtonEvent );
	Menu_AddItem( &s_ds.menu, &s_ds.back );
	DFS_InitBitmap( &s_ds.go, GO_ART0, GO_ART1, ID_GO, 630, 412, 96, 48, QMF_PULSEIFFOCUS | QMF_RIGHT_JUSTIFY, DFS_ButtonEvent );
	Menu_AddItem( &s_ds.menu, &s_ds.go );
	s_ds.optionsSpin.generic.type = MTYPE_SPINCONTROL;
	s_ds.optionsSpin.generic.name = "> options";
	s_ds.optionsSpin.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_ds.optionsSpin.generic.id = ID_OPTIONS;
	s_ds.optionsSpin.generic.x = 495;
	s_ds.optionsSpin.generic.y = 392;
	s_ds.optionsSpin.generic.callback = DFS_ButtonEvent;
	s_ds.optionsSpin.itemnames = s_ds.panelTitles;
	s_ds.optionsSpin.numitems = s_ds.npanel;
	Menu_AddItem( &s_ds.menu, &s_ds.optionsSpin );

	DFS_ShowPanel( 0 );
	DFS_SetDb( firstDb );
	DFS_SetFilters();
	DFS_Rebuild();
	UI_PushMenu( &s_ds.menu );
}
