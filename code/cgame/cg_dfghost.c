// cg_dfghost.c -- DeFRaG ghosts (cgame 0x127af / 0x12785 / 0x12620 / 0x13e09...; see disasm/CGAME/ghost_system.md)
//
// Ported: the `ghost #N|#all stop|record|play [name]` command, the recorder (player pose + animation state sampled from
// cg.predictedPlayerEntity every df_ghostN_RecordInterval ms), the DeFRaG ghost file (header + packet stream, types 2/3/4/7;
// types 5/6/8 of files written by DeFRaG are skipped when reading), time-synchronised playback with interpolation, loop
// handling, and drawing through the normal CG_Player (DrawPlayer modes 1/2/3, NoDrawRadius).
// Later iterations added missiles/sprites, the label, the marker (DrawPlayer 4), miniview, AutoMode and the play camera modes
// (see disasm/PORT_STATUS.md).
// NOT ported: monitored variables (type 8), negative play timescale (reverse playback), the animation timescale
// field (DeFRaG's extra centity float at +624).
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

typedef struct {		// one monitored variable (slot+312952, 76 bytes): a cvar whose value is recorded with the ghost (format 3)
	char		name[64];			// empty = unused
	float		cur;				// +64 current / recorded value
	float		last;				// +68 value last written to the file
	float		orig;				// +72 cvar value before the playback changed it
} dgVar_t;

typedef struct {
	int			state;				// 0 idle, 1 recording, 2 playing (slot+164)
	dgVar_t		var[10];
	int			camWord;			// slot+312216: df_ghostN_PlayCameraMode as it was when the slot started
	fileHandle_t f;
	int			remain;				// bytes left to read (playing)
	int			fileLen;			// slot+132: file length (SIZE in the status line)
	int			frames;				// slot+136: recording: frames written; playing: ((fileLen - 24) / 624) << 2 (CHS ghost info items)
	int			lastTime;			// recording: last sample time; playing: anchor of the current frame
	int			discont;			// recording: pending discontinuity (teleport) flag
	qboolean	haveNext;
	int			ia, ib;				// ring indices of the frames a / b (slot+172 / +176)
	qboolean	anchor;				// slot+312188: the time base is set at the first cursor update after loading
	dgFrame_t	a, b;				// playing: current and next frame (in reverse playback: the later and the earlier one)
	dgFrame_t	p;				// playing: the frame before the current one (velocity, CHS items)
	centity_t	cent;
	qboolean	centInit;
	char		name[64];
} dgSlot_t;

static dgSlot_t	dgSlot[DG_SLOTS];

// The frame ring of a playing slot (slot+180, 500 frames of 624 bytes in DeFRaG): frames are streamed from the file into the ring, the cursor
// (slot+168) is the index of the NEXT frame; a frame with time 0 is empty.  Playing backwards walks the frames that are still in the ring.
#define DG_RING			500
static dgFrame_t	dgRing[DG_SLOTS][DG_RING];
static int			dgCur[DG_SLOTS];

