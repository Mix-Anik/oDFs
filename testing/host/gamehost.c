/*
 * gamehost.c - headless host that runs the real qagame.qvm (see gamehost.h)
 *
 * The behaviour of every syscall follows the engine's SV_GameSystemCalls (server/sv_game.c); the
 * entity/world calls use the engine's own sv_world.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

#include "gamehost.h"
#include "pmvm.h"
#include "../common/enginestubs.h"
#include "coll.h"

/* ---- the engine's `sv` global that sv_world.c uses ---- */

server_t sv;

sharedEntity_t *SV_GentityNum( int num ) {
	return (sharedEntity_t *)( (byte *)sv.gentities + sv.gentitySize * num );
}

svEntity_t *SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[gEnt->s.number];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	return SV_GentityNum( (int)( svEnt - sv.svEntities ) );
}

/* ---- host state ---- */

#define MAX_HCVARS			1024
#define MAX_HOST_CLIENTS	8
#define MAX_COMMANDS		256

typedef struct {
	char	name[MAX_QPATH];
	char	string[MAX_CVAR_VALUE_STRING];
	char	reset[MAX_CVAR_VALUE_STRING];
	int		flags;
	int		modCount;
	float	value;
	int		integer;
	int		userSet;		/* created by GH_SetCvar before the game registered it */
} hcvar_t;

typedef struct {
	int		clientNum;
	char	text[1024];
} hcommand_t;

struct gamehost_s {
	pmvm_t		*vm;
	hcvar_t		cvars[MAX_HCVARS];
	int			numCvars;
	int			infoModified;			/* a SERVERINFO / SYSTEMINFO cvar changed */

	char		*configstrings[MAX_CONFIGSTRINGS];
	char		userinfo[MAX_HOST_CLIENTS][MAX_INFO_STRING];
	int			clientActive[MAX_HOST_CLIENTS];
	usercmd_t	cmd[MAX_HOST_CLIENTS];

	int			time;
	const char	*entityParse;
	byte		*gameClients;			/* host pointer into the VM */
	int			gameClientSize;
	int			maxclients;

	int			offHealth, offTimeResidual, offEventTime;	/* from offsets.qvm; -1 when not loaded */
	int			sizeEntity, sizeClient;

	hcommand_t	commands[MAX_COMMANDS];
	int			numCommands;

	char		error[256];
	char		unhandled[256];
};

static gamehost_t *gHost;

/* ---- cvars ---- */

static hcvar_t *CvarFind( gamehost_t *gh, const char *name ) {
	int i;

	for ( i = 0; i < gh->numCvars; i++ ) {
		if ( !Q_stricmp( gh->cvars[i].name, name ) ) {
			return &gh->cvars[i];
		}
	}
	return NULL;
}

static void CvarSetValue( hcvar_t *cv, const char *value ) {
	Q_strncpyz( cv->string, value, sizeof( cv->string ) );
	cv->value = (float)atof( value );
	cv->integer = atoi( value );
	cv->modCount++;
}

static hcvar_t *CvarCreate( gamehost_t *gh, const char *name, const char *value, int flags ) {
	hcvar_t *cv = CvarFind( gh, name );

	if ( cv ) {
		cv->flags |= flags;
		return cv;
	}
	if ( gh->numCvars >= MAX_HCVARS ) {
		return NULL;
	}
	cv = &gh->cvars[gh->numCvars++];
	memset( cv, 0, sizeof( *cv ) );
	Q_strncpyz( cv->name, name, sizeof( cv->name ) );
	CvarSetValue( cv, value );
	cv->modCount = 1;
	Q_strncpyz( cv->reset, value, sizeof( cv->reset ) );
	cv->flags = flags;
	return cv;
}

void GH_SetCvar( gamehost_t *gh, const char *name, const char *value ) {
	hcvar_t *cv = CvarFind( gh, name );

	if ( !cv ) {
		cv = CvarCreate( gh, name, value, 0 );
		if ( cv ) {
			cv->userSet = 1;
		}
		return;
	}
	CvarSetValue( cv, value );
	cv->userSet = 1;
	if ( cv->flags & ( CVAR_SERVERINFO | CVAR_SYSTEMINFO ) ) {
		gh->infoModified = 1;
	}
}

