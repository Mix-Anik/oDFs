/*
 * gamehost.h - headless host that runs the real qagame.qvm
 *
 * Plays the part of the engine's server for the game module: cvars, configstrings, userinfo,
 * entity linking and traces (the engine's own sv_world.c), the entity string, the call sequence of
 * SV_SpawnServer / SV_DirectConnect / SV_ClientEnterWorld / SV_ClientThink / SV_Frame.
 *
 * Singleton: the engine's world code keeps its state in a global, so only one host can be used at a time.
 */
#ifndef GAMEHOST_H
#define GAMEHOST_H

#include "server.h"		/* engine_shim: sharedEntity_t, svEntity_t, g_public.h */

typedef struct gamehost_s gamehost_t;

gamehost_t	*GH_Create( const char *qvmPath, char *err, int errSize );
void		GH_Destroy( gamehost_t *gh );

/* cvars the game finds when it registers its own (server settings of the recorded game); call before GH_StartMap */
void		GH_SetCvar( gamehost_t *gh, const char *name, const char *value );

/* offsets of internal game fields (health, timers) from an offsets.qvm built with the game's headers */
int			GH_LoadOffsets( gamehost_t *gh, const char *path, char *err, int errSize );
void		GH_SetHealth( gamehost_t *gh, int clientNum, int health );
void		GH_SetEventTime( gamehost_t *gh, int clientNum, int time );
void		GH_SetTimeResidual( gamehost_t *gh, int clientNum, int msec );
int			GH_GetTimeResidual( gamehost_t *gh, int clientNum );

/* SV_SpawnServer: loads the map, runs GAME_INIT at `startTime` and the settle frames (3 x 100 ms) */
int			GH_StartMap( gamehost_t *gh, const void *bsp, int length, int startTime, char *err, int errSize );

/* SV_DirectConnect + SV_ClientEnterWorld for one human client; returns its client number or -1 */
int			GH_ConnectClient( gamehost_t *gh, const char *userinfo, char *err, int errSize );

/* the usercmd the next GH_ClientThink() will see (SV_ClientThink) */
void		GH_SetUsercmd( gamehost_t *gh, int clientNum, const usercmd_t *cmd );
int			GH_ClientThink( gamehost_t *gh, int clientNum );

/* SV_Frame: advances the server clock to `time` and runs GAME_RUN_FRAME */
int			GH_RunFrame( gamehost_t *gh, int time );

int			GH_Time( const gamehost_t *gh );
playerState_t	*GH_PlayerState( gamehost_t *gh, int clientNum );
sharedEntity_t	*GH_Entity( gamehost_t *gh, int num );
int			GH_NumEntities( const gamehost_t *gh );
const char	*GH_Configstring( const gamehost_t *gh, int num );

/* last fatal error, and a note of syscalls the host does not implement */
const char	*GH_Error( const gamehost_t *gh );
const char	*GH_Unhandled( const gamehost_t *gh );

/* server commands the game sent (for tests that look at them); the list is cleared by GH_ClearCommands() */
int			GH_NumCommands( const gamehost_t *gh );
const char	*GH_Command( const gamehost_t *gh, int index, int *clientNum );
void		GH_ClearCommands( gamehost_t *gh );

#endif
