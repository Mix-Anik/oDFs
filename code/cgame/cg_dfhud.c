// cg_dfhud.c -- the DeFRaG HUD: widget draw and status widgets
//
// Ported from the DeFRaG cgame (disasm/CGAME/hud_widget_system.md): widget draw 0x259a, HUD init 0x2fdd, HUD frame
// 0x2a12 (best/last time, speed, power-up timers, health, armor, ammo, frags, checkpoint line), health glow 0x235d,
// power-up list 0x225b/0x22ea, colour lookup 0x2559, colour parser 0x24ac, speed text 0x41b6.
// The df_hud/* images come from the DeFRaG client pk3. Enabled with df_hud 1 (default 0 in oDFs).
//
// Not ported yet: flag-carrier box (CTF), holdable box, jump meters, spectator-follow name, the overlay helpers.
#include "cg_local.h"

vmCvar_t	df_hud;

// sv_cheats as the SERVER reports it (cgame global 0x11d40c, parsed from the systeminfo configstring by 0x3a70). The client-side cvar is not
// used: while a demo plays it does not follow the recorded server (it read 1, which made the race time decoder take the plain path and
// show garbage 'billions' in recorded demos).
qboolean CG_DF_Cheats( void ) {
	return atoi( Info_ValueForKey( CG_ConfigString( CS_SYSTEMINFO ), "sv_cheats" ) ) != 0 ? qtrue : qfalse;
}

// DeFRaG (cgame 0x1cd86 -> 0x2a12): the DeFRaG HUD is used when df_hud > 1, or df_hud == 1 on a server that publishes defrag_vers
// (global 0x11d298); otherwise the vanilla Draw2D runs. df_hud defaults to 1, so a DeFRaG/oDFs server gets the DeFRaG HUD and no
// vanilla weapon-select bar / status bar.
qboolean CG_DF_HudOn( void ) {
	if ( df_hud.integer > 1 ) {
		return qtrue;
	}
	return ( df_hud.integer == 1 && CG_DF_Serverinfo( "defrag_vers" )[0] ) ? qtrue : qfalse;
}
vmCvar_t	df_hud_drawStatus;
vmCvar_t	df_hud_drawSpectFollow;	// 0x11a9a0
int			dfFollowStamp;			// 0x11ccec, cleared when the followed client changes
vmCvar_t	df_hud_color;
vmCvar_t	df_hud_colorRGB;
vmCvar_t	df_hud_fontcolor;
vmCvar_t	df_hud_fontcolorRGB;
vmCvar_t	df_hud_fontshadow;
vmCvar_t	df_hud_transparency;
vmCvar_t	df_hud_frameTransparency;
vmCvar_t	df_hud_colorwarning;
vmCvar_t	df_hud_forceteamcolors;
vmCvar_t	df_drawBesttime;
vmCvar_t	df_drawSpeed;

enum {
	SH_ICON_NULL, SH_ICON_HEALTH, SH_HALO_RED, SH_HALO_BLUE, SH_ICON_ARMOR, SH_ICON_FRAGS, SH_ICON_UNUSED,
	SH_ICON_QUAD, SH_ICON_BS, SH_ICON_HASTE, SH_ICON_INVIS, SH_ICON_REGEN, SH_ICON_FLIGHT, SH_ICON_TELEPORT, SH_ICON_MEDKIT,
	SH_164_B, SH_164_I, SH_126_B, SH_126_I, SH_116_B, SH_116_I, SH_94_B, SH_94_I, SH_78_B, SH_78_I, SH_36_B, SH_36_I,
	SH_COUNT
};

static const char *dfHudShaderNames[SH_COUNT] = {
	"df_hud/icon_null", "df_hud/icon_health", "df_hud/icon_health_haloRed", "df_hud/icon_health_haloBlue",
	"df_hud/icon_armor", "df_hud/icon_frags", NULL,
	"df_hud/icon_quad", "df_hud/icon_bs", "df_hud/icon_haste", "df_hud/icon_invis", "df_hud/icon_regen",
	"df_hud/icon_flight", "df_hud/icon_teleport", "df_hud/icon_medkit",
	"df_hud/164x32_border", "df_hud/164x32_inner", "df_hud/126x32_border", "df_hud/126x32_inner",
	"df_hud/116x32_border", "df_hud/116x32_inner", "df_hud/94x32_border", "df_hud/94x32_inner",
	"df_hud/78x32_border", "df_hud/78x32_inner", "df_hud/36x32_border", "df_hud/36x32_inner"
};

static qhandle_t	dfSh[SH_COUNT];
static qhandle_t	dfCkptSmallBorder[9], dfCkptSmallInner[9], dfCkptLargeBorder[9], dfCkptLargeInner[9];

typedef struct {
	int				x, y, w, h;
	int				id;			// +0x10 (power-up id)
	const char		*text;		// +0x14
	const int		*value;		// +0x18
	int				warn;		// +0x1c
	int				width;		// +0x20 field width in characters
	const qhandle_t	*icon;		// +0x24
	const qhandle_t	*border;	// +0x28
	const qhandle_t	*inner;		// +0x2c
	int				visible;	// +0x30
	int				mode;		// +0x34
	int				px, py;		// +0x44 / +0x48 position resolved by the last draw
} dfWidget_t;

