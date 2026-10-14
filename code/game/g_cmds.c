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
#include "g_local.h"

#ifdef MISSIONPACK
#include "../../ui/menudef.h"			// for the voice chats
#endif

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1000];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted;

	// don't send scores to bots, they don't parse it
	if ( ent->r.svFlags & SVF_BOT ) {
		return;
	}

	// DeFRaG scoreboard protocol (qagame 0x1d880, parsed by cgame DF_SVCmd_ParseScores 0x3fb7):
	//   scores <n> <speedAwardUps> <speedAwardClient> "<speedAwardName>" { <client> <bestTimeMs> <ping> <followClient> }*n
	// see disasm/G_CLIENT/scoreboard.md. (Vanilla sent 14 fields per client + team scores.)
	string[0] = 0;
	stringlength = 0;

	numSorted = level.numConnectedClients;

	for (i=0 ; i < numSorted ; i++) {
		int		ping;
		int		follow;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}
		if ( cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW ) {
			follow = cl->sess.spectatorClient;
		} else {
			follow = -1;
		}

		Com_sprintf (entry, sizeof(entry), " %i %i %i %i", level.sortedClients[i],
			cl->pers.bestFinishTime, ping, follow );
		j = strlen(entry);
		if (stringlength + j >= sizeof(string))
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	trap_SendServerCommand( ent-g_entities, va("scores %i %i %i \"%s\"%s", i,
		g_speedAwardValue, g_speedAwardClient, g_speedAwardName, string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}



/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Cheats are not enabled on this server.\n\"");
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent-g_entities, "print \"You must be alive to use this command.\n\"");
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap_Argc();
	for ( i = start ; i < c ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}


/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger( const char * s ) {
	int			i;
	int			len;
	qboolean	foundDigit;

	len = strlen( s );
	foundDigit = qfalse;

	for ( i=0 ; i < len ; i++ ) {
		if ( !isdigit( s[i] ) ) {
			return qfalse;
		}

		foundDigit = qtrue;
	}

	return foundDigit;
}


/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s, qboolean checkNums, qboolean checkNames ) {
	gclient_t	*cl;
	int			idnum;
	char		cleanName[MAX_STRING_CHARS];

	if ( checkNums ) {
		// numeric values could be slot numbers
		if ( StringIsInteger( s ) ) {
			idnum = atoi( s );
			if ( idnum >= 0 && idnum < level.maxclients ) {
				cl = &level.clients[idnum];
				if ( cl->pers.connected == CON_CONNECTED ) {
					return idnum;
				}
			}
		}
	}

	if ( checkNames ) {
		// check for a name match
		for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			Q_strncpyz(cleanName, cl->pers.netname, sizeof(cleanName));
			Q_CleanStr(cleanName);
			if ( !Q_stricmp( cleanName, s ) ) {
				return idnum;
			}
		}
	}

	trap_SendServerCommand( to-g_entities, va("print \"User %s is not on the server\n\"", s));
	return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (gentity_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			i;
	qboolean	give_all;
	gentity_t		*it_ent;
	trace_t		trace;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	name = ConcatArgs( 1 );

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all || Q_stricmp( name, "health") == 0)
	{
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NUM_WEAPONS) - 1 - 
			( 1 << WP_GRAPPLING_HOOK ) - ( 1 << WP_NONE );
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			ent->client->ps.ammo[i] = 999;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		ent->client->ps.stats[STAT_ARMOR] = 200;

		if (!give_all)
			return;
	}

	if (Q_stricmp(name, "excellent") == 0) {
		ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "impressive") == 0) {
		ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "gauntletaward") == 0) {
		ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "defend") == 0) {
		ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "assist") == 0) {
		ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
		return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem (name);
		if (!it) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (gentity_t *ent)
{
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f(gentity_t *ent)
{
	if(!ent->client->pers.localClient)
	{
		trap_SendServerCommand(ent-g_entities,
			"print \"The levelshot command must be executed by a local client\n\"");
		return;
	}

	if(!CheatsOk(ent))
		return;

	// doesn't work in single player
	if(g_gametype.integer == GT_SINGLE_PLAYER)
	{
		trap_SendServerCommand(ent-g_entities,
			"print \"Must not be in singleplayer mode for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap_SendServerCommand(ent-g_entities, "clientLevelShot");
}


/*
==================
Cmd_TeamTask_f
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap_SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}


/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		// DeFRaG (0x1e02f): "kill" as a spectator stops following, or else re-begins
		// the spectator (respawns it at a spawn point) -- see disasm/G_CLIENT/Cmd_Kill.md
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		} else {
			ClientBegin( ent - g_entities );
		}
		return;
	}
	if (ent->health <= 0) {
		return;
	}
	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die (ent, ent, ent, 100000, MOD_SUICIDE);
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	if ( client->sess.sessionTeam == TEAM_RED ) {
		trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " joined the red team.\n\"",
			client->pers.netname) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " joined the blue team.\n\"",
		client->pers.netname));
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " joined the spectators.\n\"",
		client->pers.netname));
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " joined the battle.\n\"",
		client->pers.netname));
	}
}

/*
=================
SetTeam
=================
*/
void SetTeam( gentity_t *ent, const char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}

		if ( g_teamForceBalance.integer && !client->pers.localClient && !( ent->r.svFlags & SVF_BOT ) ) {
			int		counts[TEAM_NUM_TEAMS];

			counts[TEAM_BLUE] = TeamCount( clientNum, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( clientNum, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				trap_SendServerCommand( clientNum, 
					"cp \"Red team has too many players.\n\"" );
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				trap_SendServerCommand( clientNum, 
					"cp \"Blue team has too many players.\n\"" );
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_TOURNAMENT)
		&& level.numNonSpectatorClients >= 2 ) {
		team = TEAM_SPECTATOR;
	} else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer ) {
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	// if the player was dead leave the body, but only if they're actually in game
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->pers.connected == CON_CONNECTED ) {
		CopyToBodyQue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		player_die (ent, ent, ent, 100000, MOD_SUICIDE);

	}

	// they go to the end of the line for tournements
	if(team == TEAM_SPECTATOR && oldTeam != team)
		AddTournamentQueue(client);

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	// get and distribute relevant parameters
	ClientUserinfoChanged( clientNum );

	// client hasn't spawned yet, they sent an early team command, teampref userinfo, or g_teamAutoJoin is enabled
	if ( client->pers.connected != CON_CONNECTED ) {
		return;
	}

	BroadcastTeamChange( client, oldTeam );

	ClientBegin( clientNum );
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing( gentity_t *ent ) {
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;	
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;

	SetClientViewAngle( ent, ent->client->ps.viewangles );

	// don't use dead view angles
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		ent->client->ps.stats[STAT_HEALTH] = 1;
	}
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		oldTeam = ent->client->sess.sessionTeam;
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap_SendServerCommand( ent-g_entities, "print \"Blue team\n\"" );
			break;
		case TEAM_RED:
			trap_SendServerCommand( ent-g_entities, "print \"Red team\n\"" );
			break;
		case TEAM_FREE:
			trap_SendServerCommand( ent-g_entities, "print \"Free team\n\"" );
			break;
		case TEAM_SPECTATOR:
			trap_SendServerCommand( ent-g_entities, "print \"Spectator team\n\"" );
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent-g_entities, "print \"May not switch teams more than once per 5 seconds.\n\"" );
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	trap_Argv( 1, s, sizeof( s ) );

	SetTeam( ent, s );

	ent->client->switchTeamTime = level.time + 5000;
}


/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
	}

	if ( dir != 1 && dir != -1 ) {
		G_Error( "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	// if dedicated follow client, just switch between the two auto clients
	if (ent->client->sess.spectatorClient < 0) {
		if (ent->client->sess.spectatorClient == -1) {
			ent->client->sess.spectatorClient = -2;
		} else if (ent->client->sess.spectatorClient == -2) {
			ent->client->sess.spectatorClient = -1;
		}
		return;
	}

	clientnum = ent->client->sess.spectatorClient;
	original = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}


/*
==================
G_Say
==================
*/

/*
==================
G_IsIgnoring

DeFRaG func_0x1de19: does 'ent' ignore client 'num'?
==================
*/
qboolean G_IsIgnoring( gentity_t *ent, int num ) {
	if ( num < 0 || num > 63 ) {
		return qfalse;
	}
	return ( ent->client->pers.ignoreMask[num >> 5] >> ( num & 31 ) ) & 1;
}

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message ) {
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	// DeFRaG (G_Say, bytecode 0x1e614): skip listeners who ignore the speaker
	if ( ent->client && G_IsIgnoring( other, ent->client->ps.clientNum ) ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	// no chatting to players in tournements
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		return;
	}

	trap_SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\"", 
		mode == SAY_TEAM ? "tchat" : "chat",
		name, Q_COLOR_ESCAPE, color, message));
}