// ---- cvars ----
typedef struct { const char *name, *def; } dgCv_t;
static const dgCv_t dgCvars[] = {
	{ "df_ghosts_NoDrawRadius", "200" }, { "df_ghosts_FileExtension", "" }, { "df_ghosts_AutoMode", "4" }, { "df_ghosts_AutoModeMP", "0" },
	{ "df_ghosts_SelfMissilesOnly", "1" }, { "df_ghosts_LiveAmmo", "0" }, { "df_ghosts_Command", "" }, { "df_ghosts_SpriteInfo", "" }, { "df_ghosts_SpriteSize", "8" }, { "df_ghosts_DrawOutput", "0" },
	{ "df_ghost1_RecordFileFormat", "2" }, { "df_ghost1_RecordInterval", "50" }, { "df_ghost1_RecordView", "0" },
	{ "df_ghosts_MiniviewDraw", "3" }, { "df_ghosts_MiniviewDrawGun", "1" }, { "df_ghosts_MiniviewCameraMode", "1" }, { "df_ghosts_MiniviewSize", "25" },
	{ "df_ghosts_MiniviewOffsetX", "235" }, { "df_ghosts_MiniviewOffsetY", "-155" }, { "df_ghosts_MiniviewBorderSize", "4" }, { "df_ghosts_MiniviewFOV", "90" },
	{ "df_ghost1_PlayCameraMode", "0" }, { "df_ghost2_PlayCameraMode", "0" }, { "df_ghost1_PlayTimescale", "1.0" }, { "df_ghost1_PlayDrawMissiles", "2" }, { "df_ghost1_PlayDrawPlayer", "3" }, { "df_ghost1_PlayLoop", "0" },
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
		Q_strncpyz( ext, "boo_01", sizeof( ext ) );		// 0x109e9: literal at 0xb750
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

// ---- monitored variables (record format 3 / packet 8; 0x10f3a / 0x10fff / 0x11046 / 0x11bf5 / 0x1141c / 0x116d3 / 0x11ee4 / 0x110ff) ----
// The names come from system/Ghost-MonitoredVars.cfg (up to 10 tokens). Recording at format 3 writes a packet 8 (name[64] + float) whenever a
// value changed; playing with a camera mode of 15 or 16 sets the cvars to the recorded values every frame and restores them when the ghost stops.
// The pseudo variable "cg_thirdperson" is the actual third-person state of the view. (DeFRaG writes the cvars through its safe-set 0x80bf;
// here they are set directly.)
#define DG_VARS_FILE	"system/Ghost-MonitoredVars.cfg"

static float DG_CvarValue( const char *name ) {
	char buf[MAX_CVAR_VALUE_STRING];

	trap_Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atof( buf );
}

static void DG_VarSet( const char *name, float v, qboolean precise ) {
	trap_Cvar_Set( name, v == (float)(int)v ? va( "%i", (int)v ) : va( precise ? "%.3f" : "%f", v ) );
}

// 0x10fff: the values that do not come from a cvar
static void DG_VarsSpecial( int slot ) {
	int i;

	for ( i = 0; i < 10; i++ ) {
		if ( dgSlot[slot].var[i].name[0] && !Q_stricmp( "cg_thirdperson", dgSlot[slot].var[i].name ) ) {
			dgSlot[slot].var[i].cur = (float)cg.renderingThirdPerson;
		}
	}
}

// 0x11bf5 (format 3 only): current values from the cvars
static void DG_VarsRefresh( int slot ) {
	int i;

	for ( i = 0; i < 10; i++ ) {
		if ( dgSlot[slot].var[i].name[0] ) {
			dgSlot[slot].var[i].cur = DG_CvarValue( dgSlot[slot].var[i].name );
		}
	}
	DG_VarsSpecial( slot );
}

// 0x10f3a + 0x11046: load the name list and take the start values (record format 3)
static void DG_VarsStart( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	int verbose = (int)DG_CvarF( "df_ghosts_DrawOutput", 0 ) != 0;		// 0x11046: message flags 7 / 0
	char buf[10000 + 1];
	const char *p, *tok;
	fileHandle_t f;
	int len, n = 0, i;

	if ( (int)DG_SlotCvar( slot, "RecordFileFormat", 2 ) != 3 ) {
		return;
	}
	Com_Memset( s->var, 0, sizeof( s->var ) );
	len = trap_FS_FOpenFile( DG_VARS_FILE, &f, FS_READ );		// 0x8da0 (max 10000 bytes)
	if ( !f ) {
		if ( verbose ) {
			CG_Printf( "^1ERROR: File Not Found --> ( %s )\n", DG_VARS_FILE );
			CG_Printf( "^3GHOST WARNING:^7 Empty MonitorVars File ^4-->^7 ( ^3%s^7 )\n", DG_VARS_FILE );
		}
		return;
	}
	if ( len >= 10000 ) {
		if ( verbose ) {
			CG_Printf( "^1ERROR: File Too Large --> ( %s is %i, max allowed is %i )\n", DG_VARS_FILE, len, 10000 );
			CG_Printf( "^3GHOST WARNING:^7 Empty MonitorVars File ^4-->^7 ( ^3%s^7 )\n", DG_VARS_FILE );
		}
		trap_FS_FCloseFile( f );
		return;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	if ( verbose ) {
		CG_Printf( "^2GHOST:^7 Parsing File ^4-->^7 ( ^2%s^7 )\n", DG_VARS_FILE );
	}
	p = buf;
	while ( n < 10 ) {
		tok = COM_Parse( &p );
		if ( !tok[0] ) {
			break;
		}
		Q_strncpyz( s->var[n++].name, tok, sizeof( s->var[0].name ) );
	}
	if ( !n ) {
		CG_Printf( "^1GHOST ERROR:^7 While Parsing File ^4-->^7 ( ^1%s^7 )\n", DG_VARS_FILE );
		return;
	}
	if ( verbose ) {
		CG_Printf( "^2GHOST:^7 Success Parsing File ^4-->^7 ( ^2%s^7 )\n", DG_VARS_FILE );
	}
	for ( i = 0; i < n; i++ ) {
		s->var[i].orig = DG_CvarValue( s->var[i].name );
		s->var[i].last = -9999.0f;			// (float)data word 0x20b0 = 0xffffd8f1 = -9999 (0x11046): differs from any real value, so the first sample writes every variable
		s->var[i].cur = s->var[i].orig;
	}
	DG_VarsSpecial( slot );
}

// 0x1141c: packet 8 for every variable whose value changed since the last write
static void DG_VarsWrite( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	int i;

	for ( i = 0; i < 10 && s->var[i].name[0]; i++ ) {
		if ( s->var[i].cur == s->var[i].last ) {
			continue;
		}
		DG_PutI( slot, 8 );
		trap_FS_Write( s->var[i].name, 64, s->f );
		trap_FS_Write( &s->var[i].cur, 4, s->f );
		s->var[i].last = s->var[i].cur;
	}
}

// 0x116d3: a packet 8 read from the file
static void DG_VarsPacket( int slot, const char *name, float value ) {
	dgSlot_t *s = &dgSlot[slot];
	int i;

	for ( i = 0; i < 10; i++ ) {
		if ( s->var[i].name[0] && !Q_stricmp( s->var[i].name, name ) ) {
			s->var[i].cur = value;
			return;
		}
	}
	for ( i = 0; i < 10; i++ ) {
		if ( !s->var[i].name[0] ) {
			Q_strncpyz( s->var[i].name, name, sizeof( s->var[i].name ) );
			s->var[i].cur = value;
			s->var[i].orig = DG_CvarValue( s->var[i].name );
			return;
		}
	}
}

// 0x11ee4 (every frame of a playing slot) and 0x110ff (when it stops): camera modes 15 / 16 only
static void DG_VarsApply( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	int i, verbose = (int)DG_CvarF( "df_ghosts_DrawOutput", 0 ) != 0;

	if ( s->camWord != 15 && s->camWord != 16 ) {
		return;
	}
	for ( i = 0; i < 10 && s->var[i].name[0]; i++ ) {
		if ( DG_CvarValue( s->var[i].name ) == s->var[i].cur ) {
			continue;
		}
		DG_VarSet( s->var[i].name, s->var[i].cur, qfalse );
		if ( verbose ) {
			CG_Printf( "GHOST: Var [ %i ] ^4-->^7 Change ( %s = %.3f )\n", i, s->var[i].name, s->var[i].cur );
		}
	}
}

static void DG_VarsRestore( int slot ) {
	dgSlot_t *s = &dgSlot[slot];
	int i, verbose = (int)DG_CvarF( "df_ghosts_DrawOutput", 0 ) != 0;

	if ( s->camWord != 15 && s->camWord != 16 ) {
		return;
	}
	if ( verbose ) {
		CG_Printf( "GHOST: Cleanup\n" );
	}
	for ( i = 0; i < 10 && s->var[i].name[0]; i++ ) {
		DG_VarSet( s->var[i].name, s->var[i].orig, qtrue );
		if ( verbose ) {
			CG_Printf( "GHOST: Var Restored [ %i ] ( %s = %.3f )\n", i, s->var[i].name, s->var[i].orig );
		}
	}
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
		case 8: {			// monitored variable (0x116d3): name[64] + float
			char	nm[64];
			float	val;

			if ( !DG_Get( slot, nm, 64 ) || !DG_Get( slot, &val, 4 ) ) {
				return qfalse;
			}
			nm[63] = 0;
			DG_VarsPacket( slot, nm, val );
			break;
		}
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
// ---- status lines (cgame 0x13721), selected by df_ghosts_DrawOutput: 0 off, 1 white, 2-4 df_chs0..2 colours ----
// The command flags of the executor 0x13b9a: bit 2 = print "command failed", bit 4 = print the success line (the console command and the
// df_ghosts_Command pump pass 7; "#all" with stop/play drops bits 1+2).
enum { DG_ST_FAILED = 1, DG_ST_RECORD, DG_ST_PLAY, DG_ST_STOP, DG_ST_NEWBEST, DG_ST_BEST };
static int dgBestMs;		// 0x12a1e8: best ghost time shown by kinds 5/6 and compared by the AutoMode run hook
static void DG_DefaultName( int slot, char *out, int size );

static void DG_Status( int slot, int kind ) {
	dgSlot_t *s = &dgSlot[slot];
	int mode = (int)DG_CvarF( "df_ghosts_DrawOutput", 0 );
	int A = '7', C = '7', B = '7';		// 0x43c / 0x440 / 0x504
	char buf[MAX_CVAR_VALUE_STRING];

	if ( mode < 1 || mode > 4 ) {
		return;
	}
	if ( mode >= 2 ) {
		B = '4';
		trap_Cvar_VariableStringBuffer( va( "df_chs%i_FontColor", mode - 2 ), buf, sizeof( buf ) );
		A = atoi( buf ) + '0';
		trap_Cvar_VariableStringBuffer( va( "df_chs%i_LabelColor", mode - 2 ), buf, sizeof( buf ) );
		C = atoi( buf ) + '0';
	}
	switch ( kind ) {
	case DG_ST_FAILED:
		CG_Printf( "GHOST ^%c%i^7: [ ^%c%s ^7] ^%c--> ^7command failed - ghost is: ^%c%s\n", A, slot + 1, A, s->name, B, C,
			s->state == 1 ? "recording" : s->state == 2 ? "playing" : "stopped" );
		break;
	case DG_ST_RECORD:
		CG_Printf( "GHOST ^%c%i^7: [ ^%c%s ^7- FORMAT: ^%c%i ^7INTERVAL: ^%c%i ^7VIEW: ^%c%i ^7] ^%c--> ^%cRecord\n", A, slot + 1, A, s->name,
			C, (int)DG_SlotCvar( slot, "RecordFileFormat", 2 ), C, (int)DG_SlotCvar( slot, "RecordInterval", 50 ), C,
			(int)DG_SlotCvar( slot, "RecordView", 0 ), B, C );
		break;
	case DG_ST_PLAY:
		CG_Printf( "GHOST ^%c%i^7: [ ^%c%s ^7- SIZE: ^%c%i ^7TIMESCALE: ^%c%3.3f ^7] ^%c--> ^%cPlay\n", A, slot + 1, A, s->name,
			C, s->fileLen, C, DG_SlotCvar( slot, "PlayTimescale", 1.0f ), B, C );
		break;
	case DG_ST_STOP:
		CG_Printf( "GHOST ^%c%i^7: [ ^%c%s ^7] ^%c--> ^%cStop\n", A, slot + 1, A, s->name, B, C );
		break;
	case DG_ST_NEWBEST:
	case DG_ST_BEST: {
		char defName[MAX_QPATH];

		DG_DefaultName( slot, defName, sizeof( defName ) );		// 0x118ae( buf, slot, 1 ): the DEFAULT name of the slot is printed
		CG_Printf( kind == DG_ST_NEWBEST ? "GHOST ^%c%i^7: [ ^%c%s ^7] ^%c--> ^%cNew Best Ghost ^7(^%c%s^7)\n" :
			"GHOST ^%c%i^7: [ ^%c%s ^7] ^%c--> ^%cBest Ghost Time ^7(^%c%s^7)\n", A, slot + 1, A, defName, B, C, C, CG_DF_FormatTime( dgBestMs ) );
		break;
	}
	}
}

static void DG_Close( int slot, int flags ) {		// 0x139df: stop (0x11b8a) + status lines
	dgSlot_t *s = &dgSlot[slot];

	if ( s->state == 0 ) {
		if ( flags & 2 ) {
			DG_Status( slot, DG_ST_FAILED );
		}
		return;
	}
	if ( s->state == 2 ) {
		DG_VarsRestore( slot );		// 0x139df -> 0x110ff
	}
	if ( s->f ) {
		trap_FS_FCloseFile( s->f );
	}
	s->f = 0;
	s->state = 0;
	s->haveNext = qfalse;
	s->centInit = qfalse;
	if ( flags & 4 ) {
		DG_Status( slot, DG_ST_STOP );
	}
}

static void DG_Stop( int slot, int flags ) {
	DG_Close( slot, flags );
}

// 0x10ec6 (from CG_Shutdown 0x7969): every slot's file is closed (0x11b8a), no status lines and no monitored-variable restore
void CG_DF_GhostShutdown( void ) {
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].f ) {
			trap_FS_FCloseFile( dgSlot[i].f );
		}
		dgSlot[i].f = 0;
		dgSlot[i].state = 0;
	}
}

