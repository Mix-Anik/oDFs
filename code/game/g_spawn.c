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

qboolean	G_SpawnString( const char *key, const char *defaultString, char **out ) {
	int		i;

	if ( !level.spawning ) {
		*out = (char *)defaultString;
//		G_Error( "G_SpawnString() called while not spawning" );
	}

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		if ( !Q_stricmp( key, level.spawnVars[i][0] ) ) {
			*out = level.spawnVars[i][1];
			return qtrue;
		}
	}

	*out = (char *)defaultString;
	return qfalse;
}

qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atof( s );
	return present;
}

qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atoi( s );
	return present;
}

qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	sscanf( s, "%f %f %f", &out[0], &out[1], &out[2] );
	return present;
}



//
// fields are needed for spawning from the entity string
//
typedef enum {
	F_INT,
	F_FLOAT,
	F_STRING,
	F_VECTOR,
	F_ANGLEHACK
} fieldtype_t;

typedef struct
{
	char	*name;
	size_t	ofs;
	fieldtype_t	type;
} field_t;

field_t fields[] = {
	{"classname", FOFS(classname), F_STRING},
	{"origin", FOFS(s.origin), F_VECTOR},
	{"model", FOFS(model), F_STRING},
	{"model2", FOFS(model2), F_STRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"target", FOFS(target), F_STRING},
	{"targetname", FOFS(targetname), F_STRING},
	{"message", FOFS(message), F_STRING},
	{"team", FOFS(team), F_STRING},
	{"wait", FOFS(wait), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(health), F_INT},
	{"dmg", FOFS(damage), F_INT},
	{"angles", FOFS(s.angles), F_VECTOR},
	{"angle", FOFS(s.angles), F_ANGLEHACK},
	{"targetShaderName", FOFS(targetShaderName), F_STRING},
	{"targetShaderNewName", FOFS(targetShaderNewName), F_STRING},

	{NULL}
};


typedef struct {
	char	*name;
	void	(*spawn)(gentity_t *ent);
} spawn_t;

void SP_info_player_start (gentity_t *ent);
void SP_info_player_deathmatch (gentity_t *ent);
void SP_info_player_intermission (gentity_t *ent);

void SP_func_plat (gentity_t *ent);
void SP_func_static (gentity_t *ent);
void SP_func_rotating (gentity_t *ent);
void SP_func_bobbing (gentity_t *ent);
void SP_func_pendulum( gentity_t *ent );
void SP_func_button (gentity_t *ent);
void SP_func_door (gentity_t *ent);
void SP_func_train (gentity_t *ent);
void SP_func_timer (gentity_t *self);

void SP_trigger_always (gentity_t *ent);
void SP_trigger_multiple (gentity_t *ent);
void SP_trigger_push (gentity_t *ent);
void SP_trigger_push_velocity (gentity_t *ent);
void SP_trigger_teleport (gentity_t *ent);
void SP_trigger_hurt (gentity_t *ent);

void SP_target_remove_powerups( gentity_t *ent );
void SP_target_give (gentity_t *ent);
void SP_target_delay (gentity_t *ent);
void SP_target_multimanager (gentity_t *ent);
void SP_target_speed (gentity_t *ent);
void SP_target_init (gentity_t *ent);
void SP_target_fragsFilter (gentity_t *ent);
void SP_target_smallprint (gentity_t *ent);
void SP_target_speaker (gentity_t *ent);
void SP_target_print (gentity_t *ent);
void SP_target_laser (gentity_t *self);
void SP_target_score( gentity_t *ent );
void SP_target_teleporter( gentity_t *ent );
void SP_target_relay (gentity_t *ent);
void SP_target_kill (gentity_t *ent);
void SP_target_position (gentity_t *ent);
void SP_target_location (gentity_t *ent);
void SP_target_push (gentity_t *ent);

void SP_light (gentity_t *self);
void SP_info_null (gentity_t *self);
void SP_info_notnull (gentity_t *self);
void SP_info_camp (gentity_t *self);
void SP_path_corner (gentity_t *self);

void SP_misc_teleporter_dest (gentity_t *self);
void SP_misc_model(gentity_t *ent);
void SP_misc_portal_camera(gentity_t *ent);
void SP_misc_portal_surface(gentity_t *ent);

