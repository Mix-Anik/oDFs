/*
 * pmhost.c - headless test host (see pmhost.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"

#include "pmhost.h"
#include "pmvm.h"
#include "../common/enginestubs.h"
#include "../pmove_qvm/pmtest.h"
#include "coll.h"

int gHostColl;

/* syscall numbers as the VM passes them (the asm constant inverted: trap_Trace -25 -> 24) */
enum {
	SC_PRINT = 0,
	SC_ERROR = 1,
	SC_TRACE = 24,
	SC_POINT_CONTENTS = 25,
	SC_SNAPVECTOR = 42,
	SC_MEMSET = 100,
	SC_MEMCPY = 101,
	SC_STRNCPY = 102,
	SC_SIN = 103,
	SC_COS = 104,
	SC_ATAN2 = 105,
	SC_SQRT = 106,
	SC_FLOOR = 110,
	SC_CEIL = 111
};

#define MAX_HOST_ENTITIES	256

typedef struct {
	int				number;
	clipHandle_t	handle;
	vec3_t			origin, angles;
} hostEntity_t;

struct pmhost_s {
	pmvm_t		*vm;
	int			reqAddr;		/* address of the pmReq_t inside the VM */
	int			trajAddr;		/* address of the pmTrajReq_t inside the VM */
	hostEntity_t	ents[MAX_HOST_ENTITIES];
	int			numEnts;
	int			mapLoaded;
	char		error[256];
};

static float ArgF( const int32_t *a, int i ) {
	float f;

	memcpy( &f, &a[i], sizeof( f ) );
	return f;
}

static int FloatAsInt( float f ) {
	int i;

	memcpy( &i, &f, sizeof( i ) );
	return i;
}

/* like VMA(): NULL stays NULL, everything else is masked into the data segment */
static void *Ptr( pmvm_t *vm, int32_t addr ) {
	return addr ? PMVM_Ptr( vm, addr ) : NULL;
}

/* SV_Trace without entities */
static void WorldTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs,
						const vec3_t end, int contentmask ) {
	if ( !mins ) {
		mins = vec3_origin;
	}
	if ( !maxs ) {
		maxs = vec3_origin;
	}
	CM_BoxTrace( results, start, end, mins, maxs, 0, contentmask, qfalse );
	results->entityNum = results->fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
}

/* SV_ClipMoveToEntities for brush models (assumed to be CONTENTS_SOLID, as every bmodel the server
   marks SOLID_BMODEL has solid or body contents) */
static void ClipToEntities( const pmhost_t *h, trace_t *result, const vec3_t start, const vec3_t end,
							const vec3_t mins, const vec3_t maxs, int passEntityNum, int contentmask ) {
	int i;

	if ( !( contentmask & CONTENTS_SOLID ) ) {
		return;
	}
	if ( !mins ) {
		mins = vec3_origin;
	}
	if ( !maxs ) {
		maxs = vec3_origin;
	}
	for ( i = 0; i < h->numEnts; i++ ) {
		const hostEntity_t *e = &h->ents[i];
		trace_t trace;

		if ( result->allsolid ) {
			return;
		}
		if ( e->number == passEntityNum ) {
			continue;
		}
		CM_TransformedBoxTrace( &trace, start, end, mins, maxs, e->handle, contentmask, e->origin, e->angles, qfalse );
		if ( trace.allsolid ) {
			result->allsolid = qtrue;
			trace.entityNum = e->number;
		} else if ( trace.startsolid ) {
			result->startsolid = qtrue;
			trace.entityNum = e->number;
		}
		if ( trace.fraction < result->fraction ) {
			qboolean oldStart = result->startsolid;

			trace.entityNum = e->number;
			*result = trace;
			result->startsolid |= oldStart;
		}
	}
}