static int CvarIndex( gamehost_t *gh, const hcvar_t *cv ) {
	return (int)( cv - gh->cvars );
}

/* Cvar_Update for the module's vmCvar_t */
static void VmCvarUpdate( gamehost_t *gh, vmCvar_t *vc ) {
	hcvar_t *cv;

	if ( vc->handle < 0 || vc->handle >= gh->numCvars ) {
		return;
	}
	cv = &gh->cvars[vc->handle];
	if ( vc->modificationCount == cv->modCount ) {
		return;
	}
	vc->modificationCount = cv->modCount;
	vc->value = cv->value;
	vc->integer = cv->integer;
	Q_strncpyz( vc->string, cv->string, sizeof( vc->string ) );
}

static void VmCvarRegister( gamehost_t *gh, vmCvar_t *vc, const char *name, const char *def, int flags ) {
	hcvar_t *cv = CvarFind( gh, name );

	if ( !cv ) {
		cv = CvarCreate( gh, name, def ? def : "", flags );
	} else {
		cv->flags |= flags;
		if ( !cv->userSet ) {
			/* the game's own default; reset value follows */
			Q_strncpyz( cv->reset, def ? def : "", sizeof( cv->reset ) );
		}
	}
	if ( !cv ) {
		return;
	}
	if ( cv->flags & ( CVAR_SERVERINFO | CVAR_SYSTEMINFO ) ) {
		gh->infoModified = 1;
	}
	if ( vc ) {
		vc->handle = CvarIndex( gh, cv );
		vc->modificationCount = -1;
		VmCvarUpdate( gh, vc );
	}
}

/* Cvar_InfoString: every cvar with the flag, in creation order */
static void CvarInfoString( gamehost_t *gh, int bit, char *out, int size ) {
	char info[BIG_INFO_STRING];
	int i;

	info[0] = '\0';
	for ( i = 0; i < gh->numCvars; i++ ) {
		const hcvar_t *cv = &gh->cvars[i];

		if ( ( cv->flags & bit ) && cv->string[0] ) {
			Info_SetValueForKey_s( info, sizeof( info ), cv->name, cv->string );
		}
	}
	Q_strncpyz( out, info, size );
}

/* ---- configstrings ---- */

static void SetConfigstring( gamehost_t *gh, int index, const char *val ) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return;
	}
	if ( !val ) {
		val = "";
	}
	free( gh->configstrings[index] );
	gh->configstrings[index] = strdup( val );
}

/* what SV_Frame does when a SERVERINFO / SYSTEMINFO cvar changed */
static void UpdateInfoConfigstrings( gamehost_t *gh ) {
	char info[BIG_INFO_STRING];

	CvarInfoString( gh, CVAR_SERVERINFO, info, sizeof( info ) );
	SetConfigstring( gh, CS_SERVERINFO, info );
	CvarInfoString( gh, CVAR_SYSTEMINFO, info, sizeof( info ) );
	SetConfigstring( gh, CS_SYSTEMINFO, info );
	gh->infoModified = 0;
}

/* ---- helpers ---- */

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

static void *Ptr( pmvm_t *vm, int32_t addr ) {
	return addr ? PMVM_Ptr( vm, addr ) : NULL;
}

static void AddCommand( gamehost_t *gh, int clientNum, const char *text ) {
	if ( gh->numCommands < MAX_COMMANDS ) {
		gh->commands[gh->numCommands].clientNum = clientNum;
		Q_strncpyz( gh->commands[gh->numCommands].text, text, sizeof( gh->commands[0].text ) );
		gh->numCommands++;
	}
}

/* SV_SetBrushModel */
static void SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t h;
	vec3_t mins, maxs;

	if ( !name || name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name ? name : "NULL" );
	}
	ent->s.modelindex = atoi( name + 1 );
	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy( mins, ent->r.mins );
	VectorCopy( maxs, ent->r.maxs );
	ent->r.bmodel = qtrue;
	ent->r.contents = -1;
	SV_LinkEntity( ent );
}