#define EC		"\x19"

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT];
	char		location[64];

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	switch ( mode ) {
	default:
	case SAY_ALL:
		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, chatText );
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC") (%s)"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
		else
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
		if (target && target->inuse && target->client && g_gametype.integer >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"] (%s)"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
		else
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_MAGENTA;
		break;
	}

	Q_strncpyz( text, chatText, sizeof(text) );

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text );
		return;
	}

	// echo the text to the console
	if ( g_dedicated.integer ) {
		G_Printf( "%s%s\n", name, text);
	}

	// send it to all the appropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_SayTo( ent, other, mode, color, name, text );
	}
}

static void SanitizeChatText( char *text ) {
	int i;

	for ( i = 0; text[i]; i++ ) {
		if ( text[i] == '\n' || text[i] == '\r' ) {
			text[i] = ' ';
		}
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 ) {
	char		*p;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	SanitizeChatText( p );

	G_Say( ent, NULL, mode, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap_Argc () < 3 ) {
		trap_SendServerCommand( ent-g_entities, "print \"Usage: tell <player id> <message>\n\"" );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	p = ConcatArgs( 2 );

	SanitizeChatText( p );

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}


#ifdef MISSIONPACK
static void G_VoiceTo( gentity_t *ent, gentity_t *other, int mode, const char *id, qboolean voiceonly ) {
	int color;
	char *cmd;

	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( mode == SAY_TEAM && !OnSameTeam(ent, other) ) {
		return;
	}
	// no chatting to players in tournements
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		return;
	}

	if (mode == SAY_TEAM) {
		color = COLOR_CYAN;
		cmd = "vtchat";
	}
	else if (mode == SAY_TELL) {
		color = COLOR_MAGENTA;
		cmd = "vtell";
	}
	else {
		color = COLOR_GREEN;
		cmd = "vchat";
	}

	trap_SendServerCommand( other-g_entities, va("%s %d %d %d %s", cmd, voiceonly, ent->s.number, color, id));
}

void G_Voice( gentity_t *ent, gentity_t *target, int mode, const char *id, qboolean voiceonly ) {
	int			j;
	gentity_t	*other;

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	if ( target ) {
		G_VoiceTo( ent, target, mode, id, voiceonly );
		return;
	}

	// echo the text to the console
	if ( g_dedicated.integer ) {
		G_Printf( "voice: %s %s\n", ent->client->pers.netname, id);
	}

	// send it to all the appropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_VoiceTo( ent, other, mode, id, voiceonly );
	}
}

/*
==================
Cmd_Voice_f
==================
*/
static void Cmd_Voice_f( gentity_t *ent, int mode, qboolean arg0, qboolean voiceonly ) {
	char		*p;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	SanitizeChatText( p );

	G_Voice( ent, NULL, mode, p, voiceonly );
}

