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

MAIN MENU SERVER LIST

=======================================================================
*/


#include "ui_local.h"

/*
oDFs: the short server list of the main menu.  It is the favorites of the server browser (the server1..server16 cvars): every
address is pinged like the browser does it (the info reply has name, map and players), the most players first.  The list stays as it was last made (also after a game, until REFRESH or F5): a new one is collected on the side and
replaces it when it is finished, and when nothing came back the old one stays.

The engine has one ping queue, which the server browser also clears when it starts: so MainServers_Stop is called before
any other menu is opened.  A first list that was cut short is made new when the main menu is back.
*/

#define MS_ADRLEN			64
#define MS_MAX				64
#define MS_QUEUE			160
#define MS_PINGS			32
#define MS_GIVE_UP			15000

#define MS_IDLE				0
#define MS_BUSY				1
#define MS_DONE				2

typedef struct {
	char		adr[MS_ADRLEN];
	int			start;
	qboolean	fav;
} msSlot_t;

typedef struct {
	int			state;
	int			startTime, doneTime, nextTick;

	char		queue[MS_QUEUE][MS_ADRLEN];
	qboolean	queueFav[MS_QUEUE];
	int			numQueue, nextQueue;

	msSlot_t	pings[MS_PINGS];

	mainServer_t	lists[2][MS_MAX];		// the one that is shown, and the one that is made
	int			nums[2];
	int			show, build;
	qboolean	loaded;					// a list has been finished
} mainservers_t;

static mainservers_t	ms;


// an address of digits, dots and a port: the engine answers a ping with exactly that, a name would not be found again
static qboolean MS_PlainAddress( const char *adr ) {
	qboolean	colon = qfalse;

	if ( !*adr ) {
		return qfalse;
	}
	for ( ; *adr; adr++ ) {
		if ( *adr == ':' ) {
			colon = qtrue;
		} else if ( *adr != '.' && ( *adr < '0' || *adr > '9' ) ) {
			return qfalse;
		}
	}
	return colon;
}

static int MS_MaxPing( void ) {
	int		maxPing = (int)trap_Cvar_VariableValue( "cl_maxPing" );

	return maxPing < 100 ? 100 : maxPing;
}

static void MS_Queue( const char *adr, qboolean fav ) {
	int		i;

	if ( ms.numQueue >= MS_QUEUE ) {
		return;
	}
	for ( i = 0; i < ms.numQueue; i++ ) {
		if ( !Q_stricmp( ms.queue[i], adr ) ) {
			return;
		}
	}
	Q_strncpyz( ms.queue[ms.numQueue], adr, MS_ADRLEN );
	ms.queueFav[ms.numQueue] = fav;
	ms.numQueue++;
}

static int QDECL MS_Compare( const void *arg1, const void *arg2 ) {
	const mainServer_t	*a = (const mainServer_t *)arg1;
	const mainServer_t	*b = (const mainServer_t *)arg2;
	int		r = b->clients - a->clients;		// the most players first

	if ( !r ) {
		r = a->ping - b->ping;
	}
	if ( !r ) {
		r = Q_stricmp( a->host, b->host );
	}
	return r;
}

static void MS_Sort( void ) {
	if ( ms.nums[ms.build] > 1 ) {
		qsort( ms.lists[ms.build], ms.nums[ms.build], sizeof( mainServer_t ), MS_Compare );
	}
}

static mainServer_t *MS_Find( const char *adr ) {
	int		i;

	for ( i = 0; i < ms.nums[ms.build]; i++ ) {
		if ( !Q_stricmp( ms.lists[ms.build][i].adr, adr ) ) {
			return &ms.lists[ms.build][i];
		}
	}
	return NULL;
}

// a ping answer into the list
static void MS_Insert( const char *adr, const char *info, int pingtime, qboolean fav ) {
	mainServer_t	*s;

	if ( ms.nums[ms.build] >= MS_MAX || MS_Find( adr ) ) {
		return;
	}
	s = &ms.lists[ms.build][ms.nums[ms.build]++];
	memset( s, 0, sizeof( *s ) );
	Q_strncpyz( s->adr, adr, sizeof( s->adr ) );
	Q_strncpyz( s->host, Info_ValueForKey( info, "hostname" ), sizeof( s->host ) );
	Q_CleanStr( s->host );
	if ( !s->host[0] ) {
		Q_strncpyz( s->host, adr, sizeof( s->host ) );
	}
	Q_strncpyz( s->map, Info_ValueForKey( info, "mapname" ), sizeof( s->map ) );
	Q_CleanStr( s->map );
	Q_strlwr( s->map );
	s->clients = atoi( Info_ValueForKey( info, "clients" ) );
	s->maxclients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
	s->ping = pingtime;
	s->needpass = atoi( Info_ValueForKey( info, "g_needpass" ) ) != 0;
	s->fav = fav;
}

static void MS_ClearQueues( void ) {
	int		i;

	for ( i = 0; i < MS_PINGS; i++ ) {
		ms.pings[i].adr[0] = '\0';
		trap_LAN_ClearPing( i );
	}
}