static dfWidget_t	wBest, wLast, wSpeed, wPower[3], wFlag, wHoldable, wHealth, wArmor, wAmmo, wFrags, wCkpt;
vmCvar_t			df_hud_3DIcons;					// 0x11aef0: flag carrier icon as 3D model (1) or 16x16 sprite (0)
static int			powerList[3] = { -1, -1, -1 };	// 0x11cca8
static int			powerCount;						// 0x11cca4
int				dfLastRunMs;					// 0x11d284
static int			dfShownHealth, dfHealthNext;	// 0x11ccd4 / 0x11ccd8
static float		dfCustomColor[4], dfCustomFont[4];

static const vec4_t	dfPalette[13] = {
	{ 0, 0, 0, 1 }, { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 1, 1, 0, 1 }, { 0, 0, 1, 1 }, { 0, 1, 1, 1 }, { 1, 0, 1, 1 }, { 1, 1, 1, 1 },
	{ 0.25f, 0.25f, 0.25f, 1 }, { 1, 0.5f, 0, 1 }, { 0.5f, 0, 0.6f, 1 }, { 0, 0.4f, 0, 1 }, { 0.6f, 0.6f, 0.1f, 1 }
};

// ---- helpers --------------------------------------------------------------------------------------------------

// DeFRaG 0x24ac: up to three numbers of at most 3 digits each (value 0..255 -> 0..1, clamped); the character after every number --
// the delimiter, or the 4th digit of a longer run -- is skipped, so "150025001500" reads as 150 250 150. Returns qfalse unless three
// numbers were found (out[3] is set to 1 on success).
static qboolean CG_DF_ParseColor( const char *s, float *out ) {
	char	buf[4];
	int		i, lc, n = 0, len;
	float	v;

	if ( !*s ) {
		return qfalse;
	}
	len = strlen( s );
	for ( i = 0; i < len && n < 3; i++ ) {
		lc = 0;
		while ( *s >= '0' && *s <= '9' && lc < 3 ) {
			buf[lc++] = *s++;
		}
		if ( *s ) {
			s++;
		}
		if ( lc ) {
			buf[lc] = 0;
			v = atoi( buf ) / 255.0f;
			out[n++] = v > 1.0f ? 1.0f : v;
		}
	}
	if ( n != 3 ) {
		return qfalse;
	}
	out[3] = 1.0f;
	return qtrue;
}

// DeFRaG 0x2559
static const float *CG_DF_HudColor( int index, const float *custom ) {
	if ( custom && custom[0] >= 0 ) {		// 0x2559: a custom colour is in use unless its red component is negative (the 'unset' marker)
		return custom;
	}
	if ( index >= 0 && index < 13 ) {
		return dfPalette[index];
	}
	return colorWhite;
}

static void CG_DF_InitWidget( dfWidget_t *w, int x, int y, int width, int warn, int border, int inner ) {
	memset( w, 0, sizeof( *w ) );
	w->x = x;
	w->y = y;
	w->w = 128;
	w->h = 32;
	w->width = width;
	w->warn = warn;
	w->border = &dfSh[border];
	w->inner = &dfSh[inner];
}

// ---- init (0x2fdd) ----------------------------------------------------------------------------------------------

void CG_DF_HudInit( void ) {
	int i;

	for ( i = 0; i < SH_COUNT; i++ ) {
		dfSh[i] = dfHudShaderNames[i] ? trap_R_RegisterShaderNoMip( dfHudShaderNames[i] ) : 0;
	}
	for ( i = 0; i < 9; i++ ) {
		dfCkptSmallBorder[i] = trap_R_RegisterShaderNoMip( va( "df_hud/ckptbg_s_border_%i", i ) );
		dfCkptSmallInner[i] = trap_R_RegisterShaderNoMip( va( "df_hud/ckptbg_s_inner_%i", i ) );
		dfCkptLargeBorder[i] = trap_R_RegisterShaderNoMip( va( "df_hud/ckptbg_l_border_%i", i ) );
		dfCkptLargeInner[i] = trap_R_RegisterShaderNoMip( va( "df_hud/ckptbg_l_inner_%i", i ) );
	}

	CG_DF_InitWidget( &wBest, 5, 159, 6, 0, SH_78_B, SH_78_I );
	CG_DF_InitWidget( &wLast, 5, 201, 6, 0, SH_116_B, SH_116_I );
	CG_DF_InitWidget( &wSpeed, 5, 243, 6, 0, SH_116_B, SH_116_I );
	for ( i = 0; i < 3; i++ ) {
		CG_DF_InitWidget( &wPower[i], 5, 438, 2, 6, SH_78_B, SH_78_I );
	}
	CG_DF_InitWidget( &wFlag, 5, 438, 0, 0, SH_36_B, SH_36_I );		// W4 0x11cdec
	CG_DF_InitWidget( &wHoldable, 5, 438, 0, 0, SH_36_B, SH_36_I );		// W5 0x11ce40
	CG_DF_InitWidget( &wHealth, 93, 438, 3, 25, SH_94_B, SH_94_I );
	wHealth.icon = &dfSh[SH_ICON_NULL];
	wHealth.visible = 1;
	CG_DF_InitWidget( &wArmor, 199, 438, 3, 20, SH_94_B, SH_94_I );
	wArmor.icon = &dfSh[SH_ICON_ARMOR];
	CG_DF_InitWidget( &wAmmo, 305, 438, 3, 0, SH_94_B, SH_94_I );
	CG_DF_InitWidget( &wFrags, 504, 438, 5, 20, SH_126_B, SH_126_I );
	wFrags.icon = &dfSh[SH_ICON_FRAGS];
	CG_DF_InitWidget( &wCkpt, 498, 438, 0, 0, SH_94_B, SH_94_I );
	wCkpt.w = 256;
	wCkpt.border = wCkpt.inner = NULL;
	powerCount = 0;
	powerList[0] = powerList[1] = powerList[2] = -1;
}

