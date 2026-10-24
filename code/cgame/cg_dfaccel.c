// cg_dfaccel.c -- DeFRaG acceleration meter (df_accel_*): a second crosshair / glyph that slides sideways to show how far the
// player's wish direction is from the optimal strafe angle.  Port of cgame functions 0x15c24 (frame driver), 0x15a60 (simulation),
// 0x15961 (angles), 0x15ff4/0x15f83/0x8f3a (draw gate), 0x15e45/0x15d20 (draw).  See disasm/CGAME/accel_meter.md.
// Not ported: the SPEED:/PERCENT: percentage state and log (0x1584c/0x157f8, df_accel_Output), the DeFRaG ghost players (0x13e70) and
// the jump history logger (0x16513).

#include "cg_local.h"

vmCvar_t	df_accel_Draw;
vmCvar_t	df_accel_Mode;
vmCvar_t	df_accel_OffsetX;
vmCvar_t	df_accel_OffsetY;
vmCvar_t	df_accel_FontSize;
vmCvar_t	df_accel_Scale;
vmCvar_t	df_accel_AdjustMaxSpeed;
vmCvar_t	df_accel_Output;
vmCvar_t	df_chs_UsePredicted;

// state at 0x2a9584..0x2a9730
static int		acHave;				// 0x23fc: airborne tracking started
static float	acAngleAcc;			// 0x2a9584 accumulated rotation of the history vector (degrees)
static int		acLastTime;			// 0x2a95c8
static float	acDt;				// 0x2a95f0
static vec3_t	acStartPos;			// 0x2a9728
static vec3_t	acStartVel;			// 0x2a9734
static vec3_t	acCurVel;			// 0x2a9740
static vec3_t	acHist;				// 0x2a974c
static float	acAngleR, acAngleL;	// 0x2a971c / 0x2a9720 (the two offsets, degrees)
static vec3_t	acFwd, acRight;		// 0x2a95cc / 0x2a95d8

// 0x85c0: key mask of the viewed player
int CG_DF_Keys( int mask ) {
	if ( df_chs_UsePredicted.integer ) {
		return cg.predictedPlayerState.stats[13] & mask;
	}
	return cg.snap->ps.stats[13] & mask;
}

// 0x155d2: movement part of a usercmd (from the key mask when watching demos / other players)
static void CG_DF_ReadCmd( usercmd_t *cmd ) {
	if ( cg.demoPlayback || ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		cmd->forwardmove = cmd->rightmove = cmd->upmove = 0;
		if ( CG_DF_Keys( 1 ) ) {
			cmd->forwardmove = 127;
		}
		if ( CG_DF_Keys( 2 ) ) {
			cmd->forwardmove = -127;
		}
		if ( CG_DF_Keys( 8 ) ) {
			cmd->rightmove = -127;
		}
		if ( CG_DF_Keys( 16 ) ) {
			cmd->rightmove = 127;
		}
	} else {
		trap_GetUserCmd( trap_GetCurrentCmdNumber(), cmd );
	}
}

// 0x15658: PM_CmdScale
static float CG_DF_CmdScale( const usercmd_t *cmd, int speed ) {
	int		max;
	float	total, t;

	max = abs( cmd->forwardmove );
	if ( abs( cmd->rightmove ) > max ) {
		max = abs( cmd->rightmove );
	}
	if ( abs( cmd->upmove ) > max ) {
		max = abs( cmd->upmove );
	}
	if ( !max ) {
		return 0;
	}
	total = sqrt( cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove );
	t = (float)speed * max / ( 127.0f * total );
	return t;
}

// 0x15774: wish speed used by the simulation step.  Re-reads the usercmd with upmove forced to 0 and reuses the (normalised, z = 0)
// forward/right vectors left over from the last CG_DF_WishVel call, so it differs from len * scale of that call while jump/crouch is held.
static float CG_DF_StepWishSpeed( void ) {
	usercmd_t	cmd;
	vec3_t		w;
	float		scale;
	int			i;

	memset( &cmd, 0, sizeof( cmd ) );
	CG_DF_ReadCmd( &cmd );
	cmd.upmove = 0;
	scale = CG_DF_CmdScale( &cmd, cg.snap->ps.speed );
	for ( i = 0; i < 2; i++ ) {
		w[i] = acFwd[i] * cmd.forwardmove + acRight[i] * cmd.rightmove;
	}
	w[2] = 0;
	return VectorLength( w ) * scale;
}

