// cg_dfdemocam.c -- DeFRaG demo camera (df_demos_CameraMode 0..11, df_mp_TrackPlayerNum); cgame 0x1549d driver, 0x7c81 track record, 0x15040 target,
// 0x15119 transition, 0x14741 third-person offset, 0x14f18 aim; see disasm/CGAME/demo_camera.md
//
// Ported: modes 4..11 (4..7 = the position saved by `savepos`, falling back to third person; 8 = the tracked client's own view; 9 = third person at
// 100 units; 10 / 11 = third person at cg_thirdPersonRange), the smooth transition between targets (df_demos_CameraTransitionSpeed), the
// df_demos_CamOffset* cvars of the replay camera, the tracked-client record (df_mp_TrackPlayerNum) and the hide-body list (modes 2 and 8).
// Modes 1..3: key-driven free cameras (free fly / first person at the tracked client with free look / third person around it).
// Mode 8 on another client also takes that client's weapon and draws its gun (0x15280, CG_DF_TrackedViewGun in cg_dfghost.c).
// Not ported: the PVS fix, the `targetposname` presets.

#include "cg_local.h"

vmCvar_t	df_demos_CameraMode, df_mp_TrackPlayerNum, df_demos_CameraTransitionSpeed;
vmCvar_t	df_ghosts_CheckPVS;
qboolean	cg_dfClearAreamask;		// set by a camera that left the player's PVS; applied after cg.refdef.areamask is copied

// 0x8f57: DeFRaG's PVS test - when `p` is not visible from the player's predicted origin the area mask is cleared
// (all areas drawn).  The original clears cg.refdef.areamask right away; oDFs copies the snapshot mask later, so it is deferred.
void CG_DF_CheckPVS( const vec3_t p ) {
	if ( !trap_R_inPVS( cg.predictedPlayerState.origin, p ) ) {
		cg_dfClearAreamask = qtrue;
	}
}
vmCvar_t	df_demos_HidePacketEntities, df_demos_HideLocalEntities, df_demos_UnlockThirdPersonPitch;

typedef struct {
	int		mode, init;
	vec3_t	posA, angA, posB, angB, posCur, angCur, posOut, angOut;
	float	progress, speed;
	int		lastTime;
	float	dt;
} dcCam_t;

typedef struct {
	int		client, weapon;
	vec3_t	pos, ang, pos2;
} dcTrack_t;

static dcCam_t	dcDemoCam, dcReplayCam, *dcCur = &dcDemoCam;
#define dc ( *dcCur )

static float DC_Clamp01( float x ) {
	if ( !( x >= 0 ) ) {
		return 0;
	}
	return x <= 1 ? x : 1;
}

// 0x7c81: the tracked client (df_mp_TrackPlayerNum) or the viewer
static void DC_Track( dcTrack_t *t ) {
	int c = df_mp_TrackPlayerNum.integer;

	t->client = cg.clientNum;
	t->weapon = cg.predictedPlayerState.weapon;
	VectorCopy( cg.refdef.vieworg, t->pos );
	VectorCopy( cg.refdefViewAngles, t->ang );
	VectorCopy( cg.predictedPlayerState.origin, t->pos2 );
	if ( c == cg.clientNum || c < 0 || c >= MAX_CLIENTS || !cgs.clientinfo[c].infoValid || cgs.clientinfo[c].team >= TEAM_SPECTATOR ) {
		return;
	}
	if ( !trap_R_inPVS( cg_entities[c].lerpOrigin, cg.refdef.vieworg ) ) {		// 0x7ce2
		if ( df_ghosts_CheckPVS.integer ) {
			return;
		}
		cg_dfClearAreamask = qtrue;
	}
	t->client = c;
	t->weapon = cg_entities[c].currentState.weapon;
	VectorCopy( cg_entities[c].lerpOrigin, t->pos );
	VectorCopy( cg_entities[c].lerpAngles, t->ang );
	VectorCopy( cg_entities[c].lerpOrigin, t->pos2 );
	t->pos[2] += 26;		// DeFRaG's *(5200)
}

// 0x7c81 for other modules (the ghost miniview, mode 7/8): the tracked client's record; returns the client number
qboolean CG_DF_TrackRecord( int *client, int *weapon, vec3_t pos, vec3_t ang ) {
	dcTrack_t t;

	DC_Track( &t );
	*client = t.client;
	*weapon = t.weapon;
	VectorCopy( t.pos, pos );
	VectorCopy( t.ang, ang );
	return qtrue;
}