void SP_shooter_rocket( gentity_t *ent );
void SP_shooter_plasma( gentity_t *ent );
void SP_shooter_grenade( gentity_t *ent );
void SP_shooter_bfg( gentity_t *ent );
void SP_shooter_rocket_targetplayer( gentity_t *ent );
void SP_shooter_plasma_targetplayer( gentity_t *ent );
void SP_shooter_grenade_targetplayer( gentity_t *ent );

void SP_team_CTF_redplayer( gentity_t *ent );
void SP_team_CTF_blueplayer( gentity_t *ent );

void SP_team_CTF_redspawn( gentity_t *ent );
void SP_team_CTF_bluespawn( gentity_t *ent );

#ifdef MISSIONPACK
void SP_team_blueobelisk( gentity_t *ent );
void SP_team_redobelisk( gentity_t *ent );
void SP_team_neutralobelisk( gentity_t *ent );
#endif
void SP_item_botroam( gentity_t *ent ) { }

spawn_t	spawns[] = {
	// info entities don't do anything at all, but provide positional
	// information for things controlled by other processes
	{"info_player_start", SP_info_player_start},
	{"info_player_deathmatch", SP_info_player_deathmatch},
	{"info_player_intermission", SP_info_player_intermission},
	{"info_null", SP_info_null},
	{"info_notnull", SP_info_notnull},		// use target_position instead
	{"info_camp", SP_info_camp},

	{"func_plat", SP_func_plat},
	{"func_button", SP_func_button},
	{"func_door", SP_func_door},
	{"func_static", SP_func_static},
	{"func_rotating", SP_func_rotating},
	{"func_bobbing", SP_func_bobbing},
	{"func_pendulum", SP_func_pendulum},
	{"func_train", SP_func_train},
	{"func_group", SP_info_null},
	{"func_timer", SP_func_timer},			// rename trigger_timer?

	// Triggers are brush objects that cause an effect when contacted
	// by a living player, usually involving firing targets.
	// While almost everything could be done with
	// a single trigger class and different targets, triggered effects
	// could not be client side predicted (push and teleport).
	{"trigger_always", SP_trigger_always},
	{"trigger_multiple", SP_trigger_multiple},
	{"trigger_push", SP_trigger_push},
	{"trigger_push_velocity", SP_trigger_push_velocity},
	{"trigger_teleport", SP_trigger_teleport},
	{"trigger_hurt", SP_trigger_hurt},

	// targets perform no action by themselves, but must be triggered
	// by another entity
	{"target_give", SP_target_give},
	{"target_remove_powerups", SP_target_remove_powerups},
	{"target_delay", SP_target_delay},
	{"target_multimanager", SP_target_multimanager},
	{"target_speed", SP_target_speed},
	{"target_init", SP_target_init},
	{"target_fragsFilter", SP_target_fragsFilter},
	{"target_smallprint", SP_target_smallprint},
	{"target_speaker", SP_target_speaker},
	{"target_print", SP_target_print},
	{"target_laser", SP_target_laser},
	{"target_score", SP_target_score},
	{"target_teleporter", SP_target_teleporter},
	{"target_relay", SP_target_relay},
	{"target_kill", SP_target_kill},
	{"target_position", SP_target_position},
	{"target_location", SP_target_location},
	{"target_push", SP_target_push},

	// DeFRaG race timer entities
	{"target_startTimer",  SP_target_startTimer},
	{"target_stopTimer",   SP_target_stopTimer},
	{"target_checkpoint",  SP_target_checkpoint},

	{"light", SP_light},
	{"path_corner", SP_path_corner},

	{"misc_teleporter_dest", SP_misc_teleporter_dest},
	{"misc_model", SP_misc_model},
	{"misc_portal_surface", SP_misc_portal_surface},
	{"misc_portal_camera", SP_misc_portal_camera},

	{"shooter_rocket", SP_shooter_rocket},
	{"shooter_grenade", SP_shooter_grenade},
	{"shooter_plasma", SP_shooter_plasma},
	{"shooter_bfg", SP_shooter_bfg},
	{"shooter_rocket_targetplayer", SP_shooter_rocket_targetplayer},
	{"shooter_plasma_targetplayer", SP_shooter_plasma_targetplayer},
	{"shooter_grenade_targetplayer", SP_shooter_grenade_targetplayer},

	{"team_CTF_redplayer", SP_team_CTF_redplayer},
	{"team_CTF_blueplayer", SP_team_CTF_blueplayer},

	{"team_CTF_redspawn", SP_team_CTF_redspawn},
	{"team_CTF_bluespawn", SP_team_CTF_bluespawn},

#ifdef MISSIONPACK
	{"team_redobelisk", SP_team_redobelisk},
	{"team_blueobelisk", SP_team_blueobelisk},
	{"team_neutralobelisk", SP_team_neutralobelisk},
#endif
	{"item_botroam", SP_item_botroam},

	{NULL, NULL}
};