// 0x156f7: wish velocity (unnormalised, z = 0) for the given view angles; returns the command scale
static float CG_DF_WishVel( vec3_t out, const vec3_t angles ) {
	usercmd_t	cmd;
	vec3_t		up;
	float		scale;
	int			i;

	memset( &cmd, 0, sizeof( cmd ) );
	CG_DF_ReadCmd( &cmd );
	scale = CG_DF_CmdScale( &cmd, cg.snap->ps.speed );
	AngleVectors( angles, acFwd, acRight, up );
	acFwd[2] = 0;
	acRight[2] = 0;
	VectorNormalize( acFwd );
	VectorNormalize( acRight );
	for ( i = 0; i < 2; i++ ) {
		out[i] = acFwd[i] * cmd.forwardmove + acRight[i] * cmd.rightmove;
	}
	out[2] = 0;
	return scale;
}

// 0x1558d: is there ground within 5 units below pos?
static qboolean CG_DF_GroundBelow( const vec3_t pos ) {
	static const vec3_t mins = { -15, -15, -24 }, maxs = { 15, 15, 32 };
	trace_t	tr;
	vec3_t	end;

	VectorCopy( pos, end );
	end[2] -= 5;
	trap_CM_BoxTrace( &tr, pos, end, mins, maxs, 0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY );
	return tr.fraction < 1.0f;
}

// 0x15579: azimuth in [0,360) from the x-component angle and the sign of y
static float CG_DF_Azimuth( const vec3_t v, float ang ) {
	return v[1] < 0 ? 360.0f - ang : ang;
}

static float CG_DF_AcosDeg( float c ) {
	return 180.0f * acos( c ) / M_PI;
}

// 0x15961: offsets between the wish direction and the predicted acceleration direction
static void CG_DF_Angles( const vec3_t accelDir, const vec3_t wishDir, float turned ) {
	vec3_t	a, b, c;
	float	aa, ab, ac, w, p;

	VectorCopy( acStartVel, a );
	VectorCopy( accelDir, b );
	VectorCopy( wishDir, c );
	VectorNormalize( a );
	VectorNormalize( b );
	VectorNormalize( c );
	aa = CG_DF_Azimuth( a, CG_DF_AcosDeg( a[0] ) );
	ab = CG_DF_Azimuth( b, CG_DF_AcosDeg( b[0] ) );
	ac = CG_DF_Azimuth( c, CG_DF_AcosDeg( c[0] ) );

	w = ac;
	p = ( aa + ab ) + turned;
	if ( p >= 360.0f ) {
		p -= 360.0f;
	}
	if ( fabs( w - p ) > 180.0f ) {
		if ( w >= 180.0f ) w -= 360.0f;
		if ( p >= 180.0f ) p -= 360.0f;
	}
	acAngleL = w - p;

	w = ac;
	p = ( aa - ab ) - turned;
	if ( p >= 360.0f ) {
		p -= 360.0f;
	}
	if ( fabs( p - w ) > 180.0f ) {
		if ( w >= 180.0f ) w -= 360.0f;
		if ( p >= 180.0f ) p -= 360.0f;
	}
	acAngleR = p - w;
}


// ---- percentage state (0x1584c / 0x157f8): feeds the AM items of the CHS panels; the SPEED:/PERCENT: log (df_accel_Output) is not ported ----
dfAmState_t	dfAm;
static int	acStartTime;		// 0x2a9724
static int	acHaveH;			// 0x2a9718

// 0x157f8: best possible speed after `ms` of optimal strafing (assumes g_speed 320 like DeFRaG)
static float CG_DF_IdealSpeed( float start, float ms ) {
	float w = 320.0f, pm = (float)pmove_msec.integer, dt = 0.001f * pm, a, b;

	if ( pm == 0 || ms == 0 ) {
		return start;
	}
	a = w * dt;
	b = a * ( 2.0f * w - a );
	return sqrt( start * start + b * ( ms / pm ) );
}

