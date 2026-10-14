// cg_dfob.c -- DeFRaG overbounce predictor (cgame 0xb21d and helpers), shown through the CHS items 50-79 / 300-310.
// For each of 17 "slots" (B G J p P r R Qp QP Qr QR SG SJ Sp SP Sr SR: which projectile/jump scenario) it simulates the vertical motion
// frame by frame (gravity rounded to an integer per frame, as the game does) and reports whether and when the player is within
// (1e-5, 0.25) units of the target plane, i.e. whether an overbounce is possible.  Only slots that are needed by a visible CHS item are
// computed.  See disasm/CGAME/ob_predictor.md and chs.md.  Internal numbering follows the original; the meaning of the slot letters
// is taken from the label table only (not from the code).

#include "cg_local.h"

vmCvar_t	df_ob_Fast;
vmCvar_t	df_ob_AllSlopes;
vmCvar_t	df_ob_OffsetStartPosZ;
vmCvar_t	df_ob_OffsetStopPosZ;

#define OB_SLOTS	17

int			dfObRes[OB_SLOTS];		// 0x129b88: -1 none, 0 / 1 / 2 classification of the hit
float		dfObOut1[OB_SLOTS];		// 0x129c10: value printed first by the "offsets" items
float		dfObOut2[OB_SLOTS];		// 0x129bcc

static int	obCtrA, obCtrB;			// 0x16e0 / 0x16e4 (phase counters, mod 3 / mod 4)

const char *dfObNames[OB_SLOTS] = { "B", "G", "J", "p", "P", "r", "R", "Qp", "QP", "Qr", "QR", "SG", "SJ", "Sp", "SP", "Sr", "SR" };
static const char *obBlank[OB_SLOTS] = { " ", " ", " ", " ", " ", " ", " ", "  ", "  ", "  ", "  ", "  ", "  ", "  ", "  ", "  ", "  " };

// CHS item -> slot mask (data 0x17f4); an item makes the predictor compute the slots in its mask
static const struct { int id, mask; } obGate[28] = {
	{ 50, 7 }, { 51, 6144 }, { 52, 120 }, { 53, 122880 }, { 54, 1920 }, { 55, 129151 }, { 56, 131071 }, { 57, 119 }, { 58, 120951 },
	{ 59, 6151 }, { 60, 3 }, { 61, 1 }, { 62, 2 }, { 63, 4 }, { 64, 2048 }, { 65, 4096 }, { 66, 8 }, { 67, 16 }, { 68, 32 }, { 69, 64 },
	{ 70, 8192 }, { 71, 16384 }, { 72, 32768 }, { 73, 65536 }, { 74, 128 }, { 75, 256 }, { 76, 512 }, { 77, 1024 }
};

// data 0x16f4 / 0x176c: per-row, per-phase offsets added to z and vz for slots 3..10 / 13..16
static const float obAddZ[32] = {
	0.5731f, 0.5743f, 0.5744f, 0.5734f,  4.8195f, 4.8208f, 4.8207f, 4.8197f,  3.9645f, 3.9731f, 3.9736f, 3.9684f,  8.2113f, 8.2203f, 8.2196f, 8.2138f,
	1.7694f, 1.7743f, 1.7741f, 1.7717f,  6.0173f, 6.0206f, 6.0208f, 6.0192f,  7.9499f, 7.9729f, 7.9742f, 7.9654f,  12.1977f, 12.2202f, 12.2185f, 12.2101f
};
static const float obAddVz[32] = {
	68, 69, 69, 68,  332, 333, 333, 332,  492, 494, 493, 493,  756, 758, 757, 757,
	218, 219, 219, 218,  482, 483, 483, 482,  991, 993, 994, 992,  1255, 1258, 1257, 1256
};
static const float obPhaseZ[3] = { 0.145f, 0.185f, 0.23f };

static int CG_DF_ObItemSelected( int id ) {
	int p, k;

	for ( p = 1; p <= 2; p++ ) {
		if ( !CG_DF_DrawAllowed( dfChs[p].draw.integer ) ) {
			continue;
		}
		for ( k = 0; k < 8; k++ ) {
			if ( abs( dfChs[p].info[k].integer ) % 1000 == id ) {
				return 1;
			}
		}
	}
	return 0;
}