// 0x14741: third-person chase position behind `pos`/`ang` (range back, yawOffset degrees), traced against the world
static void DC_ThirdPerson( vec3_t pos, vec3_t ang, float yawOffset, float range ) {
	vec3_t	eye, fwd, right, up, look, cam, tmp, mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };
	float	a[3], s, c, dist, hd;
	trace_t	tr;

	VectorCopy( pos, eye );
	eye[2] += cg.predictedPlayerState.viewheight;
	VectorCopy( ang, a );
	if ( !df_demos_UnlockThirdPersonPitch.integer && a[0] > 45 ) {
		a[0] = 45;
	}
	AngleVectors( a, fwd, NULL, NULL );
	VectorMA( eye, 512, fwd, look );
	VectorCopy( eye, cam );
	cam[2] += 8;
	a[0] = df_demos_UnlockThirdPersonPitch.integer ? ang[0] : ang[0] * 0.5f;
	a[1] = ang[1];
	a[2] = ang[2];
	AngleVectors( a, fwd, right, up );
	s = sin( yawOffset * M_PI / 180 );
	c = cos( yawOffset * M_PI / 180 );
	VectorMA( cam, -range * c, fwd, cam );
	VectorMA( cam, -range * s, right, cam );
	CG_Trace( &tr, eye, mins, maxs, cam, cg.clientNum, MASK_SOLID );
	if ( tr.fraction != 1.0f ) {
		VectorCopy( tr.endpos, cam );
		cam[2] += 32 * ( 1.0f - tr.fraction );
		CG_Trace( &tr, eye, mins, maxs, cam, cg.clientNum, MASK_SOLID );
		VectorCopy( tr.endpos, cam );
	}
	VectorCopy( cam, pos );
	VectorSubtract( look, cam, tmp );
	hd = sqrt( tmp[0] * tmp[0] + tmp[1] * tmp[1] );
	if ( hd < 1 ) {
		hd = 1;
	}
	dist = atan2( tmp[2], hd );
	ang[0] = -57.29578f * dist;
	ang[1] -= yawOffset;
}

// savepos string "placeplayer x y z p y r ..." from the cvar named by saveposname (cgame 0x7d59)
static qboolean DC_SavedPos( vec3_t pos, vec3_t ang ) {
	char name[128], val[512];
	const char *p;
	const char *tok;
	float v[6];
	int i;

	trap_Cvar_VariableStringBuffer( "saveposname", name, sizeof( name ) );
	if ( !name[0] ) {
		return qfalse;
	}
	trap_Cvar_VariableStringBuffer( name, val, sizeof( val ) );
	p = val;
	tok = COM_Parse( &p );
	if ( Q_stricmp( tok, "placeplayer" ) ) {
		return qfalse;
	}
	for ( i = 0; i < 6; i++ ) {
		v[i] = atof( COM_Parse( &p ) );
	}
	v[2] += 26;
	VectorSet( pos, v[0], v[1], v[2] );
	VectorSet( ang, v[3], v[4], v[5] );
	return qtrue;
}

// 0x14d30: camera "sequences". df_demos_CamSeqPosition / Speed / Mode hold space separated lists; whenever the camera is idle
// (cam+4 == 0, called at the top of 0x15040) the first token of each list is moved into saveposname / df_demos_CameraTransitionSpeed /
// df_demos_CameraMode and the rest of the list stays behind in the cvar's LOCAL string only (DeFRaG's write-back flag 0x420 is never set,
// so the real cvar keeps the full list until it is changed). An empty Position list ends the update early (returns 0), as in DeFRaG.
vmCvar_t	df_demos_CamSeqPosition, df_demos_CamSeqSpeed, df_demos_CamSeqMode;

static qboolean DC_SeqPop( vmCvar_t *list, const char *target ) {
	char		buf[MAX_CVAR_VALUE_STRING];
	const char	*p = list->string, *tok;

	if ( !list->string[0] ) {
		return qfalse;
	}
	Q_strncpyz( buf, list->string, sizeof( buf ) );
	p = buf;
	tok = COM_Parse( &p );
	if ( !tok[0] ) {
		return qfalse;
	}
	trap_Cvar_Set( target, tok );
	Q_strncpyz( list->string, p, sizeof( list->string ) );
	return qtrue;
}