static void CG_DF_AccelPercent( qboolean latch ) {
	float start = VectorLength( acStartVel ), cur = VectorLength( acCurVel ), hist = VectorLength( acHist );
	float ideal = CG_DF_IdealSpeed( start, (float)( cg.time - acStartTime ) ), pa = 0, ps = 0;

	if ( !df_accel_AdjustMaxSpeed.integer ) {
		hist = ideal;
	}
	if ( hist - start != 0 ) {
		pa = 100.0f * ( ( cur - start ) / ( hist - start ) );
	}
	if ( hist != 0 ) {
		ps = 100.0f * ( cur / hist );
	}
	if ( latch ) {
		dfAm.cur = cur; dfAm.max = hist; dfAm.trueMax = ideal; dfAm.accelPct = pa; dfAm.speedPct = ps;
		if ( !acHaveH ) {
			dfAm.hCur = cur; dfAm.hMax = hist; dfAm.hAccel = pa; dfAm.hSpeed = ps;
			acHaveH = 1;
		}
	} else {
		dfAm.hCur = cur; dfAm.hMax = hist; dfAm.hAccel = pa; dfAm.hSpeed = ps;
	}

	// df_accel_Output (0x15903): print the latched measurement to the console
	if ( df_accel_Output.integer && latch ) {
		int lc = dfChs[2].labelColor.integer, fc = dfChs[2].fontColor.integer;

		CG_Printf( "\n" );
		CG_Printf( "SPEED:   ^%istart ^7= ^%i%8.3f ^%ispeed ^7= ^%i%8.3f ^%imax ^7= ^%i%8.3f\n",
			lc, fc, start, lc, fc, cur, lc, fc, hist );
		CG_Printf( "PERCENT: ^%iaccel ^7= ^%i%8.3f ^%ispeed ^7= ^%i%8.3f\n",
			lc, fc, pa, lc, fc, ps );
	}
}

// 0x15a60: one simulation step
static void CG_DF_AccelStep( const vec3_t pos, const vec3_t vel, const vec3_t wishDir, float wishSpeed, float mult, qboolean grounded ) {
	vec3_t	h, dir;
	float	wish, add, cosv, ratio, theta, speed, m;

	if ( grounded ) {
		acAngleR = acAngleL = 0;
		if ( acHave ) {
			CG_DF_AccelPercent( qtrue );
		}
		acHave = 0;
		acLastTime = cg.time;
		return;
	}
	if ( !acHave ) {
		acStartPos[0] = pos[0]; acStartPos[1] = pos[1]; acStartPos[2] = pos[2];
		VectorCopy( vel, acStartVel );
		acStartVel[2] = 0;
		VectorSet( acHist, VectorLength( acStartVel ), 0, 0 );
		acAngleAcc = 0;
		acStartTime = cg.time;
		acHave = 1;
	}
	VectorCopy( vel, acCurVel );
	acCurVel[2] = 0;

	VectorCopy( acHist, h );
	VectorNormalize( h );
	acAngleAcc += CG_DF_AcosDeg( h[0] );

	(void)wishSpeed;		// the original ignores its wishSpeed argument and calls 0x15774 here
	wish = CG_DF_StepWishSpeed();
	if ( wish == 0 && ( cg.demoPlayback ) ) {
		wish = cg.snap->ps.speed;
	}
	acDt = 0.001f * ( cg.time - acLastTime );
	acLastTime = cg.time;

	add = ( mult * acDt ) * wish;
	m = VectorLength( acHist );
	ratio = m != 0 ? ( wish - add ) / m : 0;
	theta = 0;
	if ( fabs( ratio ) <= 1.0f ) {
		theta = acos( ratio );
	}
	cosv = cos( theta );
	speed = VectorLength( acHist );
	if ( df_accel_AdjustMaxSpeed.integer ) {
		speed += ( ( speed - 750.0f ) / 25.0f ) * acDt;
	}
	acHist[0] = speed;
	acHist[1] = 0;
	acHist[2] = 0;
	acHist[0] += add * cosv;
	acHist[1] += add * sin( theta );
	acHist[2] = 0;
	dir[0] = cosv;
	dir[1] = sin( theta );
	dir[2] = 0;
	CG_DF_Angles( dir, wishDir, acAngleAcc );
}

