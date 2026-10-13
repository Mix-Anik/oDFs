/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"

//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
void Use_Target_Give( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t	*t;
	trace_t		trace;

	if ( !activator->client ) {
		return;
	}

	if ( !ent->target ) {
		return;
	}

	memset( &trace, 0, sizeof( trace ) );
	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		if ( !t->item ) {
			continue;
		}
		// DeFRaG (0x252fc): make the item available to this client for the touch, then mark it taken
		t->perClientPickupTime[activator->client->ps.clientNum] = 0;
		Touch_Item( t, activator, &trace );
		t->perClientPickupTime[activator->client->ps.clientNum] = -1;

		// make sure it isn't going to respawn or show any events
		t->nextthink = 0;
		trap_UnlinkEntity( t );
	}
}

void SP_target_give( gentity_t *ent ) {
	ent->use = Use_Target_Give;
}


//==========================================================

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8)
takes away all the activators powerups.
Used to drop flight powerups into death puts.
*/
void Use_target_remove_powerups( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if( !activator->client ) {
		return;
	}

	if( activator->client->ps.powerups[PW_REDFLAG] ) {
		Team_ReturnFlag( TEAM_RED );
	} else if( activator->client->ps.powerups[PW_BLUEFLAG] ) {
		Team_ReturnFlag( TEAM_BLUE );
	} else if( activator->client->ps.powerups[PW_NEUTRALFLAG] ) {
		Team_ReturnFlag( TEAM_FREE );
	}

	memset( activator->client->ps.powerups, 0, sizeof( activator->client->ps.powerups ) );
}

void SP_target_remove_powerups( gentity_t *ent ) {
	ent->use = Use_target_remove_powerups;
}


//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8)
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
void Think_Target_Delay( gentity_t *ent ) {
	G_UseTargets( ent, ent->activator );
}

void Use_Target_Delay( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
	ent->think = Think_Target_Delay;
	ent->activator = activator;
}

void SP_target_delay( gentity_t *ent ) {
	// check delay for backwards compatibility
	if ( !G_SpawnFloat( "delay", "0", &ent->wait ) ) {
		G_SpawnFloat( "wait", "1", &ent->wait );
	}

	if ( !ent->wait ) {
		ent->wait = 1;
	}
	ent->use = Use_Target_Delay;
}


//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void Use_Target_Score (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	// DeFRaG (0x25457): adds straight to PERS_SCORE -- no AddScore() gating (warmup / defrag mode),
	// no score plum, no CalculateRanks(). target_fragsFilter polls this value.
	if ( !activator || !activator->client ) {
		return;
	}
	activator->client->ps.persistant[PERS_SCORE] += ent->count;
}

void SP_target_score( gentity_t *ent ) {
	if ( !ent->count ) {
		ent->count = 1;
	}
	ent->use = Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) redteam blueteam private
"message"	text to print
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	// DeFRaG (0x25485, see disasm/G_TRIGGER/target_smallprint.md): target_print sends the
	// small "cpsm" print: spawnflags 8 = everybody, otherwise only the activator.
	// The vanilla team (1/2) and private (4) flags are not honoured.
	if ( ent->spawnflags & 8 ) {
		trap_SendServerCommand( -1, va( "cpsm \"%s\"", ent->message ? ent->message : "" ) );
		return;
	}
	if ( !activator ) {
		return;
	}
	trap_SendServerCommand( activator - g_entities, va( "cpsm \"%s\"", ent->message ? ent->message : "" ) );
}

void SP_target_print( gentity_t *ent ) {
	ent->use = Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"		wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"	wait variance, default is 0
*/
void Use_Target_Speaker (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	if (ent->spawnflags & 3) {	// looping sound toggles
		if (ent->s.loopSound)
			ent->s.loopSound = 0;	// turn it off
		else
			ent->s.loopSound = ent->noise_index;	// start it
	}else {	// normal sound
		if ( ent->spawnflags & 8 ) {
			G_AddEvent( activator, EV_GENERAL_SOUND, ent->noise_index );
		} else if (ent->spawnflags & 4) {
			G_AddEvent( ent, EV_GLOBAL_SOUND, ent->noise_index );
		} else {
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->noise_index );
		}
	}
}

