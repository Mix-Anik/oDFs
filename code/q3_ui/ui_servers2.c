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

MULTIPLAYER MENU (SERVER BROWSER)

oDFs: the list, ping and master logic are the DeFRaG / Quake 3 ones, the menu is made in the colours of the map and demo
browsers, but plain: a search box, the source and the filters as small tabs, a list with sortable column headings and a
scroll bar, and the selected server (levelshot, details, players) beside it.

The mode column (run / tricks / fast caps) is not in the servers' info reply, so it is asked from their full status, a few at
a time in the background.

=======================================================================
*/


#include "ui_local.h"


#define MAX_GLOBALSERVERS		256
#define MAX_PINGREQUESTS		32
#define MAX_ADDRESSLENGTH		64
#define MAX_HOSTNAMELENGTH		40
#define MAX_MAPNAMELENGTH		24
#define MAX_LISTBOXITEMS		256
#define MAX_LOCALSERVERS		128
#define MAX_STATUSLENGTH		96

#define ART_UNKNOWNMAP			"menu/art/unknownmap"

#define SB_SEARCHMAX			48
#define SB_ADDRMAX				60
#define SB_MAXWORDS				4
#define SB_MAXPLAYERS			16
#define SB_BULK					3			// servers whose full status is asked at the same time
#define SB_STATUS_TIMEOUT		3000
#define SB_DOUBLECLICK			600
#define SB_NOTICE_TIME			4000

// layout (640 x 480)
#define SB_LX			10
#define SB_LW			424
#define SB_FIELD_Y		9
#define SB_SRC_Y		33
#define SB_TYPE_Y		55
#define SB_CHIP_H		18
#define SB_HEAD_Y		80
#define SB_HEAD_H		16
#define SB_LIST_Y		98
#define SB_LIST_ROWS	18
#define SB_SB_X			436
#define SB_SB_W			12
#define SB_SB_MINTHUMB	20
#define SB_RX			462
#define SB_RW			168
#define SB_STATUS_Y		( SB_LIST_Y + SB_LIST_ROWS * SMALLCHAR_HEIGHT + 6 )
#define SB_ADDR_Y		416
#define SB_BUTTON_H		26
#define SB_BUTTON_Y		442
#define SB_BUTTON_MARGIN	10

// the columns of the list, from its left edge
#define SB_COL_STAR		4
#define SB_COL_LOCK		12
#define SB_COL_NAME		22		// 20 characters
#define SB_COL_MAP		186		// 13 characters
#define SB_COL_MODE		296		// 6 characters
#define SB_COL_PLAYERS	386		// right edge
#define SB_COL_PING		418		// right edge
#define SB_NAME_CHARS	20
#define SB_MAP_CHARS	13

#define ID_SEARCH			1
#define ID_ADDR				2
#define ID_BACK				4
#define ID_REFRESH			5
#define ID_FAV				6
#define ID_CONNECT			7
#define ID_LIST				8
#define ID_SRC				20		// + UIAS_*
#define ID_TYPE				40		// + 0 any, 1 run, 2 tricks, 3 fast caps
#define ID_SHOWFULL			50
#define ID_SHOWEMPTY		51
#define ID_SORT				60		// + column

#define UIAS_LOCAL			0
#define UIAS_GLOBAL0			1
#define UIAS_GLOBAL1			2
#define UIAS_GLOBAL2			3
#define UIAS_GLOBAL3			4
#define UIAS_GLOBAL4			5
#define UIAS_GLOBAL5			6
#define UIAS_FAVORITES			7
#define UIAS_NUM_SOURCES		8

#define UI_MAX_MASTER_SERVERS	6

#define SORT_HOST			0
#define SORT_MAP			1
#define SORT_CLIENTS		2
#define UISORT_PING			3
#define SORT_MODE			5		// 4 is what an old ui_browserSortKey default says: that one is the ping, too
#define SB_COLUMNS			5

const char* punkbuster_items[] = {
	"Disabled",
	"Enabled",
	NULL
};

static const char *sbSourceNames[UIAS_NUM_SOURCES] = { "LOCAL", "INTERNET", "M 1", "M 2", "M 3", "M 4", "M 5", "FAVORITES" };
static const int sbSourceOrder[UIAS_NUM_SOURCES] = { UIAS_GLOBAL0, UIAS_LOCAL, UIAS_FAVORITES, UIAS_GLOBAL1, UIAS_GLOBAL2, UIAS_GLOBAL3, UIAS_GLOBAL4, UIAS_GLOBAL5 };
static const char *sbTypeNames[4] = { "ALL", "RUN", "TRICKS", "FAST CAPS" };
static const char *sbTypeShort[4] = { "-", "RUN", "TRICKS", "FCAPS" };

// the columns: heading, sort key, left edge and width (pixels, from the list's left edge)
static const char *sbColName[SB_COLUMNS] = { "NAME", "MAP", "MODE", "PLAYERS", "PING" };
static const int sbColSort[SB_COLUMNS] = { SORT_HOST, SORT_MAP, SORT_MODE, SORT_CLIENTS, UISORT_PING };
static const int sbColX[SB_COLUMNS] = { SB_COL_NAME, SB_COL_MAP, SB_COL_MODE, SB_COL_PLAYERS - 56, SB_COL_PING - 32 };
static const int sbColW[SB_COLUMNS] = { 160, 104, 48, 56, 32 };