/*
==================
Cmd_VoiceTell_f
==================
*/
static void Cmd_VoiceTell_f( gentity_t *ent, qboolean voiceonly ) {
	int			targetNum;
	gentity_t	*target;
	char		*id;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap_Argc () < 3 ) {
		trap_SendServerCommand( ent-g_entities, va( "print \"Usage: %s <player id> <voice id>\n\"", voiceonly ? "votell" : "vtell" ) );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	id = ConcatArgs( 2 );

	SanitizeChatText( id );

	G_LogPrintf( "vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, id );
	G_Voice( ent, target, SAY_TELL, id, voiceonly );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Voice( ent, ent, SAY_TELL, id, voiceonly );
	}
}


/*
==================
Cmd_VoiceTaunt_f
==================
*/
static void Cmd_VoiceTaunt_f( gentity_t *ent ) {
	gentity_t *who;
	int i;

	if (!ent->client) {
		return;
	}

	// insult someone who just killed you
	if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number) {
		// i am a dead corpse
		if (!(ent->enemy->r.svFlags & SVF_BOT)) {
			G_Voice( ent, ent->enemy, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		if (!(ent->r.svFlags & SVF_BOT)) {
			G_Voice( ent, ent,        SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		ent->enemy = NULL;
		return;
	}
	// insult someone you just killed
	if (ent->client->lastkilled_client >= 0 && ent->client->lastkilled_client != ent->s.number) {
		who = g_entities + ent->client->lastkilled_client;
		if (who->client) {
			// who is the person I just killed
			if (who->client->lasthurt_mod == MOD_GAUNTLET) {
				if (!(who->r.svFlags & SVF_BOT)) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse );	// and I killed them with a gauntlet
				}
				if (!(ent->r.svFlags & SVF_BOT)) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse );
				}
			} else {
				if (!(who->r.svFlags & SVF_BOT)) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLINSULT, qfalse );	// and I killed them with something else
				}
				if (!(ent->r.svFlags & SVF_BOT)) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLINSULT, qfalse );
				}
			}
			ent->client->lastkilled_client = -1;
			return;
		}
	}

	if (g_gametype.integer >= GT_TEAM) {
		// praise a team mate who just got a reward
		for(i = 0; i < MAX_CLIENTS; i++) {
			who = g_entities + i;
			if (who->client && who != ent && who->client->sess.sessionTeam == ent->client->sess.sessionTeam) {
				if (who->client->rewardTime > level.time) {
					if (!(who->r.svFlags & SVF_BOT)) {
						G_Voice( ent, who, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					if (!(ent->r.svFlags & SVF_BOT)) {
						G_Voice( ent, ent, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					return;
				}
			}
		}
	}

	// just say something
	G_Voice( ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse );
}
#endif



static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

static const int numgc_orders = ARRAY_LEN( gc_orders );

void Cmd_GameCommand_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	int			order;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 3 ) {
		trap_SendServerCommand( ent-g_entities, va( "print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1 ) );
		return;
	}

	trap_Argv( 2, arg, sizeof( arg ) );
	order = atoi( arg );

	if ( order < 0 || order >= numgc_orders ) {
		trap_SendServerCommand( ent-g_entities, va("print \"Bad order: %i\n\"", order));
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order] );
	G_Say( ent, target, SAY_TELL, gc_orders[order] );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, gc_orders[order] );
	}
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", vtos(ent->r.currentOrigin) ) );
}

static const char *gameNames[] = {
	"Free For All",
	"Tournament",
	"Single Player",
	"Team Deathmatch",
	"Capture the Flag",
	"One Flag CTF",
	"Overload",
	"Harvester"
};


/*
==================
DeFRaG vote system (func_0x3142 callvote/cv, CheckVote 0x33dc), see disasm/G_CLIENT/callvote.md

Active on dedicated/variant servers (g_dfVariant) while df_vote_system is non-zero; otherwise the vanilla vote code runs.
level.voteTime holds the vote DEADLINE here (df_vote_voteDelay seconds after the call).
Not ported: the gametype and kill_OBs commands (gametype name switch and df_ob_KillOBs consumer undecoded/absent).
==================
*/
qboolean G_DFVotesActive( void ) {
	return ( g_dfVariant && df_vote_system.integer ) ? qtrue : qfalse;
}

typedef struct {
	const char	*name;
	const char	*format;		// executed command for commands without a validator
	const char	*allowCvar;
	int			needsArg;
	const char	*usage;
} dfVoteCmd_t;

static const dfVoteCmd_t dfVoteCmds[] = {
	{ "map",          "",                  "df_vote_allow_map",          1, "usage: map <map name> - changes the map.\n" },
	{ "devmap",       "",                  "df_vote_allow_devmap",       1, "usage: devmap <map name> - changes the map with cheats enabled.\n" },
	{ "map_restart",  "map_restart 3",     "df_vote_allow_map_restart",  0, "" },
	{ "nextmap",      "vstr nextmap",      "df_vote_allow_nextmap",      0, "" },
	{ "promode",      "",                  "df_vote_allow_promode",      1, "usage: promode <n> - 0: vq3, 1: cpm.\n" },
	{ "mode",         "",                  "df_vote_allow_mode",         1, "usage: mode <n> - changes the game mode.\n" },
	{ "kick",         "",                  "df_vote_allow_kick",         1, "usage: kick <player name> - kicks the specified player.\n" },
	{ "clientkick",   "",                  "df_vote_allow_clientkick",   1, "usage: clientkick <client num> - kicks the specified client.\n" },
	{ "interference", "",                  "df_vote_allow_interference", 1,
	  "usage: interference <n> - changes how players interact with one another.\n  0: full interaction\n  1: interaction only with missiles\n  2: interaction except for missiles\n  3: no interaction\n" },
};
#define NUM_DFVOTECMDS ( sizeof( dfVoteCmds ) / sizeof( dfVoteCmds[0] ) )