static qboolean InPVS( const vec3_t p1, const vec3_t p2, qboolean portals ) {
	int leafnum, cluster, area1, area2;
	byte *mask;

	leafnum = CM_PointLeafnum( p1 );
	cluster = CM_LeafCluster( leafnum );
	area1 = CM_LeafArea( leafnum );
	mask = CM_ClusterPVS( cluster );

	leafnum = CM_PointLeafnum( p2 );
	cluster = CM_LeafCluster( leafnum );
	area2 = CM_LeafArea( leafnum );
	if ( mask && ( !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) ) {
		return qfalse;
	}
	if ( portals && !CM_AreasConnected( area1, area2 ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, int capsule ) {
	clipHandle_t ch;
	trace_t trace;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, gEnt->r.currentOrigin,
		gEnt->r.currentAngles, capsule );
	return trace.startsolid;
}

static void Unhandled( gamehost_t *gh, int num ) {
	char buf[24];

	Com_sprintf( buf, sizeof( buf ), " %d", num );
	if ( !strstr( gh->unhandled, buf + 1 ) && strlen( gh->unhandled ) + strlen( buf ) < sizeof( gh->unhandled ) ) {
		Q_strcat( gh->unhandled, sizeof( gh->unhandled ), buf );
	}
}

/* ---- the syscalls ---- */

static int HostSyscall( pmvm_t *vm, const int32_t *a ) {
	gamehost_t *gh = (gamehost_t *)vm->user;

	switch ( a[0] ) {
	case G_PRINT:
		if ( getenv( "DFG_GPRINT" ) ) {
			fputs( (const char *)PMVM_Ptr( vm, a[1] ), stdout );
		}
		return 0;
	case G_ERROR:
		snprintf( vm->error, sizeof( vm->error ), "G_Error: %s", (const char *)PMVM_Ptr( vm, a[1] ) );
		return 0;
	case G_MILLISECONDS:
		return gh->time;

	case G_CVAR_REGISTER:
		VmCvarRegister( gh, (vmCvar_t *)Ptr( vm, a[1] ), (const char *)Ptr( vm, a[2] ), (const char *)Ptr( vm, a[3] ),
			a[4] );
		return 0;
	case G_CVAR_UPDATE:
		VmCvarUpdate( gh, (vmCvar_t *)PMVM_Ptr( vm, a[1] ) );
		return 0;
	case G_CVAR_SET:
		{
			const char *name = (const char *)PMVM_Ptr( vm, a[1] ), *val = (const char *)Ptr( vm, a[2] );
			hcvar_t *cv = CvarFind( gh, name );

			if ( !cv ) {
				cv = CvarCreate( gh, name, val ? val : "", 0 );
			} else {
				CvarSetValue( cv, val ? val : "" );
			}
			if ( cv && ( cv->flags & ( CVAR_SERVERINFO | CVAR_SYSTEMINFO ) ) ) {
				gh->infoModified = 1;
			}
		}
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		{
			hcvar_t *cv = CvarFind( gh, (const char *)PMVM_Ptr( vm, a[1] ) );

			return cv ? cv->integer : 0;
		}
	case G_CVAR_VARIABLE_STRING_BUFFER:
		{
			hcvar_t *cv = CvarFind( gh, (const char *)PMVM_Ptr( vm, a[1] ) );

			Q_strncpyz( (char *)PMVM_Ptr( vm, a[2] ), cv ? cv->string : "", a[3] );
		}
		return 0;

	case G_ARGC:
		return 0;
	case G_ARGV:
		if ( a[3] > 0 ) {
			*(char *)PMVM_Ptr( vm, a[2] ) = '\0';
		}
		return 0;

	case G_FS_FOPEN_FILE:
		/* the recorded game's files are not available: reads fail, writes are accepted and dropped */
		if ( a[3] == FS_READ ) {
			if ( a[2] ) {
				*(int *)PMVM_Ptr( vm, a[2] ) = 0;
			}
			return -1;
		}
		if ( a[2] ) {
			*(int *)PMVM_Ptr( vm, a[2] ) = 1;
		}
		return 0;
	case G_FS_READ:
		return 0;
	case G_FS_WRITE:
		return 0;
	case G_FS_FCLOSE_FILE:
		return 0;
	case G_FS_SEEK:
		return 0;
	case G_FS_GETFILELIST:
		return 0;

	case G_SEND_CONSOLE_COMMAND:
		return 0;
	case G_LOCATE_GAME_DATA:
		sv.gentities = (sharedEntity_t *)PMVM_Ptr( vm, a[1] );
		sv.num_entities = a[2];
		sv.gentitySize = a[3];
		gh->gameClients = (byte *)PMVM_Ptr( vm, a[4] );
		gh->gameClientSize = a[5];
		return 0;
	case G_DROP_CLIENT:
		return 0;
	case G_SEND_SERVER_COMMAND:
		AddCommand( gh, a[1], (const char *)PMVM_Ptr( vm, a[2] ) );
		return 0;

	case G_SET_CONFIGSTRING:
		SetConfigstring( gh, a[1], (const char *)Ptr( vm, a[2] ) );
		return 0;
	case G_GET_CONFIGSTRING:
		{
			const char *s = ( a[1] >= 0 && a[1] < MAX_CONFIGSTRINGS && gh->configstrings[a[1]] ) ? gh->configstrings[a[1]] : "";

			Q_strncpyz( (char *)PMVM_Ptr( vm, a[2] ), s, a[3] );
		}
		return 0;
	case G_GET_USERINFO:
		if ( a[1] >= 0 && a[1] < MAX_HOST_CLIENTS ) {
			Q_strncpyz( (char *)PMVM_Ptr( vm, a[2] ), gh->userinfo[a[1]], a[3] );
		} else {
			*(char *)PMVM_Ptr( vm, a[2] ) = '\0';
		}
		return 0;
	case G_SET_USERINFO:
		if ( a[1] >= 0 && a[1] < MAX_HOST_CLIENTS ) {
			Q_strncpyz( gh->userinfo[a[1]], (const char *)PMVM_Ptr( vm, a[2] ), sizeof( gh->userinfo[0] ) );
		}
		return 0;
	case G_GET_SERVERINFO:
		Q_strncpyz( (char *)PMVM_Ptr( vm, a[1] ), gh->configstrings[CS_SERVERINFO] ? gh->configstrings[CS_SERVERINFO] : "",
			a[2] );
		return 0;

	case G_SET_BRUSH_MODEL:
		SetBrushModel( (sharedEntity_t *)PMVM_Ptr( vm, a[1] ), (const char *)Ptr( vm, a[2] ) );
		return 0;
	case G_TRACE:
		SV_Trace( (trace_t *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ), (const float *)Ptr( vm, a[3] ),
			(const float *)Ptr( vm, a[4] ), (const float *)PMVM_Ptr( vm, a[5] ), a[6], a[7], qfalse );
		return 0;
	case G_TRACECAPSULE:
		SV_Trace( (trace_t *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ), (const float *)Ptr( vm, a[3] ),
			(const float *)Ptr( vm, a[4] ), (const float *)PMVM_Ptr( vm, a[5] ), a[6], a[7], qtrue );
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( (const float *)PMVM_Ptr( vm, a[1] ), a[2] );
	case G_IN_PVS:
		return InPVS( (const float *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ), qtrue );
	case G_IN_PVS_IGNORE_PORTALS:
		return InPVS( (const float *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ), qfalse );
	case G_ADJUST_AREA_PORTAL_STATE:
		{
			svEntity_t *svEnt = SV_SvEntityForGentity( (sharedEntity_t *)PMVM_Ptr( vm, a[1] ) );

			if ( svEnt->areanum2 != -1 ) {
				CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, a[2] );
			}
		}
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( a[1], a[2] );
	case G_LINKENTITY:
		SV_LinkEntity( (sharedEntity_t *)PMVM_Ptr( vm, a[1] ) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( (sharedEntity_t *)PMVM_Ptr( vm, a[1] ) );
		return 0;
	case G_ENTITIES_IN_BOX:
		return SV_AreaEntities( (const float *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ),
			(int *)PMVM_Ptr( vm, a[3] ), a[4] );
	case G_ENTITY_CONTACT:
		return EntityContact( (const float *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ),
			(const sharedEntity_t *)PMVM_Ptr( vm, a[3] ), qfalse );
	case G_ENTITY_CONTACTCAPSULE:
		return EntityContact( (const float *)PMVM_Ptr( vm, a[1] ), (const float *)PMVM_Ptr( vm, a[2] ),
			(const sharedEntity_t *)PMVM_Ptr( vm, a[3] ), qtrue );

	case G_BOT_ALLOCATE_CLIENT:
		return -1;
	case G_BOT_FREE_CLIENT:
		return 0;

	case G_GET_USERCMD:
		if ( a[1] >= 0 && a[1] < MAX_HOST_CLIENTS ) {
			*(usercmd_t *)PMVM_Ptr( vm, a[2] ) = gh->cmd[a[1]];
		}
		return 0;

	case G_GET_ENTITY_TOKEN:
		{
			const char *s = COM_Parse( &gh->entityParse );
			char *dst = (char *)PMVM_Ptr( vm, a[1] );
			const int size = a[2] - 1;

			/* like the engine (the defrag mod relies on the buffer being zero padded) */
			if ( size >= 0 ) {
				Q_strncpy( dst, s, size );
				dst[size] = '\0';
			}
			if ( !gh->entityParse && s[0] == '\0' ) {
				return qfalse;
			}
			return qtrue;
		}

	case G_DEBUG_POLYGON_CREATE:
	case G_DEBUG_POLYGON_DELETE:
		return 0;
	case G_REAL_TIME:
		{
			qtime_t *q = (qtime_t *)Ptr( vm, a[1] );

			if ( q ) {
				memset( q, 0, sizeof( *q ) );
				q->tm_year = 120;		/* a fixed date: the tests must not depend on the wall clock */
				q->tm_mday = 1;
			}
			return 1577836800;
		}
	case G_SNAPVECTOR:
		{
			float *v = (float *)PMVM_Ptr( vm, a[1] );

			v[0] = (float)rint( v[0] );
			v[1] = (float)rint( v[1] );
			v[2] = (float)rint( v[2] );
		}
		return 0;

	/* shared syscalls */
	case 100:
		memset( PMVM_Ptr( vm, a[1] ), a[2], (size_t)a[3] );
		return a[1];
	case 101:
		memmove( PMVM_Ptr( vm, a[1] ), PMVM_Ptr( vm, a[2] ), (size_t)a[3] );
		return a[1];
	case 102:
		Q_strncpy( (char *)PMVM_Ptr( vm, a[1] ), (const char *)PMVM_Ptr( vm, a[2] ), a[3] );
		return a[1];
	case 103:
		return FloatAsInt( (float)sin( ArgF( a, 1 ) ) );
	case 104:
		return FloatAsInt( (float)cos( ArgF( a, 1 ) ) );
	case 105:
		return FloatAsInt( (float)atan2( ArgF( a, 1 ), ArgF( a, 2 ) ) );
	case 106:
		return FloatAsInt( (float)sqrt( ArgF( a, 1 ) ) );
	case G_FLOOR:
		return FloatAsInt( (float)floor( ArgF( a, 1 ) ) );
	case G_CEIL:
		return FloatAsInt( (float)ceil( ArgF( a, 1 ) ) );
	case G_TESTPRINTINT:
		return sprintf( (char *)PMVM_Ptr( vm, a[1] ), "%i", (int)a[2] );
	case G_TESTPRINTFLOAT:
		return sprintf( (char *)PMVM_Ptr( vm, a[1] ), "%f", ArgF( a, 2 ) );
	}

	Unhandled( gh, a[0] );
	return 0;
}

/* ---- module calls ---- */

static int VMCall( gamehost_t *gh, int command, int a0, int a1, int a2, int nargs ) {
	int32_t args[4];
	jmp_buf jb;
	int r;

	args[0] = command;
	args[1] = a0;
	args[2] = a1;
	args[3] = a2;

	gh->vm->error[0] = '\0';
	Stub_ErrorJmp = &jb;
	if ( setjmp( jb ) ) {
		Stub_ErrorJmp = NULL;
		snprintf( gh->error, sizeof( gh->error ), "engine error: %s", Stub_ErrorMsg );
		return -1;
	}
	r = PMVM_Call( gh->vm, 1 + nargs, args );
	Stub_ErrorJmp = NULL;
	if ( gh->vm->error[0] ) {
		snprintf( gh->error, sizeof( gh->error ), "%s", gh->vm->error );
		return -1;
	}
	return r;
}

gamehost_t *GH_Create( const char *qvmPath, char *err, int errSize ) {
	gamehost_t *gh = (gamehost_t *)calloc( 1, sizeof( *gh ) );

	gh->vm = PMVM_Load( qvmPath, HostSyscall, gh, err, errSize );
	if ( !gh->vm ) {
		free( gh );
		return NULL;
	}
	gHost = gh;
	memset( &sv, 0, sizeof( sv ) );
	return gh;
}

void GH_Destroy( gamehost_t *gh ) {
	int i;

	if ( !gh ) {
		return;
	}
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		free( gh->configstrings[i] );
	}
	PMVM_Free( gh->vm );
	free( gh );
	if ( gHost == gh ) {
		gHost = NULL;
	}
}