/*
===============
G_CallSpawn

Finds the spawn function for the entity and calls it,
returning qfalse if not found
===============
*/
qboolean G_CallSpawn( gentity_t *ent ) {
	spawn_t	*s;
	gitem_t	*item;

	if ( !ent->classname ) {
		G_Printf ("G_CallSpawn: NULL classname\n");
		return qfalse;
	}

	// check item spawn functions
	for ( item=bg_itemlist+1 ; item->classname ; item++ ) {
		if ( !strcmp(item->classname, ent->classname) ) {
			G_SpawnItem( ent, item );
			return qtrue;
		}
	}

	// check normal spawn functions
	for ( s=spawns ; s->name ; s++ ) {
		if ( !strcmp(s->name, ent->classname) ) {
			// found it
			s->spawn(ent);
			return qtrue;
		}
	}
	G_Printf ("%s doesn't have a spawn function\n", ent->classname);
	return qfalse;
}

/*
=============
G_NewString

Builds a copy of the string, translating \n to real linefeeds
so message texts can be multi-line
=============
*/
char *G_NewString( const char *string ) {
	char	*newb, *new_p;
	int		i,l;
	
	l = strlen(string) + 1;

	newb = G_Alloc( l );

	new_p = newb;

	// turn \n into a real linefeed
	for ( i=0 ; i< l ; i++ ) {
		if (string[i] == '\\' && i < l-1) {
			i++;
			if (string[i] == 'n') {
				*new_p++ = '\n';
			} else {
				*new_p++ = '\\';
			}
		} else {
			*new_p++ = string[i];
		}
	}
	
	return newb;
}




/*
===============
G_ParseField

Takes a key/value pair and sets the binary values
in a gentity
===============
*/
void G_ParseField( const char *key, const char *value, gentity_t *ent ) {
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;

	for ( f=fields ; f->name ; f++ ) {
		if ( !Q_stricmp(f->name, key) ) {
			// found it
			b = (byte *)ent;

			switch( f->type ) {
			case F_STRING:
				*(char **)(b+f->ofs) = G_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *)(b+f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			}
			return;
		}
	}
}

#define ADJUST_AREAPORTAL() \
	if(ent->s.eType == ET_MOVER) \
	{ \
		trap_LinkEntity(ent); \
		trap_AdjustAreaPortalState(ent, qtrue); \
	}

/*
===================
G_ParseModeKeys

DeFRaG func_0x1c9c: translate the mode-exclusion spawn keys into ent->flags bits.
Returns qfalse for "notdefrag" (caller stops without freeing).
===================
*/
qboolean G_ParseModeKeys( gentity_t *ent ) {
	int		i;
	char	*value;

	G_SpawnInt( "notdefrag", "0", &i );
	if ( i ) {
		return qfalse;
	}
	G_SpawnInt( "notcpm", "0", &i );	if ( i ) ent->flags |= FL_NOT_CPM;
	G_SpawnInt( "notvq3", "0", &i );	if ( i ) ent->flags |= FL_NOT_VQ3;
	G_SpawnInt( "notmp", "0", &i );		if ( i ) ent->flags |= FL_NOT_MP;
	G_SpawnInt( "notsp", "0", &i );		if ( i ) ent->flags |= FL_NOT_SP;
	G_SpawnInt( "notdf", "0", &i );		if ( i ) ent->flags |= FL_NOT_DF;
	G_SpawnInt( "nottm", "0", &i );		if ( i ) ent->flags |= FL_NOT_TM;
	G_SpawnInt( "notfc", "0", &i );		if ( i ) ent->flags |= FL_NOT_FC;
	G_SpawnInt( "notteam", "0", &i );	if ( i ) ent->flags |= FL_NOT_FC;
	G_SpawnInt( "notfree", "0", &i );	if ( i ) ent->flags |= FL_NOT_DF | FL_NOT_TM;
	if ( G_SpawnString( "gametype", NULL, &value ) ) {
		// strstr semantics (func_0x5ce7): a gametype list that doesn't mention a family excludes the entity from it
		if ( !strstr( value, "ffa" ) ) ent->flags |= FL_NOT_DF | FL_NOT_TM;
		if ( !strstr( value, "ctf" ) ) ent->flags |= FL_NOT_FC;
	}
	return qtrue;
}

