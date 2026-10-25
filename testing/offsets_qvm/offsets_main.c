/*
 * offsets_main.c - field offsets inside the game's structures
 *
 * Built with the same compiler and headers as qagame.qvm, so the numbers are the layout of the
 * compiled game. The host reads a few internal fields (health, timers) that are not part of the
 * playerState the network carries.
 */
#include "../../code/game/g_local.h"

#define OFF_ENT( x )	( (int)&( ( (gentity_t *)0 )->x ) )
#define OFF_CL( x )		( (int)&( ( (gclient_t *)0 )->x ) )

int vmMain( int command, int arg0, int arg1, int arg2 ) {
	switch ( command ) {
	case 1:	return OFF_ENT( health );
	case 2:	return OFF_CL( timeResidual );
	case 3:	return sizeof( gentity_t );
	case 4:	return sizeof( gclient_t );
	case 5:	return OFF_ENT( eventTime );
	}
	return -1;
}