static int OffsetsSyscall( pmvm_t *vm, const int32_t *a ) {
	snprintf( vm->error, sizeof( vm->error ), "offsets.qvm made syscall %d", a[0] );
	return 0;
}

int GH_LoadOffsets( gamehost_t *gh, const char *path, char *err, int errSize ) {
	pmvm_t *vm = PMVM_Load( path, OffsetsSyscall, gh, err, errSize );
	int32_t cmd;

	if ( !vm ) {
		return -1;
	}
	cmd = 1; gh->offHealth = PMVM_Call( vm, 1, &cmd );
	cmd = 2; gh->offTimeResidual = PMVM_Call( vm, 1, &cmd );
	cmd = 3; gh->sizeEntity = PMVM_Call( vm, 1, &cmd );
	cmd = 4; gh->sizeClient = PMVM_Call( vm, 1, &cmd );
	cmd = 5; gh->offEventTime = PMVM_Call( vm, 1, &cmd );
	if ( vm->error[0] || gh->offHealth <= 0 || gh->offTimeResidual <= 0 ) {
		snprintf( err, errSize, "offsets.qvm: %s", vm->error[0] ? vm->error : "bad offsets" );
		PMVM_Free( vm );
		return -1;
	}
	PMVM_Free( vm );
	return 0;
}