static const char *dfInterferenceNames[4] = {
	"full interaction", "interaction only with missiles", "interaction except for missiles", "no interaction"
};

void G_RegisterVoteCvars( void ) {
	int i;

	trap_Cvar_Register( NULL, "df_vote_allowVotes",      "1",  CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "df_vote_passVoteOnRatio", ".6", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "df_vote_maxVotes",        "5",  CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "df_vote_voteDelay",       "30", CVAR_ARCHIVE );
	for ( i = 0; i < NUM_DFVOTECMDS; i++ ) {
		trap_Cvar_Register( NULL, dfVoteCmds[i].allowCvar, !Q_stricmp( dfVoteCmds[i].name, "interference" ) ? "0" : "1", CVAR_ARCHIVE );
	}
}

static qboolean G_DFVoteDigit( const char *arg, int lo, int hi, int *out ) {
	if ( arg[0] < '0' || arg[0] > '9' ) {
		return qfalse;
	}
	*out = atoi( arg );
	return ( *out >= lo && *out <= hi ) ? qtrue : qfalse;
}

// validators: on success they fill level.voteString (executed) and level.voteDisplayString (shown)
static qboolean G_DFVoteValidate( gentity_t *ent, const dfVoteCmd_t *cmd, const char *arg ) {
	char	s[MAX_STRING_CHARS];
	int		v, num, i;
	char	clean[MAX_NETNAME];

	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", cmd->name, arg );

	if ( !Q_stricmp( cmd->name, "map" ) || !Q_stricmp( cmd->name, "devmap" ) ) {
		fileHandle_t f;
		if ( trap_FS_FOpenFile( va( "maps/%s.bsp", arg ), &f, FS_READ ) < 0 ) {
			trap_SendServerCommand( ent - g_entities, "print \"Unknown map.\n\"" );
			return qfalse;
		}
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", cmd->name, arg, s );
		return qtrue;
	}
	if ( !Q_stricmp( cmd->name, "nextmap" ) ) {
		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
		if ( !s[0] ) {
			trap_SendServerCommand( ent - g_entities, "print \"Nextmap not set.\n\"" );
			return qfalse;
		}
	}
	if ( !Q_stricmp( cmd->name, "promode" ) ) {
		if ( !G_DFVoteDigit( arg, 0, 1, &v ) ) {
			return qfalse;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "df_promode %i; map_restart 3", v );
		return qtrue;
	}
	if ( !Q_stricmp( cmd->name, "mode" ) ) {
		if ( !G_DFVoteDigit( arg, 1, 7, &v ) ) {
			return qfalse;
		}
		if ( g_dfMode == 2 ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "df_mode_tricksmode %i; map_restart 3", v );
		} else if ( g_dfMode == 3 ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "df_mode_fastcaps %i; map_restart 3", v );
		} else {
			trap_SendServerCommand( ent - g_entities, "print \"Game modes don't apply to the current gametype.\n\"" );
			return qfalse;
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd->name, "kick" ) || !Q_stricmp( cmd->name, "clientkick" ) ) {
		qboolean byName = !Q_stricmp( cmd->name, "kick" );

		num = -1;
		if ( byName ) {
			for ( i = 0; i < level.maxclients; i++ ) {
				if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
					continue;
				}
				Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
				Q_CleanStr( clean );
				if ( !Q_stricmp( arg, level.clients[i].pers.netname ) || !Q_stricmp( arg, clean ) ) {
					num = i;
					break;
				}
			}
			if ( num < 0 ) {
				trap_SendServerCommand( ent - g_entities, "print \"Unknown player.\n\"" );
				return qfalse;
			}
		} else if ( !G_DFVoteDigit( arg, 0, 63, &num ) ) {
			trap_SendServerCommand( ent - g_entities, "print \"Client num out of range.\n\"" );
			return qfalse;
		}
		if ( level.clients[num].pers.connected != CON_CONNECTED ) {
			trap_SendServerCommand( ent - g_entities, byName ? "print \"Player not connected.\n\"" : "print \"Not a connected client.\n\"" );
			return qfalse;
		}
		if ( level.clients[num].pers.localClient ) {
			trap_SendServerCommand( ent - g_entities, "print \"Cannot kick host player.\n\"" );
			return qfalse;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %i", num );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s", level.clients[num].pers.netname );
		return qtrue;
	}
	if ( !Q_stricmp( cmd->name, "interference" ) ) {
		if ( !G_DFVoteDigit( arg, 0, 3, &v ) ) {
			return qfalse;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "df_mp_InterferenceOff %i; map_restart 3", v );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "interference %s", dfInterferenceNames[v] );
		return qtrue;
	}
	// map_restart / nextmap: fixed command
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", cmd->format );
	return qtrue;
}