// 0x118ae: default ghost name = record base name + "_NN" (NN = slot + 1, two digits)
static void DG_DefaultName( int slot, char *out, int size ) {
	char base[MAX_QPATH];

	CG_DF_RecordBaseName( base, sizeof( base ) );
	Com_sprintf( out, size, "%s_%02i", base, slot + 1 );
}

static void DG_SetName( int slot, const char *name ) {
	char map[MAX_QPATH];

	if ( name[0] ) {
		Q_strncpyz( dgSlot[slot].name, name, sizeof( dgSlot[slot].name ) );
	} else {
		DG_DefaultName( slot, dgSlot[slot].name, sizeof( dgSlot[slot].name ) );
	}
}

static void DG_Record( int slot, const char *name, int flags ) {		// 0x13913 -> 0x11958
	dgSlot_t *s = &dgSlot[slot];
	char path[MAX_QPATH], map[MAX_QPATH], hdr[16];

	if ( s->state ) {
		if ( flags & 2 ) {
			DG_Status( slot, DG_ST_FAILED );
		}
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
	s->camWord = (int)DG_SlotCvar( slot, "PlayCameraMode", 0 );
	DG_VarsStart( slot );		// 0x11a04 -> 0x11046
	if ( flags & 4 ) {
		DG_Status( slot, DG_ST_RECORD );
	}
}

static void DG_Play( int slot, const char *name, int flags ) {		// 0x13979 -> 0x11a0a
	dgSlot_t *s = &dgSlot[slot];
	char path[MAX_QPATH];
	int len, type, magic, ver;
	char hdr[16];

	if ( s->state ) {
		if ( flags & 2 ) {
			DG_Status( slot, DG_ST_FAILED );
		}
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
	s->fileLen = len;
	s->frames = ( ( len - 24 ) / 624 ) << 2;		// 0x11a61
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
	Com_Memset( dgRing[slot], 0, sizeof( dgRing[slot] ) );
	dgCur[slot] = 0;
	s->camWord = (int)DG_SlotCvar( slot, "PlayCameraMode", 0 );
	if ( !DG_ReadFrame( slot, &dgRing[slot][0] ) ) {
		CG_Printf( "^1GHOST: [ %s ] --> empty ghost\n", s->name );
		trap_FS_FCloseFile( s->f );
		s->f = 0;
		return;
	}
	s->a = s->b = dgRing[slot][0];
	s->haveNext = qfalse;
	s->anchor = qtrue;
	s->lastTime = cg.time;
	s->state = 2;
	if ( flags & 4 ) {
		DG_Status( slot, DG_ST_PLAY );
	}
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
	if ( format == 3 ) {
		DG_VarsRefresh( slot );		// 0x11bf5
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
	if ( format >= 3 ) {		// 0x1141c
		DG_VarsWrite( slot );
	}
	DG_PutI( slot, 7 );
	s->frames++;		// 0x114a2
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

// df_ghosts_LiveAmmo 1..4 on a server with sv_cheats (0x12290: the constant at data 8348 is always 1, 1221252 = LiveAmmo, 1182676 = cheats on):
// the ghost's missiles are not drawn but fired for real
static int DG_LiveAmmo( void ) {
	int v = (int)Com_Clamp( 0, 4, DG_CvarF( "df_ghosts_LiveAmmo", 0 ) );

	return CG_DF_Cheats() ? v : 0;
}

static int dgLastFire[DG_SLOTS];		// 1221056: frame ring index of the last frame that fired for each slot

// 0x122ad: a missile that has no counterpart in the previous frame was just fired: shoot a real one with the server's `firemissile` cheat
// command from the ghost's eye (origin + 24 up) along its view angles; LiveAmmo 2 = quaded, 3 = self-style, 4 = both
static void DG_LiveFire( int slot, const dgFrame_t *fr, const dgMissile_t *r, qboolean matched, int bIdx, int live ) {
	vec3_t fwd;

	if ( r->weapon == WP_GRAPPLING_HOOK ) {
		return;
	}
	if ( !matched ) {
		if ( dgLastFire[slot] < bIdx ) {
			AngleVectors( fr->angles, fwd, NULL, NULL );
			trap_SendConsoleCommand( va( "firemissile %f %f %f %f %f %f %i %i %i\n", fr->origin[0], fr->origin[1], fr->origin[2] + 24.0f,
				fwd[0], fwd[1], fwd[2], r->weapon, ( live == 2 || live == 4 ) ? 1 : 0, ( live == 3 || live == 4 ) ? 1 : 0 ) );
		}
		dgLastFire[slot] = bIdx;		// 0x123a8: stored whether it fired or not (also when the ring wrapped and the old value was higher)
	} else if ( bIdx < dgLastFire[slot] ) {
		dgLastFire[slot] = 0;		// 0x123b2: a counterpart exists and the ring index went back: forget the last shot
	}
}

static void DG_DrawMissiles( int slot, float frac, qboolean discont, int skipIdx ) {		// 0x13317 / 0x12199 / 0x1f3a0
	dgSlot_t *s = &dgSlot[slot];
	const dgFrame_t *to = s->haveNext ? &s->b : &s->a, *from = &s->a;
	int k, m, i, live = DG_LiveAmmo();

	for ( k = 0; k < to->numMis; k++ ) {
		const dgMissile_t *r = &to->mis[k], *q = NULL;
		centity_t me;
		vec3_t d;

		if ( !r->weapon ) {
			break;
		}
		if ( k == skipIdx ) {		// 0x13317: the camera's own missile is not drawn (unless the camera is third person)
			continue;
		}
		for ( m = 0; m < from->numMis; m++ ) {
			if ( from->mis[m].num == r->num ) {
				q = &from->mis[m];
				break;
			}
		}
		if ( live ) {
			DG_LiveFire( slot, to, r, q != NULL, s->haveNext ? s->ib : s->ia, live );
			continue;		// 0x13317: the ghost's missile is not drawn while LiveAmmo is on
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
// ---- play camera (cgame 0x12a10, doc CGAME/ghost_camera.md): df_ghostN_PlayCameraMode puts the view at the playing ghost ----
// Ported: modes 1-17 (5-12 missile cameras, 13/14 free look / look at targetposname).  NOT ported: the view-weapon redraw 0x1296b(1)
// at the end of 0x12a10 (the ordinary CG_AddViewWeapon shows the ghost's weapon instead).
typedef struct {
	int		mode;			// ctx+32: camera mode that claimed the view (0 = none)
	int		hideBody;		// ctx+4: the viewer sits at the ghost, do not draw its body/label
	int		shadow;			// ctx+8: draw only the player shadow (0x26a50 = CG_PlayerShadow)
	int		third;			// ctx+12: missile camera in third person
	int		found;			// ctx+16: a missile was found
	int		scanFwd;		// ctx+20: scan the missile records from 0 upwards (else from 9 down)
	int		followAng;		// ctx+24: look along the missile's heading
	int		missileIdx;		// ctx+28: followed missile (hidden unless third person), -1 = none
} dgCtx_t;

static int		dgDelta[3];			// 1221076: persistent view-angle delta of 0x8523
static qboolean	dgCamClaimed;		// only the first playing slot with a camera mode gets the view each frame (0x127af)
static qboolean	dgInMini;			// 1221216: rendering the miniview

// 0x1296b(1): the view weapon drawn from a blank player state (only the weapon survives)
static void DG_BlankGun( void ) {
	playerState_t	savedPS = cg.predictedPlayerState;
	centity_t		savedEnt = cg.predictedPlayerEntity;
	centity_t		*cent = &cg_entities[cg.predictedPlayerState.clientNum];
	int				savedFlags = cent->currentState.eFlags;

	cent->currentState.eFlags = 0;
	Com_Memset( &cg.predictedPlayerState, 0, sizeof( cg.predictedPlayerState ) );
	Com_Memset( &cg.predictedPlayerEntity, 0, sizeof( cg.predictedPlayerEntity ) );
	cg.predictedPlayerState.weapon = savedPS.weapon;
	cg.predictedPlayerEntity.currentState.weapon = savedEnt.currentState.weapon;
	CG_AddViewWeapon( &cg.predictedPlayerState );
	cg.predictedPlayerState = savedPS;
	cg.predictedPlayerEntity = savedEnt;
	cent->currentState.eFlags = savedFlags;
}

// 0x129fe (the part that maps onto oDFs state): no powerups on the view entity, no land bob
static void DG_ClearViewState( void ) {
	cg.predictedPlayerEntity.currentState.powerups = 0;
	cg.landChange = 0;
	cg.xyspeed = 0;			// 0x129fe also clears the three view-bob words (976508 / 976512 / 976516)
	cg.bobcycle = 0;
	cg.bobfracsin = 0;
}

// 0x15280, called by demo camera mode 8 while the tracked client is not the viewer: that client's weapon becomes the view weapon and,
// with cg_drawGun set, its gun is drawn at once from a blank state (third-person flag off meanwhile)
void CG_DF_TrackedViewGun( int weapon ) {
	int third = cg.renderingThirdPerson;

	cg.renderingThirdPerson = qfalse;
	cg.predictedPlayerState.weapon = weapon;
	cg.predictedPlayerEntity.currentState.weapon = weapon;
	if ( cg_drawGun.integer ) {
		DG_ClearViewState();
		DG_BlankGun();
	}
	cg.renderingThirdPerson = third;
}

// 0x128d6: cg.renderingThirdPerson (958772) for a camera mode; unlisted modes keep the current value
static void DG_ThirdPersonFor( int mode ) {
	switch ( mode ) {
	case 1: case 2: case 5: case 6: case 9: case 10: cg.renderingThirdPerson = 0; break;
	case 3: case 4: case 7: case 8: case 11: case 12: cg.renderingThirdPerson = 1; break;
	default: break;
	}
}

// 0x8523: view angles from the current usercmd + persistent delta (pitch clamped to +-16000, the overflow moved into the delta)
static void DG_UserAngles( vec3_t out ) {
	usercmd_t	cmd;
	int			i;
	short		s;

	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	for ( i = 0; i < 3; i++ ) {
		s = (short)( cmd.angles[i] + dgDelta[i] );
		if ( i == 0 ) {
			if ( s > 16000 ) {
				dgDelta[0] = 16000 - cmd.angles[0];
				s = 16000;
			} else if ( s < -16000 ) {
				dgDelta[0] = -16000 - cmd.angles[0];
				s = -16000;
			}
		}
		out[i] = SHORT2ANGLE( s );
	}
}

// 0x128fc: eye height of the ghost (crouch flag of the current frame)
static void DG_EyeHeight( const dgSlot_t *s ) {
	cg.refdef.vieworg[2] += 26;
	if ( s->a.ducked ) {
		cg.refdef.vieworg[2] -= 14;
	}
}

// 0x128fc/0x7b8c users: the ghost's weapon becomes the local view weapon
static void DG_ViewWeapon( int weapon ) {
	cg.predictedPlayerEntity.currentState.weapon = weapon;
	cg.predictedPlayerState.weapon = weapon;
}

// 0x7b8c: view position and angles (+ axis)
static void DG_SetView( const vec3_t org, const vec3_t ang ) {
	VectorCopy( org, cg.refdef.vieworg );
	VectorCopy( ang, cg.refdefViewAngles );
	AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
}

// 0x85d8: while the walk key is held, the horizontal mouse movement since it was pressed rolls the camera
static void DG_RollLook( vec3_t ang ) {
	static float	base;		// data 5688
	usercmd_t		cmd;

	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	if ( !( cmd.buttons & BUTTON_WALKING ) ) {
		base = (float)-cmd.angles[1];
	} else {
		short s = (short)(int)( (float)cmd.angles[1] + base );

		ang[2] -= SHORT2ANGLE( s );
	}
}

// 0x7d59("targetposname", &out): the position stored by `savepos` in the cvar named by `targetposname` ("placeplayer x y z ...", atoi)
static void DG_TargetPos( vec3_t out ) {
	char		name[1024], def[1024];
	const char	*p, *tok;

	VectorClear( out );		// (the original leaves its stack garbage in `out` when the cvar is missing)
	trap_Cvar_VariableStringBuffer( "targetposname", name, sizeof( name ) );
	if ( !name[0] ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( name, def, sizeof( def ) );
	p = def;
	tok = COM_ParseExt( &p, qtrue );
	if ( Q_stricmp( tok, "placeplayer" ) ) {
		return;
	}
	out[0] = atoi( COM_ParseExt( &p, qtrue ) );
	out[1] = atoi( COM_ParseExt( &p, qtrue ) );
	out[2] = atoi( COM_ParseExt( &p, qtrue ) );
}

// 0x165e7: the replay camera owns the view
static qboolean DG_ReplayCamOwnsView( void ) {
	return CG_DF_ReplayOwnsView();
}

// 0x1a265 (ghost follow): the first playing ghost with a camera mode, when that mode is 17: its position and frame time
qboolean CG_DF_GhostFollow( vec3_t pos, int *time ) {
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 2 && (int)DG_SlotCvar( i, "PlayCameraMode", 0 ) != 0 ) {
			if ( dgSlot[i].camWord != 17 ) {
				return qfalse;
			}
			VectorCopy( dgSlot[i].cent.lerpOrigin, pos );
			*time = dgRing[i][dgCur[i]].time;
			return qtrue;
		}
	}
	return qfalse;
}

// 0x12a10: returns qtrue when the mode claimed the view
static qboolean DG_Camera( int slot, const vec3_t org, const vec3_t ang, int mode, float frac, dgCtx_t *ctx ) {
	dgSlot_t *s = &dgSlot[slot];
	vec3_t userAng;
	qboolean handled = qtrue;

	if ( cg.demoPlayback ) {
		DG_UserAngles( userAng );
	}
	DG_ThirdPersonFor( mode );
	switch ( mode ) {
	case 1:
		DG_SetView( org, ang );
		DG_EyeHeight( s );
		DG_ViewWeapon( s->a.weapon );
		ctx->hideBody = 1;
		ctx->shadow = 1;
		break;
	case 2:
		VectorCopy( org, cg.refdef.vieworg );
		if ( cg.demoPlayback ) {
			VectorCopy( userAng, cg.refdefViewAngles );
		}
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
		DG_EyeHeight( s );
		DG_ViewWeapon( 0 );
		cg_drawCrosshair.integer = 0;
		ctx->hideBody = 1;
		ctx->shadow = 1;
		break;
	case 3:
		DG_SetView( org, ang );
		CG_OffsetThirdPersonView();
		break;
	case 4:
		VectorCopy( org, cg.refdef.vieworg );
		if ( cg.demoPlayback ) {
			VectorCopy( userAng, cg.refdefViewAngles );
		}
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
		CG_OffsetThirdPersonView();
		break;
	case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: {
		// missile cameras: follow a missile of the ghost's frames (matched by id between frame A and B)
		const dgFrame_t *fa = &s->a, *fb = s->haveNext ? &s->b : &s->a;
		const dgMissile_t *ma = NULL, *mb = NULL;
		int n, k, m;

		ctx->third = ( ( mode - 5 ) >> 1 ) & 1;
		ctx->scanFwd = mode < 9;
		ctx->followAng = ( ( mode - 5 ) & 1 ) == 0;
		ctx->found = 0;
		for ( n = 0; n < 10 && !mb; n++ ) {
			k = ctx->scanFwd ? n : 9 - n;
			if ( k >= fb->numMis || !fb->mis[k].weapon ) {
				continue;
			}
			for ( m = 0; m < fa->numMis; m++ ) {
				if ( fa->mis[m].num == fb->mis[k].num ) {
					ma = &fa->mis[m];
					mb = &fb->mis[k];
					if ( mb->weapon != 9 ) {
						ctx->missileIdx = k;
					}
					break;
				}
			}
		}
		if ( mb ) {
			vec3_t dir;
			int i;

			ctx->found = 1;
			DG_ViewWeapon( 0 );
			cg_drawCrosshair.integer = 0;
			for ( i = 0; i < 3; i++ ) {
				cg.refdef.vieworg[i] = ma->org[i] + frac * ( mb->org[i] - ma->org[i] );
			}
			if ( ctx->followAng ) {
				VectorSubtract( mb->org, ma->org, dir );
				vectoangles( dir, cg.refdefViewAngles );
				AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
				if ( dir[2] < 0 ) {
					cg_thirdPersonRange.value = (float)-abs( (int)cg_thirdPersonRange.value );
				}
			} else if ( cg.demoPlayback ) {
				VectorCopy( userAng, cg.refdefViewAngles );
				AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
			}
			if ( ctx->third ) {
				cg.refdef.vieworg[2] -= 35;
				CG_OffsetThirdPersonView();
			}
		} else {
			// no missile: the ghost's own (interpolated) position
			int i;

			for ( i = 0; i < 3; i++ ) {
				cg.refdef.vieworg[i] = s->a.origin[i] + frac * ( ( s->haveNext ? s->b.origin[i] : s->a.origin[i] ) - s->a.origin[i] );
			}
			if ( ctx->followAng ) {
				for ( i = 0; i < 3; i++ ) {
					cg.refdefViewAngles[i] = LerpAngle( s->a.angles[i], s->haveNext ? s->b.angles[i] : s->a.angles[i], frac );
				}
			} else if ( cg.demoPlayback ) {
				VectorCopy( userAng, cg.refdefViewAngles );
			}
			AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
			DG_ViewWeapon( 0 );
			if ( ctx->followAng ) {
				DG_ViewWeapon( s->a.weapon );
			}
			if ( ctx->third ) {
				CG_OffsetThirdPersonView();
			} else {
				DG_EyeHeight( s );
				ctx->hideBody = 1;
				ctx->shadow = 1;
			}
		}
		cg_thirdPersonRange.value = (float)abs( (int)cg_thirdPersonRange.value );
		break;
	}
	case 13: case 14: {
		vec3_t a, t, dir;

		cg_drawCrosshair.integer = 0;
		cg_draw2D.integer = 0;
		cg_drawGun.integer = 0;
		if ( mode == 13 ) {
			VectorCopy( ang, a );
		} else {
			DG_TargetPos( t );
			VectorSubtract( t, org, dir );
			VectorNormalize( dir );
			vectoangles( dir, a );
		}
		DG_RollLook( a );
		DG_SetView( org, a );
		DG_EyeHeight( s );
		ctx->hideBody = 1;
		break;
	}
	case 15:
		DG_SetView( org, ang );
		DG_EyeHeight( s );
		DG_ViewWeapon( s->a.weapon );
		ctx->hideBody = 1;
		break;
	case 16:
		ctx->hideBody = 1;
		break;
	case 17:
		if ( (int)DG_CvarF( "df_demos_ReplayCameraMode", 0 ) && DG_ReplayCamOwnsView() ) {
			ctx->shadow = 1;
		} else if ( cg.renderingThirdPerson ) {
			DG_SetView( org, ang );
			CG_OffsetThirdPersonView();
		} else {
			DG_SetView( org, ang );
			DG_EyeHeight( s );
			DG_ViewWeapon( s->a.weapon );
			ctx->hideBody = 1;
			ctx->shadow = 1;
		}
		break;
	default:
		handled = qfalse;
		break;
	}
	if ( handled ) {
		ctx->mode = mode;
		if ( df_ghosts_CheckPVS.integer ) {
			CG_DF_CheckPVS( cg.refdef.vieworg );
		}
		if ( !dgInMini ) {
			DG_BlankGun();		// tail of 0x12a10: 0x1296b(1) (CG_DrawActiveFrame skips its own gun while a ghost camera plays, 0x109d6)
		}
	}
	return handled;
}

static int DG_RingPrev( int i ) {		// 0x123bd
	return i > 0 ? i - 1 : DG_RING - 1;
}

static int DG_RingNext( int i ) {		// 0x123d3
	return ( i + 1 ) % DG_RING;
}

// 0x11786 into a ring slot: an unreadable frame leaves the slot empty (time 0)
static void DG_RingFill( int slot, int idx ) {
	dgFrame_t *f = &dgRing[slot][idx];

	if ( !DG_ReadFrame( slot, f ) ) {
		f->time = 0;
	}
}

// df_ghosts_AutoModeMP (clamped 0..1, 1221244) with a play timescale of exactly 1: the playback follows the recorded frame times in absolute
// time (0x12421 / 0x11e15), otherwise the time base is advanced by the scaled frame gaps
static qboolean DG_AbsoluteSync( float ts ) {
	return (int)Com_Clamp( 0, 1, DG_CvarF( "df_ghosts_AutoModeMP", 0 ) ) != 0 && ts == 1.0f;
}

// 0x12421 (+ 0x123e2 time-base anchor): advances the cursor in the frame ring until the pair (A, B) brackets the playback time.  Forward:
// A = the frame before the cursor, B = the cursor.  Backwards (negative timescale): the pair is swapped (A = the later frame, B = the
// earlier one) and only frames still in the ring can be reached; at the oldest one the ghost stands still.  Fills s->a / s->b / s->p.
static void DG_Advance( int slot, float ts ) {
	dgSlot_t *s = &dgSlot[slot];
	dgFrame_t *ring = dgRing[slot];
	int *cur = &dgCur[slot];
	float ats = ts < 0 ? -ts : ts;
	qboolean absSync = DG_AbsoluteSync( ts );
	int a, b, nxt;

	if ( s->anchor ) {		// 0x123e2
		s->lastTime = absSync ? cg.time - ring[*cur].time : cg.time;
		s->anchor = qfalse;
	}
	a = DG_RingPrev( *cur );
	b = *cur;
	for ( ;; ) {
		if ( ts == 0 ) {
			a = b;
			break;
		}
		if ( ts > 0 && ring[a].time != 0 && ring[b].time == 0 ) {		// the end of the file: nothing to advance to
			break;
		}
		if ( ring[a].time == 0 || ring[a].time >= ring[b].time ) {		// 0x1252e: no valid pair
			if ( ts < 0 ) {
				a = b;
				break;
			}
		} else if ( absSync ) {
			if ( !( ring[b].time + s->lastTime < cg.time ) ) {
				break;
			}
		} else {
			float dt = (float)( ring[b].time - ring[a].time ), el = ( cg.time - s->lastTime ) * ats, q = 1.0f;

			if ( el != 0 ) {
				q = dt / el;
				if ( q < 0 ) {
					q = -q;
				}
			}
			if ( q >= 1.0f ) {
				break;
			}
			s->lastTime = (int)( s->lastTime + dt / ats );
		}
		if ( ts > 0 ) {		// 0x1253b
			nxt = DG_RingNext( *cur );
			if ( ring[nxt].time < ring[*cur].time ) {
				DG_RingFill( slot, nxt );
			}
			a = *cur;
			*cur = nxt;
			b = *cur;
			if ( ring[*cur].time == 0 ) {
				goto done;
			}
		} else {			// 0x125c8
			*cur = a;
			a = DG_RingPrev( *cur );
			b = *cur;
		}
	}
	if ( ts > 0 ) {		// 0x125e8
		a = DG_RingPrev( *cur );
		b = *cur;
	} else {
		int t = b;

		b = a;
		a = t;
	}
done:
	s->ia = a;
	s->ib = b;
	s->a = ring[a];
	s->b = ring[b];
	s->haveNext = a != b && ring[a].time != 0 && ring[b].time != 0;
	nxt = DG_RingPrev( a );
	s->p = ( ring[nxt].time != 0 && ring[nxt].time < ring[a].time ) ? ring[nxt] : ring[a];
}

static void DG_PlayFrame( int slot, qboolean mini ) {
	dgSlot_t *s = &dgSlot[slot];
	float ts = DG_SlotCvar( slot, "PlayTimescale", 1.0f );
	int mode = (int)DG_SlotCvar( slot, "PlayDrawPlayer", 3 );
	int loop = (int)DG_SlotCvar( slot, "PlayLoop", 0 );
	float elapsed, frac = 0, radius;
	int dt;
	vec3_t org, ang, d;
	centity_t *c = &s->cent;
	qboolean near, invis, discont, nearFlag, hideBody, drawBody;
	dgCtx_t ctx;

	float ats = ts < 0 ? -ts : ts;

	if ( mini && (int)DG_CvarF( "df_ghosts_MiniviewCameraMode", 1 ) == 0 ) {		// 0x127af: b != 0 and mode word 0 -> skip the slot
		return;
	}
	if ( !mini ) {		// the miniview pass (0x127af a = 0) does not advance the cursor
		DG_Advance( slot, ts );
	}
	// 0x11e15: the playback time since the time base, in frame-time units (absolute against frame A for the AutoModeMP sync)
	elapsed = DG_AbsoluteSync( ts ) ? (float)( cg.time - ( s->a.time + s->lastTime ) ) : ( cg.time - s->lastTime ) * ats;
	discont = s->haveNext && ( s->b.discont || s->a.discont );
	if ( s->haveNext && !discont ) {
		dt = s->b.time - s->a.time;
		if ( dt < 0 ) {
			dt = -dt;		// backwards: A is the later frame
		}
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
	// the play camera (0x12a10) may put the view at this ghost; then the NoDrawRadius test (13125) is skipped
	Com_Memset( &ctx, 0, sizeof( ctx ) );
	ctx.missileIdx = -1;
	{
		int cm = mini ? (int)DG_CvarF( "df_ghosts_MiniviewCameraMode", 1 ) : (int)DG_SlotCvar( slot, "PlayCameraMode", 0 );

		if ( cm < 0 || cm > 17 ) {
			cm = 0;
		}
		if ( cm && !dgCamClaimed ) {
			if ( DG_Camera( slot, org, ang, cm, frac, &ctx ) ) {
				dgCamClaimed = qtrue;
			}
		}
	}
	if ( ( !near || ctx.mode ) && (int)DG_SlotCvar( slot, "PlayDrawMissiles", 2 ) >= 1 ) {
		DG_DrawMissiles( slot, frac, discont, ctx.third ? -1 : ctx.missileIdx );
	}
	if ( !near && (int)DG_SlotCvar( slot, "PlayDrawMissiles", 2 ) >= 2 ) {
		DG_DrawSprites( slot );
	}
	nearFlag = ctx.mode ? qfalse : near;
	hideBody = ctx.mode ? ctx.hideBody : near;
	invis = qfalse;
	drawBody = qtrue;
	switch ( mode ) {
	case 1:
		drawBody = !hideBody;
		break;
	case 2:
		drawBody = !hideBody;
		invis = qtrue;
		break;
	case 3:
		if ( nearFlag ) {
			invis = qtrue;
		} else {
			drawBody = !hideBody;
		}
		break;
	case 4:		// marker (0x13125 case 4): a white radius-10 sprite with the "medal_gauntlet" shader at the interpolated position
		drawBody = qfalse;
		if ( !hideBody ) {
			refEntity_t re;

			Com_Memset( &re, 0, sizeof( re ) );
			re.reType = RT_SPRITE;
			re.customShader = cgs.media.medalGauntlet;
			re.radius = 10;
			re.shaderRGBA[0] = re.shaderRGBA[1] = re.shaderRGBA[2] = re.shaderRGBA[3] = 255;
			VectorCopy( org, re.origin );
			trap_R_AddRefEntityToScene( &re );
			DG_Label( slot, c->lerpOrigin );
		}
		break;
	default:
		drawBody = qfalse;		// 0 = hidden
		break;
	}
	if ( drawBody ) {
		c->currentState.powerups = invis ? ( 1 << PW_INVIS ) : 0;
		CG_Player( c );
		if ( !nearFlag ) {
			DG_Label( slot, c->lerpOrigin );
		}
	}
	if ( ctx.shadow ) {
		float plane;

		CG_PlayerShadow( c, &plane );		// 0x26a50
	}
	if ( !mini ) {
		DG_VarsApply( slot );		// 0x11ee4
		if ( dgRing[slot][dgCur[slot]].time == 0 ) {		// 0x12872: the cursor ran into an empty frame = the end of the file
			char keep[sizeof( s->name )];

			switch ( loop ) {
			case 2:			// freeze on the last frame
				dgCur[slot] = DG_RingPrev( dgCur[slot] );
				break;
			case 1:			// restart
				Q_strncpyz( keep, s->name, sizeof( keep ) );
				DG_Close( slot, 7 );
				DG_Play( slot, keep, 7 );
				break;
			default:
				DG_Close( slot, 7 );
				break;
			}
		}
	}
}

// ---- miniview (cgame 0x7a04 -> gate 0x13488 -> 0x134dc; doc CGAME/ghost_camera.md "Miniview") ----
// After the main frame (HUD included) a second small scene is rendered: the player seen with the opposite third-person state,
// the tracked client (df_mp_TrackPlayerNum), or a playing ghost through df_ghosts_MiniviewCameraMode.

static qboolean DG_AnyPlaying( void ) {		// 0x140ea
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 2 ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean DG_GhostCamActive( void ) {		// 0x13e70: a playing ghost whose own camera mode is set
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 2 && (int)DG_SlotCvar( i, "PlayCameraMode", 0 ) != 0 ) {
			return qtrue;
		}
	}
	return qfalse;
}

// CG_DrawActiveFrame (0x29d7d): the ordinary view weapon is skipped while a ghost camera plays (0x109d6, the camera draws the gun itself)
// or in demo camera mode 2 (0x146fa)
qboolean CG_DF_SkipViewWeapon( void ) {
	if ( DG_GhostCamActive() ) {
		return qtrue;
	}
	return cg.demoPlayback && df_demos_CameraMode.integer == 2;
}

// 0x10912: the tracked-client miniview is active (record of the tracked client is valid and it is not the viewer)
static qboolean DG_MiniTracked( int client, int draw ) {
	if ( !dgInMini || draw == 0 ) {
		return qfalse;
	}
	if ( client == cg.predictedPlayerState.clientNum ) {
		return qfalse;
	}
	if ( client != (int)DG_CvarF( "df_mp_TrackPlayerNum", -1 ) ) {
		return qfalse;
	}
	return draw == 7 || draw == 8;
}

// 0x13488
static qboolean DG_MiniGate( void ) {
	int draw = (int)DG_CvarF( "df_ghosts_MiniviewDraw", 3 );
	qboolean any = DG_AnyPlaying();

	if ( draw == 0 ) {
		return qfalse;
	}
	if ( !any && draw <= 1 ) {
		return qfalse;
	}
	if ( !cg.demoPlayback && !any && draw == 2 ) {
		return qfalse;
	}
	if ( !cg.demoPlayback && draw == 3 ) {
		return qfalse;
	}
	if ( draw == 8 ) {
		int client, weapon;
		vec3_t p, a;

		CG_DF_TrackRecord( &client, &weapon, p, a );
		if ( (int)DG_CvarF( "df_mp_TrackPlayerNum", -1 ) != client ) {
			return qfalse;
		}
	}
	return qtrue;
}

// 0x1296b(1): the view weapon drawn from a blank player state (only the weapon survives); 0x129e6 forces cg_drawGun on meanwhile
static void DG_MiniGun( qboolean blank ) {
	int keep = cg_drawGun.integer;

	if ( !cg_drawGun.integer ) {
		cg_drawGun.integer = 1;
	}
	if ( !blank ) {
		CG_AddViewWeapon( &cg.predictedPlayerState );
	} else {
		DG_BlankGun();
	}
	cg_drawGun.integer = keep;
}

// 0x134dc
static void DG_Miniview( void ) {
	int		draw = (int)DG_CvarF( "df_ghosts_MiniviewDraw", 3 );
	int		camMode = (int)DG_CvarF( "df_ghosts_MiniviewCameraMode", 1 );
	int		drawGun = (int)DG_CvarF( "df_ghosts_MiniviewDrawGun", 1 );
	int		size = (int)DG_CvarF( "df_ghosts_MiniviewSize", 25 );
	float	offX = DG_CvarF( "df_ghosts_MiniviewOffsetX", 235 ), offY = DG_CvarF( "df_ghosts_MiniviewOffsetY", -155 );
	float	border = DG_CvarF( "df_ghosts_MiniviewBorderSize", 4 );
	int		fov = (int)DG_CvarF( "df_ghosts_MiniviewFOV", 90 );
	int		saveTP = cg.renderingThirdPerson, saveFovI = cg_fov.integer, tpAfter;
	float	saveFov = cg_fov.value, scale640, scaleSize;
	int		sx = cg.refdef.x, sy = cg.refdef.y, sw = cg.refdef.width, sh = cg.refdef.height;
	qboolean any = DG_AnyPlaying(), ghostOwns = qfalse, track, gun;
	int		i, client = 0, weapon = 0;
	vec3_t	tpos, tang;

	if ( any ) {
		DG_ThirdPersonFor( camMode );
	}
	cg_fov.integer = fov;
	cg_fov.value = (float)fov;
	if ( camMode == 0 || !any ) {
		switch ( draw ) {
		case 1: case 2: case 3: case 4:
			cg.renderingThirdPerson = cg.renderingThirdPerson ? 0 : 1;
			if ( DG_GhostCamActive() ) {
				cg.renderingThirdPerson = 0;
			}
			break;
		case 5: case 7: case 8:
			cg.renderingThirdPerson = 0;
			break;
		case 6:
			cg.renderingThirdPerson = 1;
			break;
		default:
			break;
		}
	} else {
		ghostOwns = qtrue;
	}
	// 0x29e5e: view offsets + fov for the (possibly toggled) third-person state
	if ( cg.renderingThirdPerson ) {
		CG_OffsetThirdPersonView();
	} else {
		CG_OffsetFirstPersonView();
	}
	CG_CalcFov();

	scale640 = (float)sw / 640.0f;
	scaleSize = (float)size / 100.0f;
	cg.refdef.width = (int)( (float)sw * scaleSize );
	cg.refdef.height = (int)( (float)sh * scaleSize );
	cg.refdef.x = (int)( (float)sw / 2.0f - (float)cg.refdef.width / 2.0f + offX * scale640 );
	cg.refdef.y = (int)( (float)sh / 2.0f - (float)cg.refdef.height / 2.0f + offY * scale640 );
	if ( border > 0 ) {
		float bx = (float)cg.refdef.x - border, by = (float)cg.refdef.y - border;
		float bw = (float)cg.refdef.width + 2 * border, bh = (float)cg.refdef.height + 2 * border;

		trap_R_DrawStretchPic( bx, by, bw, bh, bx / 64.0f, by / 64.0f, ( bx + bw ) / 64.0f, ( by + bh ) / 64.0f, cgs.media.backTileShader );
	}

	tpAfter = cg.renderingThirdPerson;
	track = qfalse;
	if ( !any && ( draw == 7 || draw == 8 ) ) {
		CG_DF_TrackRecord( &client, &weapon, tpos, tang );
	}
	if ( !any ) {
		// 0x10912 is evaluated with the record filled by 0x7c81 (draw 7/8 only)
		track = ( draw == 7 || draw == 8 ) ? DG_MiniTracked( client, draw ) : qfalse;
	}
	if ( track ) {
		cg.renderingThirdPerson = 1;
	}
	if ( ghostOwns ) {
		cg.renderingThirdPerson = 1;
	}
	if ( !cg.hyperspace ) {
		CG_AddPacketEntities();
		CG_AddMarks();
		CG_AddParticles();
		CG_AddLocalEntities();
	}
	cg.renderingThirdPerson = tpAfter;

	if ( any ) {
		gun = qtrue;
		if ( camMode ) {
			cg.renderingThirdPerson = saveTP;
			dgCamClaimed = qfalse;
			for ( i = 0; i < DG_SLOTS; i++ ) {
				if ( dgSlot[i].state == 2 ) {
					DG_PlayFrame( i, qtrue );
				}
			}
		}
		if ( ( camMode == 15 || camMode == 16 ) && cg.renderingThirdPerson ) {
			gun = qfalse;
		}
		if ( gun && drawGun ) {
			DG_MiniGun( ghostOwns );
		}
	} else {
		if ( draw == 7 || draw == 8 ) {
			VectorCopy( tpos, cg.refdef.vieworg );
			VectorCopy( tang, cg.refdefViewAngles );
			AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
			VectorCopy( tang, cg.predictedPlayerState.viewangles );
			cg.predictedPlayerState.weapon = weapon;
			cg.predictedPlayerEntity.currentState.weapon = weapon;
		}
		if ( !track ) {
			if ( !cg.renderingThirdPerson ) {
				VectorCopy( cg.predictedPlayerState.origin, cg.refdef.vieworg );
				cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;
				VectorCopy( cg.predictedPlayerState.viewangles, cg.refdefViewAngles );
				AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
				if ( drawGun ) {
					DG_MiniGun( qfalse );
				}
			}
		} else if ( drawGun ) {
			// 0x129fe: clear powerups and the land-bob state, then the blank-state view weapon
			DG_ClearViewState();
			DG_MiniGun( qtrue );
		}
	}
	trap_R_RenderScene( &cg.refdef );
	cg.refdef.x = sx;
	cg.refdef.y = sy;
	cg.refdef.width = sw;
	cg.refdef.height = sh;
	cg.renderingThirdPerson = saveTP;
	cg_fov.integer = saveFovI;
	cg_fov.value = saveFov;
}

// 0x7a04
void CG_DF_Miniview( void ) {
	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD || !cg.snap ) {
		return;
	}
	dgInMini = qtrue;
	if ( DG_MiniGate() ) {
		DG_Miniview();
	}
	dgInMini = qfalse;
}

// ---- per-frame entry (after the entities of the frame were added; cgame 0x79c1 -> 0x12785 / 0x127af) ----
static void DG_CommandCvar( void );

void CG_DF_GhostFrame( void ) {
	int i;

	dgCamClaimed = qfalse;
	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 1 ) {
			DG_Sample( i );
		} else if ( dgSlot[i].state == 2 ) {
			DG_PlayFrame( i, qfalse );
		}
	}
	DG_CommandCvar();
	CG_DF_SmokeTrails();
	CG_DF_AutoRecFrame();
	CG_DF_EnforceMaxFps();
}

// ---- `ghost` command (0x13e09 -> parser 0x13a4a -> executor 0x13b9a) ----
// ---- AutoMode (cgame 0x143f0 run start / 0x14474 run finish; df_ghosts_AutoMode 3..8, df_ghosts_AutoModeMP) ----
// The "best ghost" index is the small text file  system/ghosts/<base>.grec  containing "<ghost name> <time ms>" (0x14110 reads, 0x141f4
// writes it including the terminating NUL). Mode 3: record into slot 0 and play the best ghost in slot 1 (live); 4: the same, recording
// only when sv_cheats is 0; 5: play the default-named files of slots 0 and 1; 6 (demos): record only; 7/8 (demos): as 3, 8 updates the best
// even without an improvement. If the default name of slot 0 IS the best ghost's name the roles swap (record slot 1, play slot 0).
#define DG_GREC_DIR "system/ghosts"

static qboolean DG_BestRead( char *name, int nameSize, char *timeStr, int timeSize ) {		// 0x14110
	char			path[MAX_QPATH], base[MAX_QPATH], text[256];
	const char		*p, *tok;
	fileHandle_t	f;
	int				len;

	name[0] = timeStr[0] = 0;
	CG_DF_RecordBaseName( base, sizeof( base ) );
	Com_sprintf( path, sizeof( path ), DG_GREC_DIR "/%s.grec", base );
	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( !f ) {
		return qfalse;
	}
	if ( len > (int)sizeof( text ) - 1 ) {
		len = sizeof( text ) - 1;
	}
	trap_FS_Read( text, len, f );
	trap_FS_FCloseFile( f );
	text[len] = 0;
	p = text;
	tok = COM_Parse( &p );
	Q_strncpyz( name, tok, nameSize );
	tok = COM_Parse( &p );
	Q_strncpyz( timeStr, tok, timeSize );
	return name[0] && timeStr[0];
}

static qboolean DG_BestWrite( int slot, const char *timeStr ) {		// 0x141f4
	char			path[MAX_QPATH], base[MAX_QPATH], text[128];
	fileHandle_t	f;

	CG_DF_RecordBaseName( base, sizeof( base ) );
	Com_sprintf( path, sizeof( path ), DG_GREC_DIR "/%s.grec", base );
	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		return qfalse;
	}
	Com_sprintf( text, sizeof( text ), "%s %s", dgSlot[slot].name, timeStr );
	trap_FS_Write( text, strlen( text ) + 1, f );
	trap_FS_FCloseFile( f );
	return qtrue;
}

static qboolean DG_AutoGate( int *mode ) {		// common prologue of 0x143f0 / 0x14474
	char si[BIG_INFO_STRING];
	int v, mp;

	Q_strncpyz( si, CG_ConfigString( CS_SERVERINFO ), sizeof( si ) );
	v = atoi( Info_ValueForKey( si, "defrag_vers" ) );
	if ( v > 0 && v < 1000 ) {
		v *= 100;
	}
	mp = ( v >= 18100 && atoi( Info_ValueForKey( si, "defrag_gametype" ) ) < 5 );		// 0x11d2ac (CHS item 225 "multiplayer")
	if ( mp && !(int)DG_CvarF( "df_ghosts_AutoModeMP", 0 ) ) {
		return qfalse;
	}
	if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR ) {
		return qfalse;
	}
	*mode = (int)DG_CvarF( "df_ghosts_AutoMode", 4 );
	return qtrue;
}

static void DG_AutoBest( qboolean doRecord, qboolean doPlay, int flags ) {		// 0x14270
	char	bestName[128], bestTime[32], defName[MAX_QPATH], name[MAX_QPATH];
	int		recSlot = 0, playSlot = 1, i;

	for ( i = 0; i < 2; i++ ) {
		DG_Close( i, 0 );
	}
	DG_BestRead( bestName, sizeof( bestName ), bestTime, sizeof( bestTime ) );
	DG_DefaultName( 0, defName, sizeof( defName ) );
	dgBestMs = bestTime[0] ? atoi( bestTime ) : 0;
	Q_strncpyz( name, defName, sizeof( name ) );
	if ( !strcmp( defName, bestName ) ) {
		recSlot = 1;
		playSlot = 0;
		DG_DefaultName( 1, name, sizeof( name ) );
	}
	if ( doRecord ) {
		DG_Record( recSlot, name, flags );
	}
	if ( bestName[0] && doPlay ) {
		DG_Play( playSlot, bestName, flags );
		if ( flags & 4 ) {
			DG_Status( playSlot, DG_ST_BEST );
		}
	}
}

void CG_DF_GhostRunStart( void ) {		// 0x143f0 (from the TimerStarted handler 0x1ae1, flags 7)
	char	si[BIG_INFO_STRING];
	int		mode, flags = 7, i;

	if ( !DG_AutoGate( &mode ) ) {
		return;
	}
	if ( !cg.demoPlayback && mode == 3 ) {
		DG_AutoBest( qtrue, qtrue, flags );
	}
	if ( !cg.demoPlayback && mode == 4 ) {
		char sys[BIG_INFO_STRING];

		Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
		DG_AutoBest( atoi( Info_ValueForKey( sys, "sv_cheats" ) ) ? qfalse : qtrue, qtrue, flags );		// 0x11d40c = sv_cheats != 0
	}
	if ( mode == 5 ) {		// 0x14370
		char n0[MAX_QPATH], n1[MAX_QPATH];

		for ( i = 0; i < 2; i++ ) {
			DG_Close( i, 0 );
		}
		DG_DefaultName( 0, n0, sizeof( n0 ) );
		DG_DefaultName( 1, n1, sizeof( n1 ) );
		DG_Play( 0, n0, flags );
		DG_Play( 1, n1, flags );
	}
	if ( cg.demoPlayback && mode == 6 ) {		// 0x143e5
		DG_Record( 0, "", flags );
	}
	if ( cg.demoPlayback && ( mode == 7 || mode == 8 ) ) {
		DG_AutoBest( qtrue, qtrue, flags );
	}
	(void)si;
}

void CG_DF_GhostRunEnd( int ms ) {		// 0x14474( ms, reason 0, update 1, flags 7 ) from the TimerStopped handler 0x1aec
	int		mode, flags = 7, reason = 0, update = 1;
	int		recIdx = -1, i;
	qboolean stopRec, stopPlay;
	char	bestName[128], bestTime[32], timeStr[32];

	if ( !DG_AutoGate( &mode ) || mode < 3 || mode > 8 ) {
		return;
	}
	if ( cg.demoPlayback && mode != 7 && mode != 8 ) {
		update = 0;
	}
	switch ( reason ) {
	case 0: stopRec = qtrue; stopPlay = qfalse; break;
	case 1: update = 0; stopRec = qfalse; stopPlay = qtrue; break;
	default: stopRec = stopPlay = qtrue; break;
	}
	for ( i = 0; i < 2; i++ ) {		// the highest slot index recording wins (0x14520 / 0x14526)
		if ( dgSlot[i].state == 1 ) {
			recIdx = i;
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( dgSlot[i].state == 1 && stopRec ) {
			DG_Close( i, flags );
		} else if ( dgSlot[i].state == 2 && stopPlay ) {
			DG_Close( i, flags );
		}
	}
	if ( !update ) {
		return;
	}
	DG_BestRead( bestName, sizeof( bestName ), bestTime, sizeof( bestTime ) );
	Com_sprintf( timeStr, sizeof( timeStr ), "%i", ms );
	if ( bestName[0] ) {
		int bestMs = atoi( bestTime );

		if ( !( ms < bestMs || ( df_tieOverwrites.integer && ms == bestMs ) || mode == 8 ) ) {
			return;
		}
		if ( recIdx == -1 || ms <= 0 ) {
			return;
		}
	} else if ( recIdx == -1 ) {
		return;
	}
	DG_BestWrite( recIdx, timeStr );
	dgBestMs = ms;
	if ( flags & 4 ) {
		DG_Status( recIdx, DG_ST_NEWBEST );
	}
}

static void DG_Help( void ) {
	CG_Printf( "^1GHOST --> Use #index to specify ghost number\n^5Example: ^2ghost #all play\n" );
}

static void DG_Exec( const char *idx, const char *cmd, const char *name ) {		// executor of 0x13b9a
	int first, last, i, flags = 7;		// 0x13dd0 passes 7

	if ( idx[0] != '#' ) {
		CG_Printf( "^1GHOST --> INVALID INDEX: Use #index to specify ghost number\n" );
		return;
	}
	if ( !Q_stricmp( idx, "#all" ) ) {
		first = 0;
		last = DG_SLOTS - 1;
		if ( !Q_stricmp( cmd, "stop" ) || !Q_stricmp( cmd, "play" ) ) {
			flags &= ~3;		// 0x13bd3: no failure lines for #all
		}
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
			DG_Stop( i, flags );
		} else if ( !Q_stricmp( cmd, "record" ) ) {
			DG_Record( i, name, flags );
		} else if ( !Q_stricmp( cmd, "play" ) ) {
			DG_Play( i, name, flags );
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

// 0x13e70 + the head of the CHS item function: the first playing ghost with a camera mode is "tracked", and the position / velocity / angles
// items then read that ghost instead of the player
qboolean CG_DF_GhostTracked( vec3_t pos, vec3_t vel, vec3_t ang ) {
	int i;

	for ( i = 0; i < DG_SLOTS; i++ ) {
		if ( dgSlot[i].state == 2 && dgSlot[i].camWord != 0 ) {
			CG_DF_GhostVelocity( i, vel );
			VectorCopy( dgSlot[i].cent.lerpOrigin, pos );
			VectorCopy( dgSlot[i].cent.lerpAngles, ang );
			return qtrue;
		}
	}
	return qfalse;
}

// CHS items 81..86 (cgame 0xd6b5..0xd8ab): kind 0 = the state of the slot, 1 = its record settings, 2 = its play settings
void CG_DF_GhostInfo( int slot, int kind, char *label, char *value, int size ) {
	const dgSlot_t *s = &dgSlot[slot];
	const char *state = s->state == 1 ? "recording" : s->state == 2 ? "playing" : "stopped";

	switch ( kind ) {
	case 3:		// item 80: the settings shared by all ghosts (label set by the caller)
		Com_sprintf( value, size, "%i %i", (int)DG_CvarF( "df_ghosts_AutoMode", 4 ), (int)DG_CvarF( "df_ghosts_NoDrawRadius", 200 ) );
		break;
	case 0:
		Com_sprintf( label, size, "MODE [ghost #%i]", slot + 1 );
		if ( s->state == 1 ) {
			Com_sprintf( value, size, "%s [ %s - %3i ]", state, s->name, s->frames );
		} else if ( s->state == 2 ) {
			Com_sprintf( value, size, "%s [ %s - %3i %i %3.3f ]", state, s->name, dgCur[slot], dgRing[slot][dgCur[slot]].time,
				Com_Clamp( -10.0f, 10.0f, DG_SlotCvar( slot, "PlayTimescale", 1.0f ) ) );
		} else {
			Com_sprintf( value, size, "%s", state );
		}
		break;
	case 1:
		Com_sprintf( label, size, "SET [ghost #%i] (rec): (File View Int)", slot + 1 );
		Com_sprintf( value, size, "%i %i %i", (int)DG_SlotCvar( slot, "RecordFileFormat", 2 ), (int)DG_SlotCvar( slot, "RecordView", 0 ),
			(int)DG_SlotCvar( slot, "RecordInterval", 50 ) );
		break;
	case 2:
		Com_sprintf( label, size, "SET [ghost #%i] (play): (Cam DrawP DrawM Loop TS)", slot + 1 );
		Com_sprintf( value, size, "%i %i %i %i %3.3f", (int)DG_SlotCvar( slot, "PlayCameraMode", 0 ), (int)DG_SlotCvar( slot, "PlayDrawPlayer", 3 ),
			(int)DG_SlotCvar( slot, "PlayDrawMissiles", 2 ), (int)DG_SlotCvar( slot, "PlayLoop", 0 ),
			Com_Clamp( -10.0f, 10.0f, DG_SlotCvar( slot, "PlayTimescale", 1.0f ) ) );
		break;
	}
}

qboolean CG_DF_GhostPos( int slot, vec3_t out ) {		// the ghost's drawn position (0x...: cent.lerpOrigin at slot+312928)
	if ( !CG_DF_GhostPlaying( slot ) || !dgSlot[slot].centInit ) {
		return qfalse;
	}
	VectorCopy( dgSlot[slot].cent.lerpOrigin, out );
	return qtrue;
}
