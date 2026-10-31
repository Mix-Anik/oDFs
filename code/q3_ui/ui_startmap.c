// ui_startmap.c -- DeFRaG start-map menu (ui 0x3825 and the 0x222d..0x36fc cluster), see disasm/UI/start_map_menu.md.
// Map databases of ui_dfmaps.c, option panels (Map Info / Timer / Gameplay / Weapons) writing the server cvars, and the
// run-server handler.
//
// oDFs: DeFRaG opened this menu from four main menu buttons (Defrag VQ3 / CPM, Tricks Mode, Fast Caps), each with a paged grid of
// 50 map buttons.  Here it is one "Maps" browser, laid out like the demo browser:
//  - SEARCH box: case insensitive, every word has to occur in the map name, long name or author ("lucy cpm" is two words);
//    names that start with the first word come first.  The list is refiltered with every key, Up / Down / PgUp / PgDn move
//    the selection without leaving the box, Enter starts the selected map
//  - MODE chips ALL / VQ3 / CPM / TRICKS / FAST CAPS (F1..F5, or Left / Right).  VQ3, CPM, TRICKS and FAST CAPS are the old main menu
//    buttons: they filter the list and decide how the map is started.  ALL (the default) lists every map; the map is started in
//    the mode the "Play as" spin shows (the last mode that was used when the map has it, else the first one it has).  Each chip
//    shows how many maps match the search in that mode
//  - the list holds the maps of the DeFRaG (*.defi) and Quake 3 (arenas.txt, *.arena) map files and every other maps/*.bsp of
//    the game folder and its pk3s (map packs, maps without a map file: type "unlisted")
//  - the right edge of a row is your best time on the map in the mode the row would start in, taken from the names of the demos
//    that carry the current player name (ui_dfbest.c); WATCH (or Ctrl+Enter) plays that demo and comes back here afterwards
//  - TYPE chips (run / training / level / accuracy) filter the DeFRaG maps
//  - tabs INFO / GAMEPLAY / WEAPONS / TIMER switch the panel on the right (only those the mode has: VQ3 and CPM have INFO),
//    "Server Cheats" and "Run Server" are always shown under it
//  - scroll bar, mouse wheel and double click like the demo browser; mode, type and the last map are kept in df_ui_maps*

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

#define DFS_MAXMAPS		16384		// both databases and the unlisted maps
#define DFS_MAXEXTRA	8192		// maps/*.bsp that no map file lists
#define DFS_BSPLISTSIZE	262144
#define DFS_MAXPANEL	4
#define DFS_PANELITEMS	16
#define DFS_ALLFLAGS	0x7fffffff
#define DFS_MAXWORDS	4
#define DFS_SEARCHMAX	64
#define DFS_DOUBLECLICK	600

// geometry: the browser on the left (as wide as 340 pixels), the panel on the right
#define DFS_LX			10
#define DFS_LW			340
#define DFS_FIELD_Y		9
#define DFS_FIELD_CHARS	41
#define DFS_MODE_Y		31
#define DFS_MODE_H		36		// label and match count on two lines
#define DFS_TYPE_Y		71
#define DFS_TYPE_H		16
#define DFS_LIST_Y		93
#define DFS_LIST_ROWS	19
#define DFS_LIST_CHARS	40
#define DFS_SB_X		334
#define DFS_SB_W		16
#define DFS_SB_MINTHUMB	20
#define DFS_RX			360
#define DFS_RW			270
#define DFS_TAB_Y		7
#define DFS_TAB_H		20
#define DFS_PANEL_X		( DFS_RX + DFS_RW / 2 + 25 )	// the spin controls: labels end left of it, values start right of it
#define DFS_PREVIEW_Y	36
#define DFS_PLAY_Y		372		// "Play as" (ALL only), then Server Cheats and Run Server
#define DFS_RUN_Y		392
#define DFS_BUTTON_W	96
#define DFS_BUTTON_H	30
#define DFS_BUTTON_Y	438
#define DFS_BUTTON_MARGIN	12

#define ID_GO			1
#define ID_BACK			2
#define ID_WATCH		3
#define ID_LIST			20
#define ID_SEARCH		21
#define ID_MODE			30		// + filter
#define ID_TYPE			40		// + type
#define ID_TAB			50		// + panel index
#define ID_CHEATS		60
#define ID_RUN			61
#define ID_PLAY			62
#define ID_PANEL		200		// + panel index
#define ID_WNONE		300
#define ID_WALL			301
#define ID_TIMER		400		// + group 0..2

#define DFS_SAME		( -1 )	// panel item gap: on the row of the item before

enum { MODE_VQ3, MODE_CPM, MODE_TRICKS, MODE_FCAPS, MODE_COUNT };
#define FILTER_COUNT	( MODE_COUNT + 1 )		// the mode chips: 0 = ALL, 1 + mode
enum { PK_MAPINFO, PK_GAMEPLAY, PK_WEAPONS, PK_TIMER };

#define MB_VQ3			( 1 << MODE_VQ3 )
#define MB_CPM			( 1 << MODE_CPM )
#define MB_TRICKS		( 1 << MODE_TRICKS )
#define MB_FCAPS		( 1 << MODE_FCAPS )
#define MB_ALL			15
#define MB_CAPS			( MB_TRICKS | MB_FCAPS )

typedef struct {
	int				kind;
	const char		*title;
	int				modes;							// the modes that have this panel
	int				x, top;							// spin controls x, y of the first row
	menucommon_s	*items[DFS_PANELITEMS];
	int				imodes[DFS_PANELITEMS];			// the modes an item is shown in
	int				igap[DFS_PANELITEMS];			// pixels before the item, DFS_SAME
	int				count;
} dfPanel_t;

// one map of either database
typedef struct {
	const dfEntry_t	*e;
	int				db;					// 1 defrag, 2 quake3, 3 unlisted (a .bsp that no map file lists)
	int				modes;				// MB_* of the modes that list it
	char			hay[100];			// "name longname author", lower case: what the search looks in
} dfRec_t;

typedef struct {
	menuframework_s	menu;

	menufield_s		search;
	menulist_s		list;
	menutext_s		modeChip[FILTER_COUNT];
	menutext_s		typeChip[5];
	menutext_s		tab[DFS_MAXPANEL];
	menutext_s		back, watch, go;
	menubitmap_s	preview;
	menulist_s		cheatsSpin, runSpin, playSpin;

	// panels
	menulist_s		spin[16];
	int				nspin;
	menuradiobutton_s	check[10];
	menutext_s		wnone, wall;
	menulist_s		timer[9];
	menulist_s		*wOB, *wGameMode, *wMove, *wGpOB;
	dfPanel_t		panel[DFS_MAXPANEL];
	int				npanel;
	int				curPanel;

	// state
	int				filter;			// the mode chip: 0 = ALL, 1 + MODE_*
	int				mode;			// MODE_*: what the panels show and the map is started as (the filter's, with ALL the selected map's)
	int				pref;			// MODE_*: the mode ALL starts a map in when the map has it
	int				appliedMode;	// the mode DFS_ApplyMode last set up (-1: none yet)
	int				type;			// 0 any, 1 run, 2 training, 3 level, 4 accuracy
	int				cheats;			// 634032
	int				runServer;		// 634028
	int				view[DFS_MAXMAPS];		// indexes into dfsRecs of the listed maps
	int				tier1[DFS_MAXMAPS];
	int				counts[FILTER_COUNT];	// matches of the search for each mode chip
	char			applied[DFS_SEARCHMAX + 1];	// the search text the view was built for
	const dfRec_t	*sel;
	qhandle_t		previewShader;
	char			countText[48];

	qboolean		keyNav;			// the last input was a navigation key: the focused chip / button is lit
	int				lastCursorX, lastCursorY;
	qboolean		sbDrag;
	int				sbGrab;
	int				clickIndex, clickTime;
	char			timerShadow[3];
} dfStart_t;

static dfStart_t	s_ds;

// the records live as long as the ui (the databases are read once)
static dfRec_t		dfsRecs[DFS_MAXMAPS];
static int			dfsNumRecs;
static const char	*dfsBlank[DFS_MAXMAPS];		// the scroll list's item names (it is drawn by DFS_ListDraw)
static dfEntry_t	dfsExtras[DFS_MAXEXTRA];	// the unlisted maps
static char			dfsBspList[DFS_BSPLISTSIZE];