void CG_DF_SetLastRunTime( int ms ) {
	dfLastRunMs = ms;
}

// ---- widget draw (0x259a) ----------------------------------------------------------------------------------------

static void CG_DF_DrawWidget( dfWidget_t *w ) {
	float		col[4];
	const float	*c;
	vec4_t		fontCol;
	int			x, y;

	if ( !df_hud_drawStatus.integer ) {
		return;
	}
	if ( !w->visible ) {
		w->mode = 0;
		return;
	}
	w->mode = 1;
	x = w->x;
	y = w->y;
	w->px = x;
	w->py = y;

	if ( w->value && df_hud_colorwarning.integer && *w->value < w->warn ) {
		c = ( df_hud_color.integer == 1 ) ? colorYellow : colorRed;		// warning colour (alpha 1)
		Vector4Copy( c, col );
	} else {
		Vector4Copy( CG_DF_HudColor( df_hud_color.integer, dfCustomColor ), col );
	}
	if ( w->inner && *w->inner ) {
		col[3] = df_hud_transparency.value;
		trap_R_SetColor( col );
		CG_DrawPic( x, y, w->w, w->h, *w->inner );
	}
	if ( w->border && *w->border ) {
		col[3] = df_hud_frameTransparency.value;
		trap_R_SetColor( col );
		CG_DrawPic( x, y, w->w, w->h, *w->border );
	}
	trap_R_SetColor( NULL );

	x += 10;
	y += ( w->h - 16 ) / 2;
	if ( w->icon && *w->icon ) {
		CG_DrawPic( x, y, 16, 16, *w->icon );
		x += 26;
	}
	Vector4Copy( CG_DF_HudColor( df_hud_fontcolor.integer, dfCustomFont ), fontCol );
	if ( w->text ) {
		CG_DrawStringExt( x + ( w->width - (int)strlen( w->text ) ) * 16, y, w->text, fontCol, qtrue, df_hud_fontshadow.integer, 16, 16, w->width );
	} else if ( w->value ) {
		CG_DrawStringExt( w->x + 36, y, va( w->width == 3 ? "%3i" : "%2i", *w->value ), fontCol, qtrue, df_hud_fontshadow.integer, 16, 16, w->width );
	}
}

// ---- power-up list (0x225b / 0x22ea) -------------------------------------------------------------------------------