static int CG_DF_ObSlotWanted( int slot ) {
	int i;

	for ( i = 0; i < 28; i++ ) {
		if ( CG_DF_ObItemSelected( obGate[i].id ) && ( obGate[i].mask & ( 1 << slot ) ) ) {
			return 1;
		}
	}
	return 0;
}

// 0xb55c: h = height above the target after n frames
static int CG_DF_ObClassify( float h, int n, float vz ) {
	if ( h >= 0.25f ) {
		return 0;
	}
	if ( h <= 1e-5f ) {
		return 0;
	}
	if ( n == 1 && vz != 0 ) {
		return 1;
	}
	return 2;
}

// 0xb50a: n frames of gravity
static void CG_DF_ObStep( float *z, float *vz, float dt, int g, int n ) {
	float gf = (float)g * dt;
	int   i;

	for ( i = 1; i <= n; i++ ) {
		*z += ( 0.5f * ( ( *vz + *vz ) - gf ) ) * dt;
		*vz = floor( ( *vz - gf ) + 0.5f );
	}
}

// 0xb583: closed-form solver (df_ob_Fast 1)
static int CG_DF_ObFast( float z, float vz, float target, float dt, int g, float *out1, float *out2 ) {
	float	gr, frac, h, v, disc, nf, zz;
	int		it, res = -1;

	gr = floor( (float)g * dt + 0.5f );
	frac = (float)g * dt - gr;
	h = z - target;
	v = vz * dt;
	disc = sqrt( v * v + ( ( 2.0f * gr ) * h ) * dt );
	nf = floor( ( v + disc ) / ( gr * dt ) );
	nf -= 1.0f;
	for ( it = 1; it <= 100; it++ ) {
		*out2 = h;			// previous height
		zz = ( ( z + v * nf ) - ( ( ( 0.5f * frac ) * nf ) * dt ) ) - ( ( ( ( 0.5f * gr ) * nf ) * nf ) * dt );
		h = zz - target;
		*out1 = h;			// new height
		if ( h < 0.25f ) {
			res = CG_DF_ObClassify( h, (int)nf, vz );
			break;
		}
		nf += 1.0f;
	}
	return res;
}

// 0xb66f: frame-by-frame solver (df_ob_Fast 0)
static int CG_DF_ObFull( float z, float vz, float target, float dt, int g, float *out1, float *out2 ) {
	float	zz = z, v = vz, h = 0;
	int		i, res = -1;

	for ( i = 1; i <= 10000; i++ ) {
		*out2 = h;
		h = zz - target;
		*out1 = h;
		if ( h < 0.25f ) {
			res = CG_DF_ObClassify( h, i, vz );
			break;
		}
		CG_DF_ObStep( &zz, &v, dt, g, 1 );
	}
	return res;
}

// 0xb4ce and its helpers: scenario specific adjustments of start height, velocity and target height
static void CG_DF_ObAdjust( int slot, float *z, float *vz, float *target, float dt, float g ) {
	int row = -1, n = 1;

	if ( slot >= 11 && slot <= 16 ) {											// 0xb3f5
		*z += obPhaseZ[obCtrA % 3];
	}
	if ( slot == 2 || slot == 12 ) {											// 0xb412
		*vz += 270.0f;
	}
	if ( slot >= 3 && slot <= 16 ) {											// 0xb432
		static const int rowOf[14] = { 0, 1, 2, 3, 4, 5, 6, 7, -1, -1, 0, 1, 2, 3 };		// slots 3..16

		row = rowOf[slot - 3];
	}
	if ( row != -1 ) {															// 0xb3a2
		int idx = row * 4 + ( obCtrB % 4 );

		*z += obAddZ[idx];
		*vz += obAddVz[idx];
	}
	// 0xb484
	if ( slot == 0 || slot == 1 || slot == 11 ) {
		n = 0;
	} else if ( slot == 2 || slot == 12 ) {
		n = 20;
	}
	if ( n ) {
		CG_DF_ObStep( z, vz, dt, (int)g, n );
	}
	*z += df_ob_OffsetStartPosZ.value;											// 0xb3d8
	*target += df_ob_OffsetStopPosZ.value;
}