void Cmd_DFCallVote_f( gentity_t *ent ) {
	char	arg1[64], arg2[64], list[MAX_STRING_CHARS];
	int		i, max, col = 0;
	const dfVoteCmd_t *cmd = NULL;
	int		clientNum = ent - g_entities;

	if ( level.voteTime ) {
		trap_SendServerCommand( clientNum, "print \"A vote is already in progress.\n\"" );
		return;
	}
	if ( !trap_Cvar_VariableIntegerValue( "df_vote_allowVotes" ) ) {
		trap_SendServerCommand( clientNum, "print \"Voting not allowed here.\n\"" );
		return;
	}
	max = trap_Cvar_VariableIntegerValue( "df_vote_maxVotes" );
	if ( max && ent->client->pers.voteCount >= max ) {
		trap_SendServerCommand( clientNum, "print \"You have used up all of your votes.\n\"" );
		return;
	}

	trap_Argv( 1, arg1, sizeof( arg1 ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );

	if ( !arg1[0] ) {
		Q_strncpyz( list, "print \"Available vote commands:^3", sizeof( list ) );
		for ( i = 0; i < NUM_DFVOTECMDS; i++ ) {
			if ( !trap_Cvar_VariableIntegerValue( dfVoteCmds[i].allowCvar ) ) {
				continue;
			}
			Q_strcat( list, sizeof( list ), va( col % 3 ? " %-15.15s" : "\n%-15.15s", dfVoteCmds[i].name ) );
			col++;
		}
		Q_strcat( list, sizeof( list ), "^7\n\"" );
		trap_SendServerCommand( clientNum, list );
		return;
	}

	level.voteString[0] = 0;
	level.voteDisplayString[0] = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		level.clients[i].ps.eFlags &= ~EF_VOTED;
		level.clients[i].pers.dfVote = 0;
	}

	// refuse command separators / quotes in the argument (the executed string is built from it)
	for ( i = 0; arg2[i]; i++ ) {
		if ( arg2[i] == ';' || arg2[i] == '\n' || arg2[i] == '\r' || arg2[i] == '"' ) {
			trap_SendServerCommand( clientNum, "print \"Invalid vote string.\n\"" );
			return;
		}
	}

	for ( i = 0; i < NUM_DFVOTECMDS; i++ ) {
		if ( !Q_stricmp( arg1, dfVoteCmds[i].name ) ) {
			cmd = &dfVoteCmds[i];
			break;
		}
	}
	if ( !cmd ) {
		trap_SendServerCommand( clientNum, "print \"Vote command not recognized.\n\"" );
		return;
	}
	if ( !trap_Cvar_VariableIntegerValue( cmd->allowCvar ) ) {
		trap_SendServerCommand( clientNum, va( "print \"Vote command ^3%s^7 not allowed here.\n\"", cmd->name ) );
		return;
	}
	if ( cmd->needsArg && !arg2[0] ) {
		trap_SendServerCommand( clientNum, va( "print \"Missing argument.\n%s\"", cmd->usage ) );
		return;
	}
	if ( !G_DFVoteValidate( ent, cmd, arg2 ) ) {
		return;
	}

	level.voteTime = level.time + 1000 * trap_Cvar_VariableIntegerValue( "df_vote_voteDelay" );
	level.voteExecuteTime = 0;
	ent->client->ps.eFlags |= EF_VOTED;
	ent->client->pers.dfVote = 1;
	ent->client->pers.voteCount++;
	level.voteYes = 1;
	level.voteNo = 0;

	trap_SendServerCommand( -1, va( "print \"%s ^7called a vote: %s.\n\"", ent->client->pers.netname, level.voteDisplayString ) );
	trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
	trap_SetConfigstring( CS_VOTE_YES, "1" );
	trap_SetConfigstring( CS_VOTE_NO, "0" );
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f( gentity_t *ent ) {
	char*	c;
	int		i;
	char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_STRING_TOKENS];

	if ( G_DFVotesActive() ) {
		Cmd_DFCallVote_f( ent );
		return;
	}

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here.\n\"" );
		return;
	}

	if ( level.voteTime ) {
		trap_SendServerCommand( ent-g_entities, "print \"A vote is already in progress.\n\"" );
		return;
	}
	if ( ent->client->pers.voteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent-g_entities, "print \"You have called the maximum number of votes.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );

	// check for command separators in arg2
	for( c = arg2; *c; ++c) {
		switch(*c) {
			case '\n':
			case '\r':
			case ';':
				trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
				return;
			break;
		}
	}

	if ( !Q_stricmp( arg1, "map_restart" ) ) {
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
	} else if ( !Q_stricmp( arg1, "map" ) ) {
	} else if ( !Q_stricmp( arg1, "g_gametype" ) ) {
	} else if ( !Q_stricmp( arg1, "kick" ) ) {
	} else if ( !Q_stricmp( arg1, "clientkick" ) ) {
	} else if ( !Q_stricmp( arg1, "g_doWarmup" ) ) {
	} else if ( !Q_stricmp( arg1, "timelimit" ) ) {
	} else if ( !Q_stricmp( arg1, "fraglimit" ) ) {
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, kick <player>, clientkick <clientnum>, g_doWarmup, timelimit <time>, fraglimit <frags>.\n\"" );
		return;
	}

	// if there is still a vote to be executed
	if ( level.voteExecuteTime ) {
		// don't start a vote when map change or restart is in progress
		if ( !Q_stricmpn( level.voteString, "map", 3 )
			|| !Q_stricmpn( level.voteString, "nextmap", 7 ) ) {
			trap_SendServerCommand( ent-g_entities, "print \"Vote after map change.\n\"" );
			return;
		}

		level.voteExecuteTime = 0;
		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}

	// special case for g_gametype, check for bad values
	if ( !Q_stricmp( arg1, "g_gametype" ) ) {
		i = atoi( arg2 );
		if( i == GT_SINGLE_PLAYER || i < GT_FFA || i >= GT_MAX_GAME_TYPE) {
			trap_SendServerCommand( ent-g_entities, "print \"Invalid gametype.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[i] );
	} else if ( !Q_stricmp( arg1, "map" ) ) {
		// special case for map changes, we want to reset the nextmap setting
		// this allows a player to change maps, but not upset the map rotation
		char	s[MAX_STRING_CHARS];

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (*s) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
		} else {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
		}
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
		char	s[MAX_STRING_CHARS];

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (!*s) {
			trap_SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} else if ( !Q_stricmp( arg1, "clientkick" ) || !Q_stricmp( arg1, "kick" ) ) {
		i = ClientNumberFromString( ent, arg2, !Q_stricmp( arg1, "clientkick" ), !Q_stricmp( arg1, "kick" ) );
		if ( i == -1 ) {
			return;
		}

		if ( level.clients[i].pers.localClient ) {
			trap_SendServerCommand( ent - g_entities, "print \"Cannot kick host player.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %d", i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s", level.clients[i].pers.netname );
	} else {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	}

	trap_SendServerCommand( -1, va("print \"%s called a vote.\n\"", ent->client->pers.netname ) );

	// start the voting, the caller automatically votes yes
	level.voteTime = level.time;
	level.voteYes = 1;
	level.voteNo = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		level.clients[i].ps.eFlags &= ~EF_VOTED;
	}
	ent->client->ps.eFlags |= EF_VOTED;

	trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );	
	trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64];

	if ( !level.voteTime ) {
		trap_SendServerCommand( ent-g_entities, "print \"No vote in progress.\n\"" );
		return;
	}
	if ( ent->client->ps.eFlags & EF_VOTED ) {
		trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" );
		return;
	}

	trap_SendServerCommand( ent-g_entities, "print \"Vote cast.\n\"" );

	ent->client->ps.eFlags |= EF_VOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	ent->client->pers.dfVote = ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) ? 1 : 2;

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.voteYes++;
		trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	} else {
		level.voteNo++;
		trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	char*	c;
	int		i, team, cs_offset;
	char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_STRING_TOKENS];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here.\n\"" );
		return;
	}

	if ( level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, "print \"A team vote is already in progress.\n\"" );
		return;
	}
	if ( ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent-g_entities, "print \"You have called the maximum number of team votes.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	for ( i = 2; i < trap_Argc(); i++ ) {
		if (i > 2)
			strcat(arg2, " ");
		trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - strlen(arg2) );
	}

	// check for command separators in arg2
	for( c = arg2; *c; ++c) {
		switch(*c) {
			case '\n':
			case '\r':
			case ';':
				trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
				return;
			break;
		}
	}

	if ( !Q_stricmp( arg1, "leader" ) ) {
		char netname[MAX_NETNAME], leader[MAX_NETNAME];

		if ( !arg2[0] ) {
			i = ent->client->ps.clientNum;
		}
		else {
			// numeric values are just slot numbers
			for (i = 0; i < 3; i++) {
				if ( !arg2[i] || arg2[i] < '0' || arg2[i] > '9' )
					break;
			}
			if ( i >= 3 || !arg2[i]) {
				i = atoi( arg2 );
				if ( i < 0 || i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) );
					return;
				}

				if ( !g_entities[i].inuse ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) );
					return;
				}
			}
			else {
				Q_strncpyz(leader, arg2, sizeof(leader));
				Q_CleanStr(leader);
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					if ( level.clients[i].pers.connected == CON_DISCONNECTED )
						continue;
					if (level.clients[i].sess.sessionTeam != team)
						continue;
					Q_strncpyz(netname, level.clients[i].pers.netname, sizeof(netname));
					Q_CleanStr(netname);
					if ( !Q_stricmp(netname, leader) ) {
						break;
					}
				}
				if ( i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) );
					return;
				}
			}
		}
		Com_sprintf(arg2, sizeof(arg2), "%d", i);
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
		return;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if (level.clients[i].sess.sessionTeam == team)
			trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
	}

	// start the voting, the caller automatically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam == team)
			level.clients[i].ps.eFlags &= ~EF_TEAMVOTED;
	}
	ent->client->ps.eFlags |= EF_TEAMVOTED;

	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
	trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	int			team, cs_offset;
	char		msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, "print \"No team vote in progress.\n\"" );
		return;
	}
	if ( ent->client->ps.eFlags & EF_TEAMVOTED ) {
		trap_SendServerCommand( ent-g_entities, "print \"Team vote already cast.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" );
		return;
	}

	trap_SendServerCommand( ent-g_entities, "print \"Team vote cast.\n\"" );

	ent->client->ps.eFlags |= EF_TEAMVOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );	
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Cheats are not enabled on this server.\n\"");
		return;
	}
	if ( trap_Argc() != 5 ) {
		trap_SendServerCommand( ent-g_entities, "print \"usage: setviewpos x y z yaw\n\"");
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}


