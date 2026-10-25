/*
 * golden.c - reading and writing golden files, map stripping, race time decoding
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "golden.h"
#include "../third_party/miniz/miniz.h"

#define GOLD_MAGIC		"DFGOLD1"

/* ---- growing byte buffer ---- */

typedef struct {
	unsigned char	*data;
	size_t			len, cap;
} buf_t;

static void BufPut( buf_t *b, const void *p, size_t n ) {
	if ( b->len + n > b->cap ) {
		b->cap = ( b->len + n ) * 2 + 4096;
		b->data = (unsigned char *)realloc( b->data, b->cap );
	}
	memcpy( b->data + b->len, p, n );
	b->len += n;
}

static void BufInt( buf_t *b, int v ) {
	unsigned char c[4];

	c[0] = (unsigned char)v;
	c[1] = (unsigned char)( v >> 8 );
	c[2] = (unsigned char)( v >> 16 );
	c[3] = (unsigned char)( v >> 24 );
	BufPut( b, c, 4 );
}

static void BufStr( buf_t *b, const char *s ) {
	int n = (int)strlen( s );

	BufInt( b, n );
	BufPut( b, s, n );
}

typedef struct {
	const unsigned char	*p;
	size_t				len, pos;
	int					bad;
} rd_t;

static int RdInt( rd_t *r ) {
	int v;

	if ( r->pos + 4 > r->len ) {
		r->bad = 1;
		return 0;
	}
	v = r->p[r->pos] | ( r->p[r->pos + 1] << 8 ) | ( r->p[r->pos + 2] << 16 ) | ( r->p[r->pos + 3] << 24 );
	r->pos += 4;
	return v;
}

static const unsigned char *RdBytes( rd_t *r, size_t n ) {
	const unsigned char *p = r->p + r->pos;

	if ( r->pos + n > r->len ) {
		r->bad = 1;
		return NULL;
	}
	r->pos += n;
	return p;
}

/* a string copy into a malloc'd buffer of at least `minSize` bytes */
static char *RdStr( rd_t *r, int minSize ) {
	int n = RdInt( r );
	const unsigned char *p;
	char *s;

	if ( r->bad || n < 0 || n > 1 << 24 ) {
		r->bad = 1;
		return NULL;
	}
	p = RdBytes( r, n );
	if ( !p ) {
		return NULL;
	}
	s = (char *)calloc( 1, n + 1 > minSize ? n + 1 : minSize );
	memcpy( s, p, n );
	return s;
}

static void RdStrInto( rd_t *r, char *out, int size ) {
	char *s = RdStr( r, 1 );

	if ( s ) {
		Q_strncpyz( out, s, size );
		free( s );
	}
}

/* ---- the file ---- */

void Gold_Free( golden_t *g ) {
	free( g->serverinfo );
	free( g->systeminfo );
	free( g->bsp );
	free( g->steps );
	free( g->states );
	memset( g, 0, sizeof( *g ) );
}

