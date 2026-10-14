// cg_dfghost.c -- DeFRaG ghosts (cgame 0x127af / 0x12785 / 0x12620 / 0x13e09...; see disasm/CGAME/ghost_system.md)
//
// Ported: the `ghost #N|#all stop|record|play [name]` command, the recorder (player pose + animation state sampled from
// cg.predictedPlayerEntity every df_ghostN_RecordInterval ms), the DeFRaG ghost file (header + packet stream, types 2/3/4/7;
// types 5/6/8 of files written by DeFRaG are skipped when reading), time-synchronised playback with interpolation, loop
// handling, and drawing through the normal CG_Player (DrawPlayer modes 1/2/3, NoDrawRadius).
// NOT ported: recorded missiles (type 5) and sprites (type 6), monitored variables (type 8), the sprite/label (0x12fc5),
// DrawPlayer mode 4 (marker), miniview (0x12a10), AutoMode, play camera modes, negative play timescale, the animation timescale
// field (DeFRaG's extra centity float at +624), the `demo` command, DeFRaG's status texts (0x13721; ours are our own wording).
// Assumptions (no decode): files live in system/ghosts/<name>.<df_ghosts_FileExtension, "ghost" when empty>; the default name is
// <mapname>_<NN>; slots 0/1 read df_ghost1_* / df_ghost2_*, slots 2..4 use the defaults.

#include "cg_local.h"

#define DG_SLOTS		5
#define DG_FILE_MAGIC	1
#define DG_FILE_VERSION	1016

typedef struct {
	int			num, weapon;
	vec3_t		org, vel;
} dgMissile_t;

typedef struct {
	vec3_t		org;
	int			code;		// 4 grenade / 5 rocket / 8 plasma / 9 bfg explosion
} dgSprite_t;

typedef struct {
	int			time;
	vec3_t		origin, angles;
	int			weapon, torsoAnim, legsAnim;
	int			ducked, discont;
	lerpFrame_t	torso, legs;
	dgMissile_t	mis[10];
	int			numMis;
	dgSprite_t	spr[10];
	int			numSpr;
} dgFrame_t;

typedef struct {
	int			state;				// 0 idle, 1 recording, 2 playing (slot+164)
	fileHandle_t f;
	int			remain;				// bytes left to read (playing)
	int			lastTime;			// recording: last sample time; playing: anchor of the current frame
	int			discont;			// recording: pending discontinuity (teleport) flag
	qboolean	haveNext;
	dgFrame_t	a, b;				// playing: current and next frame
	dgFrame_t	p;				// playing: the frame before the current one (velocity, CHS items)
	centity_t	cent;
	qboolean	centInit;
	char		name[64];
} dgSlot_t;

static dgSlot_t	dgSlot[DG_SLOTS];

// ---- cvars ----
typedef struct { const char *name, *def; } dgCv_t;
static const dgCv_t dgCvars[] = {
	{ "df_ghosts_NoDrawRadius", "200" }, { "df_ghosts_FileExtension", "" },
	{ "df_ghosts_SelfMissilesOnly", "1" }, { "df_ghosts_Command", "" }, { "df_ghosts_SpriteInfo", "" }, { "df_ghosts_SpriteSize", "8" }, { "df_ghosts_DrawOutput", "0" },
	{ "df_ghost1_RecordFileFormat", "2" }, { "df_ghost1_RecordInterval", "50" }, { "df_ghost1_RecordView", "0" },
	{ "df_ghost1_PlayTimescale", "1.0" }, { "df_ghost1_PlayDrawMissiles", "2" }, { "df_ghost1_PlayDrawPlayer", "3" }, { "df_ghost1_PlayLoop", "0" },
	{ "df_ghost2_RecordFileFormat", "2" }, { "df_ghost2_RecordInterval", "50" }, { "df_ghost2_RecordView", "0" },
	{ "df_ghost2_PlayTimescale", "1.0" }, { "df_ghost2_PlayDrawMissiles", "2" }, { "df_ghost2_PlayDrawPlayer", "3" }, { "df_ghost2_PlayLoop", "0" },
};

void CG_DF_GhostRegisterCvars( void ) {
	int i;

	for ( i = 0; i < ARRAY_LEN( dgCvars ); i++ ) {
		trap_Cvar_Register( NULL, dgCvars[i].name, dgCvars[i].def, CVAR_ARCHIVE );
	}
}

static float DG_CvarF( const char *name, float def ) {
	char buf[MAX_CVAR_VALUE_STRING];

	trap_Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return buf[0] ? atof( buf ) : def;
}