void SP_target_speaker( gentity_t *ent ) {
	char	buffer[MAX_QPATH];
	char	*s;

	G_SpawnFloat( "wait", "0", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	if ( !G_SpawnString( "noise", "NOSOUND", &s ) ) {
		G_Error( "target_speaker without a noise key at %s", vtos( ent->s.origin ) );
	}

	// force all client relative sounds to be "activator" speakers that
	// play on the entity that activates it
	if ( s[0] == '*' ) {
		ent->spawnflags |= 8;
	}

	if (!strstr( s, ".wav" )) {
		Com_sprintf (buffer, sizeof(buffer), "%s.wav", s );
	} else {
		Q_strncpyz( buffer, s, sizeof(buffer) );
	}
	ent->noise_index = G_SoundIndex(buffer);

	// a repeating speaker can be done completely client side
	ent->s.eType = ET_SPEAKER;
	ent->s.eventParm = ent->noise_index;
	ent->s.frame = ent->wait * 10;
	ent->s.clientNum = ent->random * 10;


	// check for prestarted looping sound
	if ( ent->spawnflags & 1 ) {
		ent->s.loopSound = ent->noise_index;
	}

	ent->use = Use_Target_Speaker;

	if (ent->spawnflags & 4) {
		ent->r.svFlags |= SVF_BROADCAST;
	}

	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	trap_LinkEntity( ent );
}



//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON
When triggered, fires a laser.  You can either set a target or a direction.
*/
void target_laser_think (gentity_t *self) {
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;

	// if pointed at another entity, set movedir to point at it
	if ( self->enemy ) {
		VectorMA (self->enemy->s.origin, 0.5, self->enemy->r.mins, point);
		VectorMA (point, 0.5, self->enemy->r.maxs, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
	}

	// fire forward and see what we hit
	VectorMA (self->s.origin, 2048, self->movedir, end);

	trap_Trace( &tr, self->s.origin, NULL, NULL, end, self->s.number, CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE);

	if ( tr.entityNum ) {
		// hurt it if we can
		G_Damage ( &g_entities[tr.entityNum], self, self->activator, self->movedir, 
			tr.endpos, self->damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);
	}

	VectorCopy (tr.endpos, self->s.origin2);

	trap_LinkEntity( self );
	self->nextthink = level.time + FRAMETIME;
}

void target_laser_on (gentity_t *self)
{
	if (!self->activator)
		self->activator = self;
	target_laser_think (self);
}

void target_laser_off (gentity_t *self)
{
	trap_UnlinkEntity( self );
	self->nextthink = 0;
}

void target_laser_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	self->activator = activator;
	if ( self->nextthink > 0 )
		target_laser_off (self);
	else
		target_laser_on (self);
}

void target_laser_start (gentity_t *self)
{
	gentity_t *ent;

	self->s.eType = ET_BEAM;

	if (self->target) {
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent) {
			G_Printf ("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
		}
		self->enemy = ent;
	} else {
		G_SetMovedir (self->s.angles, self->movedir);
	}

	self->use = target_laser_use;
	self->think = target_laser_think;

	if ( !self->damage ) {
		self->damage = 1;
	}

	if (self->spawnflags & 1)
		target_laser_on (self);
	else
		target_laser_off (self);
}

void SP_target_laser (gentity_t *self)
{
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextthink = level.time + FRAMETIME;
}


//==========================================================

void target_teleporter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gentity_t	*dest;

	if (!activator->client)
		return;
	dest = 	G_PickTarget( self->target );
	if (!dest) {
		G_Printf ("Couldn't find teleporter destination\n");
		return;
	}

	// DeFRaG (0x25881): spawnflags 1 = keep speed. The pre-teleport speed is applied along
	// the destination's facing after TeleportPlayer() has set its fixed exit velocity.
	if ( self->spawnflags & 1 ) {
		float	speed = VectorLength( activator->client->ps.velocity );

		TeleportPlayer( activator, dest->s.origin, dest->s.angles );
		VectorNormalize( activator->client->ps.velocity );
		VectorScale( activator->client->ps.velocity, speed, activator->client->ps.velocity );
		return;
	}

	TeleportPlayer( activator, dest->s.origin, dest->s.angles );
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8) KEEP_SPEED
The activator will be teleported away.
KEEP_SPEED (DeFRaG): leave with the speed the activator had, instead of the fixed exit speed.
*/
void SP_target_teleporter( gentity_t *self ) {
	if (!self->targetname)
		G_Printf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	self->use = target_teleporter_use;
}