/*
===================
G_ApplyModeExclusion

DeFRaG func_0x20e0: level-init sweep that frees entities excluded by the current physics (df_promode),
server type (dedicated) and DeFRaG mode (df_gametype 1 = run, 2 = tricks, 3 = fastcaps).
Mode auto-detection of the original is not implemented. FL_MOVER (0x800000) is set by func_plat/bobbing/train.
===================
*/
qboolean g_disableScore;	// worldspawn "disableScore"

extern vmCvar_t df_enforceSvFps;
extern vmCvar_t df_forceGametype;

// DeFRaG 0x2d02 (called at the end of the mode resolve 0x21fc) and the forced defrag_bspcrc of G_InitGame 0x1b57: the map's BSP checksum is
// published to the clients (defrag_bspcrc) and stored in system/checksums/<map>.crc, which the cgame compares with the server's value
// (cgame 0x61c6 / 0x6222: "Bsp crc" violations 5 / 6).
static void G_PublishMapChecksum( void ) {
	char			map[32], crc[32];
	fileHandle_t	f;

	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	Q_strlwr( map );
	trap_Cvar_VariableStringBuffer( "sv_mapChecksum", crc, sizeof( crc ) );
	trap_Cvar_Set( "defrag_bspcrc", va( "%i", trap_Cvar_VariableIntegerValue( "sv_mapChecksum" ) ) );
	trap_FS_FOpenFile( va( "system/checksums/%s.crc", map ), &f, FS_WRITE );
	if ( f ) {
		trap_FS_Write( crc, strlen( crc ), f );
		trap_FS_FCloseFile( f );
	}
}

void G_ResolveDfMode( void ) {
	int mode = df_gametype.integer;
	int dfFlags;

	g_dfVariant = g_dedicated.integer ? qtrue : qfalse;
	if ( mode <= 0 || mode >= 8 || mode == 4 ) {
		// invalid / 0 = auto-detect from the map: start timer -> run, team item -> fastcaps, else tricks
		if ( g_startTimerReady ) {
			mode = 1;
		} else if ( g_hasTeamItem ) {
			mode = 3;
		} else {
			mode = 2;
		}
	} else if ( mode >= 5 ) {
		mode -= 4;
		g_dfVariant = qtrue;
	}
	g_dfMode = mode;
	// 0x22ac-0x22d4: fastcaps (mode 3) runs as CTF (4), every other mode as free for all (0), whatever g_gametype the server had; the
	// cached value is updated at once (opt-out: df_forceGametype 0)
	if ( df_forceGametype.integer ) {
		trap_Cvar_Set( "g_gametype", mode == 3 ? "4" : "0" );
		trap_Cvar_Update( &g_gametype );
	}
	G_ResolveNdm();
	G_NdmTimerInit();
	trap_Cvar_Set( "defrag_gametype", va( "%i", g_dfVariant ? mode + 4 : mode ) );

	// defrag_flags (0x2318-0x2353): 1 = worldspawn "disableScore", 2 = a checkpoint timer exists (run mode always has one),
	// 0x10 = df_ndm_nopad in effect, 0x20 = df_ndm_noteleport in effect
	dfFlags = g_disableScore ? 1 : 0;
	if ( mode == 1 || g_checkpointReady ) {
		dfFlags |= 2;
	}
	if ( g_ndm.nopad ) {
		dfFlags |= 0x10;
	}
	if ( g_ndm.noteleport ) {
		dfFlags |= 0x20;
	}
	trap_Cvar_Set( "defrag_flags", va( "%i", dfFlags ) );
	// defrag_obs (0x2359-0x2376): 1 = overbounces on, i.e. df_ob_KillOBs is 0
	trap_Cvar_Set( "defrag_obs", df_ob_killobs.integer ? "0" : "1" );

	// 0x2381-0x23ed (non-dedicated path): a sv_fps outside 60..140 is set to 125 at map load; G_FrameRules then keeps it inside the
	// bounds every frame. Opt-out with df_enforceSvFps 0 (the stock default of 20 would otherwise be raised to 125).
	if ( df_enforceSvFps.integer ) {
		if ( !g_dfVariant ) {
			int svFps = trap_Cvar_VariableIntegerValue( "sv_fps" );

			if ( !trap_Cvar_VariableIntegerValue( "sv_cheats" ) && svFps != 0 && ( svFps < 60 || svFps > 140 ) ) {
				trap_Cvar_Set( "sv_fps", "125" );
			}
			// 0x23d5: no time limit on every map load (a race must not be cut off by the map timer)
			trap_Cvar_Set( "timelimit", "0" );
		} else if ( !trap_Cvar_VariableIntegerValue( "defrag_svfpsInitialized" ) ) {
			// 0x238a-0x23ac (dedicated / variant servers): one-time defaults, remembered in defrag_svfpsInitialized
			trap_Cvar_Set( "defrag_svfpsInitialized", "1" );
			trap_Cvar_Set( "sv_fps", "125" );
			trap_Cvar_Set( "g_synchronousClients", "0" );
		}
	}
	G_PublishMapChecksum();
}