static float DG_SlotCvar( int slot, const char *what, float def ) {
	if ( slot > 1 ) {
		return def;
	}
	return DG_CvarF( va( "df_ghost%i_%s", slot + 1, what ), def );
}

// ---- file helpers ----
static void DG_Path( int slot, char *out, int size ) {
	char ext[32];

	trap_Cvar_VariableStringBuffer( "df_ghosts_FileExtension", ext, sizeof( ext ) );
	if ( !ext[0] ) {
		Q_strncpyz( ext, "ghost", sizeof( ext ) );
	}
	Com_sprintf( out, size, "system/ghosts/%s.%s", dgSlot[slot].name, ext );
}

static void DG_MapName( char *out, int size ) {
	Q_strncpyz( out, Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ), size );
}

static void DG_PutI( int slot, int v ) {
	trap_FS_Write( &v, 4, dgSlot[slot].f );
}

static void DG_PutLerp( int slot, const lerpFrame_t *l ) {		// the 48-byte lerpFrame_t; the animation pointer is written as 0
	DG_PutI( slot, l->oldFrame );
	DG_PutI( slot, l->oldFrameTime );
	DG_PutI( slot, l->frame );
	DG_PutI( slot, l->frameTime );
	trap_FS_Write( &l->backlerp, 4, dgSlot[slot].f );
	trap_FS_Write( &l->yawAngle, 4, dgSlot[slot].f );
	DG_PutI( slot, l->yawing );
	trap_FS_Write( &l->pitchAngle, 4, dgSlot[slot].f );
	DG_PutI( slot, l->pitching );
	DG_PutI( slot, l->animationNumber );
	DG_PutI( slot, 0 );
	DG_PutI( slot, l->animationTime );
}

static qboolean DG_Get( int slot, void *buf, int len ) {
	dgSlot_t *s = &dgSlot[slot];

	if ( s->remain < len ) {
		s->remain = 0;
		return qfalse;
	}
	trap_FS_Read( buf, len, s->f );
	s->remain -= len;
	return qtrue;
}

static void DG_GetLerp( int slot, lerpFrame_t *l ) {
	int v[12];

	Com_Memset( l, 0, sizeof( *l ) );
	if ( !DG_Get( slot, v, sizeof( v ) ) ) {
		return;
	}
	l->oldFrame = v[0];
	l->oldFrameTime = v[1];
	l->frame = v[2];
	l->frameTime = v[3];
	Com_Memcpy( &l->backlerp, &v[4], 4 );
	Com_Memcpy( &l->yawAngle, &v[5], 4 );
	l->yawing = v[6];
	Com_Memcpy( &l->pitchAngle, &v[7], 4 );
	l->pitching = v[8];
	l->animationNumber = v[9];
	l->animation = NULL;
	l->animationTime = v[11];
}

// 0x11786: one frame = packet 2 (time), then packets until 7; returns qfalse at the end of data / on a bad packet
static qboolean DG_ReadFrame( int slot, dgFrame_t *fr ) {
	int type, first = 1, skip, tmp[8];

	Com_Memset( fr, 0, sizeof( *fr ) );
	while ( DG_Get( slot, &type, 4 ) ) {
		if ( first ) {
			if ( type != 2 ) {
				break;
			}
			first = 0;
			if ( !DG_Get( slot, &fr->time, 4 ) ) {
				break;
			}
			continue;
		}
		if ( type == 0 || type == 7 ) {
			break;
		}
		switch ( type ) {
		case 3:
			DG_Get( slot, fr->origin, 12 );
			DG_Get( slot, fr->angles, 12 );
			break;
		case 4:
			DG_Get( slot, fr->origin, 12 );
			DG_Get( slot, fr->angles, 12 );
			DG_Get( slot, &fr->ducked, 4 );
			DG_Get( slot, &fr->discont, 4 );
			DG_Get( slot, &fr->weapon, 4 );
			DG_Get( slot, &fr->torsoAnim, 4 );
			DG_Get( slot, &fr->legsAnim, 4 );
			DG_GetLerp( slot, &fr->torso );
			DG_GetLerp( slot, &fr->legs );
			break;
		case 5:				// missile (0x11624): number, weapon, origin, velocity; at most 10 per frame
			if ( fr->numMis < 10 ) {
				dgMissile_t *m = &fr->mis[fr->numMis++];

				DG_Get( slot, &m->num, 4 );
				DG_Get( slot, &m->weapon, 4 );
				DG_Get( slot, m->org, 12 );
				DG_Get( slot, m->vel, 12 );
				break;
			}
			skip = 32;
			goto skipit;
		case 6:				// explosion sprite (0x1168b): origin + code; at most 10 per frame
			if ( fr->numSpr < 10 ) {
				dgSprite_t *sp = &fr->spr[fr->numSpr++];

				DG_Get( slot, sp->org, 12 );
				DG_Get( slot, &sp->code, 4 );
				break;
			}
			skip = 16;
			goto skipit;
		case 8: skip = 68; goto skipit;		// monitored variable: not used
		skipit:
			while ( skip > 0 ) {
				int n = skip > (int)sizeof( tmp ) ? (int)sizeof( tmp ) : skip;

				if ( !DG_Get( slot, tmp, n ) ) {
					return qfalse;
				}
				skip -= n;
			}
			break;
		default:
			CG_Printf( "^1GHOST: [ %s ] --> Error ( Ghost_ReadSnapshotFromFile ): Bad Packet Type ( %i )\n", dgSlot[slot].name, type );
			dgSlot[slot].remain = 0;
			return qfalse;
		}
	}
	return fr->time != 0;
}

