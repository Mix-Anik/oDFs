/*
 * pmhost.h - headless test host: runs Pmove() from pmove.qvm against a BSP
 *
 * The host plays the part of the engine for the movement code: it loads the map with the
 * engine's collision module, executes pmove.qvm with the QVM interpreter and answers the
 * syscalls (trace against the world, point contents, SnapVector, math).
 *
 * The world is static unless PMHost_SetEntities() hands in solid brush-model entities (doors, plats,
 * func_static ...); other players, items and missiles do not exist.
 */
#ifndef PMHOST_H
#define PMHOST_H

#include "q_shared.h"

typedef struct pmhost_s pmhost_t;

/* the per-client settings ClientThink_real hands to Pmove() */
typedef struct {
	int		tracemask;
	int		pmove_fixed;
	int		pmove_msec;
	int		obs_disabled;
	int		promode;
	int		killWallbug;
} pmParams_t;

/* extra results of one step */
typedef struct {
	int		numtouch;
	int		waterlevel;
	int		watertype;
} pmStepInfo_t;

/* a solid brush-model entity (same layout as the demo reader's demoBmodel_t) */
typedef struct {
	int				number;
	int				modelindex;
	trajectory_t	pos;
	trajectory_t	apos;
} pmEntity_t;

pmhost_t	*PMHost_Create( const char *qvmPath, char *err, int errSize );
void		PMHost_Destroy( pmhost_t *h );

/* loads a BSP image (a whole .bsp file); *checksum is the engine's Com_BlockChecksum of it,
   the value DeFRaG servers publish as defrag_bspcrc */
int			PMHost_LoadMap( pmhost_t *h, const void *bsp, int length, int *checksum, char *err, int errSize );

/* the solid brush models that exist for the following steps; their positions are the trajectories
   evaluated at `time` (the server's level time of the step) */
void		PMHost_SetEntities( pmhost_t *h, const pmEntity_t *ents, int count, int time );

/* runs one Pmove(); returns 0, or -1 with the reason in PMHost_Error() */
int			PMHost_Step( pmhost_t *h, playerState_t *ps, const usercmd_t *cmd, const pmParams_t *params,
					pmStepInfo_t *info );

const char	*PMHost_Error( const pmhost_t *h );

#endif