static void DC_CamSeq( void ) {
	if ( !DC_SeqPop( &df_demos_CamSeqPosition, "saveposname" ) ) {
		return;
	}
	DC_SeqPop( &df_demos_CamSeqSpeed, "df_demos_CameraTransitionSpeed" );
	DC_SeqPop( &df_demos_CamSeqMode, "df_demos_CameraMode" );
}

// 0x15040: where the camera should be for this mode; returns qfalse when the camera was set immediately (no transition)
static qboolean DC_Target( const dcTrack_t *t, vec3_t pos, vec3_t ang ) {
	float range = cg_thirdPersonRange.value;

	if ( !dc.init ) {
		DC_CamSeq();
	}
	switch ( dc.mode ) {
	case 4: case 5: case 6: case 7:
		if ( !DC_SavedPos( pos, ang ) ) {
			VectorCopy( t->pos2, pos );
			VectorCopy( t->ang, ang );
			DC_ThirdPerson( pos, ang, 0, range );
			VectorCopy( pos, dc.posA ); VectorCopy( pos, dc.posB ); VectorCopy( pos, dc.posCur ); VectorCopy( pos, dc.posOut );
			VectorCopy( ang, dc.angA ); VectorCopy( ang, dc.angB ); VectorCopy( ang, dc.angCur ); VectorCopy( ang, dc.angOut );
			return qfalse;
		}
		break;
	case 9:
		VectorCopy( t->pos2, pos );
		VectorCopy( t->ang, ang );
		DC_ThirdPerson( pos, ang, 0, 100 );
		break;
	case 10: case 11:
		VectorCopy( t->pos2, pos );
		VectorCopy( t->ang, ang );
		DC_ThirdPerson( pos, ang, 0, range );
		break;
	default:
		VectorCopy( t->pos, pos );
		VectorCopy( t->ang, ang );
		break;
	}
	if ( VectorCompare( pos, dc.posB ) && VectorCompare( ang, dc.angB ) ) {
		return qtrue;
	}
	VectorCopy( pos, dc.posB );
	VectorCopy( ang, dc.angB );
	dc.init = 0;
	return qtrue;
}

// 0x15119 / 0x14e16 / 0x14e3f: advance the transition between A and B
static void DC_Transition( void ) {
	int		i;
	float	sp = dc.speed, d;
	vec3_t	diff;

	if ( !dc.init ) {
		VectorCopy( dc.posCur, dc.posA );
		VectorCopy( dc.angCur, dc.angA );
		dc.progress = 0;
		dc.init = 1;
	}
	if ( sp >= 0 ) {
		dc.progress = DC_Clamp01( dc.progress + ( fabs( sp ) / 100.0f ) * dc.dt );
	} else {
		VectorSubtract( dc.posA, dc.posB, diff );
		d = VectorLength( diff );
		if ( d != 0 ) {
			d = 1000.0f / d;
		}
		dc.progress = DC_Clamp01( dc.progress + ( ( fabs( sp ) / 100.0f ) * dc.dt ) * d );
	}
	if ( sp == 0 ) {
		dc.progress = 1;
	}
	for ( i = 0; i < 3; i++ ) {
		dc.posCur[i] = dc.posA[i] + dc.progress * ( dc.posB[i] - dc.posA[i] );
		dc.angCur[i] = LerpAngle( dc.angA[i], dc.angB[i], dc.progress );
	}
	if ( dc.progress >= 1.0f ) {
		dc.init = 0;
	}
}

// 0x14f18: camera angles for the aim modes
static void DC_Aim( const vec3_t dir, const vec3_t keep, vec3_t out ) {
	vec3_t a;

	switch ( dc.mode ) {
	case 4: VectorCopy( keep, out ); break;
	case 5: vectoangles( dir, out ); out[2] = keep[2]; break;
	case 6: vectoangles( dir, a ); a[0] = keep[0]; a[2] = keep[2]; VectorCopy( a, out ); break;
	case 7: vectoangles( dir, a ); a[1] = keep[1]; a[2] = keep[2]; VectorCopy( a, out ); break;
	case 8: vectoangles( dir, out ); break;
	default: VectorCopy( keep, out ); break;
	}
}