void GH_SetEventTime( gamehost_t *gh, int clientNum, int time ) {
	if ( gh->offEventTime > 0 ) {
		*(int *)( (byte *)SV_GentityNum( clientNum ) + gh->offEventTime ) = time;
	}
}

void GH_SetHealth( gamehost_t *gh, int clientNum, int health ) {
	if ( gh->offHealth > 0 ) {
		*(int *)( (byte *)SV_GentityNum( clientNum ) + gh->offHealth ) = health;
	}
}

void GH_SetTimeResidual( gamehost_t *gh, int clientNum, int msec ) {
	if ( gh->offTimeResidual > 0 ) {
		*(int *)( gh->gameClients + (size_t)gh->gameClientSize * clientNum + gh->offTimeResidual ) = msec;
	}
}

int GH_GetTimeResidual( gamehost_t *gh, int clientNum ) {
	if ( gh->offTimeResidual > 0 ) {
		return *(int *)( gh->gameClients + (size_t)gh->gameClientSize * clientNum + gh->offTimeResidual );
	}
	return -1;
}

int GH_StartMap( gamehost_t *gh, const void *bsp, int length, int startTime, char *err, int errSize ) {
	jmp_buf jb;
	int checksum, i;
	char value[64];

	Stub_SetFile( bsp, length );
	Stub_ErrorJmp = &jb;
	if ( setjmp( jb ) ) {
		Stub_ErrorJmp = NULL;
		snprintf( err, errSize, "map load failed: %s", Stub_ErrorMsg );
		return -1;
	}
	CM_LoadMap( "maps/test.bsp", qfalse, &checksum );
	SV_ClearWorld();
	Stub_ErrorJmp = NULL;

	/* the cvars the engine itself owns */
	Com_sprintf( value, sizeof( value ), "%d", checksum );
	GH_SetCvar( gh, "sv_mapChecksum", value );
	CvarCreate( gh, "sv_mapChecksum", value, CVAR_SYSTEMINFO | CVAR_ROM );
	if ( !CvarFind( gh, "sv_maxclients" ) ) {
		CvarCreate( gh, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH );
	}
	CvarCreate( gh, "mapname", "test", CVAR_SERVERINFO | CVAR_ROM );
	gh->maxclients = CvarFind( gh, "sv_maxclients" )->integer;
	if ( gh->maxclients > MAX_HOST_CLIENTS ) {
		gh->maxclients = MAX_HOST_CLIENTS;
	}

	UpdateInfoConfigstrings( gh );

	sv.state = SS_LOADING;
	gh->time = startTime;
	gh->entityParse = CM_EntityString();

	if ( VMCall( gh, GAME_INIT, gh->time, 12345, 0, 3 ) == -1 && gh->error[0] ) {
		snprintf( err, errSize, "GAME_INIT failed: %s", gh->error );
		return -1;
	}
	sv.state = SS_GAME;

	/* run a few frames to allow everything to settle */
	for ( i = 0; i < 3; i++ ) {
		if ( GH_RunFrame( gh, gh->time + 100 ) ) {
			snprintf( err, errSize, "settle frame failed: %s", gh->error );
			return -1;
		}
	}
	return 0;
}