static void MS_Start( void ) {
	char	adr[MS_ADRLEN];
	int		i;

	// the first list is shown as it is made, a later one on the side
	ms.build = ms.loaded ? 1 - ms.show : ms.show;
	ms.nums[ms.build] = 0;
	ms.numQueue = 0;
	ms.nextQueue = 0;
	MS_ClearQueues();

	// the favorites, which are listed even when nobody is on them
	for ( i = 0; i < MAX_FAVORITESERVERS; i++ ) {
		trap_Cvar_VariableStringBuffer( va( "server%d", i + 1 ), adr, sizeof( adr ) );
		if ( MS_PlainAddress( adr ) ) {
			MS_Queue( adr, qtrue );
		}
	}

	ms.state = MS_BUSY;
	ms.startTime = uis.realtime;
	ms.nextTick = 0;
}

static void MS_Finish( void ) {
	ms.state = MS_DONE;
	MS_Sort();
	// no favorite answered (no network): the old list stays
	if ( !ms.loaded || ms.nums[ms.build] > 0 || !ms.numQueue ) {
		ms.show = ms.build;
		ms.loaded = qtrue;
		ms.doneTime = uis.realtime;
	}
}

// the pings: answers into the list, new pings out
static void MS_Pump( void ) {
	char	adr[MS_ADRLEN], info[MAX_INFO_STRING];
	int		i, j, time, maxPing, pending;
	qboolean	added = qfalse;

	if ( uis.realtime < ms.nextTick ) {
		return;
	}
	ms.nextTick = uis.realtime + 20;
	maxPing = MS_MaxPing();

	for ( i = 0; i < MS_PINGS; i++ ) {
		trap_LAN_GetPing( i, adr, sizeof( adr ), &time );
		if ( !adr[0] ) {
			continue;
		}
		for ( j = 0; j < MS_PINGS; j++ ) {
			if ( ms.pings[j].adr[0] && !Q_stricmp( adr, ms.pings[j].adr ) ) {
				break;
			}
		}
		if ( j < MS_PINGS ) {
			if ( !time ) {
				time = uis.realtime - ms.pings[j].start;
				if ( time < maxPing ) {
					continue;		// still waiting
				}
			}
			if ( time < maxPing ) {
				trap_LAN_GetPingInfo( i, info, sizeof( info ) );
				MS_Insert( adr, info, time, ms.pings[j].fav );
				added = qtrue;
			}
			ms.pings[j].adr[0] = '\0';
		}
		trap_LAN_ClearPing( i );
	}

	for ( i = 0; i < 8 && ms.nextQueue < ms.numQueue; i++ ) {
		if ( trap_LAN_GetPingQueueCount() >= MS_PINGS ) {
			break;
		}
		for ( j = 0; j < MS_PINGS; j++ ) {
			if ( !ms.pings[j].adr[0] ) {
				break;
			}
		}
		if ( j >= MS_PINGS ) {
			break;
		}
		Q_strncpyz( ms.pings[j].adr, ms.queue[ms.nextQueue], MS_ADRLEN );
		ms.pings[j].start = uis.realtime;
		ms.pings[j].fav = ms.queueFav[ms.nextQueue];
		trap_Cmd_ExecuteText( EXEC_NOW, va( "ping %s\n", ms.pings[j].adr ) );
		ms.nextQueue++;
	}

	if ( added ) {
		MS_Sort();
	}

	pending = trap_LAN_GetPingQueueCount();
	if ( uis.realtime - ms.startTime > MS_GIVE_UP || ( ms.nextQueue >= ms.numQueue && !pending ) ) {
		MS_Finish();
	}
}

/*
=================
MainServers_Frame

Every frame the main menu is drawn.
=================
*/
void MainServers_Frame( void ) {
	if ( !df_ui_mainServers.integer ) {
		if ( ms.state == MS_BUSY ) {
			MainServers_Stop();
		}
		return;
	}
	if ( ms.state == MS_IDLE ) {
		MS_Start();
	}
	if ( ms.state == MS_BUSY ) {
		MS_Pump();
	}
}

/*
=================
MainServers_Refresh

Makes the list new now (not while it is being made).  The old one stays until the new one is there.
=================
*/
void MainServers_Refresh( void ) {
	if ( ms.state == MS_DONE ) {
		ms.state = MS_IDLE;
	}
}

/*
=================
MainServers_Stop

The ping queue is given up, to leave it to the menu that is opened.  What was shown stays; a first list that is not
finished is made new when the main menu is back.
=================
*/
void MainServers_Stop( void ) {
	if ( ms.state == MS_BUSY ) {
		ms.state = ms.loaded ? MS_DONE : MS_IDLE;
	}
	MS_ClearQueues();
}

int MainServers_Count( void ) {
	return ms.nums[ms.show];
}

const mainServer_t *MainServers_Get( int i ) {
	return ( i >= 0 && i < ms.nums[ms.show] ) ? &ms.lists[ms.show][i] : NULL;
}

qboolean MainServers_Busy( void ) {
	return ms.state != MS_DONE;
}

// how many favorites there are to ask
int MainServers_Favorites( void ) {
	return ms.numQueue;
}

int MainServers_Players( void ) {
	int		i, n = 0;

	for ( i = 0; i < ms.nums[ms.show]; i++ ) {
		n += ms.lists[ms.show][i].clients;
	}
	return n;
}

// seconds since the list was finished
int MainServers_Age( void ) {
	return ms.loaded ? ( uis.realtime - ms.doneTime ) / 1000 : 0;
}