static void CG_DF_AddPower( int id ) {
	int i, k, slot = -1;

	for ( i = 0; i < 3; i++ ) {
		if ( wPower[i].id == 0 ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		return;
	}
	for ( k = powerCount; k > 0; k-- ) {
		powerList[k] = powerList[k - 1];
		wPower[powerList[k]].y -= 42;
	}
	wPower[slot].y = 438;
	wPower[slot].id = id;
	wPower[slot].icon = &dfSh[SH_ICON_QUAD - 1 + id];		// 0x11d14c + id*4: quad(1) .. flight(6)
	powerList[0] = slot;
	powerCount++;
}

static void CG_DF_RemovePower( int k ) {
	int w = powerList[k], j;

	wPower[w].y = 438;
	wPower[w].id = 0;
	wPower[w].icon = NULL;
	for ( j = k; j < powerCount - 1; j++ ) {
		powerList[j] = powerList[j + 1];
		wPower[powerList[j]].y += 42;
	}
	powerCount--;
	powerList[powerCount] = -1;
}

// ---- health icon with glow (0x235d) ---------------------------------------------------------------------------------

static void CG_DF_DrawHealthIcon( int x, int y, const playerState_t *ps ) {
	vec4_t	color = { 0, 0, 0, 1 };
	int		d, disp;
	float	a;

	if ( !df_hud_drawStatus.integer ) {
		return;
	}
	// 0x2367: every 100 ms the shown value moves by (difference / 10), a difference below 10 counts as 10 (so it moves by exactly 1);
	// the catch-up loop is a plain `while ( next < cg.time )` that starts from 0, as in DeFRaG
	while ( dfHealthNext < cg.time ) {
		dfHealthNext += 100;
		d = ps->stats[STAT_HEALTH] - dfShownHealth;
		if ( d > 0 && d < 10 ) {
			d = 10;
		}
		if ( d < 0 && d > -10 ) {
			d = -10;
		}
		dfShownHealth += d / 10;
	}
	disp = dfShownHealth;

	if ( disp <= 50 ) {
		color[0] = 0.5f;
	} else if ( disp < 100 ) {
		color[0] = ( disp - 50 ) / 100.0f + 0.5f;
	} else if ( disp <= 200 ) {
		color[0] = 1.0f - ( disp - 100 ) / 100.0f;
		if ( color[0] < 0.5f ) {
			color[0] = 0.5f;
		}
		color[1] = ( disp - 100 ) / 100.0f;
		color[2] = ( disp - 100 ) / 50.0f;
		if ( color[2] > 1.0f ) {
			color[2] = 1.0f;
		}
	}
	trap_R_SetColor( color );
	CG_DrawPic( x, y, 16, 16, dfSh[SH_ICON_HEALTH] );
	if ( disp > 0 ) {
		// halos keep the icon's colour and only change the alpha: (disp-100)/100 below 100 (negative -> invisible), 1.0 from 100 up
		a = ( disp < 100 ) ? ( disp - 100 ) / 100.0f : 1.0f;
		color[3] = a < 0 ? 0 : a;
		trap_R_SetColor( color );
		CG_DrawPic( x, y, 16, 16, dfSh[SH_HALO_RED] );
		if ( disp > 100 ) {
			a = ( disp - 100 ) / 75.0f;
			color[3] = a > 1.0f ? 1.0f : a;
			trap_R_SetColor( color );
			CG_DrawPic( x, y, 16, 16, dfSh[SH_HALO_BLUE] );
		}
	}
	trap_R_SetColor( NULL );
}

// ---- speed text (0x41b6) ----------------------------------------------------------------------------------------------

const char *CG_DF_SpeedText( float speed ) {
	if ( df_drawSpeed.integer == 2 ) {
		if ( df_unit.integer == 1 ) {
			return va( "%3i k", (int)( speed * 20.0f / 320.0f ) );
		}
		if ( df_unit.integer == 2 ) {
			return va( "%3i m", (int)( speed * 20.0f / 320.0f * 0.625f ) );
		}
		return va( "%3i u", (int)speed );
	}
	if ( df_unit.integer == 1 ) {
		return va( "%3i k", (int)CG_ConvertDistance( 3.6f * speed ) );
	}
	if ( df_unit.integer == 2 ) {
		return va( "%3i m", (int)CG_ConvertDistance( 3600.0f * speed ) );
	}
	return va( "%3i u", (int)speed );
}

// ---- checkpoint line (end of 0x2a12) --------------------------------------------------------------------------------------

static void CG_DF_DrawCheckpointLine( dfWidget_t *w, int fragsShown ) {
	int		y0 = fragsShown ? 396 : 438;
	int		N = CG_DF_CheckpointCount();
	int		v, big = 0, summary;

	w->y = y0;
	if ( df_hud_drawStatus.integer && ( CG_DF_DefragFlags() & 2 ) && dfLastRunMs != 0 && df_drawCheckpointStats.integer ) {		// DeFRaG 0x2d8d..0x2da3 also hides it with sv_cheats; kept visible on purpose
		v = N > 7 ? 7 : N;
		w->visible = 1;
		w->y -= 16 * v;
		w->h = 16 * v + 32;
		summary = CG_DF_CheckpointSummary();
		big = summary > 0xea5f || summary < -0xea5f;
		w->border = big ? &dfCkptLargeBorder[v] : &dfCkptSmallBorder[v];
		w->inner = big ? &dfCkptLargeInner[v] : &dfCkptSmallInner[v];
		w->x = big ? 450 : 498;
	} else {
		w->visible = 0;
	}
	CG_DF_DrawWidget( w );
	if ( df_hud_drawStatus.integer && w->mode ) {
		CG_DF_DrawCheckpointListAt( w->px + ( big ? 170 : 120 ), y0 + 24 - 16 );
	}
}

// ---- spectator follow name (0x21d2) ----
// df_hud_drawSpectFollow 1: permanent "^4>> ^7name" while following; >1: shown for 3.2 s after each target change, fading in the last 200 ms
static void CG_DF_DrawFollowName( void ) {
	vec4_t		color;
	const char	*s;

	if ( !df_hud_drawSpectFollow.integer || !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		return;
	}
	color[0] = color[1] = color[2] = color[3] = 1.0f;
	if ( df_hud_drawSpectFollow.integer > 1 ) {
		if ( !dfFollowStamp ) {
			dfFollowStamp = cg.time + 3200;
		} else if ( cg.time > dfFollowStamp ) {
			return;
		}
		if ( dfFollowStamp - cg.time < 200 ) {
			color[3] = ( dfFollowStamp - cg.time ) / 200.0f;
		}
	}
	s = va( "^4>>^7 %s", cgs.clientinfo[cg.snap->ps.clientNum].name );
	CG_DrawStringExt( 320 - 10 * (int)strlen( s ) / 2, 400, s, color, qfalse, qtrue, 10, 16, 0 );
}

// ---- CGAZ (cgame 0x1a57a; see disasm/CGAME/cgaz.md) ---------------------------------------------------------------------
// df_hud_cgaz bit 1 = strafe-angle tape + accel bar, 2 = big speed readout, 4 = no accel bar, 8 = no tape/arrow, 16 = no optimal-angle bars
vmCvar_t	df_hud_cgaz;
vmCvar_t	df_hud_cgaz_opacity;
vmCvar_t	df_hud_cgaz_ypos;

static const vec4_t cgazWhite = { 1, 1, 1, 1 };		// data 0x40f4
// data 0x26c8: tape quadrant colours; 0x2728: optimal-angle bar colours; 0x2708: charset glyph texcoords of the velocity arrow
static const float cgazQuad[16] = { 1, 1, 0.5f, 0.5f,  0, 1, 0.5f, 0.5f,  0, 0, 0.5f, 0.5f,  1, 0, 0.5f, 0.5f };
static const float cgazOpt[32] = { 1, 1, 0, 1,  0, 0.25f, 0.25f, 1,  0, 1, 0, 1,  0.25f, 0.25f, 0.25f, 1,
                                   0.25f, 0.25f, 0.25f, 1,  0, 1, 0, 1,  0, 0.25f, 0.25f, 1,  1, 1, 0, 1 };
static const float cgazArrow[8] = { 0.4395f, 0.5f, 0.498f, 0.5586f,  0.373f, 0.5f, 0.4316f, 0.5586f };

// 0x1d161: float-coordinate coloured rectangle
static void CG_DF_FillRectF( float x, float y, float w, float h, const float *color ) {
	trap_R_SetColor( color );
	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_SetColor( NULL );
}

static float CG_DF_AngleDiff( float a, float b ) {		// 0x3124a: (a - b) wrapped to [-180, 180]
	float r = a - b;

	while ( r > 180.0f ) r -= 360.0f;
	while ( r < -180.0f ) r += 360.0f;
	return r;
}

static float CG_DF_Tan( float deg ) {					// 0x2f3f5
	float r = M_PI * deg / 180.0f;

	return sin( r ) / cos( r );
}

// angle (degrees) -> screen x offset from the centre, clamped to the field of view (0x1a869..0x1a890)
static float CG_DF_CgazX( float yawBase, float ang, float halfFov, float tanHalf ) {
	float a = CG_DF_AngleDiff( yawBase, ang );

	if ( fabs( a ) > halfFov ) {
		a = halfFov * ( a < 0 ? -1 : 1 );
	}
	return 320.0f * CG_DF_Tan( a ) / tanHalf;
}

static vec3_t	cgazPrevVel;		// 0x2b1ad4
static int		cgazPrevTime;		// 0x2b1ae4
static float	cgazAccel;			// 0x2b1ae0

static void CG_DF_DrawCGAZ( int flags, float opacity, int ypos ) {
	vec4_t	c2c, c40, c50;
	float	a, fov, halfFov, tanHalf, yaw, base;
	const playerState_t *pps, *ps;
	int		i;

	if ( !cg.snap || cg.hyperspace ) {
		return;
	}
	a = 2.0f * opacity;
	if ( a >= 1.0f ) {
		a = 1.0f;
	}
	Vector4Copy( cgazWhite, c2c ); c2c[3] = a;
	c40[0] = 0.5f; c40[1] = 0.5f; c40[2] = 1.0f; c40[3] = a;
	c50[0] = 1.0f; c50[1] = 0.5f; c50[2] = 0.0f; c50[3] = a;

	if ( flags & 2 ) {
		// big speed readout: integer part 32x48 at the lower right, ".fraction u/s" 24x36 beside it
		float spd = cg.xyspeed, fl = floor( spd );
		const char *s = va( "^%d%4.0f", 7, fl );
		const char *f;

		CG_DrawStringExt( 640 - CG_DrawStrlen( s ) * 32 - 5 * ( ( 3 * 32 ) / 4 ), 480 - (int)( 1.5f * ( 2.5f * 32 ) ),
		                  s, cgazWhite, qfalse, qtrue, 32, (int)( 1.5f * 32 ), 0 );
		f = va( "%0.01f^3u/s", spd - fl );
		CG_DrawStringExt( 640 - 5 * ( ( 3 * 32 ) / 4 ), 480 - (int)( 1.5f * ( 2.375f * 32 ) ),
		                  f + 1, cgazWhite, qfalse, qtrue, ( 3 * 32 ) / 4, ( 3 * (int)( 1.5f * 32 ) ) / 4, 0 );
	}
	if ( !( flags & 1 ) ) {
		return;
	}

	pps = &cg.predictedPlayerState;
	if ( !( flags & 4 ) ) {
		// acceleration bar: change of horizontal speed per ms between prediction steps
		vec3_t v;
		float  acc;
		vec4_t accCol = { 1, 0, 0, 0.8f };

		if ( !cg.demoPlayback && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) && !cg_nopredict.integer && !cg_synchronousClients.integer ) {
			ps = &cg.predictedPlayerState;
		} else {
			ps = cg.nextSnap ? &cg.nextSnap->ps : &cg.snap->ps;
		}
		accCol[3] *= opacity;
		if ( ps->commandTime <= cgazPrevTime ) {
			acc = cgazAccel;
		} else {
			VectorCopy( ps->velocity, v );
			v[2] = 0;
			acc = VectorLength( v ) - VectorLength( cgazPrevVel );
			acc /= (float)( ps->commandTime - cgazPrevTime );
			acc *= 256.0f;
			if ( ps->groundEntityNum != ENTITYNUM_NONE ) {
				acc *= 0.1f;
			}
			VectorCopy( v, cgazPrevVel );
			cgazPrevTime = ps->commandTime;
			cgazAccel = acc;
		}
		if ( acc > 0 ) {
			CG_DF_FillRectF( 320 + cg_crosshairSize.integer, 232, acc, 16, accCol );
		} else if ( acc < 0 ) {
			CG_DF_FillRectF( 320 - cg_crosshairSize.integer, 232, acc, 16, accCol );
		}
		CG_DF_FillRectF( 320 + cg_crosshairSize.integer + 81.92f, 236, 1.0f, 8.0f, c2c );
		CG_DF_FillRectF( 320 - cg_crosshairSize.integer - 81.92f, 236, -1.0f, 8.0f, c2c );
	}

	fov = cg.refdef.fov_x;
	halfFov = fov / 2.0f;
	tanHalf = CG_DF_Tan( halfFov );
	yaw = pps->viewangles[YAW];
	base = yaw;
	// 0x11d298 (set by 0x3c33 when defrag_vers or the legacy defrag_version key is non-empty), not the parsed number
	if ( ( pps->stats[13] & 27 ) != 0 || !( CG_DF_Serverinfo( "defrag_vers" )[0] || CG_DF_Serverinfo( "defrag_version" )[0] ) ) {
		base += 45.0f * pps->movementDir;
	}

	if ( !( flags & 8 ) ) {
		// tape: coloured 90-degree sectors around the view with ticks every 45 degrees
		float col[16];
		float ang;

		for ( i = 0; i < 16; i++ ) {
			col[i] = cgazQuad[i];
			if ( ( i & 3 ) == 3 ) {
				col[i] *= opacity;
			}
		}
		for ( ang = 0; ang < 360.0f; ang += 45.0f ) {
			float x1 = CG_DF_CgazX( yaw, ang, halfFov, tanHalf );
			float x2 = CG_DF_CgazX( yaw, ang + 90.0f, halfFov, tanHalf );
			int   q = (int)( ang / 90.0f );
			float rem = ang - 90.0f * (int)( ang / 90.0f );
			int   h;

			if ( x1 < x2 ) {
				continue;
			}
			if ( rem == 0 ) {
				CG_DF_FillRectF( x2 + 320, ypos + 16, x1 - x2, 16, &col[q * 4] );
			}
			h = (int)( 8.0f * ( 90.0f - rem ) / 90.0f );
			CG_DF_FillRectF( x1 + 320 - 1.0f, ( ypos + 16 + 8 + 8 ) - h, 2.0f, h, c2c );
		}
	}

	// velocity direction arrow + optimal strafe angle bars
	{
		float vx = pps->velocity[0], vy = pps->velocity[1];
		float speed, vyaw, accl, vlen, ang[9], aMin, aMax, aMid;
		vec4_t optCol[8];

		if ( fabs( vx ) + fabs( vy ) <= 0 ) {
			return;
		}
		speed = sqrt( vx * vx + vy * vy );
		vlen = speed;
		vyaw = 180.0f * atan2( vy, vx ) / M_PI;
		if ( !( flags & 8 ) ) {
			float e = CG_DF_AngleDiff( yaw, vyaw );		// 0x1a965 uses the raw view yaw (l7c), unlike the tape bars below which use base (lc0)
			int   flip = 0;
			float x, y, w, h;
			const float *tc;

			if ( e > 90.0f ) {
				flip = 1;
				e -= 180.0f;
			}
			if ( fabs( e ) > halfFov ) {
				e = halfFov * ( e < 0 ? -1 : 1 );
			}
			e = 320.0f * CG_DF_Tan( e ) / tanHalf;
			trap_R_SetColor( ( vx * vy ) != 0 ? c2c : c50 );
			x = e + 320.0f - 8.0f; y = ypos + 32; w = 16; h = 16;
			CG_AdjustFrom640( &x, &y, &w, &h );
			tc = &cgazArrow[flip * 4];
			trap_R_DrawStretchPic( x, y, w, h, tc[0], tc[1], tc[2], tc[3], cgs.media.charsetShader );
		}
		accl = cg.snap->ps.speed * ( pmove_msec.value / 1000.0f );
		if ( pps->groundEntityNum != ENTITYNUM_NONE ) {
			accl *= 10.0f;
			vlen *= 1.0f - 6.0f * ( pmove_msec.value / 1000.0f );
		}
		if ( ( flags & 16 ) || vlen <= cg.snap->ps.speed - accl ) {
			return;
		}
		{
			float g = cg.snap->ps.speed;

			aMin = 180.0f * acos( ( -0.5f * accl ) / vlen ) / M_PI;
			aMax = 180.0f * acos( ( g - accl ) / vlen ) / M_PI;
			aMid = 0;
			if ( vlen > g ) {
				aMid = 180.0f * acos( g / vlen ) / M_PI;
			}
		}
		for ( i = 0; i < 8; i++ ) {
			Vector4Copy( &cgazOpt[i * 4], optCol[i] );
			optCol[i][3] *= opacity;
		}
		ang[0] = vyaw - aMin;  ang[1] = vyaw - 90.0f;  ang[2] = vyaw - aMax;  ang[3] = vyaw - aMid;  ang[4] = vyaw;
		ang[5] = vyaw + aMid;  ang[6] = vyaw + aMax;   ang[7] = vyaw + 90.0f;  ang[8] = vyaw + aMin;
		for ( i = 0; i < 8; i++ ) {
			float x1 = CG_DF_CgazX( base, ang[i], halfFov, tanHalf );
			float x2 = CG_DF_CgazX( base, ang[i + 1], halfFov, tanHalf );

			if ( x1 < x2 ) {
				continue;
			}
			CG_DF_FillRectF( x2 + 320, ypos, x1 - x2, 16, optCol[i] );
			if ( i == 4 ) {
				CG_DF_FillRectF( x1 + 320 - 1.0f, ypos + 2, 2.0f, 12, c40 );
			}
		}
	}
}