// ---- stop / record / play ----
static void DG_Close( int slot, qboolean quiet ) {		// 0x11b8a (+ the status line of 0x139df)
	dgSlot_t *s = &dgSlot[slot];

	if ( s->state == 0 ) {
		return;
	}
	if ( s->f ) {
		trap_FS_FCloseFile( s->f );
	}
	if ( !quiet ) {
		CG_Printf( "^2GHOST: [ %s ] --> %s stopped\n", s->name, s->state == 1 ? "Recording" : "Playback" );
	}
	s->f = 0;
	s->state = 0;
	s->haveNext = qfalse;
	s->centInit = qfalse;
}

static void DG_Stop( int slot ) {
	DG_Close( slot, qfalse );
}

static void DG_SetName( int slot, const char *name ) {
	char map[MAX_QPATH];

	if ( name[0] ) {
		Q_strncpyz( dgSlot[slot].name, name, sizeof( dgSlot[slot].name ) );
	} else {
		DG_MapName( map, sizeof( map ) );
		Com_sprintf( dgSlot[slot].name, sizeof( dgSlot[slot].name ), "%s_%02i", map, slot + 1 );
	}
}

static void DG_Record( int slot, const char *name ) {		// 0x13913 -> 0x11958
	dgSlot_t *s = &dgSlot[slot];
	char path[MAX_QPATH], map[MAX_QPATH], hdr[16];

	if ( s->state ) {
		CG_Printf( "^3GHOST: [ %s ] --> already %s\n", s->name, s->state == 1 ? "recording" : "playing" );
		return;
	}
	Com_Memset( s, 0, sizeof( *s ) );
	DG_SetName( slot, name );
	DG_Path( slot, path, sizeof( path ) );
	trap_FS_FOpenFile( path, &s->f, FS_WRITE );
	if ( !s->f ) {
		CG_Printf( "^1GHOST: [ %s ] --> Error ( OpenForRecord )\n", s->name );
		return;
	}
	DG_PutI( slot, 1 );					// packet type 1 = header
	DG_PutI( slot, DG_FILE_MAGIC );
	DG_PutI( slot, DG_FILE_VERSION );
	DG_MapName( map, sizeof( map ) );
	Com_Memset( hdr, 0, sizeof( hdr ) );
	Q_strncpyz( hdr, map, sizeof( hdr ) );
	trap_FS_Write( hdr, 16, s->f );
	s->state = 1;
	s->lastTime = 0;
	CG_Printf( "^2GHOST: [ %s ] --> Recording\n", s->name );
}

static void DG_Play( int slot, const char *name ) {		// 0x13979 -> 0x11a0a
	dgSlot_t *s = &dgSlot[slot];
	char path[MAX_QPATH];
	int len, type, magic, ver;
	char hdr[16];

	if ( s->state ) {
		CG_Printf( "^3GHOST: [ %s ] --> already %s\n", s->name, s->state == 1 ? "recording" : "playing" );
		return;
	}
	Com_Memset( s, 0, sizeof( *s ) );
	DG_SetName( slot, name );
	DG_Path( slot, path, sizeof( path ) );
	len = trap_FS_FOpenFile( path, &s->f, FS_READ );
	if ( !s->f || len <= 0 ) {
		CG_Printf( "^1GHOST: [ %s ] --> Error ( OpenForPlay )\n", s->name );
		s->f = 0;
		return;
	}
	s->remain = len;
	if ( !DG_Get( slot, &type, 4 ) || type != 1 || !DG_Get( slot, &magic, 4 ) || !DG_Get( slot, &ver, 4 ) || !DG_Get( slot, hdr, 16 ) ) {
		CG_Printf( "^1GHOST: [ %s ] --> Error ( bad header )\n", s->name );
		trap_FS_FCloseFile( s->f );
		s->f = 0;
		return;
	}
	if ( magic > DG_FILE_MAGIC ) {
		CG_Printf( "^1GHOST: [ %s ] --> unsupported file version ( %i )\n", s->name, magic );
		trap_FS_FCloseFile( s->f );
		s->f = 0;
		return;
	}
	if ( !DG_ReadFrame( slot, &s->a ) ) {
		CG_Printf( "^1GHOST: [ %s ] --> empty ghost\n", s->name );
		trap_FS_FCloseFile( s->f );
		s->f = 0;
		return;
	}
	s->haveNext = DG_ReadFrame( slot, &s->b );
	s->lastTime = cg.time;
	s->state = 2;
	CG_Printf( "^2GHOST: [ %s ] --> Playing\n", s->name );
}

