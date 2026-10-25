/*
 * server.h - a small replacement for the engine's server.h
 *
 * The test host compiles the engine's own sv_world.c (entity linking, area queries, entity-aware
 * traces) so entities are linked, found and clipped in exactly the engine's order. sv_world.c only
 * needs the few server structures below.
 */
#ifndef ENGINE_SHIM_SERVER_H
#define ENGINE_SHIM_SERVER_H

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"

#include "../../../code/game/g_public.h"

#define	MAX_ENT_CLUSTERS	16
#define SOLID_BMODEL		0xffffff

typedef enum {
	SS_DEAD,
	SS_LOADING,
	SS_GAME
} serverState_t;

struct worldSector_s;

typedef struct svEntity_s {
	struct worldSector_s *worldSector;
	struct svEntity_s *nextEntityInWorldSector;

	entityState_t	baseline;
	int			numClusters;
	int			clusternums[MAX_ENT_CLUSTERS];
	int			lastCluster;
	int			areanum, areanum2;
	int			snapshotCounter;
} svEntity_t;

typedef struct {
	serverState_t	state;
	svEntity_t		svEntities[MAX_GENTITIES];
	sharedEntity_t	*gentities;
	int				gentitySize;
	int				num_entities;
} server_t;

extern server_t sv;

sharedEntity_t	*SV_GentityNum( int num );
svEntity_t		*SV_SvEntityForGentity( sharedEntity_t *gEnt );
sharedEntity_t	*SV_GEntityForSvEntity( svEntity_t *svEnt );

/* sv_world.c */
void	SV_ClearWorld( void );
void	SV_UnlinkEntity( sharedEntity_t *ent );
void	SV_LinkEntity( sharedEntity_t *ent );
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent );
int		SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
void	SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
			int entityNum, int contentmask, qboolean capsule );
void	SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
			int passEntityNum, int contentmask, qboolean capsule );
int		SV_PointContents( const vec3_t p, int passEntityNum );

#endif