ndmSettings_t g_ndm;

/*
DeFRaG G_InitRaceMode (func_0x1e60 body for the preset switch at 0x1f76, jump table data 0x930):
mode 1 (run): damage 1, weapons 6 (gauntlet+machinegun), flags 0x7e, no pad/teleport restriction, movers kept.
other modes: defrag_mode = df_mode_tricksmode (mode 2) / df_mode_fastcaps (mode 3); 1..7 select a preset, otherwise the df_ndm_* cvars apply:
  1: weapons 6,     nopad 2, noteleport 2, nomover      2: weapons 0x1fe, 0,0
  3: weapons 6,     0,0                                  4: weapons 0x1fe, 2,2, nomover
  5: weapons 0x404, 0,0 (hook 1)                         6: weapons 0x404, 0,0 (hook 0)
  7: damage 1, weapons 6, flags 0x7e, 0,0
(nomover is set by presets 1 and 4; the preset values for the "hook" setting are not applied here.)
*/
int g_dfSubMode = -1;	// 0x1317c8: raw df_mode_tricksmode / df_mode_fastcaps value, -1 in run mode (logged by ClientTimerStop)

void G_ResolveNdm( void ) {
	int v;

	memset( &g_ndm, 0, sizeof( g_ndm ) );
	g_dfSubMode = -1;
	if ( g_dfMode == 1 ) {
		g_ndm.damage = 1;
		g_ndm.weaponsSet = 6;
		g_ndm.flags = 0x7e;
		g_ndm.itemMask = 0x7e;
		trap_Cvar_Set( "defrag_mode", "-1" );
		return;
	}
	if ( g_dfMode == 3 ) {
		g_ndm.flags |= 0x100;		// fastcaps: the team items (flags) spawn and work
		g_ndm.itemMask |= 0x100;
	}

	v = trap_Cvar_VariableIntegerValue( g_dfMode == 3 ? "df_mode_fastcaps" : "df_mode_tricksmode" );
	g_dfSubMode = v;
	if ( v < 1 || v > 7 ) {
		v = 0;
	}
	trap_Cvar_Set( "defrag_mode", va( "%i", v ) );

	switch ( v ) {
	case 1:	g_ndm.weaponsSet = 6;     g_ndm.nopad = 2; g_ndm.noteleport = 2; g_ndm.nomover = qtrue;  break;
	case 2:	g_ndm.weaponsSet = 0x1fe;                                                              break;
	case 3:	g_ndm.weaponsSet = 6;                                                                  break;
	case 4:	g_ndm.weaponsSet = 0x1fe; g_ndm.nopad = 2; g_ndm.noteleport = 2; g_ndm.nomover = qtrue;  break;
	case 5:
	case 6:	g_ndm.weaponsSet = 0x404;                                                              break;
	case 7:	g_ndm.damage = 1; g_ndm.weaponsSet = 6; g_ndm.flags |= 0x7e; g_ndm.itemMask |= 0x7e;  break;
	default:
		// custom: the df_ndm_* cvars
		g_ndm.damage = df_ndm_damage.integer;
		g_ndm.weaponsSet = df_ndm_weaponsSet.integer;
		g_ndm.nopad = df_ndm_nopad.integer;
		g_ndm.noteleport = df_ndm_noteleport.integer;
		g_ndm.nomover = df_ndm_nomover.integer ? qtrue : qfalse;
		if ( g_ndm.damage ) {
			g_ndm.flags |= 0x18;
		}
		if ( df_ndm_powerups.integer ) {
			g_ndm.flags |= 0x60;
		}
		if ( df_ndm_weaponsPickup.integer ) {
			g_ndm.flags |= 0x6;
		}
		// df_ndm_itemsSpawn: 0 = no items, 2 = all item types, otherwise the types whose pickup works (flags)
		if ( df_ndm_itemsSpawn.integer == 0 ) {
			g_ndm.itemMask = 0;
		} else if ( df_ndm_itemsSpawn.integer == 2 ) {
			g_ndm.itemMask |= 0x7e;
		} else {
			g_ndm.itemMask |= g_ndm.flags;
		}
		break;
	}
}