/*
=================
Cmd_PlacePlayer_f

Cheat-only command that places the player at an exact origin, view
angles and velocity, with no teleport knockback/hold applied.

argv(1..3) x y z		- origin
argv(4..6) p y r		- view angles
argv(7..9) v0 v1 v2		- velocity (X Y Z)
=================
*/
void Cmd_PlacePlayer_f( gentity_t *ent ) {
	vec3_t		origin, angles, velocity;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !CheatsOk( ent ) ) {
		return;
	}
	if ( trap_Argc() != 10 ) {
		trap_SendServerCommand( ent-g_entities, "print \"usage: placeplayer x y z p y r v0 v1 v2\n\"");
		return;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 4, buffer, sizeof( buffer ) );
		angles[i] = atof( buffer );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 7, buffer, sizeof( buffer ) );
		velocity[i] = atof( buffer );
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap_UnlinkEntity( ent );

	VectorCopy( origin, ent->client->ps.origin );
	VectorCopy( velocity, ent->client->ps.velocity );
	SetClientViewAngle( ent, angles );

	// toggle the teleport bit so the client knows to not lerp
	ent->client->ps.eFlags ^= EF_TELEPORT_BIT;

	trap_LinkEntity( ent );
}


/*
=================
Cmd_RespawnPoint_f

DeFRaG client command (bytecode 0x47b1, see disasm/G_CLIENT/respawnpoint.md):
	respawnpoint set			- use current origin
	respawnpoint set x y z		- use the given point
	respawnpoint clear			- unset
ClientSpawn then spawns at the spawn entity closest to this point.
=================
*/
void Cmd_RespawnPoint_f( gentity_t *ent ) {
	char		arg[MAX_TOKEN_CHARS];
	vec3_t		point;
	int			i;

	trap_Argv( 1, arg, sizeof( arg ) );

	if ( !Q_stricmp( arg, "set" ) && trap_Argc() == 5 ) {
		for ( i = 0 ; i < 3 ; i++ ) {
			trap_Argv( i + 2, arg, sizeof( arg ) );
			point[i] = atof( arg );
		}
	} else if ( !Q_stricmp( arg, "set" ) ) {
		VectorCopy( ent->client->ps.origin, point );
	} else if ( !Q_stricmp( arg, "clear" ) ) {
		VectorClear( ent->client->pers.respawnPoint );
		trap_SendServerCommand( ent-g_entities, "print \"^2Respawnpoint Cleared\n\"" );
		return;
	} else {
		trap_SendServerCommand( ent-g_entities,
			"print \"Usage: respawnpoint set\n respawnpoint set x y z\n respawnpoint clear\n\"" );
		return;
	}

	VectorCopy( point, ent->client->pers.respawnPoint );
	trap_SendServerCommand( ent-g_entities,
		va( "print \"^2Respawnpoint Set:^7 ( %.3f %.3f %.3f )\n\"", point[0], point[1], point[2] ) );
}