// the frame function below is skipped for spectators and while the scoreboard is up; the vanilla upper-right timer/FPS must then stay
qboolean CG_DF_HudDrawsUpperRight( void ) {
	return ( CG_DF_HudOn() && cg.snap && cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR && !cg.showScores ) ? qtrue : qfalse;
}

// ---- frame (0x2a12) -------------------------------------------------------------------------------------------------------

void CG_DF_DrawHUD( void ) {
	const playerState_t	*ps;
	int			i, id, n;
	static int	secs[3];
	static char	fragText[16];
	int			ms;

	if ( !CG_DF_HudOn() || !cg.snap ) {
		return;
	}
	ps = &cg.snap->ps;
	if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}

	// custom colours (df_hud_colorRGB / df_hud_fontcolorRGB); forced team colours in team games
	if ( ps->persistant[PERS_TEAM] == TEAM_BLUE && df_hud_forceteamcolors.integer ) {
		dfCustomColor[0] = 0; dfCustomColor[1] = 0; dfCustomColor[2] = 1.0f;
	} else if ( ps->persistant[PERS_TEAM] == TEAM_RED && df_hud_forceteamcolors.integer ) {
		dfCustomColor[0] = 0.9f; dfCustomColor[1] = 0.05f; dfCustomColor[2] = 0.05f;
	} else if ( !CG_DF_ParseColor( df_hud_colorRGB.string, dfCustomColor ) ) {
		dfCustomColor[0] = -4.0f;
	}
	if ( !CG_DF_ParseColor( df_hud_fontcolorRGB.string, dfCustomFont ) ) {
		dfCustomFont[0] = -4.0f;
	}

	// best time of the reference record, last run time, speed
	wBest.visible = df_drawBesttime.integer ? 1 : 0;
	ms = CG_DF_ReferenceTime();
	if ( ms == 0 ) {
		wBest.visible = 0;
	} else {
		wBest.text = CG_DF_FormatTime( ms );
		wBest.w = ms > 0xea5f ? 256 : 128;
		wBest.width = ms > 0xea5f ? 9 : 6;
		wBest.border = &dfSh[ms > 0xea5f ? SH_164_B : SH_116_B];
		wBest.inner = &dfSh[ms > 0xea5f ? SH_164_I : SH_116_I];
	}
	CG_DF_DrawWidget( &wBest );

	wLast.visible = dfLastRunMs != 0;
	if ( dfLastRunMs ) {
		wLast.text = CG_DF_FormatTime( dfLastRunMs );
		wLast.w = dfLastRunMs > 0xea5f ? 256 : 128;
		wLast.width = dfLastRunMs > 0xea5f ? 9 : 6;
		wLast.border = &dfSh[dfLastRunMs > 0xea5f ? SH_164_B : SH_116_B];
		wLast.inner = &dfSh[dfLastRunMs > 0xea5f ? SH_164_I : SH_116_I];
	}
	CG_DF_DrawWidget( &wLast );

	if ( wSpeed.mode ) {
		wSpeed.text = CG_DF_SpeedText( (float)dfSpeedInt );		// 0x41b6 reads the integer speed cached by 0x3a4a
	}
	wSpeed.visible = df_drawSpeed.integer ? 1 : 0;
	CG_DF_DrawWidget( &wSpeed );

	// power-up timers (3 slots, ids 1..6)
	for ( id = 1; id <= 6 && powerCount < 3; id++ ) {
		if ( !ps->powerups[id] ) {
			continue;
		}
		for ( i = 0; i < powerCount; i++ ) {
			if ( wPower[powerList[i]].id == id ) {
				break;
			}
		}
		if ( i == powerCount ) {
			CG_DF_AddPower( id );
		}
	}
	for ( i = 0; i < powerCount; i++ ) {
		if ( !ps->powerups[wPower[powerList[i]].id] ) {
			CG_DF_RemovePower( i );		// 0x2b86: no re-check of the entry that moved up (a second expiry in the same frame is removed one frame later)
		}
	}
	for ( i = 0; i < powerCount; i++ ) {
		dfWidget_t *w = &wPower[powerList[i]];

		secs[i] = ( ps->powerups[w->id] - cg.time ) / 1000 + 1;
		if ( secs[i] > 99 ) {
			secs[i] = 99;
		}
		w->value = &secs[i];
		w->visible = 1;
		CG_DF_DrawWidget( w );
	}

	// flag carrier box W4 (0x2c16): below the power-up list, red flag (powerups[7]) -> TEAM_RED else TEAM_BLUE
	if ( ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG] ) {
		int team = ps->powerups[PW_REDFLAG] ? TEAM_RED : TEAM_BLUE;

		wFlag.y = 438 - 42 * powerCount;
		wFlag.visible = 1;
		CG_DF_DrawWidget( &wFlag );
		if ( df_hud_3DIcons.integer ) {
			CG_DrawFlagModel( wFlag.x, wFlag.y, 32, 32, team, qfalse );
		} else {
			CG_DrawFlagModel( wFlag.x + 10, wFlag.y + 8, 16, 16, team, qtrue );
		}
		wHoldable.x = 46;
	} else {
		wHoldable.x = 5;
	}

	// holdable item box W5 (0x2c91): same row as the flag box, x = 46 beside it. The icon pointer is `0x11d100 + 4 * stats[HOLDABLE_ITEM]`,
	// which for the 1.32 item list (teleporter 26, medkit 27) lands on the icon_teleport / icon_medkit shader globals; no other
	// holdable exists there, so any other item (Team Arena ones in oDFs) gets the bare box.
	if ( ps->stats[STAT_HOLDABLE_ITEM] > 0 && ps->stats[STAT_HOLDABLE_ITEM] < bg_numItems ) {
		wHoldable.y = 438 - 42 * powerCount;
		wHoldable.visible = 1;
		switch ( bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag ) {
		case HI_TELEPORTER:
			wHoldable.icon = &dfSh[SH_ICON_TELEPORT];
			break;
		case HI_MEDKIT:
			wHoldable.icon = &dfSh[SH_ICON_MEDKIT];
			break;
		default:
			wHoldable.icon = NULL;
			break;
		}
		CG_DF_DrawWidget( &wHoldable );
	}

	// health, armor, ammo, frags
	wHealth.value = &ps->stats[STAT_HEALTH];
	wHealth.visible = 1;
	CG_DF_DrawWidget( &wHealth );
	CG_DF_DrawHealthIcon( wHealth.px + 10, wHealth.py + 8, ps );

	wArmor.visible = ps->stats[STAT_ARMOR] != 0;
	if ( wArmor.visible ) {
		wArmor.value = &ps->stats[STAT_ARMOR];
	}
	CG_DF_DrawWidget( &wArmor );

	wAmmo.visible = ps->stats[STAT_HEALTH] > 0;
	if ( wAmmo.visible ) {
		static const int ammoWarn[10] = { 0, 0, 25, 4, 4, 4, 25, 4, 25, 6 };		// weapons 2..9 (jump table 0x1334)
		int weapon = ps->weapon;

		wAmmo.value = &ps->ammo[weapon];
		wAmmo.icon = &cg_weapons[weapon].weaponIcon;
		if ( weapon >= 2 && weapon <= 9 ) {
			wAmmo.warn = ammoWarn[weapon];
		}
	}
	CG_DF_DrawWidget( &wAmmo );

	n = 0;
	if ( cgs.fraglimit && !( CG_DF_DefragFlags() & 1 ) ) {
		wFrags.visible = 1;
		Com_sprintf( fragText, sizeof( fragText ), "%2i/%2i", ps->persistant[PERS_SCORE], cgs.fraglimit );
		wFrags.text = fragText;
		n = 1;
	} else {
		wFrags.visible = 0;
	}
	CG_DF_DrawWidget( &wFrags );

	CG_DF_DrawCheckpointLine( &wCkpt, n );
	// 0x2e5b: start at y = 4, timer (0x216a) then FPS (0x20dd), each advancing y by 20, then the jump meters
	{
		float y = 4.0f;

		CG_DF_DrawCGAZ( df_hud_cgaz.integer, df_hud_cgaz_opacity.value, df_hud_cgaz_ypos.integer );

		if ( cg_drawTimer.integer ) {
			y = CG_DrawTimer( y );
		}
		if ( cg_drawFPS.integer ) {
			y = CG_DrawFPS( y );
		}
		CG_DF_DrawJumpMeters( y );
	}
	CG_DF_DrawFollowName();
}