void G_ApplyModeExclusion( void ) {
	gentity_t	*ent;
	int			i, mode;

	G_ResolveDfMode();
	mode = g_dfMode;

	for ( i = MAX_CLIENTS, ent = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, ent++ ) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( df_promode.integer == 1 ) {
			if ( ent->flags & FL_NOT_CPM ) { G_FreeEntity( ent ); continue; }
		} else {
			if ( ent->flags & FL_NOT_VQ3 ) { G_FreeEntity( ent ); continue; }
		}
		if ( g_dfVariant ) {
			if ( ent->flags & FL_NOT_MP ) { G_FreeEntity( ent ); continue; }
		} else {
			if ( ent->flags & FL_NOT_SP ) { G_FreeEntity( ent ); continue; }
		}
		if ( mode == 1 ) {
			if ( ent->flags & FL_NOT_DF ) { G_FreeEntity( ent ); continue; }
		} else if ( mode == 2 ) {
			if ( ent->flags & FL_NOT_TM ) { G_FreeEntity( ent ); continue; }
		} else if ( mode == 3 ) {
			if ( ent->flags & FL_NOT_FC ) { G_FreeEntity( ent ); continue; }
		}
		// 0x2188-0x219e: reached in every mode by the entities that survived the mode keys (not only in fastcaps)
		if ( g_ndm.nomover && ( ent->flags & FL_MOVER ) ) {
			G_FreeEntity( ent );
		}
	}
}

/*
===================
G_SpawnGEntityFromSpawnVars

Spawn an entity and fill in all of the level fields from
level.spawnVars[], then call the class specific spawn function
===================
*/
void G_SpawnGEntityFromSpawnVars( void ) {
	int			i;
	gentity_t	*ent;


	// get the next free entity
	ent = G_Spawn();

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], ent );
	}

	// check for "notsingle" flag
	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		G_SpawnInt( "notsingle", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	}
	// DeFRaG (func_0x24b2a/func_0x1c9c): replaces vanilla's notteam/notfree/gametype handling with mode-flag keys
	// that G_ApplyModeExclusion() evaluates after all entities exist. "notdefrag" leaves the entity allocated but
	// unspawned (the bytecode returns without freeing it).
	if ( !G_ParseModeKeys( ent ) ) {
		return;
	}

#ifdef MISSIONPACK
	G_SpawnInt( "notta", "0", &i );
	if ( i ) {
		ADJUST_AREAPORTAL();
		G_FreeEntity( ent );
		return;
	}
#else
	G_SpawnInt( "notq3a", "0", &i );
	if ( i ) {
		ADJUST_AREAPORTAL();
		G_FreeEntity( ent );
		return;
	}
#endif


	// move editor origin to pos
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// if we didn't get a classname, don't bother spawning anything
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
	}
}



/*
====================
G_AddSpawnVarToken
====================
*/
char *G_AddSpawnVarToken( const char *string ) {
	int		l;
	char	*dest;

	l = strlen( string );
	if ( level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS ) {
		G_Error( "G_AddSpawnVarToken: MAX_SPAWN_VARS_CHARS" );
	}

	dest = level.spawnVarChars + level.numSpawnVarChars;
	memcpy( dest, string, l+1 );

	level.numSpawnVarChars += l + 1;

	return dest;
}