// ---- recorder (0x12785 -> 0x12620 / 0x11c4d / 0x114a2) ----
static void DG_Sample( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	const centity_t *pe = &cg.predictedPlayerEntity;
	int interval = (int)DG_SlotCvar( slot, "RecordInterval", 50 );
	int format = (int)DG_SlotCvar( slot, "RecordFileFormat", 2 );
	int view = (int)DG_SlotCvar( slot, "RecordView", 0 );
	dgFrame_t fr;

	if ( s->lastTime + interval > cg.time ) {
		return;
	}
	if ( cg.nextFrameTeleport ) {
		s->discont = 1;
	}
	Com_Memset( &fr, 0, sizeof( fr ) );
	fr.time = cg.time;
	VectorCopy( cg.predictedPlayerState.origin, fr.origin );
	VectorCopy( cg.predictedPlayerState.viewangles, fr.angles );
	fr.weapon = pe->currentState.weapon;
	fr.torsoAnim = pe->currentState.torsoAnim;
	fr.legsAnim = pe->currentState.legsAnim;
	fr.torso = pe->pe.torso;
	fr.legs = pe->pe.legs;
	fr.ducked = cg.predictedPlayerState.pm_flags & 1;
	if ( format >= 2 ) {		// 0x11cd8
		int k, onlyOwn = (int)DG_CvarF( "df_ghosts_SelfMissilesOnly", 1 );

		for ( k = 0; k < cg.snap->numEntities && fr.numMis < 10; k++ ) {
			const centity_t *me = &cg_entities[cg.snap->entities[k].number];

			if ( me->currentState.eType != ET_MISSILE || ( onlyOwn && me->currentState.clientNum != cg.predictedPlayerState.clientNum ) ) {
				continue;
			}
			fr.mis[fr.numMis].num = me->currentState.number;
			fr.mis[fr.numMis].weapon = me->currentState.weapon;
			VectorCopy( me->lerpOrigin, fr.mis[fr.numMis].org );
			VectorCopy( me->currentState.pos.trDelta, fr.mis[fr.numMis].vel );
			fr.numMis++;
		}
		{		// 0x11d58: explosions of the local entity list
			vec3_t org[10];
			int code[10], n, q;

			n = CG_DF_GhostExplosions( onlyOwn, org, code, 10 );
			for ( q = 0; q < n; q++ ) {
				VectorCopy( org[q], fr.spr[q].org );
				fr.spr[q].code = code[q];
			}
			fr.numSpr = n;
		}
	}
	fr.discont = s->discont;
	s->discont = 0;
	if ( view >= 1 ) {
		VectorCopy( cg.refdef.vieworg, fr.origin );
		VectorCopy( cg.refdefViewAngles, fr.angles );
		fr.origin[2] -= 26;
		fr.ducked = 0;
	}
	DG_PutI( slot, 2 );
	DG_PutI( slot, fr.time );
	if ( format <= 0 ) {
		DG_PutI( slot, 3 );
		trap_FS_Write( fr.origin, 12, s->f );
		trap_FS_Write( fr.angles, 12, s->f );
	} else {
		DG_PutI( slot, 4 );
		trap_FS_Write( fr.origin, 12, s->f );
		trap_FS_Write( fr.angles, 12, s->f );
		DG_PutI( slot, fr.ducked );
		DG_PutI( slot, fr.discont );
		DG_PutI( slot, fr.weapon );
		DG_PutI( slot, fr.torsoAnim );
		DG_PutI( slot, fr.legsAnim );
		DG_PutLerp( slot, &fr.torso );
		DG_PutLerp( slot, &fr.legs );
	}
	if ( format >= 2 ) {		// 0x11343
		int k;

		for ( k = 0; k < fr.numMis; k++ ) {
			if ( !fr.mis[k].weapon ) {
				break;
			}
			DG_PutI( slot, 5 );
			DG_PutI( slot, fr.mis[k].num );
			DG_PutI( slot, fr.mis[k].weapon );
			trap_FS_Write( fr.mis[k].org, 12, s->f );
			trap_FS_Write( fr.mis[k].vel, 12, s->f );
		}
	}
	if ( format >= 2 ) {		// 0x113bf
		int k;

		for ( k = 0; k < fr.numSpr; k++ ) {
			if ( !fr.spr[k].code ) {
				break;
			}
			DG_PutI( slot, 6 );
			trap_FS_Write( fr.spr[k].org, 12, s->f );
			DG_PutI( slot, fr.spr[k].code );
		}
	}
	DG_PutI( slot, 7 );
	s->lastTime = cg.time;
}