static vec4_t	sbWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	sbPlate = { 0.14f, 0.02f, 0.02f, 0.90f };
static vec4_t	sbPlateFocus = { 0.26f, 0.04f, 0.03f, 0.95f };
static vec4_t	sbPanel = { 0.10f, 0.01f, 0.01f, 0.62f };
static vec4_t	sbBorder = { 0.62f, 0.10f, 0.08f, 1.0f };
static vec4_t	sbBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	sbHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	sbLabel = { 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	sbChipFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	sbChipFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	sbChipFillSel = { 0.85f, 0.12f, 0.06f, 0.60f };
static vec4_t	sbChipBorder = { 0.80f, 0.14f, 0.10f, 1.0f };
static vec4_t	sbChipBorderSel = { 1.0f, 0.38f, 0.22f, 1.0f };
static vec4_t	sbChipOff = { 0.45f, 0.35f, 0.33f, 1.0f };
static vec4_t	sbGold = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	sbGood = { 0.55f, 0.85f, 0.55f, 1.0f };
static vec4_t	sbWarn = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	sbBad = { 1.0f, 0.35f, 0.25f, 1.0f };
static vec4_t	sbBlue = { 0.45f, 0.65f, 1.0f, 1.0f };

typedef struct {
	char	adrstr[MAX_ADDRESSLENGTH];
	int		start;
} pinglist_t;

typedef struct servernode_s {
	char	adrstr[MAX_ADDRESSLENGTH];
	char	hostname[MAX_HOSTNAMELENGTH+3];
	char	mapname[MAX_MAPNAMELENGTH];
	char	game[16];
	int		numclients;
	int		maxclients;
	int		pingtime;
	int		minPing;
	int		maxPing;
	int		dftype;			// defrag_gametype: 1 run, 2 tricks, 3 fast caps, 0 not known
	int		version;		// defrag_vers, 0 not known
	qboolean needpass;
	qboolean anon;			// sv_allowAnonymous
	qboolean responded;		// the info reply is in (a favorite that does not answer is only its address)
	qboolean probed;		// the full status has been asked (for the mode)
} servernode_t;

typedef struct {
	char	name[24];
	int		score;
	int		ping;
} sbplayer_t;

typedef struct {
	char		adr[MAX_ADDRESSLENGTH];		// the server the panel is for
	int			state;						// 0 nothing, 1 asked, 2 answered, 3 no answer
	int			start;
	int			total;						// players on the server
	int			count;						// the ones in players[]
	sbplayer_t	players[SB_MAXPLAYERS];
} sbdetail_t;

typedef struct {
	char		adr[MAX_ADDRESSLENGTH];
	int			start;
} sbbulk_t;

typedef struct {
	menuframework_s		menu;

	menufield_s			search;
	menufield_s			addr;
	menulist_s			list;
	menutext_s			src[UIAS_NUM_SOURCES];
	menutext_s			type[4];
	menutext_s			showfull, showempty;
	menutext_s			sorth[SB_COLUMNS];
	menutext_s			back, refresh, fav, connect;

	pinglist_t			pinglist[MAX_PINGREQUESTS];
	servernode_t*		view[MAX_LISTBOXITEMS];
	const char*			items[MAX_LISTBOXITEMS];
	int					numqueriedservers;
	int					*numservers;
	servernode_t		*serverlist;
	int					currentping;
	qboolean			refreshservers;
	int					nextpingtime;
	int					maxservers;
	int					refreshtime;
	char				favoriteaddresses[MAX_FAVORITESERVERS][MAX_ADDRESSLENGTH];
	int					numfavoriteaddresses;

	char				status[MAX_STATUSLENGTH];
	char				notice[MAX_STATUSLENGTH];
	int					noticeTime;
	char				selAdr[MAX_ADDRESSLENGTH];		// the selected server, found again when the list is made anew
	char				applied[SB_SEARCHMAX + 1];		// the search text the list was made for
	char				words[SB_MAXWORDS][SB_SEARCHMAX + 1];
	int					numwords;

	qboolean			keyNav;
	int					lastCursorX, lastCursorY;
	qboolean			sbDrag;
	int					sbGrab;
	int					clickIndex, clickTime;

	sbdetail_t			detail;
	sbbulk_t			bulk[SB_BULK];

	char				previewMap[MAX_MAPNAMELENGTH];
	qhandle_t			previewShader;
} arenaservers_t;

static arenaservers_t	g_arenaservers;

static servernode_t		g_globalserverlist[UI_MAX_MASTER_SERVERS][MAX_GLOBALSERVERS];
static int				g_numglobalservers[UI_MAX_MASTER_SERVERS];
static servernode_t		g_localserverlist[MAX_LOCALSERVERS];
static int				g_numlocalservers;
static servernode_t		g_favoriteserverlist[MAX_FAVORITESERVERS];
static int				g_numfavoriteservers;
static int				g_servertype;
static int				g_sortkey;
static int				g_sortrev;
static int				g_typefilter;
static int				g_emptyservers;		// show the servers without players
static int				g_fullservers;		// show the full ones

static void ArenaServers_UpdateMenu( void );
static void ArenaServers_StartRefresh( void );


/*
=================
ArenaServers_MaxPing
=================
*/
static int ArenaServers_MaxPing( void ) {
	int		maxPing;

	maxPing = (int)trap_Cvar_VariableValue( "cl_maxPing" );
	if( maxPing < 100 ) {
		maxPing = 100;
	}
	return maxPing;
}


/*
=================
Server mode: defrag_gametype and defrag_vers from an info string (the status)
=================
*/
static void SB_SetMode( servernode_t *n, const char *info ) {
	const char	*t, *v;
	int			g;

	t = Info_ValueForKey( info, "defrag_gametype" );
	v = Info_ValueForKey( info, "defrag_vers" );
	if ( *t ) {
		g = atoi( t );
		if ( g > 4 ) {
			g -= 4;		// 5..7: the same modes on a dedicated server
		}
		if ( g >= 1 && g <= 3 ) {
			n->dftype = g;
		}
	}
	if ( *v ) {
		n->version = atoi( v );
	}
}

static qboolean SB_ModeKnown( const servernode_t *n ) {
	return n->dftype != 0;
}

// "TRICKS", "-" when the server has not said
static const char *SB_ModeText( const servernode_t *n ) {
	return sbTypeShort[n->dftype];
}

static int SB_ModeRank( const servernode_t *n ) {
	return n->dftype ? n->dftype : 99;
}


/*
=================
ArenaServers_Compare
=================
*/
static int QDECL ArenaServers_Compare( const void *arg1, const void *arg2 ) {
	const servernode_t*	t1 = (const servernode_t *)arg1;
	const servernode_t*	t2 = (const servernode_t *)arg2;
	int					r = 0;

	switch( g_sortkey ) {
	case SORT_HOST:
		r = Q_stricmp( t1->hostname, t2->hostname );
		break;

	case SORT_MAP:
		r = Q_stricmp( t1->mapname, t2->mapname );
		if ( !r ) {
			r = Q_stricmp( t1->hostname, t2->hostname );
		}
		break;

	case SORT_CLIENTS:
		// the most players first
		r = t2->numclients - t1->numclients;
		if ( !r ) {
			r = Q_stricmp( t1->hostname, t2->hostname );
		}
		break;

	case SORT_MODE:
		r = SB_ModeRank( t1 ) - SB_ModeRank( t2 );
		if ( !r ) {
			r = Q_stricmp( t1->hostname, t2->hostname );
		}
		break;

	default:
		r = t1->pingtime - t2->pingtime;
		if ( !r ) {
			r = Q_stricmp( t1->hostname, t2->hostname );
		}
		break;
	}

	return g_sortrev ? -r : r;
}

static void SB_SortList( void ) {
	if ( g_arenaservers.numservers && *g_arenaservers.numservers > 1 ) {
		qsort( g_arenaservers.serverlist, *g_arenaservers.numservers, sizeof( servernode_t ), ArenaServers_Compare );
	}
}

/*
=================
ArenaServers_SourceForLAN

Convert ui's g_servertype to AS_* used by trap calls.
=================
*/
int ArenaServers_SourceForLAN(void) {
	switch( g_servertype ) {
	default:
	case UIAS_LOCAL:
		return AS_LOCAL;
	case UIAS_GLOBAL0:
	case UIAS_GLOBAL1:
	case UIAS_GLOBAL2:
	case UIAS_GLOBAL3:
	case UIAS_GLOBAL4:
	case UIAS_GLOBAL5:
		return AS_GLOBAL;
	case UIAS_FAVORITES:
		return AS_FAVORITES;
	}
}


/*
=================
Selection
=================
*/
static servernode_t *SB_Selected( void ) {
	menulist_s	*l = &g_arenaservers.list;

	if ( l->numitems > 0 && l->curvalue >= 0 && l->curvalue < l->numitems ) {
		return g_arenaservers.view[l->curvalue];
	}
	return NULL;
}

static void SB_SelectionChanged( void ) {
	servernode_t	*n = SB_Selected();

	Q_strncpyz( g_arenaservers.selAdr, n ? n->adrstr : "", sizeof( g_arenaservers.selAdr ) );
}

static void SB_Notice( const char *text ) {
	Q_strncpyz( g_arenaservers.notice, text, sizeof( g_arenaservers.notice ) );
	g_arenaservers.noticeTime = uis.realtime;
}

static void ArenaServers_Go( void ) {
	servernode_t*	servernode = SB_Selected();

	if( servernode ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", servernode->adrstr ) );
	}
}


/*
=================
Favorites
=================
*/
static int SB_FavIndex( const char *adr ) {
	int		i;

	for ( i = 0; i < g_arenaservers.numfavoriteaddresses; i++ ) {
		if ( !Q_stricmp( g_arenaservers.favoriteaddresses[i], adr ) ) {
			return i;
		}
	}
	return -1;
}

/*
=================
ArenaServers_SaveChanges
=================
*/
void ArenaServers_SaveChanges( void )
{
	int	i;

	for (i=0; i<g_arenaservers.numfavoriteaddresses; i++)
		trap_Cvar_Set( va("server%d",i+1), g_arenaservers.favoriteaddresses[i] );

	for (; i<MAX_FAVORITESERVERS; i++)
		trap_Cvar_Set( va("server%d",i+1), "" );
}

static void SB_FavRemove( const char *adr )
{
	int		i;

	i = SB_FavIndex( adr );
	if ( i >= 0 ) {
		if ( i < g_arenaservers.numfavoriteaddresses - 1 ) {
			memmove( g_arenaservers.favoriteaddresses[i], g_arenaservers.favoriteaddresses[i+1], ( g_arenaservers.numfavoriteaddresses - i - 1 ) * MAX_ADDRESSLENGTH );
		}
		g_arenaservers.numfavoriteaddresses--;
		memset( g_arenaservers.favoriteaddresses[g_arenaservers.numfavoriteaddresses], 0, MAX_ADDRESSLENGTH );
	}

	for ( i = 0; i < g_numfavoriteservers; i++ ) {
		if ( !Q_stricmp( g_favoriteserverlist[i].adrstr, adr ) ) {
			if ( i < g_numfavoriteservers - 1 ) {
				memmove( &g_favoriteserverlist[i], &g_favoriteserverlist[i+1], ( g_numfavoriteservers - i - 1 ) * sizeof( servernode_t ) );
			}
			g_numfavoriteservers--;
			memset( &g_favoriteserverlist[g_numfavoriteservers], 0, sizeof( servernode_t ) );
			break;
		}
	}

	ArenaServers_SaveChanges();
	if ( g_servertype == UIAS_FAVORITES ) {
		g_arenaservers.numqueriedservers = g_arenaservers.numfavoriteaddresses;
		g_arenaservers.currentping       = g_arenaservers.numfavoriteaddresses;
	}
	ArenaServers_UpdateMenu();
}

// node: the server as the list has it, or NULL for an address that was typed
static void SB_FavAdd( const char *adr, const servernode_t *node )
{
	qboolean	complete;

	if ( !adr[0] || SB_FavIndex( adr ) >= 0 ) {
		return;
	}
	if ( g_arenaservers.numfavoriteaddresses >= MAX_FAVORITESERVERS ) {
		SB_Notice( va( "The favorites list is full (%d servers).", MAX_FAVORITESERVERS ) );
		return;
	}

	// the favorites list holds results for all the addresses, or none (then it is refreshed when it is shown)
	complete = g_numfavoriteservers == g_arenaservers.numfavoriteaddresses;
	Q_strncpyz( g_arenaservers.favoriteaddresses[g_arenaservers.numfavoriteaddresses], adr, MAX_ADDRESSLENGTH );
	g_arenaservers.numfavoriteaddresses++;
	if ( complete && node ) {
		memcpy( &g_favoriteserverlist[g_numfavoriteservers], node, sizeof( servernode_t ) );
		g_numfavoriteservers++;
	} else {
		g_numfavoriteservers = 0;
	}
	ArenaServers_SaveChanges();
	SB_Notice( "Added to the favorites." );
	if ( g_servertype == UIAS_FAVORITES ) {
		ArenaServers_StartRefresh();
	}
}

static void SB_FavToggle( void )
{
	servernode_t	*n = SB_Selected();
	char			adr[MAX_ADDRESSLENGTH];

	if ( !n ) {
		return;
	}
	Q_strncpyz( adr, n->adrstr, sizeof( adr ) );
	if ( SB_FavIndex( adr ) >= 0 ) {
		SB_FavRemove( adr );
		SB_Notice( "Removed from the favorites." );
	} else {
		SB_FavAdd( adr, n );
	}
}


/*
=================
Search and the filters
=================
*/
static void SB_ParseSearch( void ) {
	const char	*s = g_arenaservers.search.field.buffer;
	int			n, len;

	Q_strncpyz( g_arenaservers.applied, s, sizeof( g_arenaservers.applied ) );
	g_arenaservers.numwords = 0;
	while ( *s && g_arenaservers.numwords < SB_MAXWORDS ) {
		while ( *s == ' ' ) {
			s++;
		}
		if ( !*s ) {
			break;
		}
		for ( len = 0; s[len] && s[len] != ' '; len++ )
			;
		n = g_arenaservers.numwords++;
		Q_strncpyz( g_arenaservers.words[n], s, len + 1 > SB_SEARCHMAX + 1 ? SB_SEARCHMAX + 1 : len + 1 );
		s += len;
	}
}

// the search words are all found in the name, the map or the address
static qboolean SB_Matches( const servernode_t *n ) {
	char	hay[MAX_HOSTNAMELENGTH + MAX_MAPNAMELENGTH + MAX_ADDRESSLENGTH + 8];
	int		i;

	if ( !g_emptyservers && !n->numclients ) {
		return qfalse;
	}
	if ( !g_fullservers && n->maxclients && n->numclients >= n->maxclients ) {
		return qfalse;
	}
	if ( g_typefilter && n->dftype != g_typefilter ) {
		return qfalse;
	}
	if ( g_arenaservers.numwords ) {
		Com_sprintf( hay, sizeof( hay ), "%s %s %s", n->hostname, n->mapname, n->adrstr );
		for ( i = 0; i < g_arenaservers.numwords; i++ ) {
			if ( !Q_stristr( hay, g_arenaservers.words[i] ) ) {
				return qfalse;
			}
		}
	}
	return qtrue;
}


/*
=================
ArenaServers_UpdateMenu

Makes the list of what the filters let through (the selected server stays selected), and the status line.
=================
*/
static void ArenaServers_UpdateMenu( void ) {
	menulist_s		*l = &g_arenaservers.list;
	servernode_t	*node;
	int				i, j, sel, total, maxTop;

	total = *g_arenaservers.numservers;
	node = g_arenaservers.serverlist;
	for ( i = 0, j = 0; i < total && j < MAX_LISTBOXITEMS; i++, node++ ) {
		if ( SB_Matches( node ) ) {
			g_arenaservers.view[j++] = node;
		}
	}

	sel = -1;
	if ( g_arenaservers.selAdr[0] ) {
		for ( i = 0; i < j; i++ ) {
			if ( !Q_stricmp( g_arenaservers.view[i]->adrstr, g_arenaservers.selAdr ) ) {
				sel = i;
				break;
			}
		}
	}
	l->numitems = j;
	if ( sel >= 0 ) {
		l->curvalue = sel;
		maxTop = j - l->height;
		if ( maxTop < 0 ) {
			maxTop = 0;
		}
		if ( l->top > maxTop ) {
			l->top = maxTop;
		}
		if ( l->curvalue < l->top ) {
			l->top = l->curvalue;
		}
		if ( l->curvalue >= l->top + l->height ) {
			l->top = l->curvalue - l->height + 1;
		}
	} else {
		l->curvalue = 0;
		l->top = 0;
	}
	SB_SelectionChanged();

	if ( g_arenaservers.refreshservers ) {
		if ( g_arenaservers.numqueriedservers > 0 ) {
			Com_sprintf( g_arenaservers.status, sizeof( g_arenaservers.status ), "Pinging %d of %d servers   -   SPACE stops",
				g_arenaservers.currentping > g_arenaservers.numqueriedservers ? g_arenaservers.numqueriedservers : g_arenaservers.currentping,
				g_arenaservers.numqueriedservers );
		} else {
			Q_strncpyz( g_arenaservers.status, "Scanning for servers...   -   SPACE stops", sizeof( g_arenaservers.status ) );
		}
	} else if ( total > 0 ) {
		if ( j == total ) {
			Com_sprintf( g_arenaservers.status, sizeof( g_arenaservers.status ), "%d server%s   -   F5 refreshes", total, total == 1 ? "" : "s" );
		} else {
			Com_sprintf( g_arenaservers.status, sizeof( g_arenaservers.status ), "%d of %d servers   -   F5 refreshes", j, total );
		}
	} else if ( g_arenaservers.numqueriedservers < 0 ) {
		Q_strncpyz( g_arenaservers.status, "No response from the master server.", sizeof( g_arenaservers.status ) );
	} else {
		Q_strncpyz( g_arenaservers.status, "No servers found.   -   F5 refreshes", sizeof( g_arenaservers.status ) );
	}
}


/*
=================
ArenaServers_Insert
=================
*/
static void ArenaServers_Insert( char* adrstr, char* info, int pingtime )
{
	servernode_t*	servernodeptr;

	if ((pingtime >= ArenaServers_MaxPing()) && (g_servertype != UIAS_FAVORITES))
	{
		// slow global or local servers do not get entered
		return;
	}

	// DeFRaG (ui 0x153d4): only servers whose game starts with "defrag" are listed (df_ui_browserDefragOnly 0 = every game);
	// the favorites are your own choice and stay
	if ( g_servertype != UIAS_FAVORITES && trap_Cvar_VariableValue( "df_ui_browserDefragOnly" ) && Q_stricmpn( Info_ValueForKey( info, "game" ), "defrag", 6 ) ) {
		return;
	}

	if (*g_arenaservers.numservers >= g_arenaservers.maxservers) {
		// list full;
		servernodeptr = g_arenaservers.serverlist+(*g_arenaservers.numservers)-1;
	} else {
		// next slot
		servernodeptr = g_arenaservers.serverlist+(*g_arenaservers.numservers);
		(*g_arenaservers.numservers)++;
	}

	memset( servernodeptr, 0, sizeof( *servernodeptr ) );
	servernodeptr->responded = info[0] != '\0';

	Q_strncpyz( servernodeptr->adrstr, adrstr, MAX_ADDRESSLENGTH );

	Q_strncpyz( servernodeptr->hostname, Info_ValueForKey( info, "hostname"), MAX_HOSTNAMELENGTH );
	Q_CleanStr( servernodeptr->hostname );
	if ( !servernodeptr->hostname[0] ) {
		Q_strncpyz( servernodeptr->hostname, adrstr, MAX_HOSTNAMELENGTH );
	}

	Q_strncpyz( servernodeptr->mapname, Info_ValueForKey( info, "mapname"), MAX_MAPNAMELENGTH );
	Q_CleanStr( servernodeptr->mapname );
	Q_strlwr( servernodeptr->mapname );

	Q_strncpyz( servernodeptr->game, Info_ValueForKey( info, "game"), sizeof( servernodeptr->game ) );

	servernodeptr->numclients = atoi( Info_ValueForKey( info, "clients") );
	servernodeptr->maxclients = atoi( Info_ValueForKey( info, "sv_maxclients") );
	servernodeptr->pingtime   = pingtime;
	servernodeptr->minPing    = atoi( Info_ValueForKey( info, "minPing") );
	servernodeptr->maxPing    = atoi( Info_ValueForKey( info, "maxPing") );
	servernodeptr->needpass   = atoi( Info_ValueForKey( info, "g_needpass") ) != 0;
	// DeFRaG (0x154e9): servers that allow anonymous clients are marked
	servernodeptr->anon       = atoi( Info_ValueForKey( info, "sv_allowAnonymous") ) != 0;

	SB_SetMode( servernodeptr, info );
}


/*
=================
ArenaServers_LoadFavorites

Load cvar address book entries into local lists.
=================
*/
void ArenaServers_LoadFavorites( void )
{
	int				i;
	int				j;
	int				numtempitems;
	char			adrstr[MAX_ADDRESSLENGTH];
	servernode_t	templist[MAX_FAVORITESERVERS];
	qboolean		found;

	found        = qfalse;

	// copy the old
	memcpy( templist, g_favoriteserverlist, sizeof(servernode_t)*MAX_FAVORITESERVERS );
	numtempitems = g_numfavoriteservers;

	// clear the current for sync
	memset( g_favoriteserverlist, 0, sizeof(servernode_t)*MAX_FAVORITESERVERS );
	g_numfavoriteservers = 0;

	// resync existing results with new or deleted cvars
	for (i=0; i<MAX_FAVORITESERVERS; i++)
	{
		trap_Cvar_VariableStringBuffer( va("server%d",i+1), adrstr, MAX_ADDRESSLENGTH );
		if (!adrstr[0])
			continue;

		// favorite server addresses must be maintained outside refresh list
		// this mimics local and global netadr's stored in client
		// these can be fetched to fill ping list
		strcpy( g_arenaservers.favoriteaddresses[g_numfavoriteservers], adrstr );

		// find this server in the old list
		for (j=0; j<numtempitems; j++)
			if (!Q_stricmp( templist[j].adrstr, adrstr ))
				break;

		if (j < numtempitems)
		{
			// found server - add exisiting results
			memcpy( &g_favoriteserverlist[g_numfavoriteservers], &templist[j], sizeof(servernode_t) );
			found = qtrue;
		}
		else
		{
			// add new server
			Q_strncpyz( g_favoriteserverlist[g_numfavoriteservers].adrstr, adrstr, MAX_ADDRESSLENGTH );
			g_favoriteserverlist[g_numfavoriteservers].pingtime = ArenaServers_MaxPing();
		}

		g_numfavoriteservers++;
	}

	g_arenaservers.numfavoriteaddresses = g_numfavoriteservers;

	if (!found)
	{
		// no results were found, reset server list
		// list will be automatically refreshed when selected
		g_numfavoriteservers = 0;
	}
}


/*
=================
Full status of a server (getstatus): the settings, for the mode, and the players.  trap_LAN_ServerStatus sends the request
on its first call and returns qtrue when the answer is there.

Status string: \key\value...  then  \  and one \score ping "name" per player, then \.  The info part ends at the first
empty key, so a value may be empty.
=================
*/
static void SB_ParseStatus( const char *s, char *info, int infoSize, sbplayer_t *players, int maxPlayers, int *count, int *total )
{
	const char	*p = s, *k, *ke, *v, *ve, *q, *te;
	char		line[96], *name;
	int			len;

	*count = 0;
	*total = 0;
	while ( *p == '\\' ) {
		k = p + 1;
		ke = strchr( k, '\\' );
		if ( !ke ) {
			p = k + strlen( k );
			break;
		}
		if ( ke == k ) {
			break;		// the empty key: here the info ends
		}
		v = ke + 1;
		ve = strchr( v, '\\' );
		if ( !ve ) {
			ve = v + strlen( v );
		}
		p = ve;
	}
	len = p - s;
	if ( len > infoSize - 1 ) {
		len = infoSize - 1;
	}
	memcpy( info, s, len );
	info[len] = '\0';

	q = ( *p == '\\' ) ? p + 1 : p;
	while ( *q == '\\' ) {
		const char	*sp;

		k = q + 1;
		te = strchr( k, '\\' );
		if ( !te ) {
			te = k + strlen( k );
		}
		if ( te == k ) {
			break;
		}
		len = te - k;
		if ( len > (int)sizeof( line ) - 1 ) {
			len = sizeof( line ) - 1;
		}
		memcpy( line, k, len );
		line[len] = '\0';
		if ( *count < maxPlayers ) {
			sbplayer_t	*pl = &players[*count];

			pl->score = atoi( line );
			pl->ping = 0;
			name = "unknown";
			sp = strchr( line, ' ' );
			if ( sp ) {
				pl->ping = atoi( sp + 1 );
				sp = strchr( sp + 1, ' ' );
				if ( sp ) {
					name = (char *)sp + 1;
				}
			}
			if ( *name == '"' ) {
				name++;
			}
			len = strlen( name );
			if ( len > 0 && name[len - 1] == '"' ) {
				name[len - 1] = '\0';
			}
			Q_strncpyz( pl->name, name, sizeof( pl->name ) );
			Q_CleanStr( pl->name );
			( *count )++;
		}
		( *total )++;
		q = te;
	}
}

static servernode_t *SB_FindNode( const char *adr ) {
	int		i;

	for ( i = 0; i < *g_arenaservers.numservers; i++ ) {
		if ( !Q_stricmp( g_arenaservers.serverlist[i].adrstr, adr ) ) {
			return &g_arenaservers.serverlist[i];
		}
	}
	return NULL;
}

static void SB_StatusReset( void ) {
	int		i;

	g_arenaservers.detail.adr[0] = '\0';
	g_arenaservers.detail.state = 0;
	g_arenaservers.detail.total = 0;
	g_arenaservers.detail.count = 0;
	for ( i = 0; i < SB_BULK; i++ ) {
		g_arenaservers.bulk[i].adr[0] = '\0';
	}
	trap_LAN_ServerStatus( NULL, NULL, 0 );
}

// takes what a status answer says into the list
static void SB_ApplyStatus( servernode_t *n, const char *info ) {
	SB_SetMode( n, info );
	if ( !n->game[0] ) {
		Q_strncpyz( n->game, Info_ValueForKey( info, "gamename" ), sizeof( n->game ) );
	}
	if ( atoi( Info_ValueForKey( info, "g_needpass" ) ) ) {
		n->needpass = qtrue;
	}
	n->probed = qtrue;
}

/*
Every frame: the selected server's status (its players), and the mode of the servers whose info reply did not have it,
a few at a time.
*/
static void SB_StatusFrame( void ) {
	static char		buf[3072];
	static char		info[1536];
	static sbplayer_t	players[SB_MAXPLAYERS];
	sbdetail_t		*d = &g_arenaservers.detail;
	servernode_t	*sel = SB_Selected(), *n;
	int				i, j, count, total;

	if ( !sel || !sel->responded ) {
		d->adr[0] = '\0';
		d->state = 0;
	} else if ( Q_stricmp( d->adr, sel->adrstr ) ) {
		Q_strncpyz( d->adr, sel->adrstr, sizeof( d->adr ) );
		d->state = 1;
		d->start = uis.realtime;
		d->total = 0;
		d->count = 0;
	}
	if ( d->state == 1 ) {
		if ( trap_LAN_ServerStatus( d->adr, buf, sizeof( buf ) ) ) {
			SB_ParseStatus( buf, info, sizeof( info ), d->players, SB_MAXPLAYERS, &d->count, &d->total );
			n = SB_FindNode( d->adr );
			if ( n ) {
				SB_ApplyStatus( n, info );
			}
			d->state = 2;
		} else if ( uis.realtime - d->start > SB_STATUS_TIMEOUT ) {
			d->state = 3;
			if ( sel ) {
				sel->probed = qtrue;
			}
		}
	}

	if ( g_arenaservers.refreshservers ) {
		return;
	}
	for ( i = 0; i < SB_BULK; i++ ) {
		sbbulk_t	*b = &g_arenaservers.bulk[i];

		if ( b->adr[0] ) {
			if ( trap_LAN_ServerStatus( b->adr, buf, sizeof( buf ) ) ) {
				SB_ParseStatus( buf, info, sizeof( info ), players, SB_MAXPLAYERS, &count, &total );
				n = SB_FindNode( b->adr );
				if ( n ) {
					SB_ApplyStatus( n, info );
					ArenaServers_UpdateMenu();
				}
				b->adr[0] = '\0';
			} else if ( uis.realtime - b->start > SB_STATUS_TIMEOUT ) {
				b->adr[0] = '\0';
			}
		}
		if ( !b->adr[0] ) {
			// the next server without a mode (the selected one is the panel's)
			for ( j = 0; j < *g_arenaservers.numservers; j++ ) {
				n = &g_arenaservers.serverlist[j];
				if ( n->responded && !n->probed && !SB_ModeKnown( n ) && Q_stricmp( n->adrstr, d->adr ) ) {
					n->probed = qtrue;
					Q_strncpyz( b->adr, n->adrstr, sizeof( b->adr ) );
					b->start = uis.realtime;
					trap_LAN_ServerStatus( b->adr, buf, sizeof( buf ) );
					break;
				}
			}
		}
	}
}


/*
=================
ArenaServers_StopRefresh
=================
*/
static void ArenaServers_StopRefresh( void )
{
	if (!g_arenaservers.refreshservers)
		// not currently refreshing
		return;

	g_arenaservers.refreshservers = qfalse;

	// final tally
	if (g_arenaservers.numqueriedservers >= 0)
	{
		g_arenaservers.currentping       = *g_arenaservers.numservers;
		g_arenaservers.numqueriedservers = *g_arenaservers.numservers;
	}

	// sort
	SB_SortList();

	ArenaServers_UpdateMenu();
}


/*
=================
ArenaServers_DoRefresh
=================
*/
static void ArenaServers_DoRefresh( void )
{
	int		i;
	int		j;
	int		time;
	int		maxPing;
	char	adrstr[MAX_ADDRESSLENGTH];
	char	info[MAX_INFO_STRING];

	if (uis.realtime < g_arenaservers.refreshtime)
	{
	  if (g_servertype != UIAS_FAVORITES) {
			if (g_servertype == UIAS_LOCAL) {
				if (!trap_LAN_GetServerCount(AS_LOCAL)) {
					return;
				}
			}
			if (trap_LAN_GetServerCount(ArenaServers_SourceForLAN()) < 0) {
			  // still waiting for response
			  return;
			}
	  }
	} else if (g_servertype == UIAS_LOCAL) {
		if (!trap_LAN_GetServerCount(AS_LOCAL)) {
			// no local servers found, check again
			trap_Cmd_ExecuteText( EXEC_APPEND, "localservers\n" );
			g_arenaservers.refreshtime = uis.realtime + 5000;
			return;
		}
	}

	if (uis.realtime < g_arenaservers.nextpingtime)
	{
		// wait for time trigger
		return;
	}

	// trigger at 10Hz intervals
	g_arenaservers.nextpingtime = uis.realtime + 10;

	// process ping results
	maxPing = ArenaServers_MaxPing();
	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		trap_LAN_GetPing( i, adrstr, MAX_ADDRESSLENGTH, &time );
		if (!adrstr[0])
		{
			// ignore empty or pending pings
			continue;
		}

		// find ping result in our local list
		for (j=0; j<MAX_PINGREQUESTS; j++)
			if (!Q_stricmp( adrstr, g_arenaservers.pinglist[j].adrstr ))
				break;

		if (j < MAX_PINGREQUESTS)
		{
			// found it
			if (!time)
			{
				time = uis.realtime - g_arenaservers.pinglist[j].start;
				if (time < maxPing)
				{
					// still waiting
					continue;
				}
			}

			if (time > maxPing)
			{
				// stale it out
				info[0] = '\0';
				time    = maxPing;

				// set hostname for nonresponsive favorite server
				if (g_servertype == UIAS_FAVORITES) {
					Info_SetValueForKey( info, "hostname", adrstr );
					Info_SetValueForKey( info, "game", "???" );
				}
			}
			else
			{
				trap_LAN_GetPingInfo( i, info, MAX_INFO_STRING );
			}

			// insert ping results
			ArenaServers_Insert( adrstr, info, time );

			// clear this query from internal list
			g_arenaservers.pinglist[j].adrstr[0] = '\0';
   		}

		// clear this query from external list
		trap_LAN_ClearPing( i );
	}

	// get results of servers query
	// counts can increase as servers respond
	if (g_servertype == UIAS_FAVORITES) {
	  g_arenaservers.numqueriedservers = g_arenaservers.numfavoriteaddresses;
	} else {
	  g_arenaservers.numqueriedservers = trap_LAN_GetServerCount(ArenaServers_SourceForLAN());
	}

	// send ping requests in reasonable bursts
	// iterate ping through all found servers
	for (i=0; i<MAX_PINGREQUESTS && g_arenaservers.currentping < g_arenaservers.numqueriedservers; i++)
	{
		if (trap_LAN_GetPingQueueCount() >= MAX_PINGREQUESTS)
		{
			// ping queue is full
			break;
		}

		// find empty slot
		for (j=0; j<MAX_PINGREQUESTS; j++)
			if (!g_arenaservers.pinglist[j].adrstr[0])
				break;

		if (j >= MAX_PINGREQUESTS)
			// no empty slots available yet - wait for timeout
			break;

		// get an address to ping

		if (g_servertype == UIAS_FAVORITES) {
		  strcpy( adrstr, g_arenaservers.favoriteaddresses[g_arenaservers.currentping] );
		} else {
		  trap_LAN_GetServerAddressString(ArenaServers_SourceForLAN(), g_arenaservers.currentping, adrstr, MAX_ADDRESSLENGTH );
		}

		strcpy( g_arenaservers.pinglist[j].adrstr, adrstr );
		g_arenaservers.pinglist[j].start = uis.realtime;

		trap_Cmd_ExecuteText( EXEC_NOW, va( "ping %s\n", adrstr )  );

		// advance to next server
		g_arenaservers.currentping++;
	}

	if (!trap_LAN_GetPingQueueCount())
	{
		// all pings completed
		ArenaServers_StopRefresh();
		return;
	}

	// sort what is in so far, and update the user interface with ping status
	SB_SortList();
	ArenaServers_UpdateMenu();
}