// ---- video-capture HUD (cgame 0x3387, chosen by 0x1ce20 instead of every other 2D element) ----
// df_hud_videocapture_enable: only the run timer is drawn, as plain text at (x, y): fontsize 4..32, fontcolor = colour index 0..7,
// fontshadow, fontjustify 0 centre / 1 left / 2 right.
vmCvar_t	df_hud_videocapture_enable, df_hud_videocapture_x, df_hud_videocapture_y, df_hud_videocapture_fontsize,
			df_hud_videocapture_fontcolor, df_hud_videocapture_fontshadow, df_hud_videocapture_fontjustify;

void CG_DF_DrawVideoCapture( void ) {
	const char	*s;
	int			ms, size, len, x, y;
	const float	*color;

	ms = dfLastRunMs;		// 0x3387: the decoded run time of the last-time widget (0 = not running / not shown)
	if ( !ms ) {
		return;
	}
	size = df_hud_videocapture_fontsize.integer;
	if ( size < 4 ) {
		size = 4;
	}
	if ( size > 32 ) {
		size = 32;
	}
	s = CG_DF_FormatTime( ms );
	len = strlen( s );
	switch ( df_hud_videocapture_fontjustify.integer ) {
	case 0:
		x = df_hud_videocapture_x.integer - ( len * size ) / 2;
		y = df_hud_videocapture_y.integer - size / 2;
		break;
	case 2:
		x = df_hud_videocapture_x.integer - len * size;
		y = df_hud_videocapture_y.integer;
		break;
	default:
		x = df_hud_videocapture_x.integer;
		y = df_hud_videocapture_y.integer;
		break;
	}
	color = CG_DF_HudColor( df_hud_videocapture_fontcolor.integer, NULL );		// 0x2559( cvar, 0 ): palette 0..7 / 8..12, white otherwise
	CG_DrawStringExt( x, y, s, color, qtrue, df_hud_videocapture_fontshadow.integer, size, size, len );
}