// ---- name label (0x12fc5 -> 0x9342): the text is built from per-character sprites using the DeFRaG pk3 shaders gfx/2d/characters/<a-z|0-9|-> ----
static qhandle_t dgGlyphLetter[27], dgGlyphDigit[10], dgGlyphDash;
static qboolean dgGlyphsReady;

static void DG_RegisterGlyphs( void ) {		// 0x91b5
	int i;

	for ( i = 1; i <= 26; i++ ) {
		dgGlyphLetter[i] = trap_R_RegisterShaderNoMip( va( "gfx/2d/characters/%c", 'a' + i - 1 ) );
	}
	for ( i = 0; i < 10; i++ ) {
		dgGlyphDigit[i] = trap_R_RegisterShaderNoMip( va( "gfx/2d/characters/%i", i ) );
	}
	dgGlyphDash = trap_R_RegisterShaderNoMip( "gfx/2d/characters/-" );
	dgGlyphsReady = qtrue;
}

static void DG_Label( int slot, const vec3_t origin ) {
	char text[128], buf[MAX_CVAR_VALUE_STRING];
	float size = DG_CvarF( "df_ghosts_SpriteSize", 8 ), alpha = 1.0f;
	int n, i, mode = (int)DG_CvarF( "df_ghosts_DrawOutput", 0 ), col = '7';
	vec3_t org, up = { 0, 0, 1 }, diff, right;
	refEntity_t ent;

	trap_Cvar_VariableStringBuffer( "df_ghosts_SpriteInfo", text, sizeof( text ) );
	if ( !text[0] || size <= 0 ) {
		return;
	}
	if ( !Q_stricmp( text, "$ghostnum" ) ) {
		Com_sprintf( text, sizeof( text ), "%i", slot + 1 );
	} else {
		CG_DF_ExpandVar( text, sizeof( text ) );
	}
	if ( !text[0] ) {
		return;
	}
	switch ( mode ) {
	case 1: col = '1'; break;
	case 2: case 3: case 4:
		trap_Cvar_VariableStringBuffer( va( "df_chs%i_LabelColor", mode - 2 ), buf, sizeof( buf ) );
		col = atoi( buf ) + '0';
		alpha = DG_CvarF( va( "df_chs%i_FontTransparency", mode - 2 ), 1.0f );
		break;
	}
	if ( !dgGlyphsReady ) {
		DG_RegisterGlyphs();
	}
	VectorCopy( origin, org );
	org[2] += 50;
	VectorSubtract( cg.refdef.vieworg, org, diff );
	CrossProduct( diff, up, right );
	VectorNormalize( right );
	text[63] = 0;
	Q_strupr( text );
	n = strlen( text );
	Com_Memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	ent.radius = size / 2;
	{
		const float *c = g_color_table[( ( col - '0' ) & 15 ) % 10];

		ent.shaderRGBA[0] = (byte)( 255 * c[0] );
		ent.shaderRGBA[1] = (byte)( 255 * c[1] );
		ent.shaderRGBA[2] = (byte)( 255 * c[2] );
		ent.shaderRGBA[3] = (byte)( 255 * alpha );
	}
	for ( i = 0; i < n; i++ ) {
		int ch = (byte)text[i];
		qhandle_t sh = 0;

		if ( ch >= 'A' && ch <= 'Z' ) {
			sh = dgGlyphLetter[ch - 64];
		} else if ( ch >= '0' && ch <= '9' ) {
			sh = dgGlyphDigit[ch - '0'];
		} else if ( ch == '-' ) {
			sh = dgGlyphDash;
		}
		if ( !sh ) {
			continue;
		}
		VectorMA( org, ( n / 2.0f - i ) * size, right, ent.origin );
		ent.customShader = sh;
		trap_R_AddRefEntityToScene( &ent );
	}
}