/*
=================
ArenaServers_StartRefresh
=================
*/
static void ArenaServers_StartRefresh( void )
{
	int		i;
	char	myargs[32], protocol[32];

	memset( g_arenaservers.serverlist, 0, g_arenaservers.maxservers*sizeof(servernode_t) );

	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		g_arenaservers.pinglist[i].adrstr[0] = '\0';
		trap_LAN_ClearPing( i );
	}

	SB_StatusReset();

	g_arenaservers.refreshservers    = qtrue;
	g_arenaservers.currentping       = 0;
	g_arenaservers.nextpingtime      = 0;
	*g_arenaservers.numservers       = 0;
	g_arenaservers.numqueriedservers = 0;

	// allow max 5 seconds for responses
	g_arenaservers.refreshtime = uis.realtime + 5000;

	// place menu in zeroed state
	ArenaServers_UpdateMenu();

	if( g_servertype == UIAS_LOCAL ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "localservers\n" );
		return;
	}

	if( g_servertype >= UIAS_GLOBAL0 && g_servertype <= UIAS_GLOBAL5 ) {
		// DeFRaG (0x158da): only the "empty" / "full" flags are sent
		myargs[0] = 0;

		if (g_emptyservers) {
			strcat(myargs, " empty");
		}

		if (g_fullservers) {
			strcat(myargs, " full");
		}

		protocol[0] = '\0';
		trap_Cvar_VariableStringBuffer( "debug_protocol", protocol, sizeof(protocol) );
		if (strlen(protocol)) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "globalservers %d %s%s\n", g_servertype - UIAS_GLOBAL0, protocol, myargs ));
		}
		else {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "globalservers %d %d%s\n", g_servertype - UIAS_GLOBAL0, (int)trap_Cvar_VariableValue( "protocol" ), myargs ) );
		}
	}
}