// view setup (0x14fd4)
static void DC_Apply( const vec3_t pos, const vec3_t ang ) {
	VectorCopy( pos, cg.refdef.vieworg );
	VectorCopy( ang, cg.refdefViewAngles );
	AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
	if ( df_ghosts_CheckPVS.integer ) {		// 0x14fe4
		CG_DF_CheckPVS( cg.refdef.vieworg );
	}
}

// the df_demos_CamOffset* position / angle / radius offsets (same cvars as the replay camera)
extern void CG_DF_CamOffsets( vec3_t pos, vec3_t ang );

qboolean CG_DF_HideTrackedBody( int clientNum ) {		// 0x14633: modes 2 and 8 hide the tracked client's body
	return cg.demoPlayback && df_demos_CameraMode.integer && ( df_demos_CameraMode.integer == 2 || df_demos_CameraMode.integer == 8 )
		&& clientNum == ( df_mp_TrackPlayerNum.integer >= 0 ? df_mp_TrackPlayerNum.integer : cg.clientNum ) && clientNum != cg.clientNum;
}

// ---- key-driven free camera (modes 1..3; cgame 0x14921 / 0x148f0 / 0x8523) ----
// A position and look angles owned by the camera: the angles follow the usercmd angles (pitch limited to +-87.9 degrees through an accumulated
// delta), the position moves along the view axes with the forward / side / up movement keys at 5 * |df_demos_CameraTransitionSpeed| / 100 units
// per frame-second per key unit.
static struct { qboolean init; int delta[3]; vec3_t pos, ang; } dcFree;

static void DC_FreeUpdate( float dt ) {
	usercmd_t	cmd;
	vec3_t		axis[3];
	float		f;
	char		buf[32];
	int			i, n;

	if ( !dcFree.init ) {
		dcFree.delta[0] = 0;
		dcFree.delta[1] = cg.predictedPlayerState.delta_angles[1];
		dcFree.delta[2] = cg.predictedPlayerState.delta_angles[2];
		VectorCopy( cg.refdef.vieworg, dcFree.pos );
		VectorCopy( cg.refdefViewAngles, dcFree.ang );
		dcFree.init = qtrue;
	}
	n = trap_GetCurrentCmdNumber();
	trap_GetUserCmd( n, &cmd );
	for ( i = 0; i < 3; i++ ) {		// 0x8523
		int a = (short)( cmd.angles[i] + dcFree.delta[i] );

		if ( i == 0 ) {
			if ( a > 16000 ) {
				dcFree.delta[0] = 16000 - cmd.angles[0];
			} else if ( a < -16000 ) {
				dcFree.delta[0] = -16000 - cmd.angles[0];
			}
			a = (short)( cmd.angles[0] + dcFree.delta[0] );
		}
		dcFree.ang[i] = a * ( 360.0f / 65536.0f );
	}
	AnglesToAxis( dcFree.ang, axis );
	trap_Cvar_VariableStringBuffer( "df_demos_CameraTransitionSpeed", buf, sizeof( buf ) );
	f = ( fabs( atof( buf ) ) / 100.0f ) * dt * 5.0f;
	VectorMA( dcFree.pos, f * cmd.forwardmove, axis[0], dcFree.pos );
	VectorMA( dcFree.pos, f * -cmd.rightmove, axis[1], dcFree.pos );
	dcFree.pos[2] += f * cmd.upmove;
}

// the engine proper (0x1549d for a camera object); speed < 0 is distance based
// 0x1469c: the demo camera modes that show the player: the third-person view is forced (decided before the view is built, from the last frame's
// tracked client)
static int dcLastClient = -1;

void CG_DF_DemoCamState( vec3_t pos, vec3_t ang ) {		// CHS items 320..324: the demo camera object's current pose (2790364 / 2790376)
	VectorCopy( dcDemoCam.posCur, pos );
	VectorCopy( dcDemoCam.angCur, ang );
}

qboolean CG_DF_DemoCamThirdPerson( void ) {
	static const char third[12] = { 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1 };
	int mode = df_demos_CameraMode.integer;

	if ( !cg.demoPlayback || mode == 0 ) {
		return qfalse;
	}
	if ( dcLastClient != cg.clientNum ) {
		return qtrue;
	}
	return mode >= 0 && mode <= 11 && third[mode];
}