static void DG_DrawMissiles( int slot, float frac, qboolean discont ) {		// 0x13317 / 0x12199 / 0x1f3a0
	dgSlot_t *s = &dgSlot[slot];
	const dgFrame_t *to = s->haveNext ? &s->b : &s->a, *from = &s->a;
	int k, m, i;

	for ( k = 0; k < to->numMis; k++ ) {
		const dgMissile_t *r = &to->mis[k], *q = NULL;
		centity_t me;
		vec3_t d;

		if ( !r->weapon ) {
			break;
		}
		for ( m = 0; m < from->numMis; m++ ) {
			if ( from->mis[m].num == r->num ) {
				q = &from->mis[m];
				break;
			}
		}
		Com_Memset( &me, 0, sizeof( me ) );
		if ( q && s->haveNext && !discont ) {
			for ( i = 0; i < 3; i++ ) {
				me.lerpOrigin[i] = q->org[i] + frac * ( r->org[i] - q->org[i] );
				d[i] = LerpAngle( q->vel[i], r->vel[i], frac );		// DeFRaG angle-lerps the velocity too (0x12199)
			}
		} else {
			VectorCopy( r->org, me.lerpOrigin );
			VectorCopy( r->vel, d );
		}
		VectorCopy( d, me.currentState.pos.trDelta );
		me.currentState.weapon = r->weapon == WP_GRAPPLING_HOOK ? WP_ROCKET_LAUNCHER : r->weapon;
		me.currentState.eType = ET_MISSILE;
		CG_DF_AddGhostMissile( &me );
	}
}

static void DG_DrawSprites( int slot ) {		// 0x133dc / 0x12233
	dgSlot_t *s = &dgSlot[slot];
	const dgFrame_t *to = s->haveNext ? &s->b : &s->a;
	int k;

	for ( k = 0; k < to->numSpr; k++ ) {
		refEntity_t ent;
		qhandle_t sh;

		switch ( to->spr[k].code ) {
		case 4: sh = cgs.media.grenadeExplosionShader; break;
		case 5: sh = cgs.media.rocketExplosionShader; break;
		case 8: sh = cgs.media.plasmaExplosionShader; break;
		case 9: sh = cgs.media.bfgExplosionShader; break;
		default: sh = 0; break;
		}
		if ( !sh ) {
			break;
		}
		Com_Memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		ent.radius = 51;
		ent.customShader = sh;
		ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = 84;
		VectorCopy( to->spr[k].org, ent.origin );
		trap_R_AddRefEntityToScene( &ent );
	}
}