static int HostSyscall( pmvm_t *vm, const int32_t *a ) {
	pmhost_t *h = (pmhost_t *)vm->user;

	switch ( a[0] ) {
	case SC_PRINT:
		return 0;

	case SC_ERROR:
		snprintf( vm->error, sizeof( vm->error ), "qvm error: %s", (const char *)PMVM_Ptr( vm, a[1] ) );
		return 0;

	case SC_TRACE:
		if ( !h->mapLoaded ) {
			snprintf( vm->error, sizeof( vm->error ), "trace without a map" );
			return 0;
		}
		{
			trace_t *result = (trace_t *)PMVM_Ptr( vm, a[1] );
			const float *start = (const float *)PMVM_Ptr( vm, a[2] ), *end = (const float *)PMVM_Ptr( vm, a[5] );
			const float *mins = (const float *)Ptr( vm, a[3] ), *maxs = (const float *)Ptr( vm, a[4] );

			WorldTrace( result, start, mins, maxs, end, a[7] );
			if ( result->fraction != 0 && h->numEnts ) {
				ClipToEntities( h, result, start, end, mins, maxs, a[6], a[7] );
			}
		}
		return 0;

	case SC_POINT_CONTENTS:
		if ( !h->mapLoaded ) {
			snprintf( vm->error, sizeof( vm->error ), "pointcontents without a map" );
			return 0;
		}
		return CM_PointContents( (const float *)PMVM_Ptr( vm, a[1] ), 0 );

	case SC_SNAPVECTOR:
		{
			float *v = (float *)PMVM_Ptr( vm, a[1] );

			v[0] = (float)rint( v[0] );
			v[1] = (float)rint( v[1] );
			v[2] = (float)rint( v[2] );
		}
		return 0;

	case SC_MEMSET:
		memset( PMVM_Ptr( vm, a[1] ), a[2], (size_t)a[3] );
		return a[1];

	case SC_MEMCPY:
		memmove( PMVM_Ptr( vm, a[1] ), PMVM_Ptr( vm, a[2] ), (size_t)a[3] );
		return a[1];

	case SC_STRNCPY:
		strncpy( (char *)PMVM_Ptr( vm, a[1] ), (const char *)PMVM_Ptr( vm, a[2] ), (size_t)a[3] );
		return a[1];

	case SC_SIN:
		return FloatAsInt( (float)sin( ArgF( a, 1 ) ) );
	case SC_COS:
		return FloatAsInt( (float)cos( ArgF( a, 1 ) ) );
	case SC_ATAN2:
		return FloatAsInt( (float)atan2( ArgF( a, 1 ), ArgF( a, 2 ) ) );
	case SC_SQRT:
		return FloatAsInt( (float)sqrt( ArgF( a, 1 ) ) );
	case SC_FLOOR:
		return FloatAsInt( (float)floor( ArgF( a, 1 ) ) );
	case SC_CEIL:
		return FloatAsInt( (float)ceil( ArgF( a, 1 ) ) );
	}

	snprintf( vm->error, sizeof( vm->error ), "unhandled syscall %d", a[0] );
	return 0;
}

pmhost_t *PMHost_Create( const char *qvmPath, char *err, int errSize ) {
	pmhost_t *h = (pmhost_t *)calloc( 1, sizeof( *h ) );

	h->vm = PMVM_Load( qvmPath, HostSyscall, h, err, errSize );
	if ( !h->vm ) {
		free( h );
		return NULL;
	}

	/* the module owns the request blocks; ask where they are */
	{
		int32_t arg = PMT_TRAJADDR;

		h->trajAddr = PMVM_Call( h->vm, 1, &arg );
		if ( h->trajAddr <= 0 || h->vm->error[0] ) {
			snprintf( err, errSize, "cannot find the trajectory block (%s)", h->vm->error );
			PMVM_Free( h->vm );
			free( h );
			return NULL;
		}
		arg = PMT_ADDR;

		h->reqAddr = PMVM_Call( h->vm, 1, &arg );
		if ( h->reqAddr <= 0 || h->vm->error[0] ) {
			snprintf( err, errSize, "cannot find the request block (%s)", h->vm->error );
			PMVM_Free( h->vm );
			free( h );
			return NULL;
		}
	}
	return h;
}

