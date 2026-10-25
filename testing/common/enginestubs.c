/*
 * enginestubs.c - engine services for the test tools (see enginestubs.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "q_shared.h"
#include "qcommon.h"
#include "enginestubs.h"

jmp_buf		*Stub_ErrorJmp;
char		Stub_ErrorMsg[512];

static const void	*stubFile;
static int			stubFileLength;

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	va_list ap;

	(void)code;
	va_start( ap, fmt );
	vsnprintf( Stub_ErrorMsg, sizeof( Stub_ErrorMsg ), fmt, ap );
	va_end( ap );

	if ( Stub_ErrorJmp ) {
		longjmp( *Stub_ErrorJmp, 1 );
	}
	fprintf( stderr, "fatal: %s\n", Stub_ErrorMsg );
	exit( 2 );
}

void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }

void Stub_SetFile( const void *data, int length ) {
	stubFile = data;
	stubFileLength = length;
}

int FS_ReadFile( const char *qpath, void **buffer ) {
	(void)qpath;
	if ( !stubFile ) {
		*buffer = NULL;
		return -1;
	}
	/* the collision loader wants 4 byte aligned data it may keep reading from */
	*buffer = malloc( (size_t)stubFileLength + 16 );
	memset( *buffer, 0, (size_t)stubFileLength + 16 );
	memcpy( *buffer, stubFile, stubFileLength );
	return stubFileLength;
}

void FS_FreeFile( void *buffer ) {
	free( buffer );
}

void *Z_Malloc( int size ) {
	return calloc( 1, size > 0 ? (size_t)size : 1 );
}

void Z_Free( void *ptr ) {
	free( ptr );
}

/* map data lives for the whole run; the collision code never frees it */
void *Hunk_Alloc( int size, ha_pref preference ) {
	(void)preference;
	return calloc( 1, size > 0 ? (size_t)size : 1 );
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) {
	cvar_t *v = (cvar_t *)calloc( 1, sizeof( *v ) );

	v->name = strdup( var_name );
	v->string = strdup( value );
	v->resetString = v->string;
	v->flags = flags;
	v->value = (float)atof( value );
	v->integer = atoi( value );
	return v;
}

void Cvar_SetDescription( cvar_t *var, const char *var_description ) {
	(void)var;
	(void)var_description;
}

/* referenced by the patch collision debug code only */
void BotDrawDebugPolygons( void (*drawPoly)( int color, int numPoints, float *points ), int value ) {
	(void)drawPoly;
	(void)value;
}
