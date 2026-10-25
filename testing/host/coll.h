/*
 * coll.h - the host's choice between the copies of the collision code (see engine_shim/coll_names.h)
 *
 * Include after server.h / cm_public.h. gHostColl is set per run, from the engine that recorded the demo:
 *   0  the engine as vendored (oDFe, quake3e: doubles in the brush and patch code, SSE)
 *   1  the same compiled for x87 registers (32 bit x86 builds of those engines)
 *   2  the ioq3 form of the code: floats instead of doubles (ioq3 based DeFRaG engines)
 */
#ifndef HOST_COLL_H
#define HOST_COLL_H

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "server.h"

extern int gHostColl;

/* the other copies have the same types as the plain functions */
extern __typeof__( SV_Trace ) SV_Trace_x87, SV_Trace_f32;
extern __typeof__( SV_LinkEntity ) SV_LinkEntity_x87, SV_LinkEntity_f32;
extern __typeof__( SV_UnlinkEntity ) SV_UnlinkEntity_x87, SV_UnlinkEntity_f32;
extern __typeof__( SV_PointContents ) SV_PointContents_x87, SV_PointContents_f32;
extern __typeof__( SV_ClipHandleForEntity ) SV_ClipHandleForEntity_x87, SV_ClipHandleForEntity_f32;
extern __typeof__( SV_ClearWorld ) SV_ClearWorld_x87, SV_ClearWorld_f32;
extern __typeof__( SV_AreaEntities ) SV_AreaEntities_x87, SV_AreaEntities_f32;
extern __typeof__( CM_TransformedBoxTrace ) CM_TransformedBoxTrace_x87, CM_TransformedBoxTrace_f32;
extern __typeof__( CM_LoadMap ) CM_LoadMap_x87, CM_LoadMap_f32;
extern __typeof__( CM_BoxTrace ) CM_BoxTrace_x87, CM_BoxTrace_f32;
extern __typeof__( CM_PointLeafnum ) CM_PointLeafnum_x87, CM_PointLeafnum_f32;
extern __typeof__( CM_LeafCluster ) CM_LeafCluster_x87, CM_LeafCluster_f32;
extern __typeof__( CM_LeafArea ) CM_LeafArea_x87, CM_LeafArea_f32;
extern __typeof__( CM_InlineModel ) CM_InlineModel_x87, CM_InlineModel_f32;
extern __typeof__( CM_AreasConnected ) CM_AreasConnected_x87, CM_AreasConnected_f32;
extern __typeof__( CM_PointContents ) CM_PointContents_x87, CM_PointContents_f32;
extern __typeof__( CM_NumInlineModels ) CM_NumInlineModels_x87, CM_NumInlineModels_f32;
extern __typeof__( CM_ModelBounds ) CM_ModelBounds_x87, CM_ModelBounds_f32;
extern __typeof__( CM_EntityString ) CM_EntityString_x87, CM_EntityString_f32;
extern __typeof__( CM_ClusterPVS ) CM_ClusterPVS_x87, CM_ClusterPVS_f32;
extern __typeof__( CM_AdjustAreaPortalState ) CM_AdjustAreaPortalState_x87, CM_AdjustAreaPortalState_f32;

/* a function-like macro does not expand its own name again: the last branch is the plain function */
#ifdef DFG_HAVE_X87
#define COLL_PICK( fn )		( gHostColl == 1 ? fn##_x87 : gHostColl == 2 ? fn##_f32 : fn )
#else
#define COLL_PICK( fn )		( gHostColl == 2 ? fn##_f32 : fn )
#endif

#define SV_Trace( ... )	COLL_PICK( SV_Trace )( __VA_ARGS__ )
#define SV_LinkEntity( ... )	COLL_PICK( SV_LinkEntity )( __VA_ARGS__ )
#define SV_UnlinkEntity( ... )	COLL_PICK( SV_UnlinkEntity )( __VA_ARGS__ )
#define SV_PointContents( ... )	COLL_PICK( SV_PointContents )( __VA_ARGS__ )
#define SV_ClipHandleForEntity( ... )	COLL_PICK( SV_ClipHandleForEntity )( __VA_ARGS__ )
#define SV_ClearWorld( ... )	COLL_PICK( SV_ClearWorld )( __VA_ARGS__ )
#define SV_AreaEntities( ... )	COLL_PICK( SV_AreaEntities )( __VA_ARGS__ )
#define CM_TransformedBoxTrace( ... )	COLL_PICK( CM_TransformedBoxTrace )( __VA_ARGS__ )
#define CM_LoadMap( ... )	COLL_PICK( CM_LoadMap )( __VA_ARGS__ )
#define CM_BoxTrace( ... )	COLL_PICK( CM_BoxTrace )( __VA_ARGS__ )
#define CM_PointLeafnum( ... )	COLL_PICK( CM_PointLeafnum )( __VA_ARGS__ )
#define CM_LeafCluster( ... )	COLL_PICK( CM_LeafCluster )( __VA_ARGS__ )
#define CM_LeafArea( ... )	COLL_PICK( CM_LeafArea )( __VA_ARGS__ )
#define CM_InlineModel( ... )	COLL_PICK( CM_InlineModel )( __VA_ARGS__ )
#define CM_AreasConnected( ... )	COLL_PICK( CM_AreasConnected )( __VA_ARGS__ )
#define CM_PointContents( ... )	COLL_PICK( CM_PointContents )( __VA_ARGS__ )
#define CM_NumInlineModels( ... )	COLL_PICK( CM_NumInlineModels )( __VA_ARGS__ )
#define CM_ModelBounds( ... )	COLL_PICK( CM_ModelBounds )( __VA_ARGS__ )
#define CM_EntityString( ... )	COLL_PICK( CM_EntityString )( __VA_ARGS__ )
#define CM_ClusterPVS( ... )	COLL_PICK( CM_ClusterPVS )( __VA_ARGS__ )
#define CM_AdjustAreaPortalState( ... )	COLL_PICK( CM_AdjustAreaPortalState )( __VA_ARGS__ )

#endif