int Gold_Write( const golden_t *g, const char *path, char *err, int errSize ) {
	buf_t b = { 0 };
	const int words = (int)( sizeof( playerState_t ) / 4 );
	int i, k;
	mz_ulong packedLen;
	unsigned char *packed;
	FILE *f;

	BufStr( &b, g->name );
	BufStr( &b, g->map );
	BufStr( &b, g->serverinfo );
	BufStr( &b, g->systeminfo );
	BufStr( &b, g->userinfo );
	BufInt( &b, g->hookType );
	BufInt( &b, g->lgKnockback );
	BufInt( &b, g->dedicated );
	BufInt( &b, g->startOffset );
	BufInt( &b, g->decayStep );
	BufInt( &b, g->ulps );
	BufInt( &b, g->raceOffset );
	BufInt( &b, g->bspCrc );
	BufInt( &b, g->nameMs );
	BufInt( &b, g->knownFlags );
	BufInt( &b, g->numSteps );
	BufInt( &b, words );
	BufInt( &b, g->bspLen );
	BufPut( &b, g->bsp, g->bspLen );

	for ( i = 0; i < g->numSteps; i++ ) {
		const goldStep_t *s = &g->steps[i];

		BufInt( &b, s->cmd.serverTime );
		for ( k = 0; k < 3; k++ ) {
			BufInt( &b, s->cmd.angles[k] );
		}
		BufInt( &b, s->cmd.buttons );
		BufInt( &b, s->cmd.weapon );
		BufInt( &b, s->cmd.forwardmove );
		BufInt( &b, s->cmd.rightmove );
		BufInt( &b, s->cmd.upmove );
		BufInt( &b, s->serverTime );
		BufInt( &b, s->timerMs );
		BufInt( &b, s->raceMs );
		BufInt( &b, s->flags );
	}

	/* states: a bit per word that differs from the previous state, then those words */
	for ( i = 0; i < g->numSteps; i++ ) {
		const int *cur = (const int *)&g->states[i];
		const int *prev = i ? (const int *)&g->states[i - 1] : NULL;
		unsigned char mask[( 1024 + 7 ) / 8];

		memset( mask, 0, sizeof( mask ) );
		for ( k = 0; k < words; k++ ) {
			if ( !prev || cur[k] != prev[k] ) {
				mask[k >> 3] |= 1 << ( k & 7 );
			}
		}
		BufPut( &b, mask, ( words + 7 ) / 8 );
		for ( k = 0; k < words; k++ ) {
			if ( mask[k >> 3] & ( 1 << ( k & 7 ) ) ) {
				BufInt( &b, cur[k] );
			}
		}
	}

	packedLen = mz_compressBound( (mz_ulong)b.len );
	packed = (unsigned char *)malloc( packedLen );
	if ( mz_compress2( packed, &packedLen, b.data, (mz_ulong)b.len, 9 ) != MZ_OK ) {
		Com_sprintf( err, errSize, "compression failed" );
		free( packed );
		free( b.data );
		return -1;
	}
	f = fopen( path, "wb" );
	if ( !f ) {
		Com_sprintf( err, errSize, "cannot write %s", path );
		free( packed );
		free( b.data );
		return -1;
	}
	{
		unsigned char head[8 + 12];

		memset( head, 0, sizeof( head ) );
		memcpy( head, GOLD_MAGIC, 7 );
		head[8] = GOLD_VERSION;
		head[12] = (unsigned char)b.len; head[13] = (unsigned char)( b.len >> 8 );
		head[14] = (unsigned char)( b.len >> 16 ); head[15] = (unsigned char)( b.len >> 24 );
		head[16] = (unsigned char)packedLen; head[17] = (unsigned char)( packedLen >> 8 );
		head[18] = (unsigned char)( packedLen >> 16 ); head[19] = (unsigned char)( packedLen >> 24 );
		fwrite( head, 1, sizeof( head ), f );
		fwrite( packed, 1, packedLen, f );
	}
	fclose( f );
	free( packed );
	free( b.data );
	return 0;
}