//==========================================================


/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them
*/
void target_relay_use (gentity_t *self, gentity_t *other, gentity_t *activator) {
	if ( ( self->spawnflags & 1 ) && activator->client 
		&& activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( self->spawnflags & 2 ) && activator->client 
		&& activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}
	if ( self->spawnflags & 4 ) {
		gentity_t	*ent;

		ent = G_PickTarget( self->target );
		if ( ent && ent->use ) {
			ent->use( ent, self, activator );
		}
		return;
	}
	G_UseTargets (self, activator);
}

void SP_target_relay (gentity_t *self) {
	self->use = target_relay_use;
}


//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void target_kill_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	G_Damage ( activator, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_target_kill( gentity_t *self ) {
	self->use = target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void SP_target_position( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}

static void target_location_linkup(gentity_t *ent)
{
	int i;
	int n;

	if (level.locationLinked) 
		return;

	level.locationLinked = qtrue;

	level.locationHead = NULL;

	trap_SetConfigstring( CS_LOCATIONS, "unknown" );

	for (i = 0, ent = g_entities, n = 1;
			i < level.num_entities;
			i++, ent++) {
		if (ent->classname && !Q_stricmp(ent->classname, "target_location")) {
			// lets overload some variables!
			ent->health = n; // use for location marking
			trap_SetConfigstring( CS_LOCATIONS + n, ent->message );
			n++;
			ent->nextTrain = level.locationHead;
			level.locationHead = ent;
		}
	}

	// All linked together now
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void SP_target_location( gentity_t *self ){
	self->think = target_location_linkup;
	self->nextthink = level.time + 200;  // Let them all spawn first

	G_SetOrigin( self, self->s.origin );
}


//==========================================================
// DeFRaG target_multimanager (func_0x261f7 spawn, 0x26024 use, 0x25f29 delayed fire, 0x25f76 random pick)

#define MM_MAX_TARGETS	40

/*
Choose k distinct entries out of n (n <= MM_MAX_TARGETS) uniformly at random; sel[i] = 1 for chosen ones.
*/
static void MM_SelectRandom( int *sel, int k, int n ) {
	int pool[MM_MAX_TARGETS];
	int i, remaining, r;

	if ( n > MM_MAX_TARGETS ) {
		n = MM_MAX_TARGETS;
	}
	if ( k > n ) {
		k = n;
	}
	if ( k == n ) {
		for ( i = 0; i < n; i++ ) {
			sel[i] = 1;
		}
		return;
	}
	for ( i = 0; i < n; i++ ) {
		pool[i] = i;
		sel[i] = 0;
	}
	remaining = n;
	while ( k > 0 ) {
		r = rand() % remaining;
		sel[pool[r]] = 1;
		k--;
		remaining--;
		for ( i = r; i < remaining; i++ ) {
			pool[i] = pool[i + 1];
		}
	}
}

static void MM_DelayedFire( gentity_t *self ) {
	gentity_t *mm = self->mmOwner;

	G_UseTargets( self, self->activator );
	self->nextthink = level.time + 100;
	self->think = G_FreeEntity;
	if ( mm ) {
		mm->mmRemaining--;
		if ( mm->mmRemaining == 0 ) {
			mm->activator = NULL;
		}
	}
}

/*QUAKED target_multimanager (1 0 0) (-8 -8 -8) (8 8 8) REDONLY BLUEONLY ADDWAIT STAGGER
Fires many targets. Keys "target01".."target40": "[delay:]targetname" (delay in seconds, default = "wait").
"count" how many of the listed targets (picked at random) to fire per use, default -1 = all.
"wait" default delay (1), "random" delay variance.
spawnflags: 1 only red-team activators, 2 only blue-team activators, 4 add "wait" to explicit delays, 8 stagger default delays by wait*(index+1).
*/
void Use_Target_Multimanager( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	int			sel[MM_MAX_TARGETS];
	char		*p;
	int			index;
	float		delay;

	if ( ( ent->spawnflags & 1 ) && activator && activator->client && activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( ent->spawnflags & 2 ) && activator && activator->client && activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}
	if ( ent->mmRemaining ) {
		return;		// still running a previous use
	}

	ent->activator = activator;
	ent->mmRemaining = ent->count & 0xff;
	MM_SelectRandom( sel, ent->mmRemaining, ent->count >> 8 );

	index = 0;
	p = ent->target;
	while ( p < ent->target + ent->mmTargetLen ) {
		char		*delayStr = p;
		char		*name;

		if ( strcmp( delayStr, "-1" ) == 0 ) {
			delay = ent->wait;
			if ( ent->spawnflags & 8 ) {
				delay = ent->wait * (float)( index + 1 );
			}
		} else {
			delay = atof( delayStr );
			if ( ent->spawnflags & 8 ) {
				delay += ent->wait * (float)( index + 1 );
			} else if ( ent->spawnflags & 4 ) {
				delay += ent->wait;
			}
		}
		p += strlen( delayStr ) + 1;
		name = p;
		p += strlen( name ) + 1;
		index++;

		if ( !sel[index - 1] ) {
			continue;
		}

		{
			gentity_t *t = G_PickTarget( name );

			if ( !t || !t->use ) {
				G_Printf( "Invalid target was specified in multimanager\n" );
				continue;
			}
			if ( delay <= 0.001f ) {
				ent->mmRemaining--;
				t->use( t, ent, activator );
			} else {
				gentity_t *d = G_Spawn();

				d->classname = "target_delay";
				d->mmOwner = ent;
				d->target = name;
				d->activator = activator;
				d->think = MM_DelayedFire;
				d->nextthink = level.time + (int)( 1000.0f * ( delay + ent->random * crandom() ) );
			}
		}
	}
}

void SP_target_multimanager( gentity_t *ent ) {
	char	buf[0x400 + 8];
	char	*str, *next;
	char	key[16];
	int		i, n, len, total, count, extra;

	G_SpawnFloat( "random", "0", &ent->random );
	G_SpawnFloat( "wait", "1", &ent->wait );
	G_SpawnInt( "count", "-1", &ent->count );

	if ( ent->count == 0 ) {
		G_Printf( "Warning: target_multimanager \"%s\" with count 0. No targets will be triggered!\n",
			ent->targetname ? ent->targetname : "unnamed" );
		ent->nextthink = level.time + 100;
		ent->think = G_FreeEntity;
		return;
	}

	ent->mmRemaining = 0;
	ent->mmTargetLen = 0;
	buf[0] = '\0';
	n = 0;
	total = 0;
	next = buf;
	for ( i = 0; i < MM_MAX_TARGETS; i++ ) {
		Com_sprintf( key, sizeof( key ), "target%02d", i + 1 );
		if ( !G_SpawnString( key, "", &str ) ) {
			continue;
		}
		len = strlen( str );
		if ( len == 0 ) {
			G_Printf( "Empty target-string for target%02d of multimanager \"%s\".\n", i + 1,
				ent->targetname ? ent->targetname : "unnamed" );
			continue;
		}
		extra = ( strchr( str, ':' ) == NULL ) ? 3 : 0;		// "-1:" default-delay prefix
		len += 1 + extra;
		if ( total + len > 0x400 ) {
			G_Printf( "Too much memory allocated for target_multimanager \"%s\"\n",
				ent->targetname ? ent->targetname : "unnamed" );
			ent->nextthink = level.time + 100;
			ent->think = G_FreeEntity;
			return;
		}
		if ( extra ) {
			strcat( next, "-1:" );
		}
		strcat( next, str );
		strcat( next, ":" );
		next += len;
		total += len;
		n++;
	}
	if ( n == 0 ) {
		ent->nextthink = level.time + 100;
		ent->think = G_FreeEntity;
		return;
	}

	count = ent->count;
	if ( count < 0 || count > n ) {
		count = n;
	}
	ent->count = count | ( n << 8 );

	ent->mmTargetLen = total;
	ent->target = G_Alloc( total + 1 );
	Q_strncpyz( ent->target, buf, total );	// drops the trailing ':' like DeFRaG (destsize = strlen)
	for ( i = 0; i < (int)strlen( buf ); i++ ) {
		if ( ent->target[i] == ':' ) {
			ent->target[i] = '\0';
		}
	}
	ent->use = Use_Target_Multimanager;
}


//==========================================================
// DeFRaG target_speed (use = func_0x25aa7, spawn = 0x25f14)

/*QUAKED target_speed (1 0 0) (-8 -8 -8) (8 8 8) PERCENT ADD +X -X +Y -Y +Z -Z ABSOLUTE
Modifies the activator's velocity. "speed" (default 100). Flag names are inferred from the bytecode structure
(disasm/G_TRIGGER/target_speed.md): 1 percent, 2 add (instead of set), 4/8 +X/-X, 16/32 +Y/-Y, 64/128 +Z/-Z, 256 special mode.
Axes with neither flag are ignored.
*/
void Use_Target_Speed( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	const int	f = ent->spawnflags;
	qboolean	posf[3], negf[3], sel[3];
	vec3_t		vel, sgn, A, B, V;
	float		len, M;
	int			i, n;

	posf[0] = ( f & 4 ) != 0;	negf[0] = ( f & 8 ) != 0;
	posf[1] = ( f & 0x10 ) != 0;	negf[1] = ( f & 0x20 ) != 0;
	posf[2] = ( f & 0x40 ) != 0;	negf[2] = ( f & 0x80 ) != 0;
	for ( i = 0; i < 3; i++ ) {
		sel[i] = posf[i] || negf[i];
	}

	if ( !activator || !activator->client ) {
		return;
	}

	VectorCopy( activator->client->ps.velocity, vel );
	for ( i = 0; i < 3; i++ ) {
		if ( !sel[i] ) {
			vel[i] = 0;
		}
		sgn[i] = ( fabs( vel[i] ) >= 1e-5f ) ? vel[i] / fabs( vel[i] ) : 0.0f;
		A[i] = B[i] = 1.0f;
		if ( posf[i] && negf[i] ) {
			A[i] = 1.0f;		B[i] = sgn[i];
		} else if ( posf[i] ) {
			A[i] = sgn[i];		B[i] = 1.0f;
		} else if ( negf[i] ) {
			A[i] = -sgn[i];		B[i] = -1.0f;
		}
	}

	len = VectorLength( vel );
	V[0] = V[1] = V[2] = len;

	for ( i = 0; i < 3; i++ ) {
		if ( f & 2 ) {
			if ( f & 0x100 ) {
				A[i] = 1.0f;
			}
			if ( f & 1 ) {
				V[i] = V[i] + 0.01f * ( A[i] * V[i] * ent->speed );
			} else {
				V[i] = V[i] + A[i] * ent->speed;
			}
		} else {
			if ( ent->speed < 0 ) {
				ent->speed = 0;
			}
			if ( f & 1 ) {
				V[i] = V[i] * ( 0.01f * ent->speed );
			} else {
				V[i] = ent->speed;
			}
		}
		V[i] *= B[i];
		if ( fabs( vel[i] ) > 1e-5f ) {
			vel[i] = fabs( vel[i] );
		}
	}

	if ( fabs( len ) > 1e-5f ) {
		for ( i = 0; i < 3; i++ ) {
			vel[i] /= fabs( len );
		}
	}

	if ( f & 0x100 ) {
		n = 0;
		for ( i = 0; i < 3; i++ ) {
			if ( posf[i] != negf[i] ) {
				n++;
			}
		}
		if ( n == 0 ) {
			return;
		}
		M = fabs( V[0] );
		if ( fabs( V[1] ) > M ) M = fabs( V[1] );
		if ( fabs( V[2] ) > M ) M = fabs( V[2] );
		M = sqrt( M * M * n );
		for ( i = 0; i < 3; i++ ) {
			if ( posf[i] != negf[i] ) {
				activator->client->ps.velocity[i] = posf[i] ? M : -M;
			}
		}
	} else {
		for ( i = 0; i < 3; i++ ) {
			if ( sel[i] ) {
				activator->client->ps.velocity[i] = vel[i] * V[i];
			}
		}
	}
}

void SP_target_speed( gentity_t *ent ) {
	G_SpawnFloat( "speed", "100", &ent->speed );
	ent->use = Use_Target_Speed;
}


//==========================================================
// DeFRaG target_smallprint (use = func_0x2bfb, spawn = 0x2c36)

/*QUAKED target_smallprint (1 0 0) (-8 -8 -8) (8 8 8) ALL_PLAYERS
"message" text shown with a small centre print ("cpsm" server command, client cvar df_noTargetPrint suppresses it).
Without spawnflag 8 only the activator sees it; with 8 everyone does.
*/
void Use_Target_SmallPrint( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if ( ent->spawnflags & 8 ) {
		trap_SendServerCommand( -1, va( "cpsm \"%s\"", ent->message ? ent->message : "" ) );
		return;
	}
	if ( !activator ) {
		return;
	}
	trap_SendServerCommand( activator - g_entities, va( "cpsm \"%s\"", ent->message ? ent->message : "" ) );
}

void SP_target_smallprint( gentity_t *ent ) {
	ent->use = Use_Target_SmallPrint;
}


//==========================================================
// DeFRaG target_init (use = func_0x2aa8, spawn = 0x2bf2)

void Team_ReturnFlag( int team );

/*QUAKED target_init (1 0 0) (-8 -8 -8) (8 8 8) KEEP_ARMOR KEEP_HEALTH KEEP_WEAPONS DROP_NOTHING KEEP_HOLDABLE
Resets the activator to its spawn state, then fires its targets. Each spawnflag keeps one thing:
1 armor, 2 health, 4 weapons/ammo, 8 powerups (and returns carried flags), 16 holdable item, 32 skip the machinegun when resetting weapons.
Armor/health are only reset when df_ndm_damage is set, weapons when df_ndm_weaponsPickup is set, powerups/holdable when df_ndm_powerups is set.
*/
void Use_Target_Init( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gclient_t	*client;
	int			i;

	if ( !activator || !activator->client ) {
		return;
	}
	client = activator->client;

	if ( !( ent->spawnflags & 1 ) && g_ndm.damage ) {
		client->ps.stats[STAT_ARMOR] = 0;
	}
	if ( !( ent->spawnflags & 2 ) && g_ndm.damage ) {
		int hp = client->ps.stats[STAT_MAX_HEALTH] + 25;

		activator->health = hp;
		client->ps.stats[STAT_HEALTH] = hp;
	}
	if ( !( ent->spawnflags & 4 ) && ( g_ndm.flags & 0x6 ) ) {
		for ( i = 0; i < 16; i++ ) {
			client->ps.ammo[i] = 0;
		}
		client->ps.stats[STAT_WEAPONS] = 2;
		client->ps.ammo[WP_GAUNTLET] = -1;
		if ( !( ent->spawnflags & 0x20 ) ) {
			client->ps.stats[STAT_WEAPONS] |= 4;
			client->ps.ammo[WP_MACHINEGUN] = 100;
		}
	}
	if ( !( ent->spawnflags & 8 ) && ( g_ndm.flags & 0x60 ) ) {
		if ( client->ps.powerups[PW_REDFLAG] ) {
			Team_ReturnFlag( TEAM_RED );
		} else if ( client->ps.powerups[PW_BLUEFLAG] ) {
			Team_ReturnFlag( TEAM_BLUE );
		} else if ( client->ps.powerups[PW_NEUTRALFLAG] ) {
			Team_ReturnFlag( TEAM_FREE );
		}
		memset( client->ps.powerups, 0, sizeof( client->ps.powerups ) );
	}
	if ( !( ent->spawnflags & 0x10 ) && ( g_ndm.flags & 0x60 ) ) {
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	G_UseTargets( ent, activator );
	client->ps.stats[STAT_DEFRAG_FLAGS] ^= 1;	// DeFRaG toggles bit 0 of stats[12] on every target_init use
}

void SP_target_init( gentity_t *ent ) {
	ent->use = Use_Target_Init;
}


//==========================================================
// DeFRaG target_fragsFilter (spawn = func_0x2a58, use = 0x2963, think = 0x2919, per-frame poll = tail of 0x259a)

#define MAX_FRAGSFILTERS	10

static gentity_t	*g_fragsFilters[MAX_FRAGSFILTERS];
static int			g_numFragsFilters;
static int			g_fragsMin;			// 0x131800: smallest "frags" of any filter feeding a target_stopTimer
static int			g_fragsMinTime;		// 0x131804: level.time + 200 when g_fragsMin was updated, 0 once published

void G_FragsFilterInit( void ) {
	memset( g_fragsFilters, 0, sizeof( g_fragsFilters ) );
	g_numFragsFilters = 0;
	g_fragsMin = 0;
	g_fragsMinTime = 0;
}

/*QUAKED target_fragsFilter (1 0 0) (-8 -8 -8) (8 8 8) SUBTRACT ? SHOW_NEEDED CLEAR EXACT
Fires its targets once the activator's score reaches "frags". Entities without a targetname are polled
every frame for every player. spawnflags: 1 subtract "frags" from the score when fired, 4 show a
"N more frags needed" small print while below the threshold, 8 reset the score to 0 when fired,
16 only fire on an exact score match. Feeds the smallest "frags" of filters that target a target_stopTimer
into the fraglimit cvar.
*/
void Use_Target_FragsFilter( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	int score;

	if ( !activator || !activator->client ) {
		return;
	}
	score = activator->client->ps.persistant[PERS_SCORE];

	if ( ( ent->spawnflags & 0x10 ) && score != ent->count ) {
		return;
	}
	if ( score < ent->count ) {
		if ( !( ent->spawnflags & 4 ) ) {
			return;
		}
		if ( ent->count - score == 1 ) {
			trap_SendServerCommand( activator - g_entities, va( "cpsm \"%i more frag needed\"\n", ent->count - score ) );
		} else {
			trap_SendServerCommand( activator - g_entities, va( "cpsm \"%i more frags needed\"\n", ent->count - score ) );
		}
		return;
	}

	if ( ent->fragsRegistered ) {
		// registered filters are polled every frame: fire once, then stay quiet until polling stops for a second
		if ( ent->wait > (float)level.time ) {
			ent->wait = level.time + 1000;
			return;
		}
		ent->wait = level.time + 1000;
	}

	if ( ent->spawnflags & 1 ) {
		activator->client->ps.persistant[PERS_SCORE] -= ent->count;
	}
	if ( ent->spawnflags & 8 ) {
		activator->client->ps.persistant[PERS_SCORE] = 0;
	}
	G_UseTargets( ent, activator );
}

static void Think_Target_FragsFilter( gentity_t *ent ) {
	gentity_t	*t = NULL;

	// 0x2919: every entity this filter targets that is a target_stopTimer lowers the published frag limit
	while ( ( t = G_Find( t, FOFS( targetname ), ent->target ) ) != NULL ) {
		if ( t->classname && !strcmp( t->classname, "target_stopTimer" ) ) {
			if ( g_fragsMin > ent->count || g_fragsMin == 0 ) {
				g_fragsMin = ent->count;
			}
			g_fragsMinTime = level.time + 200;
		}
	}
}

void SP_target_fragsFilter( gentity_t *ent ) {
	char	*s;
	qboolean found;

	ent->use = Use_Target_FragsFilter;
	G_SpawnInt( "frags", "0", &ent->count );
	found = G_SpawnString( "targetname", "decoy", &s );
	if ( !found && g_numFragsFilters < MAX_FRAGSFILTERS ) {
		g_fragsFilters[g_numFragsFilters++] = ent;
		ent->fragsRegistered = qtrue;
	}
	ent->think = Think_Target_FragsFilter;
	ent->nextthink = level.time + 400;
}

// called for every non-spectator client at the end of its frame
void G_FragsFilterCheck( gentity_t *clientEnt ) {
	int i;

	for ( i = 0; i < g_numFragsFilters; i++ ) {
		gentity_t *f = g_fragsFilters[i];

		if ( f && f->inuse && clientEnt->client->ps.persistant[PERS_SCORE] >= f->count ) {
			f->use( f, NULL, clientEnt );
		}
	}
}

// DeFRaG func_0x23f2: publish the smallest frags threshold as fraglimit, 200 ms after it was (re)computed
void G_FragsFilterPublish( void ) {
	if ( g_fragsMinTime != 0 && level.time > g_fragsMinTime ) {
		g_fragsMinTime = 0;
		trap_Cvar_Set( "fraglimit", va( "%i", g_fragsMin ) );
	}
}