int GH_ConnectClient( gamehost_t *gh, const char *userinfo, char *err, int errSize ) {
	int clientNum = 0, denied;

	Q_strncpyz( gh->userinfo[clientNum], userinfo, sizeof( gh->userinfo[0] ) );
	denied = VMCall( gh, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse, 3 );
	if ( gh->error[0] ) {
		snprintf( err, errSize, "ClientConnect failed: %s", gh->error );
		return -1;
	}
	if ( denied ) {
		snprintf( err, errSize, "ClientConnect refused: %s", (const char *)PMVM_Ptr( gh->vm, denied ) );
		return -1;
	}
	if ( VMCall( gh, GAME_CLIENT_BEGIN, clientNum, 0, 0, 1 ) == -1 && gh->error[0] ) {
		snprintf( err, errSize, "ClientBegin failed: %s", gh->error );
		return -1;
	}
	gh->clientActive[clientNum] = 1;
	return clientNum;
}

void GH_SetUsercmd( gamehost_t *gh, int clientNum, const usercmd_t *cmd ) {
	if ( clientNum >= 0 && clientNum < MAX_HOST_CLIENTS ) {
		gh->cmd[clientNum] = *cmd;
	}
}

int GH_ClientThink( gamehost_t *gh, int clientNum ) {
	if ( VMCall( gh, GAME_CLIENT_THINK, clientNum, 0, 0, 1 ) == -1 && gh->error[0] ) {
		return -1;
	}
	return 0;
}