int Gold_Read( golden_t *g, const char *path, char *err, int errSize ) {
	FILE *f = fopen( path, "rb" );
	unsigned char head[20], *packed = NULL, *raw = NULL;
	long fileLen;
	unsigned int rawLen, packedLen;
	mz_ulong outLen;
	rd_t r;
	int i, k, words;

	memset( g, 0, sizeof( *g ) );
	if ( !f ) {
		Com_sprintf( err, errSize, "cannot open %s", path );
		return -1;
	}
	fseek( f, 0, SEEK_END );
	fileLen = ftell( f );
	fseek( f, 0, SEEK_SET );
	if ( fileLen < (long)sizeof( head ) || fread( head, 1, sizeof( head ), f ) != sizeof( head ) ||
			memcmp( head, GOLD_MAGIC, 7 ) || head[8] != GOLD_VERSION ) {
		Com_sprintf( err, errSize, "%s is not a golden file of this version", path );
		fclose( f );
		return -1;
	}
	rawLen = head[12] | ( head[13] << 8 ) | ( head[14] << 16 ) | ( (unsigned int)head[15] << 24 );
	packedLen = head[16] | ( head[17] << 8 ) | ( head[18] << 16 ) | ( (unsigned int)head[19] << 24 );
	if ( (long)packedLen != fileLen - (long)sizeof( head ) || rawLen > ( 1u << 30 ) ) {
		Com_sprintf( err, errSize, "%s is damaged (size)", path );
		fclose( f );
		return -1;
	}
	packed = (unsigned char *)malloc( packedLen );
	raw = (unsigned char *)malloc( rawLen );
	if ( fread( packed, 1, packedLen, f ) != packedLen ) {
		Com_sprintf( err, errSize, "%s is truncated", path );
		fclose( f );
		free( packed );
		free( raw );
		return -1;
	}
	fclose( f );
	outLen = rawLen;
	if ( mz_uncompress( raw, &outLen, packed, packedLen ) != MZ_OK || outLen != rawLen ) {
		Com_sprintf( err, errSize, "%s is damaged (data)", path );
		free( packed );
		free( raw );
		return -1;
	}
	free( packed );

	r.p = raw;
	r.len = rawLen;
	r.pos = 0;
	r.bad = 0;
	RdStrInto( &r, g->name, sizeof( g->name ) );
	RdStrInto( &r, g->map, sizeof( g->map ) );
	g->serverinfo = RdStr( &r, BIG_INFO_STRING );
	g->systeminfo = RdStr( &r, BIG_INFO_STRING );
	RdStrInto( &r, g->userinfo, sizeof( g->userinfo ) );
	g->hookType = RdInt( &r );
	g->lgKnockback = RdInt( &r );
	g->dedicated = RdInt( &r );
	g->startOffset = RdInt( &r );
	g->decayStep = RdInt( &r );
	g->ulps = RdInt( &r );
	g->raceOffset = RdInt( &r );
	g->bspCrc = RdInt( &r );
	g->nameMs = RdInt( &r );
	g->knownFlags = RdInt( &r );
	g->numSteps = RdInt( &r );
	words = RdInt( &r );
	g->bspLen = RdInt( &r );
	if ( r.bad || words != (int)( sizeof( playerState_t ) / 4 ) || g->numSteps < 1 || g->numSteps > 1 << 22 ||
			g->bspLen < 0 || !g->serverinfo || !g->systeminfo ) {
		Com_sprintf( err, errSize, "%s: unexpected content (player state size %d words, this build %d)", path, words,
			(int)( sizeof( playerState_t ) / 4 ) );
		free( raw );
		Gold_Free( g );
		return -1;
	}
	{
		const unsigned char *p = RdBytes( &r, g->bspLen );

		if ( !p ) {
			Com_sprintf( err, errSize, "%s: truncated map", path );
			free( raw );
			Gold_Free( g );
			return -1;
		}
		g->bsp = (unsigned char *)malloc( g->bspLen + 16 );
		memcpy( g->bsp, p, g->bspLen );
	}
	g->steps = (goldStep_t *)calloc( g->numSteps, sizeof( goldStep_t ) );
	g->states = (playerState_t *)calloc( g->numSteps, sizeof( playerState_t ) );
	for ( i = 0; i < g->numSteps; i++ ) {
		goldStep_t *s = &g->steps[i];

		s->cmd.serverTime = RdInt( &r );
		for ( k = 0; k < 3; k++ ) {
			s->cmd.angles[k] = RdInt( &r );
		}
		s->cmd.buttons = RdInt( &r );
		s->cmd.weapon = (byte)RdInt( &r );
		s->cmd.forwardmove = (signed char)RdInt( &r );
		s->cmd.rightmove = (signed char)RdInt( &r );
		s->cmd.upmove = (signed char)RdInt( &r );
		s->serverTime = RdInt( &r );
		s->timerMs = RdInt( &r );
		s->raceMs = RdInt( &r );
		s->flags = (unsigned char)RdInt( &r );
	}
	for ( i = 0; i < g->numSteps && !r.bad; i++ ) {
		int *cur = (int *)&g->states[i];
		const unsigned char *mask = RdBytes( &r, ( words + 7 ) / 8 );

		if ( !mask ) {
			break;
		}
		if ( i ) {
			memcpy( cur, &g->states[i - 1], sizeof( playerState_t ) );
		}
		for ( k = 0; k < words; k++ ) {
			if ( mask[k >> 3] & ( 1 << ( k & 7 ) ) ) {
				cur[k] = RdInt( &r );
			}
		}
	}
	free( raw );
	if ( r.bad ) {
		Com_sprintf( err, errSize, "%s is truncated (states)", path );
		Gold_Free( g );
		return -1;
	}
	return 0;
}