/*
=================
ArenaServers_SetType
=================
*/
int ArenaServers_SetType( int type )
{
	ArenaServers_StopRefresh();

	if(type >= UIAS_GLOBAL1 && type <= UIAS_GLOBAL5)
	{
		char masterstr[2], cvarname[sizeof("sv_master1")];
		int direction;

		if (type == g_servertype || type == ((g_servertype+1) % UIAS_NUM_SOURCES)) {
			direction = 1;
		} else {
			direction = -1;
		}

		while(type >= UIAS_GLOBAL1 && type <= UIAS_GLOBAL5)
		{
			Com_sprintf(cvarname, sizeof(cvarname), "sv_master%d", type - UIAS_GLOBAL0);
			trap_Cvar_VariableStringBuffer(cvarname, masterstr, sizeof(masterstr));
			if(*masterstr)
				break;

			type += direction;
		}
	}

	g_servertype = type;

	switch( type ) {
	default:
	case UIAS_LOCAL:
		g_arenaservers.serverlist = g_localserverlist;
		g_arenaservers.numservers = &g_numlocalservers;
		g_arenaservers.maxservers = MAX_LOCALSERVERS;
		break;

	case UIAS_GLOBAL0:
	case UIAS_GLOBAL1:
	case UIAS_GLOBAL2:
	case UIAS_GLOBAL3:
	case UIAS_GLOBAL4:
	case UIAS_GLOBAL5:
		g_arenaservers.serverlist = g_globalserverlist[type-UIAS_GLOBAL0];
		g_arenaservers.numservers = &g_numglobalservers[type-UIAS_GLOBAL0];
		g_arenaservers.maxservers = MAX_GLOBALSERVERS;
		break;

	case UIAS_FAVORITES:
		g_arenaservers.serverlist = g_favoriteserverlist;
		g_arenaservers.numservers = &g_numfavoriteservers;
		g_arenaservers.maxservers = MAX_FAVORITESERVERS;
		break;

	}

	// the selection belongs to the list it was made in
	g_arenaservers.selAdr[0] = '\0';
	SB_StatusReset();

	if( !*g_arenaservers.numservers ) {
		ArenaServers_StartRefresh();
	}
	else {
		// avoid slow operation, use existing results
		g_arenaservers.currentping       = *g_arenaservers.numservers;
		g_arenaservers.numqueriedservers = *g_arenaservers.numservers;
		SB_SortList();
		ArenaServers_UpdateMenu();
	}

	return type;
}


