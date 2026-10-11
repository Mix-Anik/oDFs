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


void InitTrigger( gentity_t *self ) {
	if (!VectorCompare (self->s.angles, vec3_origin))
		G_SetMovedir (self->s.angles, self->movedir);

	trap_SetBrushModel( self, self->model );
	self->r.contents = CONTENTS_TRIGGER;		// replaces the -1 from trap_SetBrushModel
	self->r.svFlags = SVF_NOCLIENT;
}


// the wait time has passed, so set back up for another activation
void multi_wait( gentity_t *ent ) {
	ent->nextthink = 0;
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger( gentity_t *ent, gentity_t *activator ) {
	ent->activator = activator;
	if ( ent->nextthink ) {
		return;		// can't retrigger until the wait is over
	}

	if ( activator->client ) {
		if ( ( ent->spawnflags & 1 ) &&
			activator->client->sess.sessionTeam != TEAM_RED ) {
			return;
		}
		if ( ( ent->spawnflags & 2 ) &&
			activator->client->sess.sessionTeam != TEAM_BLUE ) {
			return;
		}
	}

	G_UseTargets (ent, ent->activator);

	if ( ent->wait > 0 ) {
		ent->think = multi_wait;
		ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
	} else {
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = 0;
		ent->nextthink = level.time + FRAMETIME;
		ent->think = G_FreeEntity;
	}
}

void Use_Multi( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	multi_trigger( ent, activator );
}

void Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if( !other->client ) {
		return;
	}
	multi_trigger( self, other );
}