/* ---- the map ---- */

#define BSP_LUMPS		17
#define L_SURFACES		13
#define L_DRAWVERTS		10
#define L_DRAWINDEXES	11
#define L_LIGHTMAPS		14
#define L_LIGHTGRID		15
#define L_VISIBILITY	16

#define DSURF_INTS		26
#define DVERT_BYTES		44
#define MST_PATCH_TYPE	2

static int LeInt( const unsigned char *p ) {
	return p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( p[3] << 24 );
}

static void PutLeInt( unsigned char *p, int v ) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)( v >> 8 );
	p[2] = (unsigned char)( v >> 16 );
	p[3] = (unsigned char)( v >> 24 );
}

/* Collision reads: entities, shaders, planes, nodes, leafs, leaf brushes, models, brushes, brush sides, fogs, the
   surfaces and, for the patches among them, their control points. Everything else (lightmaps, light grid,
   visibility, triangle soup vertices, indexes) is dropped. */
unsigned char *Gold_StripBsp( const unsigned char *bsp, int len, int *outLen ) {
	int ofs[BSP_LUMPS], size[BSP_LUMPS], i, total = 8 + BSP_LUMPS * 8, numSurf, numVerts;
	unsigned char *out, *newVerts;
	int newVertCount = 0, pos;
	int newSurfSize;

	if ( len < 8 + BSP_LUMPS * 8 || memcmp( bsp, "IBSP", 4 ) || LeInt( bsp + 4 ) != 46 ) {
		return NULL;
	}
	for ( i = 0; i < BSP_LUMPS; i++ ) {
		ofs[i] = LeInt( bsp + 8 + i * 8 );
		size[i] = LeInt( bsp + 12 + i * 8 );
		if ( ofs[i] < 0 || size[i] < 0 || (long long)ofs[i] + size[i] > len ) {
			return NULL;
		}
	}
	numSurf = size[L_SURFACES] / ( DSURF_INTS * 4 );
	numVerts = size[L_DRAWVERTS] / DVERT_BYTES;

	/* patch control points, back to back; the surface table points at the new positions */
	newVerts = (unsigned char *)malloc( size[L_DRAWVERTS] + 16 );
	newSurfSize = size[L_SURFACES];
	{
		unsigned char *surfs = (unsigned char *)malloc( newSurfSize + 16 );

		memcpy( surfs, bsp + ofs[L_SURFACES], newSurfSize );
		for ( i = 0; i < numSurf; i++ ) {
			unsigned char *s = surfs + i * DSURF_INTS * 4;
			int type = LeInt( s + 8 ), first = LeInt( s + 12 ), count = LeInt( s + 16 );

			if ( type == MST_PATCH_TYPE && first >= 0 && count >= 0 && first + count <= numVerts ) {
				memcpy( newVerts + newVertCount * DVERT_BYTES, bsp + ofs[L_DRAWVERTS] + first * DVERT_BYTES, count * DVERT_BYTES );
				PutLeInt( s + 12, newVertCount );
				newVertCount += count;
			} else {
				PutLeInt( s + 12, 0 );
				PutLeInt( s + 16, 0 );
				PutLeInt( s + 20, 0 );	/* firstIndex */
				PutLeInt( s + 24, 0 );	/* numIndexes */
			}
		}
		for ( i = 0; i < BSP_LUMPS; i++ ) {
			int s = size[i];

			if ( i == L_DRAWVERTS ) {
				s = newVertCount * DVERT_BYTES;
			} else if ( i == L_DRAWINDEXES || i == L_LIGHTMAPS || i == L_LIGHTGRID || i == L_VISIBILITY ) {
				s = 0;
			}
			total += ( s + 3 ) & ~3;
		}
		out = (unsigned char *)calloc( 1, total + 16 );
		memcpy( out, bsp, 8 );
		pos = 8 + BSP_LUMPS * 8;
		for ( i = 0; i < BSP_LUMPS; i++ ) {
			int s = size[i];
			const unsigned char *src = bsp + ofs[i];

			if ( i == L_DRAWVERTS ) {
				s = newVertCount * DVERT_BYTES;
				src = newVerts;
			} else if ( i == L_SURFACES ) {
				src = surfs;
			} else if ( i == L_DRAWINDEXES || i == L_LIGHTMAPS || i == L_LIGHTGRID || i == L_VISIBILITY ) {
				s = 0;
			}
			PutLeInt( out + 8 + i * 8, pos );
			PutLeInt( out + 12 + i * 8, s );
			memcpy( out + pos, src, s );
			pos += ( s + 3 ) & ~3;
		}
		free( surfs );
	}
	free( newVerts );
	*outLen = pos;
	return out;
}