/*
=================
Leaving, connecting
=================
*/
static void ArenaServers_Leave( void ) {
	ArenaServers_StopRefresh();
	ArenaServers_SaveChanges();
	trap_LAN_ServerStatus( NULL, NULL, 0 );
}

// connects to the address that was typed (the port defaults to the game's)
static void SB_Join( qboolean favorite ) {
	char	adr[SB_ADDRMAX + 1];
	char	*s = adr;

	Q_strncpyz( adr, g_arenaservers.addr.field.buffer, sizeof( adr ) );
	while ( *s == ' ' ) {
		s++;
	}
	{
		int len = strlen( s );

		while ( len > 0 && s[len - 1] == ' ' ) {
			s[--len] = '\0';
		}
	}
	if ( !*s ) {
		SB_Notice( "Type the address of a server first." );
		return;
	}
	if ( favorite ) {
		SB_FavAdd( s, NULL );
	} else {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", s ) );
	}
}


/*
=================
Drawing
=================
*/
static void SB_DrawOutline( int x, int y, int w, int h, const float *border ) {
	UI_FillRect( x, y, w, 1, border );
	UI_FillRect( x, y + h - 1, w, 1, border );
	UI_FillRect( x, y, 1, h, border );
	UI_FillRect( x + w - 1, y, 1, h, border );
}

static qboolean SB_Lit( menucommon_s *c ) {
	return ( c->flags & QMF_HASMOUSEFOCUS ) || ( g_arenaservers.keyNav && Menu_ItemAtCursor( c->parent ) == (void *)c );
}

static qboolean SB_FieldFocused( void ) {
	void	*item = Menu_ItemAtCursor( &g_arenaservers.menu );

	return item == (void *)&g_arenaservers.search || item == (void *)&g_arenaservers.addr;
}

// the source / mode / filter tabs: plain text, the selected one is filled, the pointer (or keyboard focus) outlines one in yellow
static void SB_ChipDraw( void *self ) {
	menutext_s	*c = (menutext_s *)self;
	int			id = c->generic.id;
	int			x = c->generic.left, y = c->generic.top;
	int			w = c->generic.right - x + 1, h = c->generic.bottom - y + 1;
	qboolean	selected, lit;
	vec4_t		fill;

	if ( id >= ID_SRC && id < ID_SRC + UIAS_NUM_SOURCES ) {
		selected = g_servertype == id - ID_SRC;
	} else if ( id >= ID_TYPE && id < ID_TYPE + 4 ) {
		selected = g_typefilter == id - ID_TYPE;
	} else if ( id == ID_SHOWFULL ) {
		selected = g_fullservers != 0;
	} else {
		selected = g_emptyservers != 0;
	}
	lit = SB_Lit( &c->generic );
	if ( selected || lit ) {
		Vector4Copy( selected ? sbChipFillSel : sbChipFillLit, fill );
		UI_FillRect( x, y, w, h, fill );
	}
	if ( lit ) {
		SB_DrawOutline( x, y, w, h, sbBorderFocus );
	}
	UI_DrawString( x + w / 2, y + ( h - SMALLCHAR_HEIGHT ) / 2, c->string, UI_CENTER | UI_SMALLFONT, ( selected || lit ) ? sbWhite : sbLabel );
}

// BACK / REFRESH / ...: a faint red fill and plain text; the pointer outlines one in yellow
static void SB_ButtonDraw( void *self ) {
	menutext_s	*b = (menutext_s *)self;
	qboolean	off = ( b->generic.flags & QMF_GRAYED ) != 0;
	qboolean	lit = !off && SB_Lit( &b->generic );
	int			x = b->generic.left, y = b->generic.top;
	int			w = b->generic.right - x + 1, h = b->generic.bottom - y + 1;

	UI_FillRect( x, y, w, h, lit ? sbChipFillLit : b->generic.id == ID_CONNECT && !off ? sbChipFillSel : sbChipFill );
	if ( lit ) {
		SB_DrawOutline( x, y, w, h, sbBorderFocus );
	}
	UI_DrawString( x + w / 2, y + ( h - SMALLCHAR_HEIGHT ) / 2, b->string, UI_CENTER | UI_SMALLFONT, off ? sbChipOff : ( lit || b->generic.id == ID_CONNECT ) ? sbWhite : sbLabel );
}

// the column headings: the sorted one is white with a triangle
static void SB_HeadDraw( void *self ) {
	menutext_s	*c = (menutext_s *)self;
	int			col = c->generic.id - ID_SORT;
	int			x = c->generic.left, y = c->generic.top;
	qboolean	sorted = g_sortkey == sbColSort[col];
	qboolean	lit = SB_Lit( &c->generic );
	float		*color = ( sorted || lit ) ? ( lit ? sbGold : sbWhite ) : sbLabel;
	int			w = strlen( c->string ) * SMALLCHAR_WIDTH;
	int			tx, i, up;

	if ( col >= 3 ) {
		// the numbers are right aligned
		tx = c->generic.right + 1 - w;
		if ( sorted ) {
			tx -= 10;
		}
	} else {
		tx = x + 2;
	}
	UI_DrawString( tx, y, c->string, UI_SMALLFONT, color );
	if ( sorted ) {
		// ascending (down the list: small to large) points up
		up = !g_sortrev;
		if ( sbColSort[col] == SORT_CLIENTS ) {
			up = g_sortrev;		// the most players first is the natural order here
		}
		tx += w + 3;
		for ( i = 0; i < 4; i++ ) {
			int half = up ? i : 3 - i;

			UI_FillRect( tx + 3 - half, y + 4 + i * 2, 1 + half * 2, 2, color );
		}
	}
}

static qboolean SB_ScrollbarGeometry( int *trackY, int *trackH, int *thumbY, int *thumbH ) {
	int	total = g_arenaservers.list.numitems;
	int	rows = g_arenaservers.list.height;
	int	range, top;

	*trackY = g_arenaservers.list.generic.y;
	*trackH = rows * SMALLCHAR_HEIGHT;
	if ( total <= rows ) {
		*thumbY = *trackY;
		*thumbH = *trackH;
		return qfalse;
	}
	range = total - rows;
	*thumbH = *trackH * rows / total;
	if ( *thumbH < SB_SB_MINTHUMB ) {
		*thumbH = SB_SB_MINTHUMB;
	}
	top = Com_Clamp( 0, range, g_arenaservers.list.top );
	*thumbY = *trackY + ( *trackH - *thumbH ) * top / range;
	return qtrue;
}