void PMHost_Destroy( pmhost_t *h ) {
	if ( h ) {
		PMVM_Free( h->vm );
		free( h );
	}
}

int PMHost_LoadMap( pmhost_t *h, const void *bsp, int length, int *checksum, char *err, int errSize ) {
	jmp_buf jb;

	Stub_SetFile( bsp, length );
	Stub_ErrorJmp = &jb;
	if ( setjmp( jb ) ) {
		Stub_ErrorJmp = NULL;
		snprintf( err, errSize, "map load failed: %s", Stub_ErrorMsg );
		return -1;
	}
	CM_LoadMap( "maps/test.bsp", qfalse, checksum );
	Stub_ErrorJmp = NULL;
	h->mapLoaded = 1;
	return 0;
}

/* BG_EvaluateTrajectory() run inside the QVM */
static void EvalTrajectory( pmhost_t *h, const trajectory_t *tr, int atTime, vec3_t out ) {
	pmTrajReq_t	*r = (pmTrajReq_t *)PMVM_Ptr( h->vm, h->trajAddr );
	int32_t		arg = PMT_TRAJ;

	r->tr = *tr;
	r->atTime = atTime;
	h->vm->error[0] = 0;
	PMVM_Call( h->vm, 1, &arg );
	VectorCopy( r->result, out );
}

void PMHost_SetEntities( pmhost_t *h, const pmEntity_t *ents, int count, int time ) {
	int i;

	h->numEnts = 0;
	for ( i = 0; i < count && h->numEnts < MAX_HOST_ENTITIES; i++ ) {
		hostEntity_t *e = &h->ents[h->numEnts];

		if ( !h->mapLoaded || ents[i].modelindex < 1 || ents[i].modelindex >= CM_NumInlineModels() ) {
			continue;
		}
		e->number = ents[i].number;
		e->handle = CM_InlineModel( ents[i].modelindex );
		EvalTrajectory( h, &ents[i].pos, time, e->origin );
		EvalTrajectory( h, &ents[i].apos, time, e->angles );
		h->numEnts++;
	}
}

int PMHost_Step( pmhost_t *h, playerState_t *ps, const usercmd_t *cmd, const pmParams_t *params,
				pmStepInfo_t *info ) {
	pmReq_t		*r = (pmReq_t *)PMVM_Ptr( h->vm, h->reqAddr );
	int32_t		args[2];
	jmp_buf		jb;

	memset( r, 0, sizeof( *r ) );
	r->ps = *ps;
	r->cmd = *cmd;
	r->tracemask = params->tracemask;
	r->pmove_fixed = params->pmove_fixed;
	r->pmove_msec = params->pmove_msec;
	r->obs_disabled = params->obs_disabled;
	r->promode = params->promode;
	r->killWallbug = params->killWallbug;

	args[0] = PMT_STEP;
	h->vm->error[0] = 0;

	Stub_ErrorJmp = &jb;
	if ( setjmp( jb ) ) {
		Stub_ErrorJmp = NULL;
		snprintf( h->error, sizeof( h->error ), "engine error: %s", Stub_ErrorMsg );
		return -1;
	}
	if ( PMVM_Call( h->vm, 1, args ) == -1 && h->vm->error[0] ) {
		Stub_ErrorJmp = NULL;
		snprintf( h->error, sizeof( h->error ), "%s", h->vm->error );
		return -1;
	}
	Stub_ErrorJmp = NULL;

	*ps = r->ps;
	if ( info ) {
		info->numtouch = r->numtouch;
		info->waterlevel = r->waterlevel;
		info->watertype = r->watertype;
	}
	return 0;
}

const char *PMHost_Error( const pmhost_t *h ) {
	return h->error;
}