static vec4_t	dfsColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	dfsPlate = { 0.14f, 0.02f, 0.02f, 0.90f };
static vec4_t	dfsPlateFocus = { 0.26f, 0.04f, 0.03f, 0.95f };
static vec4_t	dfsBorder = { 0.62f, 0.10f, 0.08f, 1.0f };
static vec4_t	dfsBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	dfsTime = { 0.55f, 0.85f, 0.55f, 1.0f };
static vec4_t	dfsHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	dfsLabel = { 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	dfsChipFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	dfsChipFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	dfsChipFillSel = { 0.85f, 0.12f, 0.06f, 0.60f };
static vec4_t	dfsChipBorder = { 0.80f, 0.14f, 0.10f, 1.0f };
static vec4_t	dfsChipBorderSel = { 1.0f, 0.38f, 0.22f, 1.0f };
static vec4_t	dfsChipOff = { 0.45f, 0.35f, 0.33f, 1.0f };

static const char *dfsListOffOn[] = { "off", "on", 0 };
static const char *dfsListOnOff[] = { "on", "off", 0 };
static const char *dfsListRun[] = { "no", "listen", "local", "public", 0 };
static const char *dfsListMove[] = { "VQ3", "CPM", 0 };
static const char *dfsListOnOffKill[] = { "on", "off", "kill", 0 };
static const char *dfsListItems[] = { "off", "selective", "on", 0 };
static const char *dfsListHook[] = { "Quake3", "Swinging", "Pendulum", "Quake2", 0 };
static const char *dfsListNum8[] = { "0", "1", "2", "3", "4", "5", "6", "7", 0 };
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

static const char *dfsFilterNames[FILTER_COUNT] = { "ALL", "VQ3", "CPM", "TRICKS", "FAST CAPS" };
static const int dfsFilterW[FILTER_COUNT] = { 56, 56, 56, 64, 88 };
static const char *dfsListPlay[] = { "VQ3", "CPM", "Tricks", "Fast Caps", 0 };
static const char *dfsTypeNames[5] = { "ALL", "RUN", "TRAINING", "LEVEL", "ACCURACY" };
static const int dfsTypeFlag[5] = { 0, UIDF_MAP_RUN, UIDF_MAP_TRAINING, UIDF_MAP_LEVEL, UIDF_MAP_ACCURACY };

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
// the maps: records of both databases, filtered by mode / type / search

static const char *DFS_TypeName( int flags ) {
	if ( flags & UIDF_MAP_TRAINING ) {
		return "training";
	}
	if ( flags & UIDF_MAP_LEVEL ) {
		return "level";
	}
	if ( flags & UIDF_MAP_ACCURACY ) {
		return "accuracy";
	}
	return "run";
}

static void DFS_AddRec( const dfEntry_t *e, int db, qboolean dup ) {
	dfRec_t *r;
	int modes = 0;

	if ( dfsNumRecs >= DFS_MAXMAPS ) {
		return;
	}
	r = &dfsRecs[dfsNumRecs++];
	r->e = e;
	r->db = db;
	if ( db == 1 ) {
		if ( e->flags & UIDF_MAP_VQ3 ) {
			modes |= MB_VQ3;
		}
		if ( e->flags & UIDF_MAP_CPM ) {
			modes |= MB_CPM;
		}
		modes |= MB_TRICKS;
	} else if ( db == 3 ) {
		modes = MB_VQ3 | MB_CPM | MB_TRICKS;		// nothing says what it is: it can be played any way but as a CTF map
	} else {
		if ( !dup ) {
			modes |= MB_TRICKS;			// a quake3 map that a defrag file lists too is shown once
		}
		if ( e->flags & UIDF_MAP_CTF ) {
			modes |= MB_FCAPS;
		}
	}
	r->modes = modes;
	Com_sprintf( r->hay, sizeof( r->hay ), "%s %s %s", e->map, e->longname, e->author );
	Q_strlwr( r->hay );
}

static int DFS_RecCompare( const void *a, const void *b ) {
	const dfRec_t *x = (const dfRec_t *)a;
	const dfRec_t *y = (const dfRec_t *)b;
	int c = Q_stricmp( x->e->map, y->e->map );

	return c ? c : x->db - y->db;
}

// is a map of this name among the first n (name sorted) records
static qboolean DFS_RecListed( const char *name, int n ) {
	int lo = 0, hi = n - 1;

	while ( lo <= hi ) {
		int mid = ( lo + hi ) / 2;
		int c = Q_stricmp( dfsRecs[mid].e->map, name );

		if ( !c ) {
			return qtrue;
		}
		if ( c < 0 ) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}
	return qfalse;
}

// every maps/*.bsp of the game folder and its pk3s that no map file lists (map packs, maps without a .defi / .arena)
static void DFS_AddBsps( void ) {
	char *s = dfsBspList;
	int count, i, nextra = 0, listed = dfsNumRecs;

	count = trap_FS_GetFileList( "maps", ".bsp", dfsBspList, sizeof( dfsBspList ) );
	for ( i = 0; i < count && nextra < DFS_MAXEXTRA && dfsNumRecs < DFS_MAXMAPS; i++ ) {
		int len = strlen( s );
		char name[32];

		if ( len > 4 ) {
			Q_strncpyz( name, s, sizeof( name ) );
			if ( strlen( s ) - 4 < sizeof( name ) ) {
				name[len - 4] = 0;				// ".bsp"
				if ( !DFS_RecListed( name, listed ) ) {
					dfEntry_t *e = &dfsExtras[nextra++];

					Q_strncpyz( e->map, name, sizeof( e->map ) );
					e->flags = UIDF_MAP_VQ3 | UIDF_MAP_CPM;
					DFS_AddRec( e, 3, qfalse );
				}
			}
		}
		s += len + 1;
	}
	if ( dfsNumRecs > listed ) {
		qsort( dfsRecs, dfsNumRecs, sizeof( dfsRecs[0] ), DFS_RecCompare );
	}
	Com_Printf( "Map browser: %i maps, %i of them from maps/*.bsp only.\n", dfsNumRecs, nextra );
}

// both databases are name sorted: merge them into one list, then add the maps no map file lists
static void DFS_BuildRecs( void ) {
	const dfEntry_t *a, *b;
	int i;

	if ( dfsNumRecs ) {
		return;
	}
	for ( i = 0; i < DFS_MAXMAPS; i++ ) {
		dfsBlank[i] = "";
	}
	a = (const dfEntry_t *)UI_DF_MapNext( UIDF_MAPDB_DEFRAG, DFS_ALLFLAGS, DFS_ALLFLAGS, NULL );
	b = (const dfEntry_t *)UI_DF_MapNext( UIDF_MAPDB_QUAKE3, DFS_ALLFLAGS, DFS_ALLFLAGS, NULL );
	while ( a || b ) {
		int cmp = !b ? -1 : !a ? 1 : Q_stricmp( a->map, b->map );

		if ( cmp <= 0 ) {
			DFS_AddRec( a, 1, qfalse );
			a = (const dfEntry_t *)UI_DF_MapNext( UIDF_MAPDB_DEFRAG, DFS_ALLFLAGS, DFS_ALLFLAGS, (const struct uiDfMapEntry_s *)a );
			if ( cmp == 0 ) {
				DFS_AddRec( b, 2, qtrue );
				b = (const dfEntry_t *)UI_DF_MapNext( UIDF_MAPDB_QUAKE3, DFS_ALLFLAGS, DFS_ALLFLAGS, (const struct uiDfMapEntry_s *)b );
			}
		} else {
			DFS_AddRec( b, 2, qfalse );
			b = (const dfEntry_t *)UI_DF_MapNext( UIDF_MAPDB_QUAKE3, DFS_ALLFLAGS, DFS_ALLFLAGS, (const struct uiDfMapEntry_s *)b );
		}
	}
	DFS_AddBsps();
}

static qboolean DFS_HasWord( const char *hay, const char *w, int len ) {
	const char *p;
	char first = w[0];

	for ( p = hay; *p; p++ ) {
		if ( *p == first && !Q_strncmp( p, w, len ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void DFS_ResolveMode( void );
static int DFS_RowMode( const dfRec_t *r );

static void DFS_SelectionChanged( void ) {
	const dfRec_t *old = s_ds.sel;

	s_ds.sel = s_ds.list.numitems ? &dfsRecs[s_ds.view[s_ds.list.curvalue]] : NULL;
	if ( s_ds.sel != old ) {
		s_ds.previewShader = 0;
	}
	if ( s_ds.sel ) {
		s_ds.go.generic.flags &= ~QMF_GRAYED;
		Com_sprintf( s_ds.countText, sizeof( s_ds.countText ), "%i / %i maps", s_ds.list.curvalue + 1, s_ds.list.numitems );
	} else {
		s_ds.go.generic.flags |= QMF_GRAYED;
		Q_strncpyz( s_ds.countText, "no maps", sizeof( s_ds.countText ) );
	}
	DFS_ResolveMode();
}

// puts the cursor on entry idx and scrolls it to the middle of the list
static void DFS_SetListPos( int idx ) {
	int max = s_ds.list.numitems - DFS_LIST_ROWS;

	s_ds.list.curvalue = idx;
	s_ds.list.top = Com_Clamp( 0, max > 0 ? max : 0, idx - DFS_LIST_ROWS / 2 );
	DFS_SelectionChanged();
}

/*
===============
DFS_Refilter

Rebuilds the list for the mode, type and search text.  The selection stays on its map when that is still listed, else
it goes to the top.  Every record is looked at once: that also gives the number of matches of each mode.
===============
*/
static void DFS_Refilter( qboolean keepSelection ) {
	char			term[DFS_SEARCHMAX + 1];
	const char		*words[DFS_MAXWORDS];
	int				lens[DFS_MAXWORDS];
	int				nw = 0, nview = 0, ntier = 0;
	int				i, f, flag;
	const dfRec_t	*prev = keepSelection ? s_ds.sel : NULL;
	char			*p;

	Q_strncpyz( term, s_ds.search.field.buffer, sizeof( term ) );
	Q_strlwr( term );
	Q_strncpyz( s_ds.applied, s_ds.search.field.buffer, sizeof( s_ds.applied ) );
	p = term;
	while ( *p && nw < DFS_MAXWORDS ) {
		while ( *p == ' ' ) {
			p++;
		}
		if ( !*p ) {
			break;
		}
		words[nw] = p;
		while ( *p && *p != ' ' ) {
			p++;
		}
		lens[nw] = p - words[nw];
		nw++;
		if ( *p ) {
			*p++ = 0;
		}
	}

	for ( f = 0; f < FILTER_COUNT; f++ ) {
		s_ds.counts[f] = 0;
	}
	flag = dfsTypeFlag[s_ds.type];
	for ( i = 0; i < dfsNumRecs; i++ ) {
		const dfRec_t	*r = &dfsRecs[i];
		qboolean		typeOK;
		int				w;

		for ( w = 0; w < nw; w++ ) {
			if ( !DFS_HasWord( r->hay, words[w], lens[w] ) ) {
				break;
			}
		}
		if ( w < nw ) {
			continue;
		}
		typeOK = !flag || ( r->db == 1 && ( r->e->flags & flag ) );
		for ( f = 0; f < FILTER_COUNT; f++ ) {
			int in = f == 0 ? r->modes : r->modes & ( 1 << ( f - 1 ) );

			if ( in && ( f - 1 == MODE_FCAPS || typeOK ) ) {
				s_ds.counts[f]++;
			}
		}
		f = s_ds.filter;
		if ( !( f == 0 ? r->modes : r->modes & ( 1 << ( f - 1 ) ) ) || ( f - 1 != MODE_FCAPS && !typeOK ) ) {
			continue;
		}
		if ( nw && !Q_strncmp( r->hay, words[0], lens[0] ) ) {
			s_ds.view[nview++] = i;
		} else {
			s_ds.tier1[ntier++] = i;
		}
	}
	if ( ntier ) {
		Com_Memcpy( &s_ds.view[nview], s_ds.tier1, ntier * sizeof( int ) );
		nview += ntier;
	}

	s_ds.list.numitems = nview;
	s_ds.list.curvalue = 0;
	s_ds.list.top = 0;
	if ( prev ) {
		for ( i = 0; i < nview; i++ ) {
			if ( &dfsRecs[s_ds.view[i]] == prev ) {
				DFS_SetListPos( i );
				return;
			}
		}
	}
	DFS_SelectionChanged();
}

static void DFS_SelectByName( const char *name ) {
	int i;

	for ( i = 0; i < s_ds.list.numitems; i++ ) {
		if ( !Q_stricmp( dfsRecs[s_ds.view[i]].e->map, name ) ) {
			DFS_SetListPos( i );
			return;
		}
	}
}

static void DFS_ListEvent( void *ptr, int event ) {
	if ( event == QM_GOTFOCUS ) {
		DFS_SelectionChanged();
	}
}

// ---------------------------------------------------------------------------------------------------------------
// drawing helpers

static void DFS_DrawOutline( int x, int y, int w, int h, const float *border ) {
	UI_FillRect( x, y, w, 1, border );
	UI_FillRect( x, y + h - 1, w, 1, border );
	UI_FillRect( x, y, 1, h, border );
	UI_FillRect( x + w - 1, y, 1, h, border );
}

static qboolean DFS_Lit( menucommon_s *c ) {
	return ( c->flags & QMF_HASMOUSEFOCUS ) || ( s_ds.keyNav && Menu_ItemAtCursor( c->parent ) == (void *)c );
}

// mode chips, type chips, tabs: the selected one is filled, the pointer (or keyboard focus) turns the outline yellow
static void DFS_ChipDraw( void *self ) {
	menutext_s	*c = (menutext_s *)self;
	int			id = c->generic.id;
	int			x = c->generic.left, y = c->generic.top;
	int			w = c->generic.right - x + 1, h = c->generic.bottom - y + 1;
	qboolean	selected, off = ( c->generic.flags & QMF_GRAYED ) != 0, lit;
	vec4_t		fill;
	const float	*border;
	float		*color;

	if ( id >= ID_MODE && id < ID_MODE + FILTER_COUNT ) {
		selected = s_ds.filter == id - ID_MODE;
	} else if ( id >= ID_TYPE && id < ID_TYPE + 5 ) {
		selected = s_ds.type == id - ID_TYPE;
	} else {
		selected = s_ds.curPanel == id - ID_TAB;
	}
	lit = !off && DFS_Lit( &c->generic );
	Vector4Copy( selected ? dfsChipFillSel : lit ? dfsChipFillLit : dfsChipFill, fill );
	if ( lit ) {
		fill[3] *= 0.85f + 0.15f * sin( uis.realtime * 0.006f );
	}
	border = lit ? dfsBorderFocus : selected ? dfsChipBorderSel : dfsChipBorder;
	color = off ? dfsChipOff : ( selected || lit ) ? dfsColorWhite : dfsLabel;
	UI_FillRect( x, y, w, h, fill );
	DFS_DrawOutline( x, y, w, h, border );
	if ( id >= ID_MODE && id < ID_MODE + FILTER_COUNT ) {
		// the label, and under it how many maps match the search
		UI_DrawString( x + w / 2, y + 2, c->string, UI_CENTER | UI_SMALLFONT, color );
		UI_DrawString( x + w / 2, y + 2 + SMALLCHAR_HEIGHT, va( "%i", s_ds.counts[id - ID_MODE] ), UI_CENTER | UI_SMALLFONT, selected ? dfsColorWhite : dfsHint );
		return;
	}
	UI_DrawString( x + 4, y + ( h - SMALLCHAR_HEIGHT ) / 2, c->string, UI_SMALLFONT, color );
}

// BACK / START: a faint red fill, a 2 pixel outline and plain text, as the demo browser's buttons
static void DFS_ButtonDraw( void *self ) {
	menutext_s	*b = (menutext_s *)self;
	qboolean	off = ( b->generic.flags & QMF_GRAYED ) != 0;
	qboolean	lit = !off && DFS_Lit( &b->generic );
	int			x = b->generic.left, y = b->generic.top;
	vec4_t		fill;
	const float	*border = lit ? dfsBorderFocus : dfsChipBorder;

	Vector4Copy( lit ? dfsChipFillLit : dfsChipFill, fill );
	if ( lit ) {
		fill[3] *= 0.85f + 0.15f * sin( uis.realtime * 0.006f );
	}
	UI_FillRect( x, y, DFS_BUTTON_W, DFS_BUTTON_H, fill );
	DFS_DrawOutline( x, y, DFS_BUTTON_W, DFS_BUTTON_H, border );
	DFS_DrawOutline( x + 1, y + 1, DFS_BUTTON_W - 2, DFS_BUTTON_H - 2, border );
	UI_DrawProportionalString( x + DFS_BUTTON_W / 2, y + ( DFS_BUTTON_H - 20 ) / 2, b->string, UI_CENTER | UI_SMALLFONT, off ? dfsChipOff : lit ? color_white : dfsLabel );
}

static qboolean DFS_SearchFocused( void ) {
	return Menu_ItemAtCursor( &s_ds.menu ) == (void *)&s_ds.search;
}

// the search plate; empty and unfocused it shows what it is for
static void DFS_FieldDraw( void *self ) {
	menufield_s	*f = (menufield_s *)self;
	qboolean	focus = ( Menu_ItemAtCursor( f->generic.parent ) == self );
	int			y = f->generic.y;

	UI_FillRect( DFS_LX, y - 2, DFS_LW, SMALLCHAR_HEIGHT + 4, focus ? dfsPlateFocus : dfsPlate );
	DFS_DrawOutline( DFS_LX, y - 2, DFS_LW, SMALLCHAR_HEIGHT + 4, focus ? dfsBorderFocus : dfsBorder );
	if ( !f->field.buffer[0] && !focus ) {
		UI_DrawString( DFS_LX + 4, y, "search maps / authors    F1-F5 mode", UI_SMALLFONT, dfsHint );
	} else {
		MField_Draw( &f->field, DFS_LX + 4, y, UI_SMALLFONT | ( focus ? UI_PULSE : 0 ), dfsColorWhite );
	}
}

// the map list: name, and the type (the database in TRICKS) on the right
static void DFS_ListDraw( void *self ) {
	menulist_s	*l = (menulist_s *)self;
	qboolean	hasfocus = ( l->generic.parent->cursor == l->generic.menuPosition );
	int			i, x = l->generic.x, y = l->generic.y;
	vec4_t		tagColor;

	for ( i = l->top; i < l->top + l->height && i < l->numitems; i++, y += SMALLCHAR_HEIGHT ) {
		const dfRec_t	*r = &dfsRecs[s_ds.view[i]];
		char			tag[16];
		int				best = UI_DF_BestFor( r->e->map, DFS_RowMode( r ), NULL );
		int				style = UI_LEFT | UI_SMALLFONT;
		float			*color = text_color_normal;

		if ( best ) {
			UI_DF_BestFormat( best, tag, sizeof( tag ) );
		} else {
			tag[0] = 0;
		}
		if ( i == l->curvalue ) {
			UI_FillRect( x - 2, y, l->width * SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT + 2, listbar_color );
			color = text_color_highlight;
			if ( hasfocus ) {
				style |= UI_PULSE;
			}
			Vector4Copy( dfsColorWhite, tagColor );
		} else {
			Vector4Copy( dfsTime, tagColor );
		}
		UI_DrawString( x, y, r->e->map, style, color );
		UI_DrawString( x + l->width * SMALLCHAR_WIDTH - 6, y, tag, UI_RIGHT | UI_SMALLFONT, tagColor );
	}
}

/*
===============
The scroll bar (as the demo browser's): click the track or drag the thumb with the left mouse button.  The menu system gets
key presses but no releases, so a drag is followed from DFS_MenuDraw with trap_Key_IsDown.
===============
*/
static qboolean DFS_ScrollbarGeometry( int *trackY, int *trackH, int *thumbY, int *thumbH ) {
	int	total = s_ds.list.numitems;
	int	rows = s_ds.list.height;
	int	range, top;

	*trackY = s_ds.list.generic.y;
	*trackH = rows * SMALLCHAR_HEIGHT;
	if ( total <= rows ) {
		*thumbY = *trackY;
		*thumbH = *trackH;
		return qfalse;
	}
	range = total - rows;
	*thumbH = *trackH * rows / total;
	if ( *thumbH < DFS_SB_MINTHUMB ) {
		*thumbH = DFS_SB_MINTHUMB;
	}
	top = Com_Clamp( 0, range, s_ds.list.top );
	*thumbY = *trackY + ( *trackH - *thumbH ) * top / range;
	return qtrue;
}

static void DFS_ScrollbarDrag( void ) {
	int	trackY, trackH, thumbY, thumbH, range, top;

	if ( !DFS_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	range = s_ds.list.numitems - s_ds.list.height;
	top = ( uis.cursory - s_ds.sbGrab - trackY ) * range / ( trackH - thumbH );
	s_ds.list.top = Com_Clamp( 0, range, top );
}

static void DFS_ScrollbarDraw( void ) {
	static vec4_t	trackColor = { 0.15f, 0.0f, 0.0f, 0.6f };
	static vec4_t	thumbColor = { 0.70f, 0.08f, 0.08f, 0.95f };
	static vec4_t	hotColor = { 1.0f, 0.30f, 0.15f, 1.0f };
	int				trackY, trackH, thumbY, thumbH;
	qboolean		hot;

	if ( !DFS_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	hot = s_ds.sbDrag || UI_CursorInRect( DFS_SB_X, trackY, DFS_SB_W, trackH );
	UI_FillRect( DFS_SB_X, trackY, DFS_SB_W, trackH, trackColor );
	UI_FillRect( DFS_SB_X + 1, thumbY, DFS_SB_W - 2, thumbH, hot ? hotColor : thumbColor );
}

// ---------------------------------------------------------------------------------------------------------------
// preview (ui 0x20d6): levelshot, long name, author and what the map is

static void DFS_PreviewDraw( void *ptr ) {
	menubitmap_s *b = (menubitmap_s *)ptr;
	const dfRec_t *r = s_ds.sel;
	const dfEntry_t *e = r ? r->e : NULL;
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
	if ( !e ) {
		return;
	}
	if ( e->longname[0] ) {
		UI_DrawString( (int)x, ty, e->longname, UI_CENTER | UI_SMALLFONT, dfsColorWhite );
		ty += 16;
	}
	if ( e->author[0] ) {
		UI_DrawString( (int)x, ty, va( "by %s", e->author ), UI_CENTER | UI_SMALLFONT, dfsColorWhite );
		ty += 16;
	}
	if ( r->db == 3 ) {
		UI_DrawString( (int)x, ty, "not in a map file", UI_CENTER | UI_SMALLFONT, dfsHint );
	} else if ( r->db == 1 ) {
		const char *mode = ( e->flags & 3 ) == 3 ? "VQ3 / CPM" : ( e->flags & 2 ) ? "CPM" : "VQ3";

		UI_DrawString( (int)x, ty, va( "%s  -  %s", mode, DFS_TypeName( e->flags ) ), UI_CENTER | UI_SMALLFONT, dfsHint );
	} else {
		UI_DrawString( (int)x, ty, ( e->flags & UIDF_MAP_CTF ) ? "Quake 3  -  CTF" : "Quake 3", UI_CENTER | UI_SMALLFONT, dfsHint );
	}
	{
		int best = UI_DF_BestFor( e->map, s_ds.mode, NULL );
		char text[24];

		ty += 18;
		if ( best ) {
			UI_DF_BestFormat( best, text, sizeof( text ) );
			UI_DrawString( (int)x, ty, va( "Your best: %s", text ), UI_CENTER | UI_SMALLFONT, dfsTime );
		} else {
			UI_DrawString( (int)x, ty, UI_DF_BestLoading() ? "reading your demos..." : "No demo of yours", UI_CENTER | UI_SMALLFONT, dfsHint );
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// panels

static void DFS_MoveY( menucommon_s *c, int y ) {
	int dy = y - c->y;

	c->y = y;
	c->top += dy;
	c->bottom += dy;
}

// stacks the items that the mode shows, from the top of the panel
static void DFS_LayoutPanel( dfPanel_t *p ) {
	int i, y = p->top, lastY = y;

	for ( i = 0; i < p->count; i++ ) {
		menucommon_s *c = p->items[i];

		if ( c->type == MTYPE_BITMAP || !( p->imodes[i] & ( 1 << s_ds.mode ) ) ) {
			continue;
		}
		if ( p->igap[i] == DFS_SAME ) {
			DFS_MoveY( c, lastY );
			continue;
		}
		y += p->igap[i];
		DFS_MoveY( c, y );
		lastY = y;
		y += 20;
	}
}

static void DFS_ShowPanel( int idx ) {			// 0xedc / 0xe0a
	int p, i;

	if ( idx < 0 || idx >= s_ds.npanel ) {
		return;
	}
	for ( p = 0; p < s_ds.npanel; p++ ) {
		for ( i = 0; i < s_ds.panel[p].count; i++ ) {
			if ( p == idx && ( s_ds.panel[p].imodes[i] & ( 1 << s_ds.mode ) ) ) {
				s_ds.panel[p].items[i]->flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
			} else {
				s_ds.panel[p].items[i]->flags |= QMF_INACTIVE | QMF_HIDDEN;
			}
		}
	}
	s_ds.curPanel = idx;
	DFS_LayoutPanel( &s_ds.panel[idx] );
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

// the tabs of the panels that the mode has, side by side
static void DFS_LayoutTabs( void ) {
	int p, x = DFS_RX;

	for ( p = 0; p < s_ds.npanel; p++ ) {
		menutext_s *t = &s_ds.tab[p];

		if ( s_ds.panel[p].modes & ( 1 << s_ds.mode ) ) {
			int w = strlen( t->string ) * SMALLCHAR_WIDTH + 16;

			t->generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
			t->generic.x = x;
			t->generic.left = x;
			t->generic.right = x + w - 1;
			x += w + 4;
		} else {
			t->generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
		}
	}
}

static dfPanel_t *DFS_NewPanel( int kind, const char *title, int modes, int top, int x ) {
	dfPanel_t *p = &s_ds.panel[s_ds.npanel];

	p->kind = kind;
	p->title = title;
	p->modes = modes;
	p->top = top;
	p->x = x;
	p->count = 0;
	s_ds.npanel++;
	return p;
}

static void DFS_PanelAdd( dfPanel_t *p, void *item, int modes, int gap ) {		// 0xe52
	menucommon_s *c = (menucommon_s *)item;

	if ( p->count >= DFS_PANELITEMS ) {
		return;
	}
	p->imodes[p->count] = modes ? modes : p->modes;
	p->igap[p->count] = gap;
	p->items[p->count++] = c;
	c->flags |= QMF_INACTIVE | QMF_HIDDEN;
	Menu_AddItem( &s_ds.menu, item );
}

static menulist_s *DFS_AddSpin( dfPanel_t *p, const char *label, int id, const char **list, int cur, int modes, int gap ) {
	menulist_s *s = &s_ds.spin[s_ds.nspin++];

	s->generic.type = MTYPE_SPINCONTROL;
	s->generic.name = label;
	s->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.callback = NULL;	// set by caller
	s->generic.id = id;
	s->generic.x = p->x;
	s->generic.y = 0;			// DFS_LayoutPanel
	s->itemnames = list;
	s->numitems = DFS_Count( list );
	s->curvalue = cur;
	DFS_PanelAdd( p, s, modes, gap );
	return s;
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

static void DFS_ApplyMapInfo( void ) {			// 0x30aa / 0x349a / 0x32da
	if ( s_ds.mode < MODE_TRICKS ) {
		DFS_SetInt( "df_promode", s_ds.mode );
		DFS_SetInt( "df_ob_KillOBs", s_ds.wOB->curvalue == 0 ? 1 : 0 );
	} else {
		DFS_SetInt( s_ds.mode == MODE_FCAPS ? "df_mode_fastcaps" : "df_mode_tricksmode", s_ds.wGameMode->curvalue );
		DFS_SetInt( "df_promode", s_ds.wMove->curvalue );
	}
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

// the widgets that show a cvar another panel (or an earlier visit) may have changed
static void DFS_SyncFromCvars( void ) {
	int ob = trap_Cvar_VariableValue( "df_ob_KillOBs" ) == 0 ? 1 : 0;

	s_ds.wOB->curvalue = ob;
	s_ds.wGpOB->curvalue = ob;
	s_ds.wGameMode->curvalue = DFS_ClampCvar( s_ds.mode == MODE_FCAPS ? "df_mode_fastcaps" : "df_mode_tricksmode", 7 );
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

static void DFS_RunEvent( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( ( (menucommon_s *)ptr )->id == ID_PLAY ) {
		s_ds.pref = s_ds.playSpin.curvalue;
		trap_Cvar_Set( "df_ui_mapsPlay", va( "%i", s_ds.pref ) );
		DFS_ResolveMode();			// a map that cannot be played that way keeps its own mode
	} else if ( ( (menucommon_s *)ptr )->id == ID_CHEATS ) {
		s_ds.cheats = s_ds.cheatsSpin.curvalue;
	} else {
		s_ds.runServer = s_ds.runSpin.curvalue;
	}
}

static menulist_s *DFS_PSpin( dfPanel_t *p, const char *label, const char **list, int cur, int modes, int gap ) {
	menulist_s *s = DFS_AddSpin( p, label, ID_PANEL + ( p - s_ds.panel ), list, cur, modes, gap );
	s->generic.callback = DFS_PanelEvent;
	return s;
}

// ui 0x32b9 (statusbar of the Game Mode spinner, table at data 0xc6c): while the spinner has focus its meaning is written at y 362 (oDFs: 306, the Play as row is there)
static const char *dfsModeCaptions[8] = {
	"(custom settings)", "(no weapon / no map object)", "(weapons / map objects)", "(no weapon / map objects)",
	"(weapons / no map object)", "(swinging hook)", "(quake3 hook)", "(original quake 3)"
};

static void DFS_ModeStatus( void *ptr ) {
	menulist_s *s = (menulist_s *)ptr;

	UI_DrawString( s->generic.x, 306, dfsModeCaptions[s->curvalue & 7], UI_CENTER|UI_SMALLFONT, color_white );
}

static void DFS_BuildMapInfo( void ) {		// 0x30e9 / 0x34cd / 0x330d
	dfPanel_t *p = DFS_NewPanel( PK_MAPINFO, "INFO", MB_ALL, DFS_PREVIEW_Y + 228, DFS_PANEL_X );

	// preview (0x21f0)
	s_ds.preview.generic.type = MTYPE_BITMAP;
	s_ds.preview.generic.flags = QMF_INACTIVE | QMF_OWNERDRAW;
	s_ds.preview.generic.x = DFS_RX + DFS_RW / 2;
	s_ds.preview.generic.y = DFS_PREVIEW_Y;
	s_ds.preview.generic.ownerdraw = DFS_PreviewDraw;
	s_ds.preview.width = 200;
	s_ds.preview.height = 150;
	DFS_PanelAdd( p, &s_ds.preview, 0, 0 );

	s_ds.wOB = DFS_PSpin( p, "Overbounces:", dfsListOffOn, 0, MB_VQ3 | MB_CPM, 0 );
	s_ds.wGameMode = DFS_PSpin( p, "Game Mode:", dfsListNum8, 0, MB_CAPS, 0 );
	s_ds.wGameMode->generic.statusbar = DFS_ModeStatus;
	s_ds.wMove = DFS_PSpin( p, "Movements:", dfsListMove, trap_Cvar_VariableValue( "df_promode" ) != 0 ? 1 : 0, MB_CAPS, 0 );
}

static void DFS_BuildGameplay( void ) {		// 0x2a28
	dfPanel_t *p = DFS_NewPanel( PK_GAMEPLAY, "GAMEPLAY", MB_CAPS, DFS_PREVIEW_Y + 4, DFS_PANEL_X );

	DFS_PSpin( p, "Damage:", dfsListOffOn, DFS_ClampCvar( "df_ndm_damage", 1 ), 0, 0 );
	DFS_PSpin( p, "Powerups:", dfsListOffOn, DFS_ClampCvar( "df_ndm_powerups", 1 ), 0, 0 );
	DFS_PSpin( p, "Jump-pads:", dfsListOnOffKill, DFS_ClampCvar( "df_ndm_nopad", 2 ), 0, 0 );
	DFS_PSpin( p, "Teleporters:", dfsListOnOffKill, DFS_ClampCvar( "df_ndm_noteleport", 2 ), 0, 0 );
	DFS_PSpin( p, "Movers:", dfsListOnOff, DFS_ClampCvar( "df_ndm_nomover", 1 ), 0, 0 );
	s_ds.wGpOB = DFS_PSpin( p, "OverBounces:", dfsListOffOn, 0, 0, 20 );
	DFS_PSpin( p, "Items spawn:", dfsListItems, DFS_ClampCvar( "df_ndm_itemsSpawn", 2 ), 0, 0 );
}

static void DFS_BuildWeapons( void ) {		// 0x2db3
	dfPanel_t *p = DFS_NewPanel( PK_WEAPONS, "WEAPONS", MB_CAPS, DFS_PREVIEW_Y + 4, DFS_PANEL_X );
	int sx = p->x;
	int i, set;

	DFS_PSpin( p, "Weapons pickup:", dfsListOffOn, DFS_ClampCvar( "df_ndm_weaponsPickup", 1 ), 0, 0 );
	DFS_PSpin( p, "LG knockback:", dfsListOffOn, DFS_ClampCvar( "df_lg_Knockback", 1 ), 0, 0 );
	DFS_PSpin( p, "Hook type:", dfsListHook, DFS_ClampCvar( "df_hook_Type", 3 ), 0, 0 );
	set = (int)trap_Cvar_VariableValue( "df_ndm_weaponsSet" );
	for ( i = 1; i <= 10; i++ ) {
		menuradiobutton_s *c = &s_ds.check[i - 1];

		c->generic.type = MTYPE_RADIOBUTTON;
		c->generic.name = dfsWeaponNames[i];
		c->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
		c->generic.callback = DFS_PanelEvent;
		c->generic.id = ID_PANEL + ( p - s_ds.panel );
		c->generic.x = sx;
		c->generic.y = 0;
		c->curvalue = ( set & ( 1 << i ) ) != 0;
		DFS_PanelAdd( p, c, 0, i == 1 ? 20 : 0 );
	}
	s_ds.wnone.generic.type = MTYPE_PTEXT;
	s_ds.wnone.generic.flags = QMF_PULSEIFFOCUS | QMF_RIGHT_JUSTIFY;
	s_ds.wnone.generic.callback = DFS_PanelEvent;
	s_ds.wnone.generic.id = ID_WNONE;
	s_ds.wnone.generic.x = sx - 26;
	s_ds.wnone.generic.y = 0;
	s_ds.wnone.string = "None";
	s_ds.wnone.style = UI_RIGHT | UI_SMALLFONT;
	s_ds.wnone.color = text_color_normal;
	DFS_PanelAdd( p, &s_ds.wnone, 0, 2 );
	s_ds.wall.generic.type = MTYPE_PTEXT;
	s_ds.wall.generic.flags = QMF_PULSEIFFOCUS | QMF_LEFT_JUSTIFY;
	s_ds.wall.generic.callback = DFS_PanelEvent;
	s_ds.wall.generic.id = ID_WALL;
	s_ds.wall.generic.x = sx + 26;
	s_ds.wall.generic.y = 0;
	s_ds.wall.string = "All";
	s_ds.wall.style = UI_LEFT | UI_SMALLFONT;
	s_ds.wall.color = text_color_normal;
	DFS_PanelAdd( p, &s_ds.wall, 0, DFS_SAME );
}

static void DFS_BuildTimer( void ) {			// 0x326f / 0x12e5 / 0x1147
	dfPanel_t *p = DFS_NewPanel( PK_TIMER, "TIMER", MB_TRICKS, DFS_PREVIEW_Y + 4, DFS_PANEL_X - 10 );
	static const char *labels[3] = { "Timer Start:", "Checkpoint:", "Timer Stop:" };
	static const char *cv[3] = { "df_ndm_timer_start", "df_ndm_timer_checkpoint", "df_ndm_timer_stop" };
	int g, k;

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
			s->generic.x = p->x;
			s->generic.y = 0;
			// Menu_AddItem -> SpinControl_Init measures the item names for the hit box, so the lists have to be in place first
			// (the Item list is measured with the longest group so the hit box covers every group it can switch to)
			s->itemnames = k == 0 ? ( g == 0 ? dfsListT1 : dfsListT2 ) : k == 1 ? dfsListGroup : dfsListG0;
			DFS_PanelAdd( p, s, 0, ( k == 0 && g > 0 ) ? 20 : 0 );
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
	}
}

// ---------------------------------------------------------------------------------------------------------------
// mode, type, tabs

// what the panels show and the map is started as: the filter's mode, with ALL the mode of the selected map
static void DFS_ApplyMode( void ) {
	int m = s_ds.mode;

	if ( m == s_ds.appliedMode ) {
		return;
	}
	s_ds.appliedMode = m;
	if ( m < MODE_TRICKS ) {
		s_ds.wMove->curvalue = m;		// VQ3 / CPM movements carry over to Tricks and Fast Caps
	}
	DFS_SyncFromCvars();
	if ( !( s_ds.panel[s_ds.curPanel].modes & ( 1 << m ) ) ) {
		s_ds.curPanel = 0;
	}
	DFS_LayoutTabs();
	DFS_ShowPanel( s_ds.curPanel );
}

// the mode a map is started in: the filter's, with ALL the preferred mode if the map has it, else the first one it has
static int DFS_RowMode( const dfRec_t *r ) {
	int m = s_ds.pref;

	if ( s_ds.filter ) {
		return s_ds.filter - 1;
	}
	if ( r && !( r->modes & ( 1 << m ) ) ) {
		for ( m = 0; m < MODE_COUNT - 1 && !( r->modes & ( 1 << m ) ); m++ )
			;
	}
	return m;
}

static void DFS_ResolveMode( void ) {
	int m = DFS_RowMode( s_ds.sel );

	s_ds.mode = m;
	s_ds.playSpin.curvalue = m;
	DFS_ApplyMode();
}

static void DFS_SetFilter( int f ) {
	int i;

	s_ds.filter = f;
	trap_Cvar_Set( "df_ui_mapsMode", va( "%i", f ) );
	if ( f ) {
		s_ds.pref = f - 1;
		trap_Cvar_Set( "df_ui_mapsPlay", va( "%i", s_ds.pref ) );
	}
	for ( i = 0; i < 5; i++ ) {
		if ( f - 1 == MODE_FCAPS ) {
			s_ds.typeChip[i].generic.flags |= QMF_GRAYED;		// the quake3 maps have no type
		} else {
			s_ds.typeChip[i].generic.flags &= ~QMF_GRAYED;
		}
	}
	if ( f ) {
		s_ds.playSpin.generic.flags |= QMF_INACTIVE | QMF_HIDDEN;
	} else {
		s_ds.playSpin.generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
	}
	DFS_ResolveMode();
	DFS_Refilter( qtrue );
}

static void DFS_SetType( int t ) {
	s_ds.type = t;
	trap_Cvar_Set( "df_ui_mapsType", va( "%i", t ) );
	DFS_Refilter( qtrue );
}

static void DFS_ChipEvent( void *ptr, int event ) {
	int id = ( (menucommon_s *)ptr )->id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( id >= ID_MODE && id < ID_MODE + FILTER_COUNT ) {
		DFS_SetFilter( id - ID_MODE );
	} else if ( id >= ID_TYPE && id < ID_TYPE + 5 ) {
		DFS_SetType( id - ID_TYPE );
	} else {
		DFS_ShowPanel( id - ID_TAB );
	}
}

// ---------------------------------------------------------------------------------------------------------------
// run server (0x365a) and keys (0x36fc)

static void DFS_Go( void ) {
	int type;

	if ( !s_ds.sel ) {
		return;
	}
	DFS_ApplyMapInfo();
	trap_Cvar_Set( "df_ui_mapsLast", s_ds.sel->e->map );
	if ( !s_ds.filter ) {
		trap_Cvar_Set( "df_ui_mapsPlay", va( "%i", s_ds.mode ) );		// ALL starts the next map the same way
	}
	type = s_ds.mode < MODE_TRICKS ? 1 : s_ds.mode == MODE_TRICKS ? 2 : 3;
	if ( s_ds.runServer ) {
		type += 4;
	}
	trap_Cvar_Set( "df_gametype", va( "%i", type ) );
	trap_Cvar_Set( "sv_pure", "0" );
	if ( s_ds.runServer >= 1 ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "dedicated %i\n", s_ds.runServer - 1 ) );
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, va( s_ds.cheats ? "wait ; wait ; devmap %s\n" : "wait ; wait ; map %s\n", s_ds.sel->e->map ) );
}

// plays your best demo of the selected map in the mode it would start in; the map browser comes back when it is over
static void DFS_Watch( void ) {
	char name[256];
	const char *slash;

	if ( !s_ds.sel || !UI_DF_BestDemo( s_ds.sel->e->map, s_ds.mode, name, sizeof( name ) ) ) {
		Com_Printf( "^3The best demo of this map is not there any more (F5 in the demo browser lists the demos again)\n" );
		return;
	}
	trap_Cvar_Set( "df_ui_mapsLast", s_ds.sel->e->map );
	trap_Cvar_Set( "df_ui_mapsSearch", s_ds.search.field.buffer );
	trap_Cvar_Set( "df_ghosts_Command", va( "#1 demo %s", name ) );
	slash = strrchr( name, '/' );
	trap_Cvar_Set( "df_ui_mapsReturn", slash ? slash + 1 : name );		// the demo's file name, see UI_DemosReturnAfterPlayback
	UI_ForceMenuOff();
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s\"\n", name ) );
}

static qboolean dfsRestore;		// the menu comes back from a demo: the search text is put back

/*
===============
UI_DF_MapsReturnAfterPlayback

As UI_DemosReturnAfterPlayback, for a demo that WATCH started: the map browser opens on top of the main menu again, on the same
mode and map and with the same search.  Returns qtrue when it did.
===============
*/
qboolean UI_DF_MapsReturnAfterPlayback( void ) {
	char				want[MAX_QPATH], error[8];
	uiClientState_t		cs;

	trap_Cvar_VariableStringBuffer( "df_ui_mapsReturn", want, sizeof( want ) );
	if ( !want[0] ) {
		return qfalse;
	}
	trap_Cvar_Set( "df_ui_mapsReturn", "" );
	trap_Cvar_VariableStringBuffer( "com_errorMessage", error, sizeof( error ) );
	if ( error[0] ) {
		return qfalse;
	}
	trap_GetClientState( &cs );
	if ( cs.servername[0] && Q_stricmpn( cs.servername, want, strlen( want ) ) ) {
		return qfalse;
	}
	dfsRestore = qtrue;
	UI_DF_MapsMenu();
	return qtrue;
}

static void DFS_ButtonEvent( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( ( (menucommon_s *)ptr )->id ) {
	case ID_GO:
		DFS_Go();
		break;
	case ID_WATCH:
		DFS_Watch();
		break;
	default:
		UI_PopMenu();
		break;
	}
}

static qboolean DFS_IsChip( void *item ) {
	return item && ( (menucommon_s *)item )->callback == DFS_ChipEvent
		&& ( (menucommon_s *)item )->id < ID_TAB;
}

static void DFS_SearchChanged( void ) {
	if ( strcmp( s_ds.applied, s_ds.search.field.buffer ) ) {
		DFS_Refilter( qfalse );
	}
}

static sfxHandle_t DFS_Key( int key ) {
	sfxHandle_t	sound;
	void		*item;
	qboolean	inBrowser, typing;
	int			trackY, trackH, thumbY, thumbH;

	// left click on the scroll bar: grab the thumb (a click on the track puts the middle of the thumb there)
	if ( key == K_MOUSE1 && DFS_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH )
		&& UI_CursorInRect( DFS_SB_X, trackY, DFS_SB_W, trackH ) ) {
		if ( uis.cursory >= thumbY && uis.cursory < thumbY + thumbH ) {
			s_ds.sbGrab = uis.cursory - thumbY;
		} else {
			s_ds.sbGrab = thumbH / 2;
		}
		s_ds.sbDrag = qtrue;
		DFS_ScrollbarDrag();
		return menu_null_sound;
	}

	if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW
		|| key == K_ENTER || key == K_KP_ENTER || key == K_PGUP || key == K_PGDN ) {
		s_ds.keyNav = qtrue;
	}

	// mouse wheel: scroll the list wherever the focus is
	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		if ( s_ds.list.numitems ) {
			ScrollList_Key( &s_ds.list, key );
		}
		return menu_null_sound;
	}

	// F1..F5: the mode chips
	if ( key >= K_F1 && key <= K_F5 ) {
		if ( s_ds.filter != key - K_F1 ) {
			DFS_SetFilter( key - K_F1 );
		}
		return menu_move_sound;
	}

	// a printable character anywhere outside of the search box starts a search (the key press itself is swallowed, the
	// character event that follows is typed into the box)
	if ( !DFS_SearchFocused() ) {
		int ch = key & ~K_CHAR_FLAG;

		if ( ch > ' ' && ch < 127 ) {
			Menu_SetCursorToItem( &s_ds.menu, &s_ds.search );
			if ( !( key & K_CHAR_FLAG ) ) {
				return menu_null_sound;
			}
		}
	}
	typing = DFS_SearchFocused();
	item = Menu_ItemAtCursor( &s_ds.menu );
	inBrowser = typing || item == (void *)&s_ds.list || DFS_IsChip( item );

	// Escape with a search typed clears it (the first press); the next one leaves the menu
	if ( key == K_ESCAPE && s_ds.search.field.buffer[0] ) {
		MField_Clear( &s_ds.search.field );
		DFS_SearchChanged();
		return menu_null_sound;
	}

	if ( ( key == K_ENTER || key == K_KP_ENTER ) && trap_Key_IsDown( K_CTRL ) ) {
		DFS_Watch();
		return menu_move_sound;
	}

	if ( inBrowser ) {
		switch ( key ) {
		case K_UPARROW: case K_KP_UPARROW: case K_DOWNARROW: case K_KP_DOWNARROW:
		case K_PGUP: case K_KP_PGUP: case K_PGDN: case K_KP_PGDN:
			// the list is moved wherever the focus is in the browser, so the box can stay where it is
			return s_ds.list.numitems ? ScrollList_Key( &s_ds.list, key ) : menu_buzz_sound;
		case K_HOME: case K_KP_HOME: case K_END: case K_KP_END:
			if ( !typing ) {
				return s_ds.list.numitems ? ScrollList_Key( &s_ds.list, key ) : menu_buzz_sound;
			}
			break;
		case K_LEFTARROW: case K_KP_LEFTARROW: case K_RIGHTARROW: case K_KP_RIGHTARROW:
			// the cursor of a text keeps them, an empty box has no use for them
			if ( !typing || !s_ds.search.field.buffer[0] ) {
				int step = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? FILTER_COUNT - 1 : 1;

				DFS_SetFilter( ( s_ds.filter + step ) % FILTER_COUNT );
				return menu_move_sound;
			}
			break;
		case K_ENTER: case K_KP_ENTER:
			if ( typing || item == (void *)&s_ds.list ) {
				DFS_Go();
				return menu_move_sound;
			}
			break;
		}
	}

	sound = Menu_DefaultKey( &s_ds.menu, key );
	DFS_SearchChanged();

	// double click on a map starts it
	if ( key == K_MOUSE1 && UI_CursorInRect( s_ds.list.generic.x, s_ds.list.generic.y, s_ds.list.width * SMALLCHAR_WIDTH, s_ds.list.height * SMALLCHAR_HEIGHT ) ) {
		int row = s_ds.list.top + ( uis.cursory - s_ds.list.generic.y ) / SMALLCHAR_HEIGHT;

		if ( row == s_ds.list.curvalue && row < s_ds.list.numitems ) {
			int now = trap_Milliseconds();

			if ( s_ds.clickIndex == row && now - s_ds.clickTime <= DFS_DOUBLECLICK ) {
				s_ds.clickIndex = -1;
				DFS_Go();
			} else {
				s_ds.clickIndex = row;
				s_ds.clickTime = now;
			}
		}
	}
	return sound;
}

static void DFS_MenuDraw( void ) {
	if ( uis.cursorx != s_ds.lastCursorX || uis.cursory != s_ds.lastCursorY ) {
		s_ds.lastCursorX = uis.cursorx;
		s_ds.lastCursorY = uis.cursory;
		s_ds.keyNav = qfalse;		// the mouse is in charge again
	}
	if ( s_ds.sbDrag ) {
		if ( trap_Key_IsDown( K_MOUSE1 ) ) {
			DFS_ScrollbarDrag();
		} else {
			s_ds.sbDrag = qfalse;
		}
	}
	UI_DF_BestRun( 6 );		// your times: the demo lists are parsed a little every frame
	if ( s_ds.sel && UI_DF_BestFor( s_ds.sel->e->map, s_ds.mode, NULL ) ) {
		s_ds.watch.generic.flags &= ~QMF_GRAYED;
	} else {
		s_ds.watch.generic.flags |= QMF_GRAYED;
	}
	Menu_Draw( &s_ds.menu );
	DFS_ScrollbarDraw();
	UI_DrawString( DFS_LX + DFS_LW / 2, DFS_LIST_Y + DFS_LIST_ROWS * SMALLCHAR_HEIGHT + 6, s_ds.countText, UI_CENTER | UI_SMALLFONT, dfsColorWhite );
	if ( !s_ds.list.numitems ) {
		// say where the search does find maps
		int m, y = DFS_LIST_Y + 40;

		UI_DrawString( DFS_LX + DFS_LW / 2, y, dfsNumRecs ? "No maps match" : "No maps found (scripts/*.defi)", UI_CENTER | UI_SMALLFONT, dfsColorWhite );
		for ( m = 0; m < FILTER_COUNT; m++ ) {
			if ( m != s_ds.filter && s_ds.counts[m] ) {
				y += SMALLCHAR_HEIGHT + 2;
				UI_DrawString( DFS_LX + DFS_LW / 2, y, va( "%i in %s", s_ds.counts[m], dfsFilterNames[m] ), UI_CENTER | UI_SMALLFONT, dfsHint );
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------

static void DFS_InitChip( menutext_s *c, int id, const char *label, int x, int y, int w, int h ) {
	Com_Memset( c, 0, sizeof( *c ) );
	c->generic.type = MTYPE_PTEXT;
	c->generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	c->generic.id = id;
	c->generic.x = x;
	c->generic.y = y;
	c->generic.callback = DFS_ChipEvent;
	c->generic.ownerdraw = DFS_ChipDraw;
	c->string = (char *)label;
	Menu_AddItem( &s_ds.menu, c );
	// the widget worked out its mouse area from the text: use the plate
	c->generic.left = x;
	c->generic.right = x + w - 1;
	c->generic.top = y;
	c->generic.bottom = y + h - 1;
}

static void DFS_InitButton( menutext_s *b, int id, const char *label, int x ) {
	Com_Memset( b, 0, sizeof( *b ) );
	b->generic.type = MTYPE_PTEXT;
	b->generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	b->generic.id = id;
	b->generic.x = x;
	b->generic.y = DFS_BUTTON_Y;
	b->generic.callback = DFS_ButtonEvent;
	b->generic.ownerdraw = DFS_ButtonDraw;
	b->string = (char *)label;
	Menu_AddItem( &s_ds.menu, b );
	b->generic.left = x;
	b->generic.right = x + DFS_BUTTON_W - 1;
	b->generic.top = DFS_BUTTON_Y;
	b->generic.bottom = DFS_BUTTON_Y + DFS_BUTTON_H - 1;
}

static void DFS_InitRunSpin( menulist_s *s, const char *label, int id, int y, const char **list ) {
	s->generic.type = MTYPE_SPINCONTROL;
	s->generic.name = label;
	s->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.callback = DFS_RunEvent;
	s->generic.id = id;
	s->generic.x = DFS_PANEL_X;
	s->generic.y = y;
	s->itemnames = list;
	s->numitems = DFS_Count( list );
	Menu_AddItem( &s_ds.menu, s );
}

/*
===============
UI_DF_MapsMenu (ui 0x3825)
===============
*/
void UI_DF_MapsMenu( void ) {
	char last[MAX_QPATH];
	int i, x;

	Com_Memset( &s_ds, 0, sizeof( s_ds ) );
	s_ds.menu.wrapAround = qtrue;
	s_ds.menu.fullscreen = qtrue;
	s_ds.menu.key = DFS_Key;
	s_ds.menu.draw = DFS_MenuDraw;
	s_ds.clickIndex = -1;
	s_ds.filter = DFS_ClampCvar( "df_ui_mapsMode", FILTER_COUNT - 1 );
	s_ds.pref = DFS_ClampCvar( "df_ui_mapsPlay", MODE_COUNT - 1 );
	s_ds.appliedMode = -1;
	s_ds.type = DFS_ClampCvar( "df_ui_mapsType", 4 );

	DFS_BuildRecs();
	UI_DF_BestLoad();

	s_ds.search.generic.type = MTYPE_FIELD;
	s_ds.search.generic.flags = QMF_SMALLFONT | QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_ds.search.generic.id = ID_SEARCH;
	s_ds.search.generic.ownerdraw = DFS_FieldDraw;
	s_ds.search.generic.x = DFS_LX + 4;
	s_ds.search.generic.y = DFS_FIELD_Y;
	s_ds.search.field.widthInChars = DFS_FIELD_CHARS;
	s_ds.search.field.maxchars = DFS_SEARCHMAX;
	Menu_AddItem( &s_ds.menu, &s_ds.search );
	s_ds.search.generic.left = DFS_LX;
	s_ds.search.generic.right = DFS_LX + DFS_LW - 1;
	s_ds.search.generic.top = DFS_FIELD_Y - 2;
	s_ds.search.generic.bottom = DFS_FIELD_Y - 2 + SMALLCHAR_HEIGHT + 3;

	s_ds.list.generic.type = MTYPE_SCROLLLIST;
	s_ds.list.generic.flags = QMF_PULSEIFFOCUS;
	s_ds.list.generic.id = ID_LIST;
	s_ds.list.generic.callback = DFS_ListEvent;
	s_ds.list.generic.ownerdraw = DFS_ListDraw;
	s_ds.list.generic.x = DFS_LX;
	s_ds.list.generic.y = DFS_LIST_Y;
	s_ds.list.width = DFS_LIST_CHARS;
	s_ds.list.height = DFS_LIST_ROWS;
	s_ds.list.columns = 1;
	s_ds.list.itemnames = dfsBlank;
	Menu_AddItem( &s_ds.menu, &s_ds.list );

	x = DFS_LX;
	for ( i = 0; i < FILTER_COUNT; i++ ) {
		DFS_InitChip( &s_ds.modeChip[i], ID_MODE + i, dfsFilterNames[i], x, DFS_MODE_Y, dfsFilterW[i], DFS_MODE_H );
		x += dfsFilterW[i] + 4;
	}
	x = DFS_LX;
	for ( i = 0; i < 5; i++ ) {
		int w = strlen( dfsTypeNames[i] ) * SMALLCHAR_WIDTH + 12;

		DFS_InitChip( &s_ds.typeChip[i], ID_TYPE + i, dfsTypeNames[i], x, DFS_TYPE_Y, w, DFS_TYPE_H );
		x += w + 4;
	}

	// the panel tabs (positioned by DFS_LayoutTabs), then the panels in the same order
	for ( i = 0; i < DFS_MAXPANEL; i++ ) {
		static const char *titles[DFS_MAXPANEL] = { "INFO", "GAMEPLAY", "WEAPONS", "TIMER" };

		DFS_InitChip( &s_ds.tab[i], ID_TAB + i, titles[i], DFS_RX, DFS_TAB_Y, 64, DFS_TAB_H );
	}
	DFS_BuildMapInfo();
	DFS_BuildGameplay();
	DFS_BuildWeapons();
	DFS_BuildTimer();

	DFS_InitRunSpin( &s_ds.playSpin, "Play as:", ID_PLAY, DFS_PLAY_Y, dfsListPlay );
	DFS_InitRunSpin( &s_ds.cheatsSpin, "Server Cheats:", ID_CHEATS, DFS_RUN_Y, dfsListOffOn );
	DFS_InitRunSpin( &s_ds.runSpin, "Run Server:", ID_RUN, DFS_RUN_Y + 20, dfsListRun );

	DFS_InitButton( &s_ds.back, ID_BACK, "BACK", DFS_BUTTON_MARGIN );
	DFS_InitButton( &s_ds.watch, ID_WATCH, "WATCH", 640 - DFS_BUTTON_MARGIN - 2 * DFS_BUTTON_W - 6 );
	s_ds.watch.generic.flags |= QMF_GRAYED;
	DFS_InitButton( &s_ds.go, ID_GO, "START", 640 - DFS_BUTTON_MARGIN - DFS_BUTTON_W );

	DFS_SetFilter( s_ds.filter );
	if ( dfsRestore ) {
		dfsRestore = qfalse;
		trap_Cvar_VariableStringBuffer( "df_ui_mapsSearch", s_ds.search.field.buffer, DFS_SEARCHMAX + 1 );
		s_ds.search.field.cursor = strlen( s_ds.search.field.buffer );
		DFS_Refilter( qfalse );
	}
	trap_Cvar_VariableStringBuffer( "df_ui_mapsLast", last, sizeof( last ) );
	if ( last[0] ) {
		DFS_SelectByName( last );
	}
	UI_PushMenu( &s_ds.menu );
}