/*
=================
Cmd_Ignore_f

DeFRaG client command (bytecode 0x1de5e, see disasm/G_CLIENT/ignore_command.md):
	ignore <name|number>	- toggle ignoring that client's chat
=================
*/
void Cmd_Ignore_f( gentity_t *ent ) {
	char		arg[MAX_TOKEN_CHARS];
	int			target;

	trap_Argv( 1, arg, sizeof( arg ) );
	target = ClientNumberFromString( ent, arg, qtrue, qtrue );
	if ( target < 0 ) {
		return;
	}

	if ( G_IsIgnoring( ent, target ) ) {
		trap_SendServerCommand( ent-g_entities, va( "print \"Now unignoring %s.\n\"", level.clients[target].pers.netname ) );
	} else {
		trap_SendServerCommand( ent-g_entities, va( "print \"Now ignoring %s.\n\"", level.clients[target].pers.netname ) );
	}
	if ( target >= 0 && target < 64 ) {
		ent->client->pers.ignoreMask[target >> 5] ^= 1u << ( target & 31 );
	}
}


/*
=================
Cmd_Arena_f

DeFRaG client command (bytecode 0x4a5f, see disasm/G_CLIENT/arena_command.md):
	arena <n>	- switch to multi-arena number n (re-begins the client)
=================
*/
void Cmd_Arena_f( gentity_t *ent ) {
	char		arg[MAX_TOKEN_CHARS];
	int			n;

	if ( !level.numMultiArenas ) {
		trap_SendServerCommand( ent-g_entities, "print \"Not a multiarena map\n\"" );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	n = atoi( arg );
	if ( trap_Argc() != 2 || ent->client->pers.arena == n || n > level.numMultiArenas ) {
		trap_SendServerCommand( ent-g_entities,
			va( "print \"Arena: %i of %i\n\"", ent->client->pers.arena, level.numMultiArenas ) );
		return;
	}

	ent->client->pers.arena = n;
	ClientBegin( ent-g_entities );
	// DeFRaG also stamps client+0x350 = level.time + 5000 here (shared with the team-change cooldown); consumer not located, not ported
}


/*
=================
Cmd_FireMissile_f

DeFRaG cheat command (bytecode 0x489b, see disasm/G_CLIENT/ClientCommands_sweep.md):
	firemissile x y z fx fy fz weapon quaded selfstyle
weapon is 4 (grenade), 5 (rocket), 8 (plasma) or 9 (bfg); anything else fires nothing.
=================
*/
void Cmd_FireMissile_f( gentity_t *ent ) {
	vec3_t		start, forward;
	char		buffer[MAX_TOKEN_CHARS];
	gentity_t	*missile;
	float		quadFactor;
	int			weapon, i, savedNumber;
	qboolean	selfStyle;

	if ( !CheatsOk( ent ) ) {
		return;
	}
	if ( trap_Argc() != 10 ) {
		trap_SendServerCommand( ent-g_entities,
			"print \"Usage: firemissile (start) x y z (forward) x y z weapon_num (4|5|8|9) quaded (0|1) self-style (0|1)\n\"" );
		return;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		start[i] = atof( buffer );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 4, buffer, sizeof( buffer ) );
		forward[i] = atof( buffer );
	}
	trap_Argv( 7, buffer, sizeof( buffer ) );
	weapon = (int)atof( buffer );
	trap_Argv( 8, buffer, sizeof( buffer ) );
	quadFactor = ( atof( buffer ) > 0 ) ? 3.0f : 1.0f;
	trap_Argv( 9, buffer, sizeof( buffer ) );
	selfStyle = ( atof( buffer ) != 0 );

	// self-style 0: fire with an invalid owner number, restored below
	savedNumber = ent->s.number;
	if ( !selfStyle ) {
		ent->s.number = -1;
	}

	// muzzle offset of 14 units along the aim direction, then snapped to integers
	for ( i = 0 ; i < 3 ; i++ ) {
		start[i] += 14.0f * forward[i];
		start[i] = (int)start[i];
	}

	missile = NULL;
	switch ( weapon ) {
	case 4:
		forward[2] += 0.2f;
		VectorNormalize( forward );
		missile = fire_grenade( ent, start, forward );
		break;
	case 5:
		missile = fire_rocket( ent, start, forward );
		break;
	case 8:
		missile = fire_plasma( ent, start, forward );
		break;
	case 9:
		missile = fire_bfg( ent, start, forward );
		break;
	}

	if ( !selfStyle ) {
		ent->s.number = savedNumber;
	}
	if ( !missile ) {
		return;
	}
	missile->damage = (int)( missile->damage * quadFactor );
	missile->splashDamage = (int)( missile->splashDamage * quadFactor );
}


