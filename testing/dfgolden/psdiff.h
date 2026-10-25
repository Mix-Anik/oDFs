/*
 * psdiff.h - compares two playerStates the way a client sees them
 *
 * Only what the server networks is compared (the field table of the engine's msg.c, plus the
 * stats / persistant / ammo / powerups arrays), each value cut to the width it is sent with.
 */
#ifndef PSDIFF_H
#define PSDIFF_H

#include "q_shared.h"

/* float fields count as equal when this many representable floats apart (0 = exact) */
extern int	PSDiffUlps;

#define PSDIFF_FIELDS	52		/* 48 scalar fields + 4 arrays: one bit each in the mask */

/* bit i of the returned mask is set when field i differs; returns the number of differing fields */
int			PSDiff( const playerState_t *a, const playerState_t *b, unsigned long long *mask );

const char	*PSDiffFieldName( int index );

/* "name: want X got Y" for one differing field (arrays name their first differing element) */
void		PSDiffDescribe( const playerState_t *got, const playerState_t *want, int index, char *out, int size );

#endif