// ---- playback (0x127af / 0x12421 / 0x11e15 / 0x11fd8 / 0x12078 / 0x13125) ----
static void DG_PlayFrame( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	float ts = DG_SlotCvar( slot, "PlayTimescale", 1.0f );
	int mode = (int)DG_SlotCvar( slot, "PlayDrawPlayer", 3 );
	int loop = (int)DG_SlotCvar( slot, "PlayLoop", 0 );
	float elapsed, frac = 0, radius;
	int dt;
	vec3_t org, ang, d;
	centity_t *c = &s->cent;
	qboolean near, invis, discont;

	if ( ts < 0 ) {
		ts = -ts;		// reverse playback is not ported
	}
	elapsed = ( cg.time - s->lastTime ) * ts;
	for ( ;; ) {
		if ( !s->haveNext ) {
			break;
		}
		dt = s->b.time - s->a.time;
		if ( dt > 0 && elapsed < dt ) {
			break;
		}
		if ( ts > 0 ) {
			s->lastTime += (int)( ( dt > 0 ? dt : 0 ) / ts );
		}
		s->p = s->a;
		s->a = s->b;
		s->haveNext = DG_ReadFrame( slot, &s->b );
		elapsed = ( cg.time - s->lastTime ) * ts;
	}
	if ( !s->haveNext && elapsed > 0 && s->remain <= 0 ) {		// end of data (0x127af: PlayLoop)
		char keep[sizeof( s->name )];

		Q_strncpyz( keep, s->name, sizeof( keep ) );
		DG_Close( slot, loop >= 1 );
		if ( loop >= 1 ) {
			DG_Play( slot, keep );
		}
		return;
	}
	discont = s->haveNext && ( s->b.discont || s->a.discont );
	if ( s->haveNext && !discont ) {
		dt = s->b.time - s->a.time;
		frac = dt > 0 ? elapsed / dt : 0;
		frac = frac < 0 ? 0 : ( frac > 1 ? 1 : frac );
		{
			int i;

			for ( i = 0; i < 3; i++ ) {
				org[i] = s->a.origin[i] + frac * ( s->b.origin[i] - s->a.origin[i] );
				ang[i] = LerpAngle( s->a.angles[i], s->b.angles[i], frac );
			}
		}
	} else {
		const dgFrame_t *fr = s->haveNext ? &s->b : &s->a;

		VectorCopy( fr->origin, org );
		VectorCopy( fr->angles, ang );
	}

	// 0x12078: the ghost's centity (a normal centity_t); its pe is rebuilt from the file at the start and after a discontinuity
	if ( !s->centInit || discont ) {
		Com_Memset( c, 0, sizeof( *c ) );
		c->pe.torso = s->a.torso;
		c->pe.legs = s->a.legs;
		c->pe.torso.animation = c->pe.legs.animation = NULL;
		s->centInit = qtrue;
	}
	c->currentState.number = ENTITYNUM_NONE;
	c->currentState.eType = ET_PLAYER;
	c->currentState.clientNum = cg.clientNum;
	c->currentState.weapon = s->a.weapon;
	c->currentState.torsoAnim = s->a.torsoAnim;
	c->currentState.legsAnim = s->a.legsAnim;
	c->currentState.pos.trType = TR_STATIONARY;
	c->currentValid = qtrue;
	VectorCopy( org, c->lerpOrigin );
	VectorCopy( ang, c->lerpAngles );
	VectorCopy( org, c->currentState.pos.trBase );
	VectorCopy( ang, c->currentState.apos.trBase );

	radius = DG_CvarF( "df_ghosts_NoDrawRadius", 200 );
	VectorSubtract( org, cg.predictedPlayerState.origin, d );
	near = VectorLength( d ) < radius;
	if ( !near && (int)DG_SlotCvar( slot, "PlayDrawMissiles", 2 ) >= 1 ) {
		DG_DrawMissiles( slot, frac, discont );
	}
	if ( !near && (int)DG_SlotCvar( slot, "PlayDrawMissiles", 2 ) >= 2 ) {
		DG_DrawSprites( slot );
	}
	invis = qfalse;
	switch ( mode ) {
	case 1:
		if ( near ) return;
		break;
	case 2:
		if ( near ) return;
		invis = qtrue;
		break;
	case 3:
		invis = near;
		break;
	default:
		return;		// 0 = hidden; mode 4 (marker) is not ported
	}
	c->currentState.powerups = invis ? ( 1 << PW_INVIS ) : 0;
	CG_Player( c );
	if ( !near ) {
		DG_Label( slot, c->lerpOrigin );
	}
}

// ---- per-frame entry (after the entities of the frame were added; cgame 0x79c1 -> 0x12785 / 0x127af) ----
static void DG_CommandCvar( void );

void CG_DF_GhostFrame( void ) {
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 1 ) {
			DG_Sample( i );
		} else if ( dgSlot[i].state == 2 ) {
			DG_PlayFrame( i );
		}
	}
	DG_CommandCvar();
	CG_DF_SmokeTrails();
	CG_DF_AutoRecFrame();
}

// ---- `ghost` command (0x13e09 -> parser 0x13a4a -> executor 0x13b9a) ----
static void DG_Help( void ) {
	CG_Printf( "^1GHOST --> Use #index to specify ghost number\n^5Example: ^2ghost #all play\n" );
}

static void DG_Exec( const char *idx, const char *cmd, const char *name ) {		// executor of 0x13b9a
	int first, last, i;

	if ( idx[0] != '#' ) {
		CG_Printf( "^1GHOST --> INVALID INDEX: Use #index to specify ghost number\n" );
		return;
	}
	if ( !Q_stricmp( idx, "#all" ) ) {
		first = 0;
		last = DG_SLOTS - 1;
	} else {
		first = last = atoi( idx + 1 ) - 1;
		if ( first < 0 || first >= DG_SLOTS ) {
			CG_Printf( "^1GHOST --> INVALID INDEX: %i\n", first + 1 );
			return;
		}
	}
	for ( i = first; i <= last; i++ ) {
		if ( !Q_stricmp( cmd, "none" ) ) {
			continue;
		} else if ( !Q_stricmp( cmd, "stop" ) ) {
			DG_Stop( i );
		} else if ( !Q_stricmp( cmd, "record" ) ) {
			DG_Record( i, name );
		} else if ( !Q_stricmp( cmd, "play" ) ) {
			DG_Play( i, name );
		} else if ( !Q_stricmp( cmd, "demo" ) ) {
			CG_Printf( "^3GHOST --> demo mode is not ported\n" );
			return;
		} else {
			CG_Printf( "^1GHOST --> INVALID COMMAND: %s\n", cmd );
			return;
		}
	}
}