// 0x15c24: frame driver
// ---- jump history logger (0x16513 and 0x1648f/0x164a9/0x164bc/0x16382/0x162c2/0x164d6/0x1621a, record at 0x2a95f8) ----
// Feeds CHS items 140-143 (last 12 jumps: speed / accel% / distance / height) and 145-148 (maxima).
typedef struct {
	vec3_t	pos;			// +0   last on-ground position
	vec3_t	vel;			// +12  last on-ground velocity
	int		time;			// +24  time of that record (0 = none)
	int		onGround;		// +28  previous frame: ground within 5 units
	int		resetFlag;		// +32  history is cleared at the next take-off
	int		teleport;		// +36  previous cg.nextFrameTeleport
	int		idx;			// +40  ring cursor
	float	maxDist;		// +44
	float	maxHeight;		// +48
	float	maxSpeed;		// +52
	float	maxAccel;		// +56
	float	ring[4][12];	// +60 distance, +108 height, +156 speed, +204 accel%
} dfJumpLog_t;

static dfJumpLog_t	jlog;

vmCvar_t	df_accel_HistoryResetSpeed;

static float CG_DF_HLen( float x, float y ) {
	return sqrt( x * x + y * y );
}

static float CG_DF_FMax( float a, float b ) {
	return a > b ? a : b;
}

static qboolean CG_DF_JumpSpectating( void ) {
	return cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR;
}

// 0x16513
static void CG_DF_JumpHistory( const vec3_t pos, const vec3_t vel ) {
	qboolean	ground = CG_DF_GroundBelow( pos );
	int			teleport = cg.nextFrameTeleport;
	int			idx, i;
	float		dx, dy, dz, d;

	if ( jlog.onGround && !ground ) {
		// 0x1648f: take-off; drop the history if the last ground contact was slow
		if ( jlog.resetFlag ) {
			memset( jlog.ring, 0, sizeof( jlog.ring ) );
			jlog.idx = 0;
			jlog.resetFlag = 0;
		}
	}
	if ( !jlog.onGround && ground ) {
		// 0x16382: landing; log distance, speed, accel%
		if ( jlog.time ) {
			jlog.idx = ( jlog.idx + 1 ) % 12;
			idx = jlog.idx;
			d = CG_DF_HLen( pos[0] - jlog.pos[0], pos[1] - jlog.pos[1] );
			jlog.ring[0][idx] = d;
			if ( !CG_DF_JumpSpectating() ) {
				jlog.maxDist = CG_DF_FMax( jlog.maxDist, d );
			}
			d = CG_DF_HLen( vel[0], vel[1] );
			jlog.ring[2][idx] = d;
			if ( !CG_DF_JumpSpectating() ) {
				jlog.maxSpeed = CG_DF_FMax( jlog.maxSpeed, d );
			}
			jlog.ring[3][idx] = dfAm.accelPct;
			if ( !CG_DF_JumpSpectating() ) {
				jlog.maxAccel = CG_DF_FMax( jlog.maxAccel, jlog.ring[3][idx] );
			}
		}
	}
	if ( !jlog.onGround && !ground ) {
		// 0x162c2: airborne; track the height of the current jump and the maxima
		if ( jlog.time && !jlog.teleport ) {
			idx = jlog.idx;
			dx = pos[0] - jlog.pos[0];
			dy = pos[1] - jlog.pos[1];
			dz = pos[2] - jlog.pos[2];
			jlog.ring[1][idx] = CG_DF_FMax( dz, jlog.ring[1][idx] );
			if ( !CG_DF_JumpSpectating() ) {
				jlog.maxHeight = CG_DF_FMax( dz, jlog.maxHeight );
				jlog.maxDist = CG_DF_FMax( CG_DF_HLen( dx, dy ), jlog.maxDist );
				jlog.maxSpeed = CG_DF_FMax( CG_DF_HLen( vel[0], vel[1] ), jlog.maxSpeed );
			}
		}
	}
	if ( ground || ( !teleport && jlog.teleport ) ) {
		// 0x164d6: remember the last ground state
		VectorCopy( pos, jlog.pos );
		VectorCopy( vel, jlog.vel );
		jlog.time = cg.time;
		if ( !jlog.resetFlag ) {
			jlog.resetFlag = CG_DF_HLen( vel[0], vel[1] ) < (float)df_accel_HistoryResetSpeed.integer;
		}
	}
	jlog.onGround = ground;
	jlog.teleport = teleport;
	(void)i;
}