static void DC_Run( int mode, float speed ) {
	dcTrack_t	t;
	vec3_t		pos, ang, dir;

	if ( dc.mode != mode ) {
		dc.mode = mode;
		dc.init = 0;
		VectorCopy( cg.refdef.vieworg, dc.posCur );
		VectorCopy( cg.refdefViewAngles, dc.angCur );
		VectorCopy( dc.posCur, dc.posB );
		VectorCopy( dc.angCur, dc.angB );
		dc.lastTime = cg.time;
	}
	dc.dt = ( cg.time - dc.lastTime ) * 0.001f;		// 0x8e4a
	dc.lastTime = cg.time;
	dc.speed = speed;
	DC_Track( &t );
	if ( dcCur == &dcDemoCam ) {
		dcLastClient = t.client;
	}
	if ( mode >= 1 && mode <= 3 ) {		// 0x152a8 / 0x152e9 / 0x1532a
		vec3_t p, a;

		DC_FreeUpdate( dc.dt );
		if ( mode == 1 ) {
			VectorCopy( dcFree.pos, p );
		} else if ( mode == 2 ) {
			VectorCopy( t.pos, p );
		} else {
			VectorCopy( t.pos2, p );
		}
		VectorCopy( dcFree.ang, a );
		if ( mode == 3 ) {
			DC_ThirdPerson( p, a, 0, cg_thirdPersonRange.value );
		}
		VectorCopy( p, dc.posOut );
		VectorCopy( a, dc.angOut );
		CG_DF_CamOffsets( dc.posOut, dc.angOut );
		DC_Apply( dc.posOut, dc.angOut );
		return;
	}
	if ( mode == 8 ) {		// 0x1539f: the tracked client's own view
		VectorCopy( t.pos, dc.posOut );
		VectorCopy( t.ang, dc.angOut );
		CG_DF_CamOffsets( dc.posOut, dc.angOut );
		DC_Apply( dc.posOut, dc.angOut );
		if ( t.client != cg.predictedPlayerState.clientNum ) {
			CG_DF_TrackedViewGun( t.weapon );		// 0x15280
		}
		return;
	}
	if ( !DC_Target( &t, pos, ang ) ) {
		DC_Apply( dc.posOut, dc.angOut );
		return;
	}
	DC_Transition();
	if ( mode == 9 || mode == 10 ) {		// 0x1471c: keep the moving camera out of walls
		trace_t	tr;
		vec3_t	mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };

		CG_Trace( &tr, dc.posB, mins, maxs, dc.posCur, cg.clientNum, MASK_SOLID );
		if ( tr.fraction != 1.0f ) {
			VectorCopy( tr.endpos, dc.posCur );
		}
	}
	VectorSubtract( t.pos2, dc.posCur, dir );
	VectorNormalize( dir );
	DC_Aim( dir, dc.angCur, dc.angOut );
	VectorCopy( dc.posCur, dc.posOut );
	CG_DF_CamOffsets( dc.posOut, dc.angOut );
	DC_Apply( dc.posOut, dc.angOut );
}

// replay camera modes 9..11 (cgame 0x15550): the same engine with its own camera object; mode 9 always uses speed 200
void CG_DF_ReplayFreeCamera( int mode, float speed ) {
	dcCur = &dcReplayCam;
	DC_Run( mode, mode == 9 ? 200.0f : speed );
}

void CG_DF_DemoCamera( void ) {		// 0x1549d (after CG_CalcViewValues)
	int			mode = df_demos_CameraMode.integer;
	char		buf[32];

	if ( mode < 0 || mode > 11 ) {		// 0x14ed5
		trap_Cvar_Set( "df_demos_CameraMode", va( "%i", mode < 0 ? 0 : 11 ) );
		return;
	}
	if ( df_mp_TrackPlayerNum.integer < -1 || df_mp_TrackPlayerNum.integer > 63 ) {
		trap_Cvar_Set( "df_mp_TrackPlayerNum", va( "%i", df_mp_TrackPlayerNum.integer < -1 ? -1 : 63 ) );
	}
	dcCur = &dcDemoCam;
	if ( !cg.demoPlayback || mode < 1 ) {
		dc.mode = 0;
		dcFree.init = qfalse;
		return;
	}
	trap_Cvar_VariableStringBuffer( "df_demos_CameraTransitionSpeed", buf, sizeof( buf ) );
	DC_Run( mode, atof( buf ) );
}