void CG_DF_Ghost_f( void ) {
	char idx[64], cmd[64], name[128];

	if ( trap_Argc() < 3 ) {
		DG_Help();
		return;
	}
	trap_Argv( 1, idx, sizeof( idx ) );
	trap_Argv( 2, cmd, sizeof( cmd ) );
	if ( trap_Argc() > 3 ) {
		trap_Argv( 3, name, sizeof( name ) );
	} else {
		name[0] = 0;
	}
	DG_Exec( idx, cmd, name );
}

// 0x13dd5: the cvar df_ghosts_Command ("#1 play name") is executed once and cleared (polled every frame)
static void DG_CommandCvar( void ) {
	char		line[256], idx[64], cmd[64], name[128];
	const char	*p;

	trap_Cvar_VariableStringBuffer( "df_ghosts_Command", line, sizeof( line ) );
	if ( !line[0] ) {
		return;
	}
	trap_Cvar_Set( "df_ghosts_Command", "" );
	p = line;
	Q_strncpyz( idx, COM_ParseExt( &p, qfalse ), sizeof( idx ) );
	Q_strncpyz( cmd, COM_ParseExt( &p, qfalse ), sizeof( cmd ) );
	Q_strncpyz( name, COM_ParseExt( &p, qfalse ), sizeof( name ) );
	if ( !idx[0] || !cmd[0] ) {
		DG_Help();
		return;
	}
	DG_Exec( idx, cmd, name );
}

// ---- accessors for the CHS ghost items 90-99 (cgame 0x14029 / 0x1406f / 0x140b0 / 0x13eb1 / 0x13e18 / 0x13e44) ----
qboolean CG_DF_GhostPlaying( int slot ) {
	return slot >= 0 && slot < DG_SLOTS && dgSlot[slot].state == 2;
}

float CG_DF_GhostDist( int slot, int kind ) {		// 0x14029: kind 0 = XY, 1 = dZ (ghost - player), 2 = XYZ
	vec3_t d;

	if ( !CG_DF_GhostPlaying( slot ) ) {
		return 0;
	}
	VectorSubtract( dgSlot[slot].a.origin, cg.predictedPlayerState.origin, d );
	switch ( kind ) {
	case 0: return sqrt( d[0] * d[0] + d[1] * d[1] );
	case 1: return d[2];
	case 2: return VectorLength( d );
	}
	return 0;
}

int CG_DF_GhostNearest( int kind ) {		// 0x1406f: playing ghost with the smallest distance (0 when none closer than 128000)
	float best = 128000.0f, dist;
	int i, idx = 0;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 2 ) {
			dist = CG_DF_GhostDist( i, kind );
			if ( dist < best ) {
				best = dist;
				idx = i;
			}
		}
	}
	return idx;
}

void CG_DF_GhostOrigin( int slot, vec3_t out ) {		// 0x140b0
	VectorClear( out );
	if ( CG_DF_GhostPlaying( slot ) ) {
		VectorCopy( dgSlot[slot].a.origin, out );
	}
}

void CG_DF_GhostVelocity( int slot, vec3_t out ) {		// 0x13eb1: previous frame -> current frame, units per second
	const dgSlot_t *s = &dgSlot[slot];
	vec3_t d;
	int dt;

	VectorClear( out );
	if ( !CG_DF_GhostPlaying( slot ) || s->p.time >= s->a.time ) {
		return;
	}
	dt = s->a.time - s->p.time;
	VectorSubtract( s->a.origin, s->p.origin, d );
	VectorScale( d, 1000.0f / dt, out );
}

int CG_DF_GhostHeader( int slot, int which ) {		// 0x13e18 / 0x13e44: header format / version of the playing file
	if ( !CG_DF_GhostPlaying( slot ) ) {
		return 0;
	}
	return which ? DG_FILE_VERSION : DG_FILE_MAGIC;
}

qboolean CG_DF_GhostPos( int slot, vec3_t out ) {		// the ghost's drawn position (0x...: cent.lerpOrigin at slot+312928)
	if ( !CG_DF_GhostPlaying( slot ) || !dgSlot[slot].centInit ) {
		return qfalse;
	}
	VectorCopy( dgSlot[slot].cent.lerpOrigin, out );
	return qtrue;
}