/*
====================
G_ParseSpawnVars

Parses a brace bounded set of key / value pairs out of the
level's entity strings into level.spawnVars[]

This does not actually spawn an entity.
====================
*/
qboolean G_ParseSpawnVars( void ) {
	char		keyname[MAX_TOKEN_CHARS];
	char		com_token[MAX_TOKEN_CHARS];

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;

	// parse the opening brace
	if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
		// end of spawn string
		return qfalse;
	}
	if ( com_token[0] != '{' ) {
		G_Error( "G_ParseSpawnVars: found %s when expecting {",com_token );
	}

	// go through all the key / value pairs
	while ( 1 ) {	
		// parse key
		if ( !trap_GetEntityToken( keyname, sizeof( keyname ) ) ) {
			G_Error( "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( keyname[0] == '}' ) {
			break;
		}
		
		// parse value	
		if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
			G_Error( "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( com_token[0] == '}' ) {
			G_Error( "G_ParseSpawnVars: closing brace without data" );
		}
		if ( level.numSpawnVars == MAX_SPAWN_VARS ) {
			G_Error( "G_ParseSpawnVars: MAX_SPAWN_VARS" );
		}
		level.spawnVars[ level.numSpawnVars ][0] = G_AddSpawnVarToken( keyname );
		level.spawnVars[ level.numSpawnVars ][1] = G_AddSpawnVarToken( com_token );
		level.numSpawnVars++;
	}

	return qtrue;
}



/*QUAKED worldspawn (0 0 0) ?

Every map should have exactly one worldspawn.
"music"		music wav file
"gravity"	800 is default gravity
"message"	Text to print during connection process
*/
void SP_worldspawn( void ) {
	char	*s;

	G_SpawnString( "classname", "", &s );
	if ( Q_stricmp( s, "worldspawn" ) ) {
		G_Error( "SP_worldspawn: The first entity isn't 'worldspawn'" );
	}

	// make some data visible to connecting client
	trap_SetConfigstring( CS_GAME_VERSION, GAME_VERSION );

	trap_SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ) );

	G_SpawnString( "music", "", &s );
	trap_SetConfigstring( CS_MUSIC, s );

	G_SpawnString( "message", "", &s );
	trap_SetConfigstring( CS_MESSAGE, s );				// map specific message

	trap_SetConfigstring( CS_MOTD, g_motd.string );		// message of the day

	// DeFRaG (0x24c8c) does not read the map's "gravity" key: g_gravity stays what the server has (only honoured with cheats)

	G_SpawnString( "enableDust", "0", &s );
	trap_Cvar_Set( "g_enableDust", s );

	G_SpawnString( "enableBreath", "0", &s );
	trap_Cvar_Set( "g_enableBreath", s );

	// DeFRaG: "disableScore" hides the score / fraglimit display (defrag_flags bit 1, see G_ResolveDfMode)
	G_SpawnString( "disableScore", "0", &s );
	g_disableScore = atoi( s ) ? qtrue : qfalse;

	g_entities[ENTITYNUM_WORLD].s.number = ENTITYNUM_WORLD;
	g_entities[ENTITYNUM_WORLD].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_WORLD].classname = "worldspawn";

	g_entities[ENTITYNUM_NONE].s.number = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].classname = "nothing";

	// see if we want a warmup time
	trap_SetConfigstring( CS_WARMUP, "" );
	if ( g_restarted.integer ) {
		trap_Cvar_Set( "g_restarted", "0" );
		level.warmupTime = 0;
	} else if ( g_doWarmup.integer ) { // Turn it on
		level.warmupTime = -1;
		trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
		G_LogPrintf( "Warmup:\n" );
	}

}


/*
==============
G_SpawnEntitiesFromString

Parses textual entity definitions out of an entstring and spawns gentities.
==============
*/
void G_SpawnEntitiesFromString( void ) {
	// allow calls to G_Spawn*()
	level.spawning = qtrue;
	level.numSpawnVars = 0;

	// the worldspawn is not an actual entity, but it still
	// has a "spawn" function to perform any global setup
	// needed by a level (setting configstrings or cvars, etc)
	if ( !G_ParseSpawnVars() ) {
		G_Error( "SpawnEntities: no entities" );
	}
	SP_worldspawn();

	// parse ents
	while( G_ParseSpawnVars() ) {
		G_SpawnGEntityFromSpawnVars();
	}	

	level.spawning = qfalse;			// any future calls to G_Spawn*() will be errors
}

