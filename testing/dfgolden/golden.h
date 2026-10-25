/*
 * golden.h - the golden file: everything needed to replay one demo through the game without the game, the
 * engine or the demo
 *
 *   settings   the server / system info strings of the recording, the player's userinfo, the few settings
 *              the server does not publish
 *   map        the BSP of the demo's map, stripped of everything the game and the collision code do not read
 *              (lightmaps, light grid, visibility)
 *   steps      for every snapshot: the usercmd (view angles, keys, weapon) the player sent in the step into it,
 *              and the player state the recording shows
 *
 * The file is a small header followed by one deflate stream; the states are stored as differences to the
 * previous state.
 */
#ifndef DFGOLDEN_GOLDEN_H
#define DFGOLDEN_GOLDEN_H

#include "q_shared.h"

#define GOLD_VERSION		1
#define GOLD_NO_TIME		( -2147483647 - 1 )		/* a time field that is not available */

enum {
	GS_REGULAR		= 1,	/* a plain step: the game's result is compared with the recorded state */
	GS_RAN			= 2,	/* the game runs the usercmd of this step (a respawn or teleport step carries the keys only) */
	GS_KNOWN_MISS	= 4		/* when the file was made, the port did not reproduce this step exactly */
};

#define GOLD_KNOWN_FINISH	1		/* knownFlags: the game's finish time differs from the recording's */
#define GOLD_KNOWN_X87		2		/* knownFlags: the recording engine ran its collision arithmetic in x87 registers */
#define GOLD_KNOWN_F32		4		/* knownFlags: the recording engine computes its collision in floats (ioq3 form) */

typedef struct {
	usercmd_t		cmd;
	int				serverTime;
	int				timerMs;		/* time relative to the race timer start (negative before), GOLD_NO_TIME if none */
	int				raceMs;			/* race time the recording publishes (decoded), -1 if it is not running or cannot be read */
	unsigned char	flags;
} goldStep_t;

typedef struct {
	char			name[192];			/* the demo this was made from */
	char			map[64];
	char			*serverinfo;		/* BIG_INFO_STRING */
	char			*systeminfo;
	char			userinfo[512];
	int				hookType;			/* df_hook_Type and df_lg_Knockback: not published, read from the first state */
	int				lgKnockback;
	int				dedicated;
	int				startOffset;		/* the game starts this long before the first snapshot */
	int				decayStep;			/* step of the first health decay without damage, -1 unknown */
	int				ulps;				/* float tolerance of the comparison */
	int				raceOffset;			/* clock shift between the recording and the game clock the race time was scrambled with */
	int				bspCrc;				/* checksum of the original map (systeminfo defrag_bspcrc) */
	int				nameMs;				/* the finish time in the demo's file name, -1 if there is none */
	int				knownFlags;			/* bit 0: when the file was made the game's finish time differed from the recording's;
										   bits 1, 2 (GOLD_KNOWN_X87 / _F32): which collision code the recording engine ran */
	unsigned char	*bsp;
	int				bspLen;
	int				numSteps;
	goldStep_t		*steps;
	playerState_t	*states;
} golden_t;

void	Gold_Free( golden_t *g );
int		Gold_Write( const golden_t *g, const char *path, char *err, int errSize );
int		Gold_Read( golden_t *g, const char *path, char *err, int errSize );

/* the map without the lumps nobody here reads; returns a malloc'd copy, NULL if it is not a Quake 3 BSP */
unsigned char	*Gold_StripBsp( const unsigned char *bsp, int len, int *outLen );

/* reads the race time out of the two stats words; same code as the cgame (CG_DF_DecodeCore). `key` is the sum of
   the map name letters and the DeFRaG version number the scrambling server used (19131 for this port) */
int		Gold_DecodeRaceTime( unsigned int e, unsigned int t, float originX, float velX, int health, int moveDir,
			unsigned int key, int *ms );
unsigned char	Gold_MapSalt( const char *mapName );

#endif