int GH_RunFrame( gamehost_t *gh, int time ) {
	gh->time = time;
	if ( gh->infoModified ) {
		UpdateInfoConfigstrings( gh );
	}
	if ( VMCall( gh, GAME_RUN_FRAME, time, 0, 0, 1 ) == -1 && gh->error[0] ) {
		return -1;
	}
	return 0;
}

int GH_Time( const gamehost_t *gh ) {
	return gh->time;
}

playerState_t *GH_PlayerState( gamehost_t *gh, int clientNum ) {
	return (playerState_t *)( gh->gameClients + (size_t)gh->gameClientSize * clientNum );
}

sharedEntity_t *GH_Entity( gamehost_t *gh, int num ) {
	(void)gh;
	return SV_GentityNum( num );
}

int GH_NumEntities( const gamehost_t *gh ) {
	(void)gh;
	return sv.num_entities;
}

const char *GH_Configstring( const gamehost_t *gh, int num ) {
	return ( num >= 0 && num < MAX_CONFIGSTRINGS && gh->configstrings[num] ) ? gh->configstrings[num] : "";
}

const char *GH_Error( const gamehost_t *gh ) {
	return gh->error;
}

const char *GH_Unhandled( const gamehost_t *gh ) {
	return gh->unhandled;
}

int GH_NumCommands( const gamehost_t *gh ) {
	return gh->numCommands;
}

const char *GH_Command( const gamehost_t *gh, int index, int *clientNum ) {
	if ( index < 0 || index >= gh->numCommands ) {
		return "";
	}
	if ( clientNum ) {
		*clientNum = gh->commands[index].clientNum;
	}
	return gh->commands[index].text;
}

void GH_ClearCommands( gamehost_t *gh ) {
	gh->numCommands = 0;
}