// 0x1601c: CHS items 140-143; type 0 distance, 1 height, 2 speed, 3 accel%; oldest first, zero entries skipped
void CG_DF_JumpHistoryText( int type, char *out, int size ) {
	int		i, idx;
	float	v;
	char	tmp[256];

	out[0] = 0;
	if ( type < 0 || type > 3 ) {
		return;
	}
	for ( i = 0; i < 12; i++ ) {
		idx = ( i + jlog.idx + 1 ) % 12;
		v = jlog.ring[type][idx];
		if ( v != 0.0f ) {
			Com_sprintf( tmp, sizeof( tmp ), "%s %4i", out, (int)v );
			Q_strncpyz( out, tmp, size );
		}
	}
}

// 0x16282 / 0x1626c: resetjumpmeter clears the whole 284-byte logger record (rings, maxima, last ground state)
void CG_DF_JumpLogReset( void ) {
	Com_Memset( &jlog, 0, sizeof( jlog ) );
}

// CHS items 145-148: 0 = max speed, 1 = max accel, 2 = max distance, 3 = max height
float CG_DF_JumpHistoryMax( int which ) {
	switch ( which ) {
	case 0: return jlog.maxSpeed;
	case 1: return jlog.maxAccel;
	case 2: return jlog.maxDist;
	default: return jlog.maxHeight;
	}
}

void CG_DF_AccelFrame( void ) {
	vec3_t		pos, vel, ang, wish, ground;
	float		scale, len;
	qboolean	grounded;

	if ( !cg.snap ) {
		return;
	}
	if ( !df_chs_UsePredicted.integer ) {
		VectorCopy( cg.snap->ps.origin, pos );
		VectorCopy( cg.snap->ps.velocity, vel );
		VectorCopy( cg.snap->ps.viewangles, ang );
	} else {
		VectorCopy( cg.predictedPlayerState.origin, pos );
		VectorCopy( cg.predictedPlayerState.velocity, vel );
		VectorCopy( cg.predictedPlayerState.viewangles, ang );
	}
	grounded = CG_DF_GroundBelow( pos );
	(void)ground;
	scale = CG_DF_WishVel( wish, ang );
	len = VectorNormalize( wish );
	CG_DF_AccelStep( pos, vel, wish, len * scale, 1.0f, grounded );
	if ( pos[2] < acStartPos[2] && !acHaveH && !grounded ) {
		CG_DF_AccelPercent( qfalse );
		acHaveH = 1;
	}
	if ( grounded ) {
		acHaveH = 0;
	}
	dfAm.startSpeed = VectorLength( acStartVel );
	dfAm.angleR = acAngleR;
	dfAm.angleL = acAngleL;
	CG_DF_JumpHistory( pos, vel );		// 0x15ce7
}

// 0x8e9c: HUD elements allowed right now?
static qboolean CG_DF_HudAllowed( void ) {
	if ( cg.levelShot || !cg_draw2D.integer ) {
		return qfalse;
	}
	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		return qfalse;
	}
	if ( cg.showScores || cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cg.renderingThirdPerson ) {
		return qfalse;
	}
	return qtrue;
}

// 0x8f3a / 0x8ee3: df_accel_Draw selects when the meter is shown
qboolean CG_DF_DrawAllowed( int mode ) {
	if ( mode != 4 && !CG_DF_HudAllowed() ) {
		return qfalse;
	}
	switch ( mode ) {
	case 1:
	case 4:
		return qtrue;
	case 2:
		return ( cg.demoPlayback || cg.clientNum != cg.snap->ps.clientNum ) ? qtrue : qfalse;
	case 3:
		return ( !cg.demoPlayback && cg.clientNum == cg.snap->ps.clientNum ) ? qtrue : qfalse;
	}
	return qfalse;
}