// 0xb72e
static int CG_DF_ObSlot( int slot, float z, float vz, float floorZ, float lookZ, float *out1, float *out2 ) {
	float	dt = 0.001f * pmove_msec.integer, toFloor, toLook, target = lookZ;
	int		g;

	toFloor = fabs( fabs( z ) - fabs( floorZ ) );
	toLook = fabs( fabs( z ) - fabs( lookZ ) );
	if ( !cg.snap ) {
		return -1;
	}
	g = cg.snap->ps.gravity;
	if ( slot == 0 || slot == 1 ) {
		if ( slot == 0 ) {
			target = floorZ;
			if ( vz == 0 && !( z > floorZ + 0.25f ) ) {
				vz += 270.0f;									// 0xb37a
				CG_DF_ObStep( &z, &vz, dt, g, 20 );
			}
		}
		if ( slot == 1 && vz == 0 && toLook < 0.25f && toLook != 0 ) {
			return 0;
		}
		CG_DF_ObAdjust( slot, &z, &vz, &target, dt, (float)g );
		return df_ob_Fast.integer ? CG_DF_ObFast( z, vz, target, dt, g, out1, out2 ) : CG_DF_ObFull( z, vz, target, dt, g, out1, out2 );
	}
	if ( toFloor > 1.0f ) {
		return -1;
	}
	if ( slot >= 11 && slot <= 16 ) {					// 0xb358: scenario with self-hit
		if ( toFloor >= 0.145f ) {
			return -1;
		}
		if ( toLook < 1.0f ) {
			return -1;
		}
	}
	CG_DF_ObAdjust( slot, &z, &vz, &target, dt, (float)g );
	return df_ob_Fast.integer ? CG_DF_ObFast( z, vz, target, dt, g, out1, out2 ) : CG_DF_ObFull( z, vz, target, dt, g, out1, out2 );
}

static int CG_DF_ObTraceMask( void ) {
	return ( cg.snap->ps.stats[14] & 1 ) ? 65537 : 33619969;
}

// 0xb21d, called every frame with the snapshot's origin / velocity / view angles
void CG_DF_ObFrame( void ) {
	static const vec3_t mins = { -15, -15, -24 }, maxs = { 15, 15, 32 };
	trace_t	floorTr, lookTr;
	vec3_t	pos, vel, ang, end, fwd, start;
	int		slot, haveTrace = 0;

	if ( !cg.snap ) {
		return;
	}
	VectorCopy( cg.snap->ps.origin, pos );
	VectorCopy( cg.snap->ps.velocity, vel );
	VectorCopy( cg.snap->ps.viewangles, ang );
	for ( slot = 0; slot < OB_SLOTS; slot++ ) {
		float nz;

		dfObRes[slot] = -1;
		dfObOut1[slot] = 0;
		dfObOut2[slot] = 0;
		if ( !CG_DF_ObSlotWanted( slot ) ) {
			continue;
		}
		if ( !haveTrace ) {
			// 0x84e6: straight down; 0x8444: along the view direction from the eye (+14 forward), endpos z + 24
			VectorCopy( pos, end );
			end[2] = -128000.0f;
			CG_Trace( &floorTr, pos, mins, maxs, end, cg.snap->ps.clientNum, CG_DF_ObTraceMask() );
			AngleVectors( ang, fwd, NULL, NULL );
			VectorCopy( pos, start );
			start[2] += cg.snap->ps.viewheight;
			VectorMA( start, 14.0f, fwd, start );
			VectorMA( start, 128000.0f, fwd, end );
			CG_Trace( &lookTr, start, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CG_DF_ObTraceMask() );
			lookTr.endpos[2] += 24.0f;
			haveTrace = 1;
		}
		nz = slot ? lookTr.plane.normal[2] : floorTr.plane.normal[2];
		if ( !( nz > 0 ) ) {
			continue;
		}
		if ( nz != 1.0f && !df_ob_AllSlopes.integer ) {
			continue;
		}
		dfObRes[slot] = CG_DF_ObSlot( slot, pos[2], vel[2], floorTr.endpos[2], lookTr.endpos[2], &dfObOut1[slot], &dfObOut2[slot] );
	}
	obCtrB = ( obCtrB + 1 ) % 4;
	obCtrA = ( obCtrA + 1 ) % 3;
}

// CHS text helper 0xbc00 (bbe9): append the slot name (label) or the name / blank depending on the slot result (value)
void CG_DF_ObAppend( char *buf, int size, int isValue, int slot ) {
	const char *s;

	if ( !isValue ) {
		s = dfObNames[slot];
	} else {
		s = dfObRes[slot] >= 1 ? dfObNames[slot] : obBlank[slot];
	}
	Q_strcat( buf, size, s );
}