static void SB_ScrollbarDrag( void ) {
	int	trackY, trackH, thumbY, thumbH, range, top;

	if ( !SB_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	range = g_arenaservers.list.numitems - g_arenaservers.list.height;
	top = ( uis.cursory - g_arenaservers.sbGrab - trackY ) * range / ( trackH - thumbH );
	g_arenaservers.list.top = Com_Clamp( 0, range, top );
}

static void SB_ScrollbarDraw( void ) {
	static vec4_t	trackColor = { 0.15f, 0.0f, 0.0f, 0.6f };
	static vec4_t	thumbColor = { 0.70f, 0.08f, 0.08f, 0.95f };
	static vec4_t	hotColor = { 1.0f, 0.30f, 0.15f, 1.0f };
	int				trackY, trackH, thumbY, thumbH;
	qboolean		hot;

	if ( !SB_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	hot = g_arenaservers.sbDrag || UI_CursorInRect( SB_SB_X, trackY, SB_SB_W, trackH );
	UI_FillRect( SB_SB_X, trackY, SB_SB_W, trackH, trackColor );
	UI_FillRect( SB_SB_X + 1, thumbY, SB_SB_W - 2, thumbH, hot ? hotColor : thumbColor );
}

// a search / address plate; empty and unfocused it shows what it is for
static void SB_FieldDraw( void *self ) {
	menufield_s	*f = (menufield_s *)self;
	qboolean	focus = ( Menu_ItemAtCursor( f->generic.parent ) == self );
	int			x = f->generic.left, y = f->generic.y;
	int			w = f->generic.right - x + 1;

	UI_FillRect( x, y - 2, w, SMALLCHAR_HEIGHT + 4, focus ? sbPlateFocus : sbPlate );
	if ( focus ) {
		SB_DrawOutline( x, y - 2, w, SMALLCHAR_HEIGHT + 4, sbBorderFocus );
	}
	if ( !f->field.buffer[0] && !focus ) {
		UI_DrawString( x + 4, y, f->generic.id == ID_SEARCH ? "search servers / maps    F5 refresh" : "connect to an address    Ctrl+Enter: favorite", UI_SMALLFONT, sbHint );
	} else {
		MField_Draw( &f->field, x + 4, y, UI_SMALLFONT | ( focus ? UI_PULSE : 0 ), sbWhite );
	}
}

static float *SB_PingColor( const servernode_t *n ) {
	if ( !n->responded ) {
		return sbHint;
	}
	if ( n->pingtime < n->minPing || ( n->maxPing && n->pingtime > n->maxPing ) ) {
		return sbBlue;
	}
	if ( n->pingtime < 200 ) {
		return sbGood;
	}
	if ( n->pingtime < 400 ) {
		return sbWarn;
	}
	return sbBad;
}

// the server list
static void SB_ListDraw( void *self ) {
	menulist_s	*l = (menulist_s *)self;
	qboolean	hasfocus = ( l->generic.parent->cursor == l->generic.menuPosition );
	int			i, x = l->generic.x, y = l->generic.y;
	char		name[SB_NAME_CHARS + 1], map[SB_MAP_CHARS + 1];

	for ( i = l->top; i < l->top + l->height && i < l->numitems; i++, y += SMALLCHAR_HEIGHT ) {
		const servernode_t	*n = g_arenaservers.view[i];
		qboolean			sel = i == l->curvalue;
		int					style = UI_LEFT | UI_SMALLFONT;
		float				*color = n->responded ? text_color_normal : sbChipOff;
		float				*dim = sel ? sbWhite : sbHint;

		if ( sel ) {
			UI_FillRect( x - 2, y, l->width * SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT + 2, listbar_color );
			color = text_color_highlight;
			if ( hasfocus ) {
				style |= UI_PULSE;
			}
		}
		if ( SB_FavIndex( n->adrstr ) >= 0 ) {
			UI_DrawString( x + SB_COL_STAR, y, "*", UI_LEFT | UI_SMALLFONT, sbGold );
		}
		if ( n->needpass ) {
			UI_DrawString( x + SB_COL_LOCK, y, "!", UI_LEFT | UI_SMALLFONT, sbBad );
		}
		Q_strncpyz( name, n->hostname, sizeof( name ) );
		UI_DrawString( x + SB_COL_NAME, y, name, style, color );
		Q_strncpyz( map, n->mapname, sizeof( map ) );
		UI_DrawString( x + SB_COL_MAP, y, map, UI_LEFT | UI_SMALLFONT, n->responded ? dim : sbChipOff );
		UI_DrawString( x + SB_COL_MODE, y, SB_ModeText( n ), UI_LEFT | UI_SMALLFONT, SB_ModeKnown( n ) ? ( sel ? sbWhite : sbLabel ) : sbHint );
		if ( n->responded ) {
			UI_DrawString( x + SB_COL_PLAYERS, y, va( "%i/%i", n->numclients, n->maxclients ), UI_RIGHT | UI_SMALLFONT,
				( n->maxclients && n->numclients >= n->maxclients ) ? sbBad : sel ? sbWhite : sbLabel );
			UI_DrawString( x + SB_COL_PING, y, va( "%i", n->pingtime ), UI_RIGHT | UI_SMALLFONT, SB_PingColor( n ) );
		} else {
			UI_DrawString( x + SB_COL_PLAYERS, y, "-", UI_RIGHT | UI_SMALLFONT, sbHint );
			UI_DrawString( x + SB_COL_PING, y, "-", UI_RIGHT | UI_SMALLFONT, sbHint );
		}
	}
}

static void SB_DetailRow( int y, const char *label, const char *value, float *color ) {
	char	text[14];

	UI_DrawString( SB_RX, y, label, UI_LEFT | UI_SMALLFONT, sbHint );
	Q_strncpyz( text, value, sizeof( text ) );
	UI_DrawString( SB_RX + 64, y, text, UI_LEFT | UI_SMALLFONT, color );
}

// the panel of the selected server: levelshot, what it is and who is on it
static void SB_DetailDraw( void ) {
	const servernode_t	*n = SB_Selected();
	const sbdetail_t	*d = &g_arenaservers.detail;
	char			text[24];
	int				y, i, shown;

	if ( !n ) {
		UI_DrawString( SB_RX + SB_RW / 2, SB_SRC_Y + 60, "no server selected", UI_CENTER | UI_SMALLFONT, sbHint );
		return;
	}

	// levelshot
	if ( Q_stricmp( g_arenaservers.previewMap, n->mapname ) || !g_arenaservers.previewShader ) {
		Q_strncpyz( g_arenaservers.previewMap, n->mapname, sizeof( g_arenaservers.previewMap ) );
		g_arenaservers.previewShader = n->mapname[0] ? trap_R_RegisterShaderNoMip( va( "levelshots/%s", n->mapname ) ) : 0;
		if ( !g_arenaservers.previewShader ) {
			g_arenaservers.previewShader = trap_R_RegisterShaderNoMip( ART_UNKNOWNMAP );
		}
	}
	UI_DrawHandlePic( SB_RX + 12, SB_SRC_Y, 144, 108, g_arenaservers.previewShader );

	y = SB_SRC_Y + 114;
	Q_strncpyz( text, n->hostname, 22 );
	UI_DrawString( SB_RX, y, text, UI_LEFT | UI_SMALLFONT, sbWhite );
	y += SMALLCHAR_HEIGHT;
	UI_DrawString( SB_RX, y, n->adrstr, UI_LEFT | UI_SMALLFONT, sbHint );
	y += SMALLCHAR_HEIGHT;
	SB_DetailRow( y, "Map", n->mapname, sbLabel );
	y += SMALLCHAR_HEIGHT;
	SB_DetailRow( y, "Mode", SB_ModeText( n ), SB_ModeKnown( n ) ? sbLabel : sbHint );
	y += SMALLCHAR_HEIGHT;
	if ( n->responded ) {
		SB_DetailRow( y, "Players", va( "%i / %i", n->numclients, n->maxclients ), sbLabel );
		y += SMALLCHAR_HEIGHT;
		SB_DetailRow( y, "Ping", va( "%i ms", n->pingtime ), SB_PingColor( n ) );
	} else {
		SB_DetailRow( y, "Players", "no answer", sbBad );
		y += SMALLCHAR_HEIGHT;
		SB_DetailRow( y, "Ping", "-", sbHint );
	}
	y += SMALLCHAR_HEIGHT;
	if ( n->version ) {
		SB_DetailRow( y, "Version", va( "%i.%i.%i", n->version / 10000, n->version / 100 % 100, n->version % 100 ), sbLabel );
	} else {
		SB_DetailRow( y, "Mod", n->game[0] ? n->game : "-", n->game[0] ? sbLabel : sbHint );
	}
	y += SMALLCHAR_HEIGHT;
	if ( n->needpass ) {
		UI_DrawString( SB_RX, y, "password required", UI_LEFT | UI_SMALLFONT, sbBad );
		y += SMALLCHAR_HEIGHT;
	}
	if ( n->anon ) {
		UI_DrawString( SB_RX, y, "anonymous clients", UI_LEFT | UI_SMALLFONT, sbHint );
		y += SMALLCHAR_HEIGHT;
	}

	// players
	y = SB_SRC_Y + 114 + 9 * SMALLCHAR_HEIGHT + 4;
	if ( d->state == 2 && d->total > 0 ) {
		UI_DrawString( SB_RX, y, va( "PLAYERS  %i", d->total ), UI_LEFT | UI_SMALLFONT, sbLabel );
		y += SMALLCHAR_HEIGHT;
		shown = d->count;
		if ( shown > 4 ) {
			shown = d->total > 4 ? 3 : 4;
		}
		for ( i = 0; i < shown; i++ ) {
			Q_strncpyz( text, d->players[i].name, 14 );
			UI_DrawString( SB_RX, y, text, UI_LEFT | UI_SMALLFONT, sbWhite );
			UI_DrawString( SB_RX + SB_RW, y, va( "%i", d->players[i].ping ), UI_RIGHT | UI_SMALLFONT, sbHint );
			y += SMALLCHAR_HEIGHT;
		}
		if ( d->total > shown ) {
			UI_DrawString( SB_RX, y, va( "+ %i more", d->total - shown ), UI_LEFT | UI_SMALLFONT, sbHint );
		}
	} else if ( d->state == 1 ) {
		UI_DrawString( SB_RX, y, "asking for players...", UI_LEFT | UI_SMALLFONT, sbHint );
	} else if ( d->state == 2 ) {
		UI_DrawString( SB_RX, y, "nobody is playing", UI_LEFT | UI_SMALLFONT, sbHint );
	} else if ( d->state == 3 ) {
		UI_DrawString( SB_RX, y, "no player list", UI_LEFT | UI_SMALLFONT, sbHint );
	}
}

/*
=================
ArenaServers_MenuDraw
=================
*/
static void ArenaServers_MenuDraw( void )
{
	servernode_t	*sel;
	menulist_s		*l = &g_arenaservers.list;

	if ( uis.cursorx != g_arenaservers.lastCursorX || uis.cursory != g_arenaservers.lastCursorY ) {
		g_arenaservers.lastCursorX = uis.cursorx;
		g_arenaservers.lastCursorY = uis.cursory;
		g_arenaservers.keyNav = qfalse;		// the mouse is in charge again
	}
	if ( g_arenaservers.sbDrag ) {
		if ( trap_Key_IsDown( K_MOUSE1 ) ) {
			SB_ScrollbarDrag();
		} else {
			g_arenaservers.sbDrag = qfalse;
		}
	}

	if (g_arenaservers.refreshservers)
		ArenaServers_DoRefresh();

	if ( strcmp( g_arenaservers.applied, g_arenaservers.search.field.buffer ) ) {
		SB_ParseSearch();
		ArenaServers_UpdateMenu();
	}
	SB_StatusFrame();

	sel = SB_Selected();
	g_arenaservers.refresh.string = g_arenaservers.refreshservers ? "STOP" : "REFRESH";
	g_arenaservers.fav.string = ( sel && SB_FavIndex( sel->adrstr ) >= 0 ) ? "UNFAVORITE" : "FAVORITE";
	if ( sel ) {
		g_arenaservers.fav.generic.flags &= ~QMF_GRAYED;
		g_arenaservers.connect.generic.flags &= ~QMF_GRAYED;
	} else {
		g_arenaservers.fav.generic.flags |= QMF_GRAYED;
		g_arenaservers.connect.generic.flags |= QMF_GRAYED;
	}

	// a line under the headings
	UI_FillRect( SB_LX, SB_HEAD_Y + SB_HEAD_H, SB_SB_X + SB_SB_W - SB_LX, 1, sbBorder );

	Menu_Draw( &g_arenaservers.menu );
	UI_DrawString( g_arenaservers.showfull.generic.left - 6, SB_TYPE_Y + ( SB_CHIP_H - SMALLCHAR_HEIGHT ) / 2, "show", UI_RIGHT | UI_SMALLFONT, sbHint );
	SB_ScrollbarDraw();
	SB_DetailDraw();

	if ( !l->numitems ) {
		UI_DrawString( SB_LX + SB_LW / 2, SB_LIST_Y + 40, g_arenaservers.refreshservers ? "Looking for servers..." :
			*g_arenaservers.numservers ? "No server matches" : "No servers", UI_CENTER | UI_SMALLFONT, sbWhite );
	}
	if ( g_arenaservers.notice[0] && uis.realtime - g_arenaservers.noticeTime < SB_NOTICE_TIME ) {
		UI_DrawString( SB_LX + SB_LW / 2, SB_STATUS_Y, g_arenaservers.notice, UI_CENTER | UI_SMALLFONT, sbGold );
	} else {
		UI_DrawString( SB_LX + SB_LW / 2, SB_STATUS_Y, g_arenaservers.status, UI_CENTER | UI_SMALLFONT, sbWhite );
	}
	UI_DrawProportionalString( SB_RX, 5, "SERVERS", UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW, sbLabel );
}


/*
=================
Events
=================
*/
static void SB_SetSortColumn( int col ) {
	int	key = sbColSort[col];

	if ( g_sortkey == key ) {
		g_sortrev = !g_sortrev;
	} else {
		g_sortkey = key;
		g_sortrev = 0;
	}
	trap_Cvar_SetValue( "ui_browserSortKey", g_sortkey );
	trap_Cvar_SetValue( "df_ui_browserSortRev", g_sortrev );
	SB_SortList();
	ArenaServers_UpdateMenu();
}

static void SB_ChipEvent( void *ptr, int event ) {
	int		id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s*)ptr)->id;

	if ( id >= ID_SRC && id < ID_SRC + UIAS_NUM_SOURCES ) {
		if ( id - ID_SRC != g_servertype ) {
			trap_Cvar_SetValue( "ui_browserMaster", ArenaServers_SetType( id - ID_SRC ) );
		}
	} else if ( id >= ID_TYPE && id < ID_TYPE + 4 ) {
		g_typefilter = id - ID_TYPE;
		trap_Cvar_SetValue( "df_ui_browserType", g_typefilter );
		ArenaServers_UpdateMenu();
	} else if ( id == ID_SHOWFULL ) {
		g_fullservers = !g_fullservers;
		trap_Cvar_SetValue( "ui_browserShowFull", g_fullservers );
		ArenaServers_UpdateMenu();
	} else if ( id == ID_SHOWEMPTY ) {
		g_emptyservers = !g_emptyservers;
		trap_Cvar_SetValue( "ui_browserShowEmpty", g_emptyservers );
		ArenaServers_UpdateMenu();
	} else if ( id >= ID_SORT && id < ID_SORT + SB_COLUMNS ) {
		SB_SetSortColumn( id - ID_SORT );
	}
}