// 0x15f83: draw only when there is an offset to show and (outside demos) a movement key is held
static qboolean CG_DF_AccelHasData( void ) {
	vec3_t	wish;

	if ( acAngleR == 0 && acAngleL == 0 ) {
		return qfalse;
	}
	if ( cg.demoPlayback ) {
		return qtrue;
	}
	CG_DF_WishVel( wish, cg.snap->ps.viewangles );
	return VectorLength( wish ) != 0 ? qtrue : qfalse;
}

// 0x15bd7: pixels per degree; a negative df_accel_Scale scales with speed / 600
static float CG_DF_AccelScale( void ) {
	float s = df_accel_Scale.value, a = fabs( s ), r = a, speed = VectorLength( acCurVel );

	if ( speed != 0 && s < 0 ) {
		r = a * ( speed / 600.0f );
	}
	return r < a ? a : r;
}

// 0x15ceb: x shifted by the smaller of the two angle offsets
static int CG_DF_AccelShift( int x, float scale ) {
	float v = abs( (int)acAngleR ) < abs( (int)acAngleL ) ? -acAngleR : acAngleL;

	return x + (int)( v * scale );
}

static void CG_DF_DrawCrosshairAt( float x, float y, float size, qhandle_t shader ) {
	float w = size, h = size;

	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x + cg.refdef.x + 0.5f * ( cg.refdef.width - w ), y + cg.refdef.y + 0.5f * ( cg.refdef.height - h ),
	                       w, h, 0, 0, 1, 1, shader );
}

void CG_DF_DrawAccel( void ) {
	int		style;
	float	x, y, size;
	vec4_t	col;

	if ( !cg.snap || !df_accel_Draw.integer ) {
		return;
	}
	if ( !CG_DF_DrawAllowed( df_accel_Draw.integer ) || !CG_DF_AccelHasData() ) {
		return;
	}
	if ( df_accel_Mode.integer == 0 ) {
		// 0x15e45
		qhandle_t shader;

		style = cg_drawCrosshair.integer < 0 ? 0 : cg_drawCrosshair.integer;
		shader = cgs.media.crosshairShader[style % NUM_CROSSHAIRS];
		size = cg_crosshairSize.value;
		if ( df_accel_OffsetX.integer || df_accel_OffsetY.integer ) {
			CG_DF_DrawCrosshairAt( cg_crosshairX.integer + df_accel_OffsetX.integer, cg_crosshairY.integer + df_accel_OffsetY.integer, size, shader );
		}
		x = CG_DF_AccelShift( cg_crosshairX.integer + df_accel_OffsetX.integer, CG_DF_AccelScale() );
		y = cg_crosshairY.integer + df_accel_OffsetY.integer;
		CG_DF_DrawCrosshairAt( x, y, size, shader );
		return;
	}
	// 0x15d20: glyph modes
	{
		int		fs = df_accel_FontSize.integer, cx, cy, idx;
		float	fx, fy;

		fx = cg_crosshairX.integer + ( df_accel_OffsetX.integer - fs / 2 );
		fy = cg_crosshairY.integer + ( df_accel_OffsetY.integer - fs / 2 );
		cx = CG_DF_AccelShift( (int)fx, CG_DF_AccelScale() );

		idx = ( dfChs[2].labelColor.integer & 15 ) % 10;
		VectorCopy( chsPalette[idx], col );
		col[3] = dfChs[2].fontTransparency.value;
		trap_R_SetColor( col );
		if ( df_accel_Mode.integer == 1 ) {
			CG_DrawStringExt( cx + 320, (int)fy + 240, "O", col, qfalse, dfChs[2].fontShadow.integer, fs, fs, 1 );
		}
		if ( df_accel_Mode.integer == 2 ) {
			CG_DrawStringExt( cx + 320, (int)fy + 240, "+", col, qfalse, dfChs[2].fontShadow.integer, fs, fs, 1 );
		}
		idx = ( dfChs[2].fontColor.integer & 15 ) % 10;
		VectorCopy( chsPalette[idx], col );
		col[3] = dfChs[2].fontTransparency.value;
		trap_R_SetColor( col );
		CG_DrawStringExt( (int)fx + 320, (int)fy + 240, "+", col, qfalse, dfChs[2].fontShadow.integer, fs, fs, 1 );
		trap_R_SetColor( NULL );
	}
}