/* ---- the race time ---- */

unsigned char Gold_MapSalt( const char *mapName ) {
	unsigned char salt = 0;
	char map[MAX_QPATH];
	const char *c;

	Q_strncpyz( map, mapName, sizeof( map ) );
	Q_strlwr( map );
	for ( c = map; *c; c++ ) {
		salt += (unsigned char)*c;
	}
	return salt;
}

int Gold_DecodeRaceTime( unsigned int e, unsigned int t, float originX, float velX, int health, int moveDir,
		unsigned int key, int *ms ) {
	unsigned int x, r, a;
	int i, v;

	e ^= (unsigned int)( moveDir & 0xf ) << 28;
	e ^= ( health > 0 ) ? (unsigned int)( health & 0xff ) : 0x96u;
	v = (int)floor( velX );
	e ^= (unsigned int)( v < 0 ? -v : v ) << 16;
	v = (int)floor( originX );
	e ^= (unsigned int)( v < 0 ? -v : v ) & 0xffff;
	for ( i = 2; i >= 0; i-- ) {
		( (unsigned char *)&e )[i + 1] ^= ( (unsigned char *)&e )[i];
	}
	x = t << 2;
	x += key << 8;
	x ^= t << 24;
	e ^= x;
	r = e >> 28;
	r |= ( ~r << 4 ) & 0xff;
	r |= r << 8;
	r |= r << 16;
	e = ( e & 0xf0000000u ) | ( ( e ^ r ) & 0x0fffffffu );
	a = 0;
	for ( i = 0; i < 3; i++ ) {
		a += ( ( e & 0x3fffff ) >> ( 6 * i ) ) & 0x3f;
	}
	a += ( ( e & 0x3fffff ) >> 18 ) & 0xf;
	if ( ( ( e >> 22 ) & 0x3f ) != ( a & 0x3f ) ) {
		return 0;
	}
	*ms = (int)( e & 0x3fffff );
	return 1;
}