/*QUAKED trigger_multiple (.5 .5 .5) ? RED_ONLY BLUE_ONLY
"wait" : Seconds between triggerings, 0.5 default, -1 = one time only.
"random"	wait variance, default is 0
Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)
*/
void SP_trigger_multiple( gentity_t *ent ) {
	G_SpawnFloat( "wait", "0.5", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	if ( ent->random >= ent->wait && ent->wait >= 0 ) {
		ent->random = ent->wait - FRAMETIME;
		G_Printf( "trigger_multiple has random >= wait\n" );
	}

	ent->touch = Touch_Multi;
	ent->use = Use_Multi;

	InitTrigger( ent );
	trap_LinkEntity (ent);
}



/*
==============================================================================

trigger_always

==============================================================================
*/

void trigger_always_think( gentity_t *ent ) {
	G_UseTargets(ent, ent);
	G_FreeEntity( ent );
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (gentity_t *ent) {
	// we must have some delay to make sure our use targets are present
	ent->nextthink = level.time + 300;
	ent->think = trigger_always_think;
}


/*
==============================================================================

trigger_push

==============================================================================
*/

void trigger_push_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {

	if ( !other->client ) {
		return;
	}

	// DeFRaG "no jump pads" map restriction: when "df_ndm_nopad" is set to a
	// value other than 0 or 1 (typically by a map wanting to forbid pad use
	// entirely, e.g. for a no-pad race category), touching any jump pad
	// instantly kills the player instead of launching them. Confirmed from
	// qagame.qvm disassembly (trigger_push_touch, 0x278ef, calling G_Damage
	// at 0x2015b with these exact arguments). See
	// disasm/BG_MISC/BG_TouchJumpPad.md.
	if ( df_ndm_nopad.integer != 0 && df_ndm_nopad.integer != 1 ) {
		G_Damage( other, self, self, NULL, NULL, 9999, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT );
	}

	BG_TouchJumpPad( &other->client->ps, &self->s );
}


/*
=================
AimAtTarget

Calculate origin2 so the target apogee will be hit
=================
*/
void AimAtTarget( gentity_t *self ) {
	gentity_t	*ent;
	vec3_t		origin;
	float		height, gravity, time, forward;
	float		dist;

	VectorAdd( self->r.absmin, self->r.absmax, origin );
	VectorScale ( origin, 0.5, origin );

	ent = G_PickTarget( self->target );
	if ( !ent ) {
		G_FreeEntity( self );
		return;
	}

	height = ent->s.origin[2] - origin[2];
	gravity = g_gravity.value;
	time = sqrt( height / ( .5 * gravity ) );
	if ( !time ) {
		G_FreeEntity( self );
		return;
	}

	// set s.origin2 to the push velocity
	VectorSubtract ( ent->s.origin, origin, self->s.origin2 );
	self->s.origin2[2] = 0;
	dist = VectorNormalize( self->s.origin2);

	forward = dist / time;
	VectorScale( self->s.origin2, forward, self->s.origin2 );

	self->s.origin2[2] = time * gravity;
}


/*QUAKED trigger_push (.5 .5 .5) ?
Must point at a target_position, which will be the apex of the leap.
This will be client side predicted, unlike target_push
*/
void SP_trigger_push( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	self->r.svFlags &= ~SVF_NOCLIENT;

	// make sure the client precaches this sound
	G_SoundIndex("sound/world/jumppad.wav");

	self->s.eType = ET_PUSH_TRIGGER;
	self->touch = trigger_push_touch;
	self->think = AimAtTarget;
	self->nextthink = level.time + FRAMETIME;
	trap_LinkEntity (self);
}


/*QUAKED trigger_push_velocity (.5 .5 .5) ?
DeFRaG addition. Identical to trigger_push (must point at a target_position),
except the entity's "Spawnflags" checkboxes, "random" key, and "count" key
configure BG_TouchJumpPad's custom push-direction/velocity modes instead of
relying solely on the fixed AimAtTarget trajectory vector. Confirmed from
qagame.qvm disassembly (SP_trigger_push_velocity, 0x27a76) — see
disasm/BG_MISC/BG_TouchJumpPad.md for the full bit-by-bit semantics consumed
on the client/prediction side.
*/
void SP_trigger_push_velocity( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	self->r.svFlags &= ~SVF_NOCLIENT;

	// make sure the client precaches this sound
	G_SoundIndex("sound/world/jumppad.wav");

	self->s.eType = ET_PUSH_TRIGGER;
	self->touch = trigger_push_touch;
	self->think = AimAtTarget;
	self->nextthink = level.time + FRAMETIME;

	// DeFRaG custom jumppad mode config, consumed by BG_TouchJumpPad:
	// spawnflags -> weapon (mode bitmask), random -> angles2[0]/[1],
	// count -> angles2[2] (int-to-float)
	self->s.weapon = self->spawnflags;
	self->s.angles2[0] = self->random;
	self->s.angles2[1] = self->random;
	self->s.angles2[2] = (float)self->count;

	trap_LinkEntity (self);
}


void Use_target_push( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !activator->client ) {
		return;
	}

	if ( activator->client->ps.pm_type != PM_NORMAL ) {
		return;
	}
	if ( activator->client->ps.powerups[PW_FLIGHT] ) {
		return;
	}

	VectorCopy (self->s.origin2, activator->client->ps.velocity);

	// play fly sound every 1.5 seconds
	if ( activator->fly_sound_debounce_time < level.time ) {
		activator->fly_sound_debounce_time = level.time + 1500;
		G_Sound( activator, CHAN_AUTO, self->noise_index );
	}
}

/*QUAKED target_push (.5 .5 .5) (-8 -8 -8) (8 8 8) bouncepad
Pushes the activator in the direction.of angle, or towards a target apex.
"speed"		defaults to 1000
if "bouncepad", play bounce noise instead of windfly
*/
void SP_target_push( gentity_t *self ) {
	if (!self->speed) {
		self->speed = 1000;
	}
	G_SetMovedir (self->s.angles, self->s.origin2);
	VectorScale (self->s.origin2, self->speed, self->s.origin2);

	if ( self->spawnflags & 1 ) {
		self->noise_index = G_SoundIndex("sound/world/jumppad.wav");
	} else {
		self->noise_index = G_SoundIndex("sound/misc/windfly.wav");
	}
	if ( self->target ) {
		VectorCopy( self->s.origin, self->r.absmin );
		VectorCopy( self->s.origin, self->r.absmax );
		self->think = AimAtTarget;
		self->nextthink = level.time + FRAMETIME;
	}
	self->use = Use_target_push;
}

/*
==============================================================================

trigger_teleport

==============================================================================
*/

void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	gentity_t	*dest;

	if ( !other->client ) {
		return;
	}
	if ( other->client->ps.pm_type == PM_DEAD ) {
		return;
	}
	// Spectators only?
	if ( ( self->spawnflags & 1 ) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return;
	}

	// DeFRaG "no teleporters" map restriction, parallel to trigger_push_touch's
	// "df_ndm_nopad" gate (see disasm/G_TRIGGER/trigger_push_touch.md):
	// "df_ndm_noteleport" == 1 disables the teleporter entirely (touching it
	// does nothing); >= 2 (or negative) instantly kills the toucher instead of
	// teleporting them. Unlike the jump pad case, the kill here REPLACES the
	// teleport rather than preceding it. Confirmed from qagame.qvm
	// disassembly (trigger_teleporter_touch, 0x27bf9). See
	// disasm/G_TRIGGER/trigger_teleporter_touch.md.
	if ( df_ndm_noteleport.integer == 1 ) {
		return;
	}
	if ( df_ndm_noteleport.integer != 0 ) {
		G_Damage( other, self, self, NULL, NULL, 9999, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT );
		return;
	}

	dest = 	G_PickTarget( self->target );
	if (!dest) {
		G_Printf ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( other, dest->s.origin, dest->s.angles );
}


/*QUAKED trigger_teleport (.5 .5 .5) ? SPECTATOR
Allows client side prediction of teleportation events.
Must point at a target_position, which will be the teleport destination.

If spectator is set, only spectators can use this teleport
Spectator teleporters are not normally placed in the editor, but are created
automatically near doors to allow spectators to move through them
*/
void SP_trigger_teleport( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	// unless is a spectator trigger
	if ( self->spawnflags & 1 ) {
		self->r.svFlags |= SVF_NOCLIENT;
	} else {
		self->r.svFlags &= ~SVF_NOCLIENT;
	}

	// make sure the client precaches this sound
	G_SoundIndex("sound/world/jumppad.wav");

	self->s.eType = ET_TELEPORT_TRIGGER;
	self->touch = trigger_teleporter_touch;

	trap_LinkEntity (self);
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF - SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.
It does dmg points of damage each server frame
Targeting the trigger will toggle its on / off state.

SILENT			suppresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)

*/
void hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( self->r.linked ) {
		trap_UnlinkEntity( self );
	} else {
		trap_LinkEntity( self );
	}
}

void hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	int		dflags;

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if ( self->spawnflags & 16 ) {
		self->timestamp = level.time + 1000;
	} else {
		self->timestamp = level.time + FRAMETIME;
	}

	// play sound
	if ( !(self->spawnflags & 4) ) {
		G_Sound( other, CHAN_AUTO, self->noise_index );
	}

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;
	G_Damage (other, self, self, NULL, NULL, self->damage, dflags, MOD_TRIGGER_HURT);
}

void SP_trigger_hurt( gentity_t *self ) {
	InitTrigger (self);

	self->noise_index = G_SoundIndex( "sound/world/electro.wav" );
	self->touch = hurt_touch;

	if ( !self->damage ) {
		self->damage = 5;
	}

	self->use = hurt_use;

	// link in to the world if starting active
	if ( self->spawnflags & 1 ) {
		trap_UnlinkEntity (self);
	}
	else {
		trap_LinkEntity (self);
	}
}


/*
==============================================================================

timer

==============================================================================
*/


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
This should be renamed trigger_timer...
Repeatedly fires its targets.
Can be turned on or off by using.

"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

*/
void func_timer_think( gentity_t *self ) {
	G_UseTargets (self, self->activator);
	// set time before next firing
	self->nextthink = level.time + 1000 * ( self->wait + crandom() * self->random );
}

void func_timer_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	self->activator = activator;

	// if on, turn it off
	if ( self->nextthink ) {
		self->nextthink = 0;
		return;
	}

	// turn it on
	func_timer_think (self);
}

void SP_func_timer( gentity_t *self ) {
	G_SpawnFloat( "random", "1", &self->random);
	G_SpawnFloat( "wait", "1", &self->wait );

	self->use = func_timer_use;
	self->think = func_timer_think;

	if ( self->random >= self->wait ) {
		self->random = self->wait - FRAMETIME;
		G_Printf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
	}

	if ( self->spawnflags & 1 ) {
		self->nextthink = level.time + FRAMETIME;
		self->activator = self;
	}

	self->r.svFlags = SVF_NOCLIENT;
}

// ─── DeFRaG race timer entities ──────────────────────────────────────────────
//
// Three point entities placed by mappers, fired via trigger_multiple:
//   target_startTimer  — begins the player's race timer
//   target_stopTimer   — stops the timer and records the result
//   target_checkpoint  — registers a mid-run checkpoint passage
//
// Each entity only needs a USE callback; no model or bbox is needed.
// The g_raceTimers state machine lives in g_defrag.c.

static void use_startTimer( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !g_startTimerReady ) return;
	if ( !activator || !activator->client ) return;
	G_StartRace( activator );
}

static void use_stopTimer( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !g_stopTimerReady ) return;
	if ( !activator || !activator->client ) return;
	G_StopRace( activator );
}

static void use_checkpoint( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !g_checkpointReady ) return;
	if ( !activator || !activator->client ) return;
	G_Checkpoint( activator, self->count );
}

/*QUAKED target_startTimer (0 0.5 0) (-8 -8 -8) (8 8 8)
DeFRaG race timer start entity. Place as target of a trigger_multiple brush.
When fired, starts the player's race timer.
*/
void SP_target_startTimer( gentity_t *ent ) {
	ent->use          = use_startTimer;
	g_startTimerReady = qtrue;
	// Signal to DeFRaG cgame that a start timer exists — cgame reads this
	// SERVERINFO cvar via trap_CvarVariableStringBuffer; "df" prefix = active.
	trap_Cvar_Set( "df_ndm_timer_start", "df" );
}

/*QUAKED target_stopTimer (0.5 0 0) (-8 -8 -8) (8 8 8)
DeFRaG race timer stop entity. Place as target of a trigger_multiple brush.
When fired, stops the player's timer and broadcasts the result.
*/
void SP_target_stopTimer( gentity_t *ent ) {
	ent->use         = use_stopTimer;
	g_stopTimerReady = qtrue;
	trap_Cvar_Set( "df_ndm_timer_stop", "df" );
}

/*QUAKED target_checkpoint (0 0 0.5) (-8 -8 -8) (8 8 8)
DeFRaG race checkpoint. Place as target of a trigger_multiple brush.
Each entity gets a unique index stored in ent->count.
*/
void SP_target_checkpoint( gentity_t *ent ) {
	ent->use          = use_checkpoint;
	ent->count        = g_checkpointCount++;
	g_checkpointReady = qtrue;
	trap_Cvar_Set( "df_ndm_timer_checkpoint", "df" );
}