/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f( gentity_t *ent ) {
/*
	int max, n, i;

	max = trap_AAS_PointReachabilityAreaIndex( NULL );

	n = 0;
	for ( i = 0; i < max; i++ ) {
		if ( ent->client->areabits[i >> 3] & (1 << (i & 7)) )
			n++;
	}

	//trap_SendServerCommand( ent-g_entities, va("print \"visited %d of %d areas\n\"", n, max));
	trap_SendServerCommand( ent-g_entities, va("print \"%d%% level coverage\n\"", n * 100 / max));
*/
}

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum ) {
	gentity_t *ent;
	char	cmd[MAX_TOKEN_CHARS];

	ent = g_entities + clientNum;
	if (!ent->client || ent->client->pers.connected != CON_CONNECTED) {
		if (ent->client && ent->client->pers.localClient) {
			// Handle early team command sent by UI when starting a local
			// team play game.
			trap_Argv( 0, cmd, sizeof( cmd ) );
			if (Q_stricmp (cmd, "team") == 0) {
				Cmd_Team_f (ent);
			}
		}
		return;		// not fully in game yet
	}


	trap_Argv( 0, cmd, sizeof( cmd ) );

	if ( G_AuthCommand( clientNum, cmd ) ) {	// DeFRaG 0x2d45 pre-dispatch (auth_reply / notify)
		return;
	}

	if (Q_stricmp (cmd, "say") == 0) {
		Cmd_Say_f (ent, SAY_ALL, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0) {
		Cmd_Say_f (ent, SAY_TEAM, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "tell") == 0) {
		Cmd_Tell_f ( ent );
		return;
	}
#ifdef MISSIONPACK
	if (Q_stricmp (cmd, "vsay") == 0) {
		Cmd_Voice_f (ent, SAY_ALL, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "vsay_team") == 0) {
		Cmd_Voice_f (ent, SAY_TEAM, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "vtell") == 0) {
		Cmd_VoiceTell_f ( ent, qfalse );
		return;
	}
	if (Q_stricmp (cmd, "vosay") == 0) {
		Cmd_Voice_f (ent, SAY_ALL, qfalse, qtrue);
		return;
	}
	if (Q_stricmp (cmd, "vosay_team") == 0) {
		Cmd_Voice_f (ent, SAY_TEAM, qfalse, qtrue);
		return;
	}
	if (Q_stricmp (cmd, "votell") == 0) {
		Cmd_VoiceTell_f ( ent, qtrue );
		return;
	}
	if (Q_stricmp (cmd, "vtaunt") == 0) {
		Cmd_VoiceTaunt_f ( ent );
		return;
	}
#endif
	if (Q_stricmp (cmd, "score") == 0) {
		Cmd_Score_f (ent);
		return;
	}

	// ignore all other commands when at intermission
	if (level.intermissiontime) {
		Cmd_Say_f (ent, qfalse, qtrue);
		return;
	}

	if (Q_stricmp (cmd, "give") == 0)
		Cmd_Give_f (ent);
	else if (Q_stricmp (cmd, "god") == 0)
		Cmd_God_f (ent);
	else if (Q_stricmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_stricmp (cmd, "noclip") == 0)
		Cmd_Noclip_f (ent);
	else if (Q_stricmp (cmd, "kill") == 0)
		Cmd_Kill_f (ent);
	else if (Q_stricmp (cmd, "teamtask") == 0)
		Cmd_TeamTask_f (ent);
	else if (Q_stricmp (cmd, "levelshot") == 0)
		Cmd_LevelShot_f (ent);
	else if (Q_stricmp (cmd, "follow") == 0)
		Cmd_Follow_f (ent);
	else if (Q_stricmp (cmd, "follownext") == 0)
		Cmd_FollowCycle_f (ent, 1);
	else if (Q_stricmp (cmd, "followprev") == 0)
		Cmd_FollowCycle_f (ent, -1);
	else if (Q_stricmp (cmd, "team") == 0)
		Cmd_Team_f (ent);
	else if (Q_stricmp (cmd, "where") == 0)
		Cmd_Where_f (ent);
	else if (Q_stricmp (cmd, "callvote") == 0 || Q_stricmp (cmd, "cv") == 0)
		Cmd_CallVote_f (ent);
	else if (Q_stricmp (cmd, "vote") == 0)
		Cmd_Vote_f (ent);
	else if (Q_stricmp (cmd, "callteamvote") == 0)
		Cmd_CallTeamVote_f (ent);
	else if (Q_stricmp (cmd, "teamvote") == 0)
		Cmd_TeamVote_f (ent);
	else if (Q_stricmp (cmd, "gc") == 0)
		Cmd_GameCommand_f( ent );
	else if (Q_stricmp (cmd, "setviewpos") == 0)
		Cmd_SetViewpos_f( ent );
	else if (Q_stricmp (cmd, "ignore") == 0)
		Cmd_Ignore_f( ent );
	else if (Q_stricmp (cmd, "arena") == 0)
		Cmd_Arena_f( ent );
	else if (Q_stricmp (cmd, "firemissile") == 0)
		Cmd_FireMissile_f( ent );
	else if (Q_stricmp (cmd, "respawnpoint") == 0)
		Cmd_RespawnPoint_f( ent );
	else if (Q_stricmp (cmd, "placeplayer") == 0)
		Cmd_PlacePlayer_f( ent );
	else if (Q_stricmp (cmd, "stats") == 0)
		Cmd_Stats_f( ent );
	else
		trap_SendServerCommand( clientNum, va("print \"unknown cmd %s\n\"", cmd ) );
}