static void SB_ListEvent( void *ptr, int event ) {
	if ( event == QM_GOTFOCUS ) {
		SB_SelectionChanged();
	}
}

static void SB_ButtonEvent( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( ((menucommon_s*)ptr)->id ) {
	case ID_BACK:
		ArenaServers_Leave();
		UI_PopMenu();
		break;

	case ID_REFRESH:
		if ( g_arenaservers.refreshservers ) {
			ArenaServers_StopRefresh();
		} else {
			ArenaServers_StartRefresh();
		}
		break;

	case ID_FAV:
		SB_FavToggle();
		break;

	case ID_CONNECT:
		ArenaServers_Go();
		break;
	}
}

static qboolean SB_IsChip( void *item ) {
	menucommon_s	*c = (menucommon_s *)item;

	return c && c->callback == SB_ChipEvent;
}

/*
=================
ArenaServers_MenuKey
=================
*/
static sfxHandle_t ArenaServers_MenuKey( int key ) {
	sfxHandle_t	sound;
	void		*item;
	qboolean	inBrowser, typing, addr;
	int			trackY, trackH, thumbY, thumbH;
	int			src, step, i;

	// left click on the scroll bar: grab the thumb (a click on the track puts the middle of the thumb there)
	if ( key == K_MOUSE1 && SB_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH )
		&& UI_CursorInRect( SB_SB_X, trackY, SB_SB_W, trackH ) ) {
		if ( uis.cursory >= thumbY && uis.cursory < thumbY + thumbH ) {
			g_arenaservers.sbGrab = uis.cursory - thumbY;
		} else {
			g_arenaservers.sbGrab = thumbH / 2;
		}
		g_arenaservers.sbDrag = qtrue;
		SB_ScrollbarDrag();
		return menu_null_sound;
	}

	if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW
		|| key == K_ENTER || key == K_KP_ENTER || key == K_PGUP || key == K_PGDN ) {
		g_arenaservers.keyNav = qtrue;
	}

	// mouse wheel: scroll the list wherever the focus is
	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		if ( g_arenaservers.list.numitems ) {
			ScrollList_Key( &g_arenaservers.list, key );
		}
		return menu_null_sound;
	}

	if ( key == K_F5 ) {
		if ( !g_arenaservers.refreshservers ) {
			ArenaServers_StartRefresh();
		}
		return menu_move_sound;
	}

	// SPACE stops the scan (when no box is being typed in)
	if ( key == K_SPACE && g_arenaservers.refreshservers && !SB_FieldFocused() ) {
		ArenaServers_StopRefresh();
		return menu_move_sound;
	}

	// a printable character anywhere outside of the boxes starts a search (the key press itself is swallowed, the
	// character event that follows is typed into the box)
	if ( !SB_FieldFocused() ) {
		int ch = key & ~K_CHAR_FLAG;

		if ( ch > ' ' && ch < 127 ) {
			Menu_SetCursorToItem( &g_arenaservers.menu, &g_arenaservers.search );
			if ( !( key & K_CHAR_FLAG ) ) {
				return menu_null_sound;
			}
		}
	}
	item = Menu_ItemAtCursor( &g_arenaservers.menu );
	addr = item == (void *)&g_arenaservers.addr;
	typing = item == (void *)&g_arenaservers.search;
	inBrowser = typing || item == (void *)&g_arenaservers.list || SB_IsChip( item );

	// Escape with a box typed in clears it (the first press); the next one leaves the menu
	if ( key == K_ESCAPE && typing && g_arenaservers.search.field.buffer[0] ) {
		MField_Clear( &g_arenaservers.search.field );
		return menu_null_sound;
	}
	if ( key == K_ESCAPE && addr && g_arenaservers.addr.field.buffer[0] ) {
		MField_Clear( &g_arenaservers.addr.field );
		return menu_null_sound;
	}

	if ( ( key == K_DEL || key == K_KP_DEL ) && g_servertype == UIAS_FAVORITES && item == (void *)&g_arenaservers.list ) {
		servernode_t	*n = SB_Selected();

		if ( n ) {
			char	adr[MAX_ADDRESSLENGTH];

			Q_strncpyz( adr, n->adrstr, sizeof( adr ) );
			SB_FavRemove( adr );
		}
		return menu_move_sound;
	}

	if ( addr && ( key == K_ENTER || key == K_KP_ENTER ) ) {
		SB_Join( trap_Key_IsDown( K_CTRL ) );
		return menu_move_sound;
	}

	if ( inBrowser ) {
		switch ( key ) {
		case K_UPARROW: case K_KP_UPARROW: case K_DOWNARROW: case K_KP_DOWNARROW:
		case K_PGUP: case K_KP_PGUP: case K_PGDN: case K_KP_PGDN:
			// the list is moved wherever the focus is in the browser, so the box can stay where it is
			return g_arenaservers.list.numitems ? ScrollList_Key( &g_arenaservers.list, key ) : menu_buzz_sound;
		case K_HOME: case K_KP_HOME: case K_END: case K_KP_END:
			if ( !typing ) {
				return g_arenaservers.list.numitems ? ScrollList_Key( &g_arenaservers.list, key ) : menu_buzz_sound;
			}
			break;
		case K_LEFTARROW: case K_KP_LEFTARROW: case K_RIGHTARROW: case K_KP_RIGHTARROW:
			// the cursor of a text keeps them, an empty box has no use for them: they change the source
			if ( !typing || !g_arenaservers.search.field.buffer[0] ) {
				step = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				src = 0;
				for ( i = 0; i < UIAS_NUM_SOURCES; i++ ) {
					if ( sbSourceOrder[i] == g_servertype ) {
						src = i;
					}
				}
				for ( i = 0; i < UIAS_NUM_SOURCES; i++ ) {
					src = ( src + step + UIAS_NUM_SOURCES ) % UIAS_NUM_SOURCES;
					if ( !( g_arenaservers.src[sbSourceOrder[src]].generic.flags & QMF_HIDDEN ) ) {
						break;
					}
				}
				trap_Cvar_SetValue( "ui_browserMaster", ArenaServers_SetType( sbSourceOrder[src] ) );
				return menu_move_sound;
			}
			break;
		case K_ENTER: case K_KP_ENTER:
			if ( typing || item == (void *)&g_arenaservers.list ) {
				if ( SB_Selected() ) {
					ArenaServers_Go();
					return menu_move_sound;
				}
				return menu_buzz_sound;
			}
			break;
		}
	}

	if ( key == K_MOUSE2 || key == K_ESCAPE ) {
		ArenaServers_Leave();
	}

	sound = Menu_DefaultKey( &g_arenaservers.menu, key );

	// double click on a server connects
	if ( key == K_MOUSE1 && UI_CursorInRect( g_arenaservers.list.generic.x, g_arenaservers.list.generic.y, g_arenaservers.list.width * SMALLCHAR_WIDTH, g_arenaservers.list.height * SMALLCHAR_HEIGHT ) ) {
		int row = g_arenaservers.list.top + ( uis.cursory - g_arenaservers.list.generic.y ) / SMALLCHAR_HEIGHT;

		if ( row == g_arenaservers.list.curvalue && row < g_arenaservers.list.numitems ) {
			int now = trap_Milliseconds();

			if ( g_arenaservers.clickIndex == row && now - g_arenaservers.clickTime <= SB_DOUBLECLICK ) {
				g_arenaservers.clickIndex = -1;
				ArenaServers_Go();
			} else {
				g_arenaservers.clickIndex = row;
				g_arenaservers.clickTime = now;
			}
		}
	}
	return sound;
}


