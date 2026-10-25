/*
 * psdiff.c - see psdiff.h
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "psdiff.h"

typedef struct {
	const char	*name;
	int			offset;
	int			bits;		/* 0 = float, > 0 unsigned width, < 0 signed width */
} psField_t;

#define PSF( f, b )	{ #f, (int)offsetof( playerState_t, f ), b }

/* the table of the engine's msg.c (playerStateFields) */
static const psField_t psFields[48] = {
	PSF( commandTime, 32 ), PSF( origin[0], 0 ), PSF( origin[1], 0 ), PSF( bobCycle, 8 ),
	PSF( velocity[0], 0 ), PSF( velocity[1], 0 ), PSF( viewangles[1], 0 ), PSF( viewangles[0], 0 ),
	PSF( weaponTime, -16 ), PSF( origin[2], 0 ), PSF( velocity[2], 0 ), PSF( legsTimer, 8 ),
	PSF( pm_time, -16 ), PSF( eventSequence, 16 ), PSF( torsoAnim, 8 ), PSF( movementDir, 4 ),
	PSF( events[0], 8 ), PSF( legsAnim, 8 ), PSF( events[1], 8 ), PSF( pm_flags, 16 ),
	PSF( groundEntityNum, 10 ), PSF( weaponstate, 4 ), PSF( eFlags, 16 ), PSF( externalEvent, 10 ),
	PSF( gravity, 16 ), PSF( speed, 16 ), PSF( delta_angles[1], 16 ), PSF( externalEventParm, 8 ),
	PSF( viewheight, -8 ), PSF( damageEvent, 8 ), PSF( damageYaw, 8 ), PSF( damagePitch, 8 ),
	PSF( damageCount, 8 ), PSF( generic1, 8 ), PSF( pm_type, 8 ), PSF( delta_angles[0], 16 ),
	PSF( delta_angles[2], 16 ), PSF( torsoTimer, 12 ), PSF( eventParms[0], 8 ), PSF( eventParms[1], 8 ),
	PSF( clientNum, 8 ), PSF( weapon, 5 ), PSF( viewangles[2], 0 ), PSF( grapplePoint[0], 0 ),
	PSF( grapplePoint[1], 0 ), PSF( grapplePoint[2], 0 ), PSF( jumppad_ent, 10 ), PSF( loopSound, 16 )
};

int PSDiffUlps;

static int FloatsClose( float a, float b ) {
	int ia, ib;

	if ( a == b ) {
		return 1;
	}
	if ( !PSDiffUlps ) {
		return 0;
	}
	memcpy( &ia, &a, 4 );
	memcpy( &ib, &b, 4 );
	if ( ia < 0 ) ia = (int)0x80000000 - ia;
	if ( ib < 0 ) ib = (int)0x80000000 - ib;
	return abs( ia - ib ) <= PSDiffUlps;
}

static const char *arrayNames[4] = { "stats", "persistant", "ammo", "powerups" };

/* the value as the network carries it */
static int Cut( int v, int bits ) {
	if ( bits == 32 ) {
		return v;
	}
	if ( bits < 0 ) {
		int shift = 32 + bits;			/* bits is negative: width = -bits */

		return ( v << shift ) >> shift;
	}
	return v & ( ( 1 << bits ) - 1 );
}

static int ArrayElement( const playerState_t *p, int arr, int i ) {
	switch ( arr ) {
	case 0: return p->stats[i];
	case 1: return p->persistant[i];
	case 2: return p->ammo[i];
	default: return p->powerups[i];
	}
}

/* stats / persistant / ammo travel as 16 bit values, powerups as 32 */
static int ArrayCut( int arr, int v ) {
	return arr == 3 ? v : (short)v;
}

int PSDiff( const playerState_t *a, const playerState_t *b, unsigned long long *mask ) {
	int i, n = 0;
	unsigned long long m = 0;

	for ( i = 0; i < 48; i++ ) {
		const psField_t *f = &psFields[i];
		int differ;

		if ( f->bits == 0 ) {
			differ = !FloatsClose( *(const float *)( (const char *)a + f->offset ), *(const float *)( (const char *)b + f->offset ) );
		} else {
			int x = *(const int *)( (const char *)a + f->offset ), y = *(const int *)( (const char *)b + f->offset );
			int bits = f->bits < 0 ? f->bits : f->bits;

			differ = Cut( x, bits ) != Cut( y, bits );
		}
		if ( differ ) {
			m |= 1ULL << i;
			n++;
		}
	}
	for ( i = 0; i < 4; i++ ) {
		int k, differ = 0;

		for ( k = 0; k < 16; k++ ) {
			if ( ArrayCut( i, ArrayElement( a, i, k ) ) != ArrayCut( i, ArrayElement( b, i, k ) ) ) {
				differ = 1;
				break;
			}
		}
		if ( differ ) {
			m |= 1ULL << ( 48 + i );
			n++;
		}
	}
	if ( mask ) {
		*mask = m;
	}
	return n;
}

const char *PSDiffFieldName( int index ) {
	if ( index < 48 ) {
		return psFields[index].name;
	}
	return arrayNames[index - 48];
}

void PSDiffDescribe( const playerState_t *got, const playerState_t *want, int index, char *out, int size ) {
	if ( index < 48 ) {
		const psField_t *f = &psFields[index];

		if ( f->bits == 0 ) {
			snprintf( out, size, "%s want %.9g got %.9g", f->name, *(const float *)( (const char *)want + f->offset ),
				*(const float *)( (const char *)got + f->offset ) );
		} else {
			snprintf( out, size, "%s want %d got %d", f->name, Cut( *(const int *)( (const char *)want + f->offset ), f->bits ),
				Cut( *(const int *)( (const char *)got + f->offset ), f->bits ) );
		}
	} else {
		int arr = index - 48, k;

		for ( k = 0; k < 16; k++ ) {
			if ( ArrayCut( arr, ArrayElement( got, arr, k ) ) != ArrayCut( arr, ArrayElement( want, arr, k ) ) ) {
				snprintf( out, size, "%s[%d] want %d got %d", arrayNames[arr], k, ArrayCut( arr, ArrayElement( want, arr, k ) ),
					ArrayCut( arr, ArrayElement( got, arr, k ) ) );
				return;
			}
		}
		snprintf( out, size, "%s", arrayNames[arr] );
	}
}