/*
=================
ArenaServers_MenuInit
=================
*/
static void SB_InitChip( menutext_s *c, int id, const char *label, int x, int y, int w, int h, void (*draw)( void * ) ) {
	Com_Memset( c, 0, sizeof( *c ) );
	c->generic.type = MTYPE_PTEXT;
	c->generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	c->generic.id = id;
	c->generic.x = x;
	c->generic.y = y;
	c->generic.callback = id < ID_SRC ? SB_ButtonEvent : SB_ChipEvent;
	c->generic.ownerdraw = draw;
	c->string = (char *)label;
	Menu_AddItem( &g_arenaservers.menu, c );
	// the widget worked out its mouse area from the text: use the plate
	c->generic.left = x;
	c->generic.right = x + w - 1;
	c->generic.top = y;
	c->generic.bottom = y + h - 1;
}

static void SB_InitField( menufield_s *f, int id, int x, int y, int w, int chars, int maxchars ) {
	f->generic.type = MTYPE_FIELD;
	f->generic.flags = QMF_SMALLFONT | QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	f->generic.id = id;
	f->generic.ownerdraw = SB_FieldDraw;
	f->generic.x = x + 4;
	f->generic.y = y;
	f->field.widthInChars = chars;
	f->field.maxchars = maxchars;
	Menu_AddItem( &g_arenaservers.menu, f );
	f->generic.left = x;
	f->generic.right = x + w - 1;
	f->generic.top = y - 2;
	f->generic.bottom = y - 2 + SMALLCHAR_HEIGHT + 3;
}

static int SB_Clamp( const char *cvar, int max ) {
	return Com_Clamp( 0, max, (int)trap_Cvar_VariableValue( cvar ) );
}

static void ArenaServers_MenuInit( void ) {
	int			i, x, w;
	char		masterstr[2];

	// zero set all our globals
	memset( &g_arenaservers, 0 ,sizeof(arenaservers_t) );

	ArenaServers_Cache();

	g_arenaservers.menu.fullscreen = qtrue;
	g_arenaservers.menu.wrapAround = qtrue;
	g_arenaservers.menu.draw       = ArenaServers_MenuDraw;
	g_arenaservers.menu.key        = ArenaServers_MenuKey;
	g_arenaservers.clickIndex      = -1;

	for ( i = 0; i < MAX_LISTBOXITEMS; i++ ) {
		g_arenaservers.items[i] = "";
	}

	SB_InitField( &g_arenaservers.search, ID_SEARCH, SB_LX, SB_FIELD_Y, SB_LW, 50, SB_SEARCHMAX );

	// the source chips; a master that has no address is not there
	x = SB_LX;
	for ( i = 0; i < UIAS_NUM_SOURCES; i++ ) {
		int		src = sbSourceOrder[i];
		qboolean	hidden = qfalse;

		w = strlen( sbSourceNames[src] ) * SMALLCHAR_WIDTH + 16;
		if ( src >= UIAS_GLOBAL1 && src <= UIAS_GLOBAL5 ) {
			trap_Cvar_VariableStringBuffer( va( "sv_master%d", src - UIAS_GLOBAL0 ), masterstr, sizeof( masterstr ) );
			hidden = !masterstr[0];
		}
		SB_InitChip( &g_arenaservers.src[src], ID_SRC + src, sbSourceNames[src], x, SB_SRC_Y, w, SB_CHIP_H, SB_ChipDraw );
		if ( hidden ) {
			g_arenaservers.src[src].generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
		} else {
			x += w + 4;
		}
	}

	x = SB_LX;
	for ( i = 0; i < 4; i++ ) {
		w = strlen( sbTypeNames[i] ) * SMALLCHAR_WIDTH + 16;
		SB_InitChip( &g_arenaservers.type[i], ID_TYPE + i, sbTypeNames[i], x, SB_TYPE_Y, w, SB_CHIP_H, SB_ChipDraw );
		x += w + 4;
	}

	// the filters for full and empty servers, at the right end of the row
	w = 5 * SMALLCHAR_WIDTH + 16;
	SB_InitChip( &g_arenaservers.showempty, ID_SHOWEMPTY, "EMPTY", SB_LX + SB_LW - w, SB_TYPE_Y, w, SB_CHIP_H, SB_ChipDraw );
	w = 4 * SMALLCHAR_WIDTH + 16;
	SB_InitChip( &g_arenaservers.showfull, ID_SHOWFULL, "FULL", SB_LX + SB_LW - ( 5 * SMALLCHAR_WIDTH + 16 ) - 4 - w, SB_TYPE_Y, w, SB_CHIP_H, SB_ChipDraw );

	for ( i = 0; i < SB_COLUMNS; i++ ) {
		SB_InitChip( &g_arenaservers.sorth[i], ID_SORT + i, sbColName[i], SB_LX + sbColX[i], SB_HEAD_Y, sbColW[i], SB_HEAD_H, SB_HeadDraw );
	}

	g_arenaservers.list.generic.type			= MTYPE_SCROLLLIST;
	g_arenaservers.list.generic.flags			= QMF_PULSEIFFOCUS;
	g_arenaservers.list.generic.id				= ID_LIST;
	g_arenaservers.list.generic.callback		= SB_ListEvent;
	g_arenaservers.list.generic.ownerdraw		= SB_ListDraw;
	g_arenaservers.list.generic.x				= SB_LX;
	g_arenaservers.list.generic.y				= SB_LIST_Y;
	g_arenaservers.list.width					= SB_LW / SMALLCHAR_WIDTH;
	g_arenaservers.list.height					= SB_LIST_ROWS;
	g_arenaservers.list.columns					= 1;
	g_arenaservers.list.itemnames				= g_arenaservers.items;
	Menu_AddItem( &g_arenaservers.menu, &g_arenaservers.list );

	SB_InitField( &g_arenaservers.addr, ID_ADDR, SB_LX, SB_ADDR_Y, SB_LW, 50, SB_ADDRMAX );

	// the buttons are as wide as their longest text (FAVORITE / UNFAVORITE, REFRESH / STOP)
	x = SB_BUTTON_MARGIN;
	w = 4 * SMALLCHAR_WIDTH + 24;
	SB_InitChip( &g_arenaservers.back, ID_BACK, "BACK", x, SB_BUTTON_Y, w, SB_BUTTON_H, SB_ButtonDraw );
	x += w + 6;
	w = 7 * SMALLCHAR_WIDTH + 24;
	SB_InitChip( &g_arenaservers.refresh, ID_REFRESH, "REFRESH", x, SB_BUTTON_Y, w, SB_BUTTON_H, SB_ButtonDraw );
	x += w + 6;
	w = 10 * SMALLCHAR_WIDTH + 24;
	SB_InitChip( &g_arenaservers.fav, ID_FAV, "FAVORITE", x, SB_BUTTON_Y, w, SB_BUTTON_H, SB_ButtonDraw );
	w = 7 * SMALLCHAR_WIDTH + 24;
	SB_InitChip( &g_arenaservers.connect, ID_CONNECT, "CONNECT", 640 - SB_BUTTON_MARGIN - w, SB_BUTTON_Y, w, SB_BUTTON_H, SB_ButtonDraw );

	ArenaServers_LoadFavorites();

	g_servertype = Com_Clamp( 0, UIAS_NUM_SOURCES-1, ui_browserMaster.integer );

	g_sortkey = (int)trap_Cvar_VariableValue( "ui_browserSortKey" );
	if ( g_sortkey != SORT_HOST && g_sortkey != SORT_MAP && g_sortkey != SORT_CLIENTS && g_sortkey != SORT_MODE ) {
		g_sortkey = UISORT_PING;
	}
	g_sortrev = SB_Clamp( "df_ui_browserSortRev", 1 );
	g_typefilter = SB_Clamp( "df_ui_browserType", 3 );
	g_fullservers = Com_Clamp( 0, 1, ui_browserShowFull.integer );
	g_emptyservers = Com_Clamp( 0, 1, ui_browserShowEmpty.integer );

	SB_ParseSearch();

	// force to initial state and refresh
	g_servertype = ArenaServers_SetType(g_servertype);

	trap_Cvar_Register(NULL, "debug_protocol", "", 0 );
}


/*
=================
ArenaServers_Cache
=================
*/
void ArenaServers_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_UNKNOWNMAP );
}


/*
=================
UI_ArenaServersMenu
=================
*/
void UI_ArenaServersMenu( void ) {
	ArenaServers_MenuInit();
	UI_PushMenu( &g_arenaservers.menu );
}
