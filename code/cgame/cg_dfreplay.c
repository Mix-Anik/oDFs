// cg_dfreplay.c -- DeFRaG replay / recam editing system, command layer (cgame 0x18006 dispatcher; see disasm/CGAME/replay_system.md)
//
// Ported: LOAD (script parser = SET records), SAVE (script writer), the in-memory tables (VAR / CAM / TRAN / ACT / LINK), `replay SET | GET | CLEAR | DELETE | ACTIVATE | VALIDATE | HELP`.
// NOT ported (undecoded): the GET table formats 2 / 3 and the CHAIN tracer of
// format 1 (0x1816e), the playback runtime that drives interpolants / activations / the camera, and the edit overlay 0x18873.
// DeFRaG's global 0x2b150c ("verbose", passed to the validators / echo after SET) has no decoded writer: it is the cvar
// df_demos_ReplayVerbose here (default 1), which is NOT a DeFRaG cvar.

#include "cg_local.h"

#define DR_NUM_TYPES	6

enum { DR_NONE, DR_VAR, DR_CAM, DR_TRAN, DR_ACT, DR_LINK };

#define DR_MAX_VAR	200
#define DR_MAX_CAM	100
#define DR_MAX_TRAN	100
#define DR_MAX_ACT	100
#define DR_MAX_LINK	400

typedef struct { char name[64]; float value, current, original; } drVar_t;			// 76 bytes @ 2791296
typedef struct { vec3_t origin, angles; } drCam_t;									// 24 bytes @ 2806496
typedef struct { int type, dtype, id1, id2; float speed, loop, curLoop, interp[3]; int activated; } drTran_t;	// 44 bytes @ 2808896
typedef struct { int type; vec3_t pos1, pos2; float value; int activated; } drAct_t;	// 36 bytes @ 2813296
typedef struct { int dtype1, id1, dtype2, id2; } drLink_t;							// 16 bytes @ 2816896

static drVar_t	drVars[DR_MAX_VAR];
static drCam_t	drCams[DR_MAX_CAM];
static drTran_t	drTrans[DR_MAX_TRAN];
static drAct_t	drActs[DR_MAX_ACT];
static drLink_t	drLinks[DR_MAX_LINK];

static int		drEnabled;			// 0x2b1504 (set by VALIDATE / a loaded script; gates the edit overlay)
vmCvar_t		df_demos_ReplayVerbose;
vmCvar_t		df_demos_ReplayMode, df_demos_ReplayFilename, df_demos_ReplayDrawMask;

static const int drCount[DR_NUM_TYPES] = { 0, DR_MAX_VAR, DR_MAX_CAM, DR_MAX_TRAN, DR_MAX_ACT, DR_MAX_LINK };
static const char *drTypeNames[DR_NUM_TYPES] = { "NONE", "VAR", "CAM", "TRAN", "ACT", "LINK" };
static const char *drActNames[6] = { "NONE", "MANUAL", "TIMER", "TRANEND", "RADIUS", "VOLUME" };
static const char *drTranNames[11] = { "NONE", "TIME", "POSX", "POSY", "POSZ", "POSXY", "POSXZ", "POSYX", "POSYZ", "POSZX", "POSZY" };

// ---- argument cursor (DeFRaG tokenises the rest of the command line with 0x31f8b) ----
static int drArg;
static const char	*drText;		// when non-NULL tokens come from a script file (COM_Parse), else from the command arguments

static const char *DR_Tok( void ) {
	static char	buf[MAX_STRING_CHARS];

	if ( drText ) {
		Q_strncpyz( buf, COM_Parse( &drText ), sizeof( buf ) );
		return buf;
	}
	if ( drArg >= trap_Argc() ) {
		buf[0] = 0;
		return buf;
	}
	trap_Argv( drArg++, buf, sizeof( buf ) );
	return buf;
}

// 0x8a85 + 0x8ae2 / 0x8b0d: `$var` expansion, then numeric conversion; returns qfalse (value untouched) for an empty token
static qboolean DR_ParseInt( int *v ) {
	char tok[MAX_STRING_CHARS];

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	if ( !CG_DF_ExpandVar( tok, sizeof( tok ) ) ) {
		return qfalse;
	}
	*v = atoi( tok );
	return qtrue;
}

static qboolean DR_ParseFloat( float *v ) {
	char tok[MAX_STRING_CHARS];

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	if ( !CG_DF_ExpandVar( tok, sizeof( tok ) ) ) {
		return qfalse;
	}
	*v = atof( tok );
	return qtrue;
}

static void DR_ParseVec3( vec3_t v ) {		// 0x8bb1
	int i;

	for ( i = 0; i < 3; i++ ) {
		DR_ParseFloat( &v[i] );
	}
}

// 0x8646: index of `s` in a name table (case-insensitive), -1 when absent
static int DR_Lookup( const char **names, int count, const char *s ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		if ( !Q_stricmp( names[i], s ) ) {
			return i;
		}
	}
	return -1;
}

// token -> table index when it is a plain name (0x8a85), otherwise `cur` is kept (callers pass the old value)
static int DR_NameOrKeep( const char **names, int count, const char *tok, int cur ) {
	char buf[MAX_STRING_CHARS];

	Q_strncpyz( buf, tok, sizeof( buf ) );
	if ( CG_DF_ExpandVar( buf, sizeof( buf ) ) ) {
		return DR_Lookup( names, count, buf );
	}
	return cur;
}

// ---- 0x166ef / 0x16711 / 0x16740 / 0x1676c : range and in-use checks ----
static qboolean DR_TypeOk( int type, qboolean warn ) {
	if ( type < 0 || type >= DR_NUM_TYPES ) {		// DeFRaG tests `type <= 6`
		if ( warn ) {
			CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %i )\n", type );
		}
		return qfalse;
	}
	return qtrue;
}

static qboolean DR_RangeOk( int type, int id, qboolean warn ) {		// 0x16740
	if ( !DR_TypeOk( type, warn ) ) {
		return qfalse;
	}
	if ( id < 0 || id >= drCount[type] ) {
		if ( warn ) {
			CG_Printf( "^1REPLAY ERROR:^7 %s ^3[ %i ]^7 ^4-->^7 Invalid ID\n", drTypeNames[type], id );
		}
		return qfalse;
	}
	return qtrue;
}

static qboolean DR_InUse( int type, int id ) {		// 0x1676c
	if ( !DR_RangeOk( type, id, qfalse ) ) {
		return qfalse;
	}
	switch ( type ) {
	case DR_VAR:	return drVars[id].name[0] != 0;
	case DR_CAM:	return VectorLength( drCams[id].origin ) != 0 || VectorLength( drCams[id].angles ) != 0;
	case DR_TRAN:	return drTrans[id].type != 0;
	case DR_ACT:	return drActs[id].type != 0;
	case DR_LINK:	return drLinks[id].dtype1 != 0 && drLinks[id].dtype2 != 0;
	}
	return qfalse;
}

static qboolean DR_Verbose( void ) {
	return df_demos_ReplayVerbose.integer != 0;
}

// ---- 0x18287 : GET printer, full record (format 0); the other formats are in DR_Print ----
static void DR_PrintRecord( int type, int id, int format ) {
	CG_Printf( "^2%s^7 ^3[ %i ]^7\n", drTypeNames[type], id );
	switch ( type ) {
	case DR_VAR: {
		drVar_t *v = &drVars[id];

		CG_Printf( " ^4-->^7 Name = %s\n", v->name );
		CG_Printf( " ^4-->^7 Value = %.3f\n", v->value );
		CG_Printf( " ^4-->^7 ( CurrentValue = %.3f )\n", v->current );
		CG_Printf( " ^4-->^7 ( OriginalValue = %.3f )\n", v->original );
		break;
	}
	case DR_CAM:
		CG_Printf( " ^4-->^7 Position = ( %.3f %.3f %.3f )\n", drCams[id].origin[0], drCams[id].origin[1], drCams[id].origin[2] );
		CG_Printf( " ^4-->^7 Angles = ( %.3f %.3f %.3f )\n", drCams[id].angles[0], drCams[id].angles[1], drCams[id].angles[2] );
		break;
	case DR_TRAN: {
		drTran_t *t = &drTrans[id];

		CG_Printf( " ^4-->^7 Type = %s\n", drTranNames[t->type] );
		CG_Printf( " ^4-->^7 DataType = %s\n", drTypeNames[t->dtype] );
		CG_Printf( " ^4-->^7 StartID = %i\n", t->id1 );
		CG_Printf( " ^4-->^7 StopID = %i\n", t->id2 );
		CG_Printf( " ^4-->^7 Speed = %.3f\n", t->speed );
		CG_Printf( " ^4-->^7 Loop = %.3f\n", t->loop );
		CG_Printf( " ^4-->^7 ( Interpolant0 = %.3f )\n", t->interp[0] );
		CG_Printf( " ^4-->^7 ( Interpolant1 = %.3f )\n", t->interp[1] );
		CG_Printf( " ^4-->^7 ( Interpolant2 = %.3f )\n", t->interp[2] );
		CG_Printf( " ^4-->^7 ( CurrentLoop = %.3f )\n", t->curLoop );
		CG_Printf( " ^4-->^7 ( Activated = %i )\n", t->activated );
		break;
	}
	case DR_ACT: {
		drAct_t *a = &drActs[id];

		CG_Printf( " ^4-->^7 Type = %s\n", drActNames[a->type] );
		CG_Printf( " ^4-->^7 < Position1 = ( %.3f %.3f %.3f ) >\n", a->pos1[0], a->pos1[1], a->pos1[2] );
		CG_Printf( " ^4-->^7 < Position2 = ( %.3f %.3f %.3f ) >\n", a->pos2[0], a->pos2[1], a->pos2[2] );
		CG_Printf( " ^4-->^7 < Value = %.3f >\n", a->value );
		CG_Printf( " ^4-->^7 ( Activated = %i )\n", a->activated );
		break;
	}
	case DR_LINK:
		CG_Printf( " ^4-->^7 DataType1 = %s\n", drTypeNames[drLinks[id].dtype1] );
		CG_Printf( " ^4-->^7 DataID1 = %i\n", drLinks[id].id1 );
		CG_Printf( " ^4-->^7 DataType2 = %s\n", drTypeNames[drLinks[id].dtype2] );
		CG_Printf( " ^4-->^7 DataID2 = %i\n", drLinks[id].id2 );
		break;
	}
}

static qboolean DR_RowVar( char *b, int id );
static qboolean DR_RowCam( char *b, int id );
static qboolean DR_RowTran( char *b, int id );
static qboolean DR_RowAct( char *b, int id );
static qboolean DR_RowLink( char *b, int id );
static void DR_ActHeader( char *b, int actType );
static void DR_Cat( char *buf, const char *fmt, ... ) Q_PRINTF_FUNC(2,3);
#define DR_ROWBUF 1024

// 0x1816e: the users of record (dtype, id): kind DR_TRAN = the transitions that use it as an endpoint, kind DR_LINK = the links that name it.
// Appends `( n n n )` to the line (DeFRaG's "first entry" flag is never set, so every entry is written with a leading space).
static void DR_Chain( char *b, int kind, int dtype, int id ) {
	int i;

	DR_Cat( b, "(" );
	if ( kind == DR_TRAN ) {
		for ( i = 0; i < DR_MAX_TRAN; i++ ) {
			if ( drTrans[i].dtype == dtype && ( drTrans[i].id1 == id || drTrans[i].id2 == id ) ) {
				DR_Cat( b, " %i", i );
			}
		}
	} else if ( kind == DR_LINK ) {
		for ( i = 0; i < DR_MAX_LINK; i++ ) {
			if ( ( drLinks[i].dtype1 == dtype && drLinks[i].id1 == id ) || ( drLinks[i].dtype2 == dtype && drLinks[i].id2 == id ) ) {
				DR_Cat( b, " %i", i );
			}
		}
	}
	DR_Cat( b, " )" );
}

// the column headers of the table formats (0x16d3d / 0x16e45 / 0x17069 / 0x175bc; ACT 0x17357 once per ActType)
static void DR_PrintHeaders( int type ) {
	char	b[DR_ROWBUF];
	int		i;

	switch ( type ) {
	case DR_VAR:	Com_sprintf( b, sizeof( b ), "%s  ID VarName                              Value", drTypeNames[DR_VAR] ); break;
	case DR_CAM:	Com_sprintf( b, sizeof( b ), "%s  ID      X      Y      Z    P    Y    R", drTypeNames[DR_CAM] ); break;
	case DR_TRAN:	Com_sprintf( b, sizeof( b ), "%s  ID TranType DType  ID  ID Speed Loop", drTypeNames[DR_TRAN] ); break;
	case DR_LINK:	Com_sprintf( b, sizeof( b ), "%s  ID DType  ID DType  ID", drTypeNames[DR_LINK] ); break;
	default:
		for ( i = 0; i < 6; i++ ) {
			DR_ActHeader( b, i );
			CG_Printf( i == 0 ? "^2%s\n" : "^3%s\n", b );
		}
		return;
	}
	CG_Printf( "^2%s\n", b );
}

// 0x18287: id == -1 = every slot of the type (unused ones skipped), otherwise that slot (printed even when unused).
// format 0 = full record, 1 = CHAIN (the users of the record), 2 = table row (the header first when printing all), 3 = header only
static void DR_Print( int type, int id, int format, qboolean warn ) {
	char		b[DR_ROWBUF];
	qboolean	headerDone = qfalse;
	int			i, n;

	if ( type < DR_VAR || type > DR_LINK ) {
		return;
	}
	n = drCount[type];
	if ( id != -1 && !DR_RangeOk( type, id, warn ) ) {
		return;
	}
	for ( i = ( id == -1 ? 0 : id ); ; ) {
		if ( id != -1 || DR_InUse( type, i ) ) {
			if ( format == 2 ) {
				if ( !headerDone && id == -1 ) {
					DR_PrintHeaders( type );
					headerDone = qtrue;
				}
				switch ( type ) {
				case DR_VAR:	DR_RowVar( b, i ); break;
				case DR_CAM:	DR_RowCam( b, i ); break;
				case DR_TRAN:	DR_RowTran( b, i ); break;
				case DR_ACT:	DR_RowAct( b, i ); break;
				default:		DR_RowLink( b, i ); break;
				}
				CG_Printf( "%s\n", b );
			} else if ( format == 3 ) {
				DR_PrintHeaders( type );
				if ( type == DR_TRAN ) {		// 0x17077: the list of the transition types
					int k;

					b[0] = 0;
					for ( k = 1; k < 11; k++ ) {
						DR_Cat( b, k == 1 ? "%s" : ", %s", drTranNames[k] );
					}
					CG_Printf( "^3%s\n", b );
				}
				i = n;		// one header is enough
			} else if ( format == 0 ) {
				DR_PrintRecord( type, i, 0 );
			} else if ( format == 1 ) {
				CG_Printf( "^2%s^7 ^3[ %i ]^7 ^4-->^7 ^2CHAIN^7\n", drTypeNames[type], i );
				if ( type == DR_VAR || type == DR_CAM ) {
					Com_sprintf( b, sizeof( b ), " ^4-->^7 %s ", drTypeNames[DR_TRAN] );
					DR_Chain( b, DR_TRAN, type, i );
					CG_Printf( "%s\n", b );
				}
				if ( type != DR_LINK ) {
					Com_sprintf( b, sizeof( b ), " ^4-->^7 %s ", drTypeNames[DR_LINK] );
					DR_Chain( b, DR_LINK, type, i );
					CG_Printf( "%s\n", b );
				}
			}
		}
		if ( id != -1 || ++i >= n ) {
			break;
		}
	}
}

// ---- 0x16b76 / 0x16b0b : clear ----
static void DR_ClearAll( qboolean warn ) {
	int i;

	Com_Memset( drVars, 0, sizeof( drVars ) );
	Com_Memset( drCams, 0, sizeof( drCams ) );
	Com_Memset( drTrans, 0, sizeof( drTrans ) );
	Com_Memset( drActs, 0, sizeof( drActs ) );
	Com_Memset( drLinks, 0, sizeof( drLinks ) );
	if ( warn ) {
		CG_Printf( "^2REPLAY:^7 Cleared ^4-->^7 ( ALL REPLAY MEMORY )\n" );
		for ( i = DR_VAR; i <= DR_LINK; i++ ) {
			CG_Printf( "^2REPLAY:^7 Cleared All ^4-->^7 ( ^2%4s^7 Count = %i )\n", drTypeNames[i], drCount[i] );
		}
	}
}

static void DR_Clear( int type, int id, qboolean warn ) {
	if ( id == -1 ) {
		switch ( type ) {
		case DR_VAR:	Com_Memset( drVars, 0, sizeof( drVars ) ); break;
		case DR_CAM:	Com_Memset( drCams, 0, sizeof( drCams ) ); break;
		case DR_TRAN:	Com_Memset( drTrans, 0, sizeof( drTrans ) ); break;
		case DR_ACT:	Com_Memset( drActs, 0, sizeof( drActs ) ); break;
		case DR_LINK:	Com_Memset( drLinks, 0, sizeof( drLinks ) ); break;
		default:		DR_ClearAll( warn ); return;
		}
		if ( warn ) {
			CG_Printf( "^2REPLAY:^7 Cleared All ^4-->^7 ( ^2%s^7 Count = %i )\n", drTypeNames[type], drCount[type] );
		}
		return;
	}
	if ( !DR_RangeOk( type, id, qtrue ) ) {
		return;
	}
	switch ( type ) {
	case DR_VAR:	Com_Memset( &drVars[id], 0, sizeof( drVars[id] ) ); break;
	case DR_CAM:	Com_Memset( &drCams[id], 0, sizeof( drCams[id] ) ); break;
	case DR_TRAN:	Com_Memset( &drTrans[id], 0, sizeof( drTrans[id] ) ); break;
	case DR_ACT:	Com_Memset( &drActs[id], 0, sizeof( drActs[id] ) ); break;
	case DR_LINK:	Com_Memset( &drLinks[id], 0, sizeof( drLinks[id] ) ); break;
	}
}

// ---- SET parsers (0x16c75 VAR, 0x16d4b CAM, 0x16e53 TRAN, 0x170c6 ACT, 0x173e2 LINK); return the id or -1 ----
static int DR_SetVar( void ) {
	int		id = -1;
	char	cv[MAX_STRING_CHARS];
	float	f;

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_VAR, id, qtrue ) ) {
		return -1;
	}
	{
		char tok[MAX_STRING_CHARS];

		Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
		if ( CG_DF_ExpandVar( tok, sizeof( tok ) ) ) {
			Q_strncpyz( drVars[id].name, tok, sizeof( drVars[id].name ) );
		}
	}
	DR_ParseFloat( &drVars[id].value );
	trap_Cvar_VariableStringBuffer( drVars[id].name, cv, sizeof( cv ) );
	f = atof( cv );
	drVars[id].original = f;		// +72
	drVars[id].current = f;			// +68
	return id;
}

static int DR_SetCam( void ) {
	int id = -1;

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_CAM, id, qtrue ) ) {
		return -1;
	}
	DR_ParseVec3( drCams[id].origin );
	DR_ParseVec3( drCams[id].angles );
	return id;
}

static int DR_SetTran( void ) {
	int			id = -1, v, ref;
	const char	*tok;
	drTran_t	*t;

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_TRAN, id, qtrue ) ) {
		return -1;
	}
	t = &drTrans[id];
	tok = DR_Tok();
	v = DR_NameOrKeep( drTranNames, 11, tok, t->type );
	if ( v == -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid TranType ^4-->^7 ( %s )\n", tok );
		return -1;
	}
	t->type = v;
	tok = DR_Tok();
	v = DR_NameOrKeep( drTypeNames, DR_NUM_TYPES, tok, t->dtype );
	if ( v == -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %s )\n", tok );
		return -1;
	}
	t->dtype = v;
	ref = t->id1;
	DR_ParseInt( &ref );
	if ( !DR_RangeOk( t->dtype, ref, qtrue ) ) {
		return -1;
	}
	t->id1 = ref;
	ref = t->id2;
	DR_ParseInt( &ref );
	if ( !DR_RangeOk( t->dtype, ref, qtrue ) ) {
		return -1;
	}
	t->id2 = ref;
	DR_ParseFloat( &t->speed );
	DR_ParseFloat( &t->loop );
	t->curLoop = t->loop;		// 0x16fa1: +24 = +20
	return id;
}

static int DR_SetAct( void ) {
	int			id = -1, v;
	const char	*tok;
	drAct_t		*a;

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_ACT, id, qtrue ) ) {
		return -1;
	}
	a = &drActs[id];
	tok = DR_Tok();
	v = DR_NameOrKeep( drActNames, 6, tok, a->type );
	if ( v == -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid ActType ^4-->^7 ( %s )\n", tok );
		return -1;
	}
	a->type = v;
	switch ( a->type ) {
	case 1:		// MANUAL
	case 2:		// TIMER
	case 3:		// TRANEND
		DR_ParseFloat( &a->value );
		break;
	case 4:		// RADIUS
		DR_ParseVec3( a->pos1 );
		DR_ParseFloat( &a->value );
		break;
	case 5:		// VOLUME
		DR_ParseVec3( a->pos1 );
		DR_ParseVec3( a->pos2 );
		break;
	}
	return id;
}

static int DR_SetLink( void ) {
	int			id = -1, v, ref;
	const char	*tok;
	drLink_t	*l;

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_LINK, id, qtrue ) ) {
		return -1;
	}
	l = &drLinks[id];
	tok = DR_Tok();
	v = DR_NameOrKeep( drTypeNames, DR_NUM_TYPES, tok, l->dtype1 );
	if ( v == -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %s )\n", tok );
		return -1;
	}
	if ( v != DR_ACT ) {
		CG_Printf( "^1REPLAY ERROR:^7 First DataType must be %s ^4-->^7 ( %s )\n", drTypeNames[DR_ACT], tok );
		return -1;
	}
	l->dtype1 = v;
	ref = l->id1;
	DR_ParseInt( &ref );
	if ( !DR_RangeOk( l->dtype1, ref, qtrue ) ) {
		return -1;
	}
	l->id1 = ref;
	tok = DR_Tok();
	v = DR_NameOrKeep( drTypeNames, DR_NUM_TYPES, tok, l->dtype2 );
	if ( v == -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %s )\n", tok );
		return -1;
	}
	l->dtype2 = v;
	ref = l->id2;
	DR_ParseInt( &ref );
	if ( !DR_RangeOk( l->dtype2, ref, qtrue ) ) {
		return -1;
	}
	l->id2 = ref;
	return id;
}

// ---- 0x16816 / 0x16893 / 0x168db / 0x16934 : VALIDATE ----
static qboolean DR_Referenced( int type, int id ) {
	int i;

	for ( i = 0; i < DR_MAX_TRAN; i++ ) {
		if ( drTrans[i].dtype == type && ( drTrans[i].id1 == id || drTrans[i].id2 == id ) ) {
			return qtrue;
		}
	}
	for ( i = 0; i < DR_MAX_LINK; i++ ) {
		if ( ( drLinks[i].dtype1 == type && drLinks[i].id1 == id ) || ( drLinks[i].dtype2 == type && drLinks[i].id2 == id ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean DR_AllReferenced( int type, qboolean warn ) {
	qboolean	ok = qtrue;
	int			i;

	for ( i = 0; i < drCount[type]; i++ ) {
		if ( DR_InUse( type, i ) && !DR_Referenced( type, i ) ) {
			ok = qfalse;
			if ( warn ) {
				CG_Printf( "^3REPLAY WARNING:^7 ^2%s^7 ^3[ %i ]^7 Unreferenced\n", drTypeNames[type], i );
			}
		}
	}
	return ok;
}

static qboolean DR_Validate( qboolean warn ) {
	qboolean	refs = qtrue, idsOk = qtrue;
	int			i, t;

	if ( warn ) {
		CG_Printf( "^2REPLAY:^7 Checking for Unreferenced Data Types...\n" );
	}
	for ( t = DR_VAR; t <= DR_ACT; t++ ) {
		if ( !DR_AllReferenced( t, warn ) ) {
			refs = qfalse;
		}
	}
	if ( warn && refs ) {
		CG_Printf( "^2REPLAY:^7 DataType References ^4-->^7 ^2OK\n" );
	}
	if ( warn ) {
		CG_Printf( "^2REPLAY:^7 Checking For Invalid DataID References...\n" );
	}
	for ( i = 0; i < DR_MAX_TRAN; i++ ) {
		if ( !DR_InUse( DR_TRAN, i ) ) {
			continue;
		}
		if ( !DR_InUse( drTrans[i].dtype, drTrans[i].id1 ) ) {
			idsOk = qfalse;
			if ( warn ) {
				CG_Printf( "^3REPLAY WARNING:^7 ^2%s^7 ^3[ %i ]^7 Invalid Reference ^2%s^7 ^3[ %i ]^7 (StartID)\n", drTypeNames[DR_TRAN], i, drTypeNames[drTrans[i].dtype], drTrans[i].id1 );
			}
		}
		if ( !DR_InUse( drTrans[i].dtype, drTrans[i].id2 ) ) {
			idsOk = qfalse;
			if ( warn ) {
				CG_Printf( "^3REPLAY WARNING:^7 ^2%s^7 ^3[ %i ]^7 Invalid Reference ^2%s^7 ^3[ %i ]^7 (StopID)\n", drTypeNames[DR_TRAN], i, drTypeNames[drTrans[i].dtype], drTrans[i].id2 );
			}
		}
	}
	for ( i = 0; i < DR_MAX_LINK; i++ ) {
		if ( !DR_InUse( DR_LINK, i ) ) {
			continue;
		}
		if ( !DR_InUse( drLinks[i].dtype1, drLinks[i].id1 ) ) {
			idsOk = qfalse;
			if ( warn ) {
				CG_Printf( "^3REPLAY WARNING:^7 ^2%s^7 ^3[ %i ]^7 Invalid Reference ^2%s^7 ^3[ %i ]^7 (DataID1)\n", drTypeNames[DR_LINK], i, drTypeNames[drLinks[i].dtype1], drLinks[i].id1 );
			}
		}
		if ( !DR_InUse( drLinks[i].dtype2, drLinks[i].id2 ) ) {
			idsOk = qfalse;
			if ( warn ) {
				CG_Printf( "^3REPLAY WARNING:^7 ^2%s^7 ^3[ %i ]^7 Invalid Reference ^2%s^7 ^3[ %i ]^7 (DataID2)\n", drTypeNames[DR_LINK], i, drTypeNames[drLinks[i].dtype2], drLinks[i].id2 );
			}
		}
	}
	if ( warn && idsOk ) {
		CG_Printf( "^2REPLAY:^7 DataID References ^4-->^7 ^2OK\n" );
	}
	if ( refs && idsOk ) {
		if ( warn ) {
			CG_Printf( "^2REPLAY: Script Is Valid\n" );
		}
		return qtrue;
	}
	if ( warn ) {
		CG_Printf( "^3REPLAY WARNING: Script Contains Errors\n" );
	}
	return qfalse;
}

// ---- SAVE (0x17ec1 -> writer 0x176e7): system/replayscripts/<name>.cfg ----
// table rows / headers as written by 0x16ce4 .. 0x1752a (same text the GET table formats 2 / 3 use)
static void DR_Cat( char *buf, const char *fmt, ... ) Q_PRINTF_FUNC(2,3);
static void DR_Cat( char *buf, const char *fmt, ... ) {
	va_list	argptr;
	char	text[1024];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Q_strcat( buf, 1024, text );
}

static qboolean DR_RowVar( char *b, int id ) {		// 0x16ce4
	Com_sprintf( b, DR_ROWBUF, "%s ", drTypeNames[DR_VAR] );
	DR_Cat( b, "%3i ", id );
	DR_Cat( b, "%30s ", drVars[id].name );
	DR_Cat( b, "%7.3f", drVars[id].value );
	return DR_InUse( DR_VAR, id );
}

static qboolean DR_RowCam( char *b, int id ) {		// 0x16d89
	Com_sprintf( b, DR_ROWBUF, "%s ", drTypeNames[DR_CAM] );
	DR_Cat( b, "%3i ", id );
	DR_Cat( b, "%6i ", (int)drCams[id].origin[0] );
	DR_Cat( b, "%6i ", (int)drCams[id].origin[1] );
	DR_Cat( b, "%6i ", (int)drCams[id].origin[2] );
	DR_Cat( b, "%4i ", (int)drCams[id].angles[0] );
	DR_Cat( b, "%4i ", (int)drCams[id].angles[1] );
	DR_Cat( b, "%4i", (int)drCams[id].angles[2] );
	return DR_InUse( DR_CAM, id );
}

static qboolean DR_RowTran( char *b, int id ) {		// 0x16fa7
	drTran_t *t = &drTrans[id];

	Com_sprintf( b, DR_ROWBUF, "%s ", drTypeNames[DR_TRAN] );
	DR_Cat( b, "%3i ", id );
	DR_Cat( b, "%8s ", drTranNames[t->type] );
	DR_Cat( b, "%5s ", drTypeNames[t->dtype] );
	DR_Cat( b, "%3i ", t->id1 );
	DR_Cat( b, "%3i ", t->id2 );
	DR_Cat( b, "%5i ", (int)t->speed );
	DR_Cat( b, "%4i", (int)t->loop );
	return DR_InUse( DR_TRAN, id );
}

static qboolean DR_RowAct( char *b, int id ) {		// 0x171b6
	drAct_t *a = &drActs[id];

	Com_sprintf( b, DR_ROWBUF, "%s ", drTypeNames[DR_ACT] );
	DR_Cat( b, "%3i ", id );
	DR_Cat( b, "%7s ", drActNames[a->type] );
	switch ( a->type ) {
	case 1: case 2: case 3:
		DR_Cat( b, "%6i", (int)a->value );
		break;
	case 4:
		DR_Cat( b, "%6i ", (int)a->pos1[0] );
		DR_Cat( b, "%6i ", (int)a->pos1[1] );
		DR_Cat( b, "%6i ", (int)a->pos1[2] );
		DR_Cat( b, "%6i", (int)a->value );
		break;
	case 5:
		DR_Cat( b, "%6i ", (int)a->pos1[0] );
		DR_Cat( b, "%6i ", (int)a->pos1[1] );
		DR_Cat( b, "%6i ", (int)a->pos1[2] );
		DR_Cat( b, "%6i ", (int)a->pos2[0] );
		DR_Cat( b, "%6i ", (int)a->pos2[1] );
		DR_Cat( b, "%6i", (int)a->pos2[2] );
		break;
	}
	return DR_InUse( DR_ACT, id );
}

static qboolean DR_RowLink( char *b, int id ) {		// 0x1752a
	drLink_t *l = &drLinks[id];

	Com_sprintf( b, DR_ROWBUF, "%s ", drTypeNames[DR_LINK] );
	DR_Cat( b, "%3i ", id );
	DR_Cat( b, "%5s ", drTypeNames[l->dtype1] );
	DR_Cat( b, "%3i ", l->id1 );
	DR_Cat( b, "%5s ", drTypeNames[l->dtype2] );
	DR_Cat( b, "%3i", l->id2 );
	return DR_InUse( DR_LINK, id );
}

static void DR_ActHeader( char *b, int actType ) {		// 0x17357
	static const char *fmt[6] = {
		"%s  ID ActType  <...>", "%s  ID %7s     On", "%s  ID %7s   Time", "%s  ID %7s     ID",
		"%s  ID %7s      X      Y      Z Radius", "%s  ID %7s     X1     Y1     Z1     X2     Y2     Z2"
	};

	if ( actType < 0 || actType > 5 ) {
		actType = 0;
	}
	if ( actType == 0 ) {
		Com_sprintf( b, DR_ROWBUF, fmt[0], drTypeNames[DR_ACT] );
	} else {
		Com_sprintf( b, DR_ROWBUF, fmt[actType], drTypeNames[DR_ACT], drActNames[actType] );
	}
}

static void DR_W( fileHandle_t f, const char *s ) {
	trap_FS_Write( s, strlen( s ), f );
}

static void DR_WLine( fileHandle_t f, const char *fmt, ... ) Q_PRINTF_FUNC(2,3);
static void DR_WLine( fileHandle_t f, const char *fmt, ... ) {
	va_list	argptr;
	char	text[1024];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	DR_W( f, text );
}

typedef qboolean (*drRowFn_t)( char *b, int id );

static void DR_WRows( fileHandle_t f, drRowFn_t fn, int n ) {
	char	b[DR_ROWBUF];
	int		i;

	for ( i = 0; i < n; i++ ) {
		if ( fn( b, i ) ) {
			DR_W( f, "   " );
			DR_WLine( f, "%s\r\n", b );
		}
	}
}

#define DR_BAR "///////////////////////////////////////////////////////////////////////\r\n"

static qboolean DR_WriteFile( const char *path ) {		// 0x176e7
	fileHandle_t	f;
	char			b[DR_ROWBUF], nm[MAX_QPATH];
	qtime_t			qt;
	int				i;

	trap_FS_FOpenFile( path, &f, FS_WRITE );
	if ( !f ) {
		CG_Printf( "^1REPLAY ERROR:^7 Could not create file ^4-->^7 ( %s )\n", path );
		return qfalse;
	}
	DR_W( f, DR_BAR );
	DR_W( f, "// DO NOT EXECUTE IN QUAKE3 !!!\r\n" );
	DR_W( f, "// This is a Defrag Recam/Replay script file.\r\n" );
	DR_WLine( f, "// The file must be placed in the \"%s\" folder.\r\n", "system/replayscripts" );
	DR_W( f, "// The filename should be the mapname with .cfg extension.\r\n" );
	DR_W( f, DR_BAR );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	Q_strncpyz( nm, cgs.clientinfo[cg.clientNum].name, sizeof( nm ) );
	Q_CleanStr( nm );
	DR_WLine( f, "//     SCRIPT BY: %s\r\n", nm );
	trap_Cvar_VariableStringBuffer( "mapname", b, 128 );
	DR_WLine( f, "//           MAP: %s\r\n", b );
	trap_RealTime( &qt );
	DR_WLine( f, "//          DATE: %04i-%02i-%02i - %2i:%02i:%02i\r\n", qt.tm_year + 1900, qt.tm_mon + 1, qt.tm_mday, qt.tm_hour, qt.tm_min, qt.tm_sec );
	DR_W( f, DR_BAR );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	// VAR
	DR_WLine( f, "// The maximum number of variables (%s) is: %i\r\n", drTypeNames[DR_VAR], DR_MAX_VAR );
	DR_WLine( f, "// To use the users default value for a variable, set the value for the %s to %i\r\n", drTypeNames[DR_VAR], -9999 );
	DR_W( f, "// USE: df_demos_ReplayCameraMode for cameramode changes.\r\n" );
	DR_W( f, "// USE: df_demos_ReplayVar_FOV for Field-of-View (FOV) changes.\r\n" );
	DR_W( f, "// " );
	DR_WLine( f, "%s\r\n\r\n", va( "%s  ID VarName                              Value", drTypeNames[DR_VAR] ) );
	DR_WRows( f, DR_RowVar, DR_MAX_VAR );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	// CAM
	DR_WLine( f, "// The maximum number of cameras (%s) is: %i\r\n", drTypeNames[DR_CAM], DR_MAX_CAM );
	DR_W( f, "// " );
	DR_WLine( f, "%s\r\n\r\n", va( "%s  ID      X      Y      Z    P    Y    R", drTypeNames[DR_CAM] ) );
	DR_WRows( f, DR_RowCam, DR_MAX_CAM );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	// TRAN
	DR_WLine( f, "// The maximum number of transitions (%s) is: %i\r\n", drTypeNames[DR_TRAN], DR_MAX_TRAN );
	DR_W( f, "// TranTypes: " );
	b[0] = 0;
	for ( i = 1; i < 11; i++ ) {
		DR_Cat( b, i == 1 ? "%s" : ", %s", drTranNames[i] );
	}
	DR_WLine( f, "%s\r\n", b );
	DR_W( f, "// " );
	DR_WLine( f, "%s\r\n\r\n", va( "%s  ID TranType DType  ID  ID Speed Loop", drTypeNames[DR_TRAN] ) );
	DR_WRows( f, DR_RowTran, DR_MAX_TRAN );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	// ACT
	DR_WLine( f, "// The maximum number of activators (%s) is: %i\r\n", drTypeNames[DR_ACT], DR_MAX_ACT );
	for ( i = 0; i < 6; i++ ) {
		DR_W( f, "// " );
		DR_ActHeader( b, i );
		DR_WLine( f, "%s\r\n", b );
	}
	DR_W( f, "\r\n" );
	DR_WRows( f, DR_RowAct, DR_MAX_ACT );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	// LINK
	DR_WLine( f, "// The maximum number of links (%s) is: %i\r\n", drTypeNames[DR_LINK], DR_MAX_LINK );
	DR_W( f, "// " );
	DR_WLine( f, "%s\r\n\r\n", va( "%s  ID DType  ID DType  ID", drTypeNames[DR_LINK] ) );
	DR_WRows( f, DR_RowLink, DR_MAX_LINK );
	DR_W( f, "\r\n" );
	DR_W( f, DR_BAR );
	DR_W( f, "// End Of File\r\n" );
	DR_W( f, DR_BAR );
	DR_W( f, "\r\n" );
	trap_FS_FCloseFile( f );
	CG_Printf( "^2REPLAY:^7 Wrote File ^4-->^7 ( ^2%s^7 )\n", path );
	return qtrue;
}

static void DR_CmdSave( void ) {			// 0x17ec1
	char tok[MAX_STRING_CHARS], path[MAX_QPATH + 32];

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	if ( !CG_DF_ExpandVar( tok, sizeof( tok ) ) || !tok[0] ) {
		CG_Printf( "^1REPLAY ERROR:^7 Missing Filename\n" );
		return;
	}
	Com_sprintf( path, sizeof( path ), "system/replayscripts/%s.cfg", tok );
	DR_WriteFile( path );
}

// ---- command handlers ----
static void DR_Help( void ) {
	CG_Printf( "^1replay:  ^3Controls the replay edit and debugging system.\n" );
	CG_Printf( "^5Usage:   ^2replay command <...>\n" );
	CG_Printf( "^5Example: ^2see replay documentation for details.\n" );
}

// 0x175ca: tok = DataType name, the rest of the record follows; returns qfalse on any error. echo >= 0: print the record (GET format) afterwards
static qboolean DR_SetByName( const char *tok, int echo ) {
	int type, id;

	type = DR_Lookup( drTypeNames, DR_NUM_TYPES, tok );
	switch ( type ) {
	case DR_VAR:	id = DR_SetVar(); break;
	case DR_CAM:	id = DR_SetCam(); break;
	case DR_TRAN:	id = DR_SetTran(); break;
	case DR_ACT:	id = DR_SetAct(); break;
	case DR_LINK:	id = DR_SetLink(); break;
	default:
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %s )\n", tok );
		return qfalse;
	}
	if ( id == -1 ) {
		return qfalse;
	}
	if ( echo >= 0 && DR_Verbose() ) {
		DR_Print( type, id, echo, qtrue );		// 0x18287(type, id, flags, 1) after a SET
	}
	return qtrue;
}

static void DR_CmdSet( void ) {
	char tok[MAX_STRING_CHARS];

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	DR_SetByName( tok, 0 );
}

// ---- LOAD (0x17e8c -> 0x1a237 -> 0x1901c -> 0x17659): system/replayscripts/<name>.cfg is a list of SET records (type name + fields),
// parsed with the normal Com_Parse tokenizer (so // comments work). Per-record table-row echo (GET format 2, ACT rows) is not ported.
static char		drFileName[128];
static int		drLoaded;

// the 14 replay cvars: reset by 0x190e1 and the only VAR names the partial cleanup 0x1a3ec restores (table at data 9412)
static const char *drResetCvars[14] = {
	"df_demos_ReplayCameraMode", "df_demos_ReplayTransitionSpeed",
	"df_demos_CamOffsetPositionX", "df_demos_CamOffsetPositionY", "df_demos_CamOffsetPositionZ",
	"df_demos_CamOffsetAngleX", "df_demos_CamOffsetAngleY", "df_demos_CamOffsetAngleZ",
	"df_demos_CamOffsetRadiusF", "df_demos_CamOffsetRadiusR", "df_demos_CamOffsetRadiusU",
	"df_demos_TargetPositionX", "df_demos_TargetPositionY", "df_demos_TargetPositionZ"
};

static void DR_ResetCvars( void ) {			// 0x190e1
	char fov[16];
	int i;

	for ( i = 0; i < 14; i++ ) {
		trap_Cvar_Set( drResetCvars[i], "0" );
	}
	trap_Cvar_VariableStringBuffer( "cg_fov", fov, sizeof( fov ) );
	trap_Cvar_Set( "df_demos_ReplayVar_FOV", fov );
}

static qboolean DR_LoadPath( const char *path ) {		// 0x17659
	static char	buf[32000];
	fileHandle_t f;
	int			len;
	qboolean	ok = qtrue;

	DR_ClearAll( qfalse );
	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( !f ) {
		if ( DR_Verbose() ) {
			CG_Printf( "^3REPLAY WARNING:^7 No File Found ^4-->^7 ( ^3%s^7 )\n", path );
		}
		return qfalse;
	}
	if ( len >= (int)sizeof( buf ) ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	if ( DR_Verbose() ) {
		CG_Printf( "^2REPLAY:^7 Parsing File ^4-->^7 ( ^2%s^7 )...\n", path );
	}
	drText = buf;
	for ( ;; ) {
		char tok[MAX_STRING_CHARS];

		Q_strncpyz( tok, COM_Parse( &drText ), sizeof( tok ) );
		if ( !tok[0] ) {
			break;
		}
		if ( !DR_SetByName( tok, -1 ) ) {
			ok = qfalse;
			break;
		}
	}
	drText = NULL;
	if ( !ok ) {
		CG_Printf( "^1REPLAY ERROR:^7 While Parsing File ^4-->^7 ( ^1%s^7 )\n", path );
		return qfalse;
	}
	if ( DR_Verbose() ) {
		CG_Printf( "^2REPLAY:^7 Success Parsing File ^4-->^7 ( ^2%s^7 )\n", path );
	}
	CG_Printf( "^2REPLAY:^7 Loaded File ^4-->^7 ( ^2%s^7 )\n", path );
	return qtrue;
}

static void DR_MapName( char *out, int size ) {		// 0x18ff3: default script name = current map
	Q_strncpyz( out, Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ), size );
}

static qboolean DR_LoadByName( void ) {		// 0x1901c
	char path[MAX_QPATH + 32], tmp[128];

	if ( !drFileName[0] ) {
		Q_strncpyz( tmp, df_demos_ReplayFilename.string, sizeof( tmp ) );		// 0x18fcb: the cvar (default "$demoname")
		if ( tmp[0] && CG_DF_ExpandVar( tmp, sizeof( tmp ) ) ) {
			Q_strncpyz( drFileName, tmp, sizeof( drFileName ) );
		} else {
			DR_MapName( drFileName, sizeof( drFileName ) );
		}
	}
	if ( drFileName[0] ) {
		Com_sprintf( path, sizeof( path ), "system/replayscripts/%s.cfg", drFileName );
		if ( DR_LoadPath( path ) ) {
			return qtrue;
		}
	}
	Q_strncpyz( tmp, "$demoname", sizeof( tmp ) );
	CG_DF_ExpandVar( tmp, sizeof( tmp ) );
	if ( cg.demoPlayback && !Q_stricmp( tmp, drFileName ) ) {
		return qfalse;
	}
	DR_MapName( drFileName, sizeof( drFileName ) );
	Com_sprintf( path, sizeof( path ), "system/replayscripts/%s.cfg", drFileName );
	return DR_LoadPath( path );
}

static qboolean DR_EnsureLoaded( void ) {		// 0x1a237
	if ( !drLoaded ) {
		DR_ResetCvars();
		drLoaded = 1;
		if ( !DR_LoadByName() ) {
			drEnabled = 0;
			return qfalse;
		}
		DR_Validate( DR_Verbose() );		// the result is discarded: 0x16afb always returns 1
		drEnabled = 1;
	}
	return drEnabled != 0;
}

static void DR_CmdLoad( void ) {			// 0x17e8c
	const char *tok = DR_Tok();

	drFileName[0] = 0;
	if ( tok[0] ) {
		Q_strncpyz( drFileName, tok, sizeof( drFileName ) );
	}
	drLoaded = 0;
	drEnabled = 0;
	DR_EnsureLoaded();
}

static void DR_CmdGet( void ) {			// 0x17d95
	char	tok[MAX_STRING_CHARS];
	int		type, id = -1, format = 0, t;

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	type = Q_stricmp( tok, "ALL" ) ? DR_Lookup( drTypeNames, DR_NUM_TYPES, tok ) : -1;
	DR_ParseInt( &id );
	DR_ParseInt( &format );
	if ( type == -1 ) {
		for ( t = DR_VAR; t <= DR_LINK; t++ ) {
			DR_Print( t, id, format, qtrue );
		}
	} else {
		DR_Print( type, id, format, qtrue );
	}
}

static void DR_CmdClear( void ) {		// 0x17e0f (CLEAR / DELETE)
	char	tok[MAX_STRING_CHARS];
	int		type, id = -1;

	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	type = DR_Lookup( drTypeNames, DR_NUM_TYPES, tok );
	if ( type == -1 && Q_stricmp( tok, "ALL" ) ) {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Datatype ^4-->^7 ( %s )\n", tok );
		return;
	}
	DR_ParseInt( &id );
	if ( !tok[0] && type != -1 ) {
		CG_Printf( "^1REPLAY ERROR:^7 Missing ID\n" );
		return;
	}
	DR_Clear( type, id, qtrue );
	DR_Print( type, id, 0, qfalse );
}

static void DR_CmdActivate( void ) {		// 0x17f21: only MANUAL activators react; the value defaults to 1.0
	int		id = -1;
	float	v;
	char	tok[MAX_STRING_CHARS];

	DR_ParseInt( &id );
	if ( !DR_RangeOk( DR_ACT, id, qtrue ) || !DR_InUse( DR_ACT, id ) ) {
		return;
	}
	if ( drActs[id].type != 1 ) {
		return;
	}
	v = drActs[id].value;
	Q_strncpyz( tok, DR_Tok(), sizeof( tok ) );
	if ( CG_DF_ExpandVar( tok, sizeof( tok ) ) ) {
		v = atof( tok );
	}
	if ( !tok[0] ) {
		v = 1.0f;
	}
	drActs[id].value = v;
}

// ---- playback runtime (cgame 0x1a4ed driver -> 0x1a4ac frame; see disasm/CGAME/replay_system.md "Iter 73") ----
// Per frame (ReplayMode 1 / 2 / 3, script loaded): activators 0x197e6 -> active transitions 0x1a033 -> camera 0x1a119.
// The edit overlay 0x18873 (modes 3 / 4) is DR_Overlay.  Not ported: some verbose debug prints.
static vec3_t	drCamOrigin, drCamAngles;	// 2823440 / 2823452: camera output of the transitions
static vec3_t	drTarget;					// 2823468: tracked position (the viewed player's origin)
static int		drTime;						// 2823492
static qboolean	drFreeCam;					// 2823504: the AUTOCHASE camera is active
static int		drLastTime;					// 2791292

static float DR_CvarF( const char *name ) {
	char buf[MAX_CVAR_VALUE_STRING];

	trap_Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atof( buf );
}

static float DR_Clamp( float x, float lo, float hi ) {		// 0x8285 (a NaN gives lo)
	if ( !( x >= lo ) ) {
		return lo;
	}
	if ( !( x <= hi ) ) {
		return hi;
	}
	return x;
}

static float DR_VarValue( int id ) {		// 0x19179
	float v;

	if ( id < 0 || id >= DR_MAX_VAR ) {
		return 0;
	}
	v = drVars[id].value;
	if ( v == -9999.0f ) {
		v = drVars[id].original;
	}
	return v;
}

static void DR_FormatNum( char *buf, int size, float v ) {		// "%i" when integral, else "%.3f"
	if ( v == (float)(int)v ) {
		Com_sprintf( buf, size, "%i", (int)v );
	} else {
		Com_sprintf( buf, size, "%.3f", v );
	}
}

static void DR_PropagateVar( int id ) {		// 0x19626: other VARs naming the same cvar take this one's Value as their current value
	int i;

	for ( i = 0; i < DR_MAX_VAR; i++ ) {
		if ( i != id && !strcmp( drVars[id].name, drVars[i].name ) ) {
			drVars[i].current = drVars[id].value;
		}
	}
}

static void DR_ApplyVar( int id ) {		// 0x19670: write the VAR's value into its cvar
	char	buf[MAX_STRING_CHARS];
	float	v;

	trap_Cvar_VariableStringBuffer( drVars[id].name, buf, sizeof( buf ) );
	if ( !buf[0] ) {
		return;
	}
	v = DR_VarValue( id );
	DR_FormatNum( buf, sizeof( buf ), v );
	trap_Cvar_Set( drVars[id].name, buf );
	drVars[id].current = v;
	DR_PropagateVar( id );
}

// ---- transition stepping ----
static void DR_StepTime( drTran_t *t, float dt, float speed ) {		// 0x19195 (TranType TIME)
	float	a = DR_VarValue( t->id1 ), b = DR_VarValue( t->id2 );
	float	scale = 0, p;

	if ( speed < 0 ) {
		if ( t->dtype == DR_VAR ) {
			scale = fabs( b - a );
			if ( scale != 0 ) {
				scale = 1000.0f / scale;
			}
		}
		if ( t->dtype == DR_CAM ) {
			vec3_t d;

			VectorSubtract( drCams[t->id2].origin, drCams[t->id1].origin, d );
			scale = VectorLength( d );
			if ( scale != 0 ) {
				scale = 1000.0f / scale;
			}
		}
		p = t->interp[0] + ( ( fabs( speed ) / 100.0f ) * dt ) * scale;
	} else {
		p = t->interp[0] + ( fabs( speed ) / 100.0f ) * dt;
	}
	if ( p >= 1.0f ) {
		if ( t->id1 == t->id2 ) {
			p = 1.0f;
		}
		if ( t->curLoop != 0 ) {
			p = 0;
			if ( t->curLoop > 0 ) {
				t->curLoop -= 1.0f;
			} else {
				int tmp = t->id1;

				t->curLoop += 1.0f;
				t->id1 = t->id2;
				t->id2 = tmp;
			}
		} else {
			t->curLoop = t->loop;
			t->activated = 0;
		}
	}
	t->interp[0] = DR_Clamp( p, 0, 1 );
}

static void DR_StepPos( int k, drTran_t *t, float v ) {		// 0x19317 (TranType POS*: progress of the tracked position between the two cameras on axis k)
	float lo = drCams[t->id1].origin[k], hi = drCams[t->id2].origin[k];
	float r = DR_Clamp( ( drTarget[k] - lo ) / ( hi - lo ), 0, 1 );

	t->interp[k] = DR_Clamp( r + v * 0.001f, 0, 1 );
}

// second axis of a two-letter POSxy type (the table of 0x1938d: that axis takes the record's +20 field instead of Speed)
static const signed char drAltAxis[11] = { -1, -1, -1, -1, -1, 1, 2, 0, 2, 0, 1 };

static void DR_TranStep( drTran_t *t, float dt ) {		// 0x1938d
	float	sp = t->speed, alt = t->loop;
	int		k;

	if ( sp == 0 ) {
		sp = DR_CvarF( "df_demos_ReplayTransitionSpeed" );
	}
	if ( t->type < 1 || t->type > 10 ) {
		return;
	}
	if ( t->type == 1 ) {
		DR_StepTime( t, dt, sp );
		return;
	}
	for ( k = 0; k < 3; k++ ) {
		DR_StepPos( k, t, k == drAltAxis[t->type] ? alt : sp );
	}
}

// interpolant index used for the camera angles, and for the x / y / z origin components, per TranType
static const signed char drAngIdx[11] = { 0, 0, 0, 1, 2, 0, 0, 1, 1, 2, 2 };
static const signed char drPosIdx[11][3] = {
	{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 1 }, { 2, 2, 2 },
	{ 0, 1, 0 }, { 0, 0, 2 }, { 0, 1, 1 }, { 1, 1, 2 }, { 0, 2, 2 }, { 2, 1, 2 }
};

static void DR_TranApply( drTran_t *t, float dt ) {		// 0x199c3
	int k;

	if ( !t->activated ) {
		return;
	}
	DR_TranStep( t, dt );
	if ( !t->activated && t->interp[0] == 0 ) {
		return;
	}
	if ( t->dtype == DR_VAR ) {
		char	buf[MAX_STRING_CHARS];
		float	a, b, v, p = t->interp[0];

		trap_Cvar_VariableStringBuffer( drVars[t->id1].name, buf, sizeof( buf ) );
		if ( !buf[0] ) {
			return;
		}
		a = DR_VarValue( t->id1 );
		b = DR_VarValue( t->id2 );
		v = a + p * ( b - a );
		v = ( a > b ) ? DR_Clamp( v, b, a ) : DR_Clamp( v, a, b );
		if ( !( p > 0 ) ) {
			v = a;
		}
		if ( !( p < 1.0f ) ) {
			v = b;
		}
		if ( !t->activated ) {
			v = b;
		}
		DR_FormatNum( buf, sizeof( buf ), v );
		trap_Cvar_Set( drVars[t->id1].name, buf );
		drVars[t->id1].current = v;
		DR_PropagateVar( t->id1 );
	} else if ( t->dtype == DR_CAM ) {
		const drCam_t *c1 = &drCams[t->id1], *c2 = &drCams[t->id2];

		if ( t->type < 1 || t->type > 10 ) {
			return;
		}
		for ( k = 0; k < 3; k++ ) {
			drCamAngles[k] = LerpAngle( c1->angles[k], c2->angles[k], t->interp[drAngIdx[t->type]] );
			drCamOrigin[k] = c1->origin[k] + t->interp[drPosIdx[t->type][k]] * ( c2->origin[k] - c1->origin[k] );
		}
	}
}

static void DR_StepTrans( void ) {		// 0x1a033
	float	dt;
	int		i;

	dt = ( cg.time - drLastTime ) * 0.001f;		// 0x8e4a(&last, 1)
	drLastTime = cg.time;
	for ( i = 0; i < DR_MAX_TRAN; i++ ) {
		if ( drTrans[i].activated ) {
			DR_TranApply( &drTrans[i], dt );
		}
	}
}

// ---- activators ----
static qboolean DR_ActLatch( int id ) {		// 0x194d7: fires once until the condition drops again
	if ( drActs[id].activated ) {
		return qfalse;
	}
	drActs[id].activated = 1;
	return qtrue;
}

static qboolean DR_PointInBox( const vec3_t p, const vec3_t c1, const vec3_t c2 ) {		// 0x82b6
	int i;

	for ( i = 0; i < 3; i++ ) {
		float lo = c1[i] < c2[i] ? c1[i] : c2[i];
		float hi = c1[i] < c2[i] ? c2[i] : c1[i];

		if ( p[i] > hi || !( p[i] >= lo ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean DR_ActCheck( int id ) {		// 0x194f1
	drAct_t	*a = &drActs[id];
	vec3_t	d;
	int		n;

	switch ( a->type ) {
	case 1:		// MANUAL
		if ( a->value != 0 ) {
			a->value = 0;
			return DR_ActLatch( id );
		}
		break;
	case 2:		// TIMER
		if ( !( drTime < (int)a->value ) ) {
			return DR_ActLatch( id );
		}
		break;
	case 3:		// TRANEND: transition number Value has reached its end
		n = (int)a->value;
		if ( n >= 0 && n < DR_MAX_TRAN && drTrans[n].interp[0] >= 1.0f ) {
			return DR_ActLatch( id );
		}
		break;
	case 4:		// RADIUS
		VectorSubtract( drTarget, a->pos1, d );
		if ( VectorLength( d ) < a->value ) {
			return DR_ActLatch( id );
		}
		break;
	case 5:		// VOLUME
		if ( DR_PointInBox( drTarget, a->pos1, a->pos2 ) ) {
			return DR_ActLatch( id );
		}
		break;
	}
	a->activated = 0;
	return qfalse;
}

static void DR_ActLinks( int act ) {		// 0x196f9: instant effects (VAR write / camera jump) of a fired activator
	int i;

	for ( i = 0; i < DR_MAX_LINK; i++ ) {
		const drLink_t *l = &drLinks[i];

		if ( l->dtype1 != DR_ACT || l->id1 != act || !drActs[act].activated || l->id2 == -1 ) {
			continue;
		}
		if ( l->dtype2 == DR_VAR ) {
			DR_ApplyVar( l->id2 );
		} else if ( l->dtype2 == DR_CAM ) {
			VectorCopy( drCams[l->id2].origin, drCamOrigin );		// 0x196e6
			VectorCopy( drCams[l->id2].angles, drCamAngles );
		}
	}
}

static void DR_Activators( void ) {		// 0x197e6
	qboolean	camFired = qfalse;
	int			keep = -1, a, i;

	for ( a = 0; a < DR_MAX_ACT; a++ ) {
		if ( !DR_ActCheck( a ) ) {
			continue;
		}
		for ( i = 0; i < DR_MAX_LINK; i++ ) {
			const drLink_t *l = &drLinks[i];

			if ( l->dtype1 != DR_ACT || l->id1 != a ) {
				continue;
			}
			if ( l->dtype2 == DR_TRAN ) {
				drTran_t *t = &drTrans[l->id2];

				t->activated = 1;
				t->interp[0] = t->interp[1] = t->interp[2] = 0;
				if ( t->dtype == DR_CAM ) {
					camFired = qtrue;
					keep = l->id2;
				}
			}
			if ( l->dtype2 == DR_CAM ) {
				camFired = qtrue;
				keep = -1;
			}
		}
		DR_ActLinks( a );
	}
	if ( camFired ) {		// a camera move starts: every other running camera transition stops
		for ( i = 0; i < DR_MAX_TRAN; i++ ) {
			if ( drTrans[i].dtype == DR_CAM && i != keep ) {
				drTrans[i].activated = 0;
			}
		}
	}
}

// ---- camera (0x1a119) ----
static qboolean DR_AddCvars3( vec3_t v, const char *n0, const char *n1, const char *n2 ) {		// 0x14b09 / 0x14b8b
	float		d[3];
	qboolean	changed = qfalse;
	int			i;

	d[0] = DR_CvarF( n0 );
	d[1] = DR_CvarF( n1 );
	d[2] = DR_CvarF( n2 );
	for ( i = 0; i < 3; i++ ) {
		v[i] += d[i];
		if ( d[i] != 0 ) {
			changed = qtrue;
		}
	}
	return changed;
}

static qboolean DR_AddRadius( vec3_t origin, const vec3_t angles ) {		// 0x14c0d: offsets along forward / right / up
	static const char *names[3] = { "df_demos_CamOffsetRadiusF", "df_demos_CamOffsetRadiusR", "df_demos_CamOffsetRadiusU" };
	vec3_t		axis[3];
	qboolean	changed = qfalse;
	int			i, j;

	AngleVectors( angles, axis[0], axis[1], axis[2] );
	for ( i = 0; i < 3; i++ ) {
		float s = DR_CvarF( names[i] );

		for ( j = 0; j < 3; j++ ) {
			origin[j] += axis[i][j] * s;
		}
		if ( s != 0 ) {
			changed = qtrue;
		}
	}
	return changed;
}

static void DR_CamAngles( int mode, vec3_t out, vec3_t dir ) {		// 0x1a07d
	vec3_t a;

	if ( mode < 5 || mode > 7 ) {
		return;
	}
	vectoangles( dir, a );
	if ( mode == 6 ) {
		a[0] = out[0];
	} else if ( mode == 7 ) {
		a[1] = out[1];
	}
	VectorCopy( a, out );
}

static void DR_CameraView( void ) {
	int		mode = (int)DR_CvarF( "df_demos_ReplayCameraMode" );		// 0x1a067
	vec3_t	dir, tgt;
	qboolean	posChanged, radChanged;

	if ( mode == 0 ) {
		return;
	}
	if ( mode == 8 ) {
		VectorCopy( cg.refdef.vieworg, drCamOrigin );
		VectorCopy( cg.refdefViewAngles, drCamAngles );
	}
	if ( mode >= 9 && mode <= 11 ) {		// 0x15550: the demo-camera engine with the replay's own camera object
		float sp = DR_CvarF( "df_demos_ReplayTransitionSpeed" );

		CG_DF_ReplayFreeCamera( mode, sp == 0 ? 200.0f : sp );
		return;
	}
	if ( mode == 12 ) {
		tgt[0] = DR_CvarF( "df_demos_TargetPositionX" );
		tgt[1] = DR_CvarF( "df_demos_TargetPositionY" );
		tgt[2] = DR_CvarF( "df_demos_TargetPositionZ" );
	} else {
		VectorCopy( drTarget, tgt );
	}
	VectorSubtract( tgt, drCamOrigin, dir );
	VectorNormalize( dir );
	DR_CamAngles( mode, drCamAngles, dir );
	VectorCopy( drCamOrigin, cg.refdef.vieworg );
	VectorCopy( drCamAngles, cg.refdefViewAngles );
	posChanged = DR_AddCvars3( cg.refdef.vieworg, "df_demos_CamOffsetPositionX", "df_demos_CamOffsetPositionY", "df_demos_CamOffsetPositionZ" );
	DR_AddCvars3( cg.refdefViewAngles, "df_demos_CamOffsetAngleX", "df_demos_CamOffsetAngleY", "df_demos_CamOffsetAngleZ" );
	radChanged = DR_AddRadius( cg.refdef.vieworg, cg.refdefViewAngles );
	if ( posChanged || radChanged ) {		// aim again from the displaced origin
		VectorSubtract( tgt, cg.refdef.vieworg, dir );
		VectorNormalize( dir );
		DR_CamAngles( mode, drCamAngles, dir );
		VectorCopy( drCamAngles, cg.refdefViewAngles );
		DR_AddCvars3( cg.refdefViewAngles, "df_demos_CamOffsetAngleX", "df_demos_CamOffsetAngleY", "df_demos_CamOffsetAngleZ" );
	}
	AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
	if ( df_ghosts_CheckPVS.integer ) {		// 0x1a22a (tests the camera position before the offsets)
		CG_DF_CheckPVS( drCamOrigin );
	}
}

// 0x15246: the df_demos_CamOffset* position / angle / radius offsets applied to a camera (used by the demo camera)
// Returns whether the position or radius offsets moved the camera (the angle offsets do not count, as in 0x15246).
qboolean CG_DF_CamOffsets( vec3_t pos, vec3_t ang ) {
	qboolean moved;

	moved = DR_AddCvars3( pos, "df_demos_CamOffsetPositionX", "df_demos_CamOffsetPositionY", "df_demos_CamOffsetPositionZ" );
	DR_AddCvars3( ang, "df_demos_CamOffsetAngleX", "df_demos_CamOffsetAngleY", "df_demos_CamOffsetAngleZ" );
	if ( DR_AddRadius( pos, ang ) ) {
		moved = qtrue;
	}
	return moved;
}

// 0x14b8b alone: only the angle offsets (the demo camera applies them a second time after re-aiming)
void CG_DF_CamAngleOffsets( vec3_t ang ) {
	DR_AddCvars3( ang, "df_demos_CamOffsetAngleX", "df_demos_CamOffsetAngleY", "df_demos_CamOffsetAngleZ" );
}

// ---- ReplayMode 1: spectator key state machine (0x1a323 / 0x1a2bf) and the cleanup (0x1a3a6 / 0x1a3c3 / 0x1a3ec / 0x194bf) ----
// While following another player (PMF_FOLLOW) the jump key cycles 0 = NORMAL, 1 = REPLAY (the script runs), 2 = AUTOCHASE (replay free camera, mode 9);
// the crouch key sends `team spectator`. The state is reset when the follow ends.
static int		drKey;				// 2823496
static int		drCleanup;			// 2823500: restore the replay cvars when the state leaves REPLAY
static int		drKeyStamp;			// data 9840: time of the last jump / crouch press (debounced by 500 ms)
static qboolean	drVerbose;			// 2823436: ReplayMode >= 2

static qboolean DR_KeyChange( int to, qboolean announce ) {
	qboolean ok;

	if ( drKey == to ) {
		return qfalse;
	}
	drKey = to;
	if ( drKey <= 0 || drKey >= 3 ) {
		drKey = 0;
	}
	ok = drKey == to;
	if ( !announce ) {
		return ok;
	}
	switch ( drKey ) {
	case 0:
		CG_Printf( "^2Spectator Mode: ^3NORMAL\n" );
		break;
	case 1:
		if ( drEnabled ) {
			CG_Printf( "^2Spectator Mode: ^3REPLAY\n" );
		}
		break;
	case 2:
		CG_Printf( "^2Spectator Mode: ^3AUTOCHASE\n" );
		break;
	}
	return ok;
}

static void DR_KeyFrame( int mode ) {
	usercmd_t cmd;

	drCleanup = 0;
	if ( !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		drKey = 0;
		return;
	}
	if ( drKey == 1 && mode != 1 ) {
		drCleanup = 1;
	}
	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	if ( cg.time - drKeyStamp < 500 ) {
		return;
	}
	if ( cmd.upmove ) {
		drKeyStamp = cg.time;
	}
	if ( cmd.upmove < 0 ) {
		trap_SendConsoleCommand( "team spectator\n" );
	}
	if ( mode == 1 ) {
		if ( cmd.upmove > 0 ) {
			if ( drKey == 1 ) {
				drCleanup = 1;
			}
			DR_KeyChange( drKey + 1, qtrue );
		}
		if ( cmd.upmove < 0 && drKey == 1 ) {
			drCleanup = 1;
		}
	}
}

// 0x1a3ec: the cvars of the script's VAR records are set back to their original values (a partial cleanup only restores the 14 replay cvars)
static void DR_RestoreVars( void ) {
	qboolean	changed = qfalse;
	int			i, j;

	if ( drVerbose ) {
		CG_Printf( "^2REPLAY:^7 ^3( %i )^7 ^4-->^7 ^2%s^7 Cleanup\n", drTime, drTypeNames[DR_VAR] );
	}
	for ( i = 0; i < DR_MAX_VAR; i++ ) {
		float v;

		if ( drCleanup ) {
			for ( j = 0; j < 14; j++ ) {
				if ( !Q_stricmp( drResetCvars[j], drVars[i].name ) ) {
					break;
				}
			}
			if ( j == 14 ) {
				continue;
			}
		}
		if ( !DR_InUse( DR_VAR, i ) ) {
			continue;
		}
		v = drVars[i].original;
		trap_Cvar_Set( drVars[i].name, v == (float)(int)v ? va( "%i", (int)v ) : va( "%.3f", v ) );
		changed = qtrue;
		if ( drVerbose ) {
			CG_Printf( "^2%s^7 Restored ^3[ %i ]^7 ( %s = %.3f )\n", drTypeNames[DR_VAR], i, drVars[i].name, drVars[i].original );
		}
	}
	if ( changed && drVerbose ) {
		CG_Printf( "\n" );
	}
}

static void DR_ClearLatches( void ) {		// 0x194bf
	int i;

	for ( i = 0; i < DR_MAX_ACT; i++ ) {
		drActs[i].activated = 0;
	}
}

// 0x1a3c3: restore the cvars and (arg) unload the script
static void DR_Cleanup( qboolean unload ) {
	if ( !drEnabled || drKey == 1 ) {
		return;
	}
	DR_RestoreVars();
	DR_ClearLatches();
	if ( unload ) {
		DR_ClearAll( qfalse );
		drLoaded = 0;
		drEnabled = 0;
	}
}

// 0x1a4a6 (from CG_Shutdown 0x7969): the script's VAR cvars are put back, whatever the ReplayMode (no unload, the module is going away)
void CG_DF_ReplayShutdown( void ) {
	DR_RestoreVars();
}

// 0x165e7: the replay camera owns the view
qboolean CG_DF_ReplayOwnsView( void ) {
	int rm = df_demos_ReplayMode.integer;

	if ( cg.demoPlayback && DR_CvarF( "df_demos_CameraMode" ) != 0 ) {
		return qfalse;
	}
	if ( cg.demoPlayback ) {
		return rm >= 1 && rm <= 3;
	}
	return ( rm == 1 && drKey == 1 ) || ( rm >= 2 && rm <= 3 );
}

// 0x16631 + 0x1666d + 0x1469c, called right after the third-person decision of CG_DrawActiveFrame (0x7972): while the replay camera owns the
// view cg_fov is replaced by `$df_demos_ReplayVar_FOV` (put back afterwards), and the camera modes that look at the player force the third-person view
void CG_DF_ReplayViewPrep( void ) {
	static int fovVar = -99999, fovSaved = -99999;
	int rcm;

	if ( CG_DF_ReplayOwnsView() ) {
		char buf[64];

		Q_strncpyz( buf, "$df_demos_ReplayVar_FOV", sizeof( buf ) );
		if ( CG_DF_ExpandVar( buf, sizeof( buf ) ) ) {
			fovVar = atoi( buf );
		}
		if ( fovVar != -99999 ) {
			if ( fovSaved == -99999 ) {
				fovSaved = cg_fov.integer;
			}
			cg_fov.integer = fovVar;
			cg_fov.value = (float)fovVar;
		}
	} else {
		if ( fovSaved != -99999 ) {
			cg_fov.integer = fovSaved;
			cg_fov.value = (float)fovSaved;
		}
		fovVar = fovSaved = -99999;
	}
	if ( CG_DF_DemoCamThirdPerson() ) {
		cg.renderingThirdPerson = qtrue;
		return;
	}
	rcm = (int)DR_CvarF( "df_demos_ReplayCameraMode" );
	if ( drFreeCam ) {
		cg.renderingThirdPerson = qtrue;
	} else if ( df_demos_ReplayMode.integer != 0 && rcm != 0 && CG_DF_ReplayOwnsView() ) {
		static const char third[13] = { 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 };

		if ( rcm >= 0 && rcm <= 12 && third[rcm] ) {
			cg.renderingThirdPerson = qtrue;
		}
	}
}

// 0x1a265: the tracked position / time; the ghost-follow variant (live play, ReplayMode >= 2, a ghost playing with camera mode 17) uses the ghost's
static void DR_Track( int mode ) {
	VectorCopy( cg.predictedPlayerState.origin, drTarget );
	drTime = cg.time;
	if ( !cg.demoPlayback && mode >= 2 ) {
		vec3_t	pos;
		int		t;

		if ( CG_DF_GhostFollow( pos, &t ) ) {
			VectorCopy( pos, drTarget );
			drTime = t;
		}
	}
}

// ---- the edit-mode world overlay (0x18873; ReplayMode 3 / 4 with a validated script) ----
// Draws the script's data in the world; df_demos_ReplayDrawMask selects the parts: 1 = id numbers at the markers, 2 = a rocket at every camera pointing
// along its angles, 4 = lines between the two cameras of every camera transition, 8 = lines from an activator to the camera of its link, 16 = the
// activators (white marker sprites, plasma / BFG balls at the radius ends and the box corners), 32 = box wireframe of VOLUME activators, 64 = their
// translucent faces.  Colours: cameras and transitions red, links and RADIUS activators green, VOLUME centre / box blue, corners yellow / green.
static const byte drRed[4] = { 255, 0, 0, 255 }, drGreen[4] = { 0, 255, 0, 255 }, drBlue[4] = { 0, 0, 255, 255 }, drYellow[4] = { 255, 255, 0, 255 };

static void DR_OvLine( const vec3_t a, const vec3_t b, const byte *rgba ) {		// 0x948a
	refEntity_t re;

	Com_Memset( &re, 0, sizeof( re ) );
	VectorCopy( a, re.origin );
	VectorCopy( b, re.oldorigin );
	AxisClear( re.axis );
	re.reType = RT_RAIL_CORE;
	re.customShader = cgs.media.railCoreShader;
	Com_Memcpy( re.shaderRGBA, rgba, 4 );
	trap_R_AddRefEntityToScene( &re );
}

// 0x9229: the decimal digits of `n` as camera-facing sprites (cgs.media.numberShaders, 10 = minus) centred on `pos`
static void DR_OvNumber( const vec3_t pos, int n, int size, const byte *rgba ) {
	static const vec3_t up = { 0, 0, 1 };
	refEntity_t	re;
	vec3_t		d, right;
	int			digits[16], count = 0, k;
	qboolean	neg = n < 0;

	if ( neg ) {
		n = -n;
	}
	do {
		digits[count++] = n % 10;
		n /= 10;
	} while ( n != 0 && count < 14 );
	if ( neg ) {
		digits[count++] = 10;
	}
	Com_Memset( &re, 0, sizeof( re ) );
	re.reType = RT_SPRITE;
	re.radius = (float)( size / 2 );
	Com_Memcpy( re.shaderRGBA, rgba, 4 );
	VectorSubtract( cg.refdef.vieworg, pos, d );
	CrossProduct( d, up, right );
	VectorNormalize( right );
	for ( k = 0; k < count; k++ ) {
		VectorMA( pos, ( (float)count / 2.0f - (float)k ) * (float)size, right, re.origin );
		re.customShader = cgs.media.numberShaders[digits[count - 1 - k]];
		if ( re.customShader ) {
			trap_R_AddRefEntityToScene( &re );
		}
	}
}

// the number above (and optionally below) a marker: DeFRaG offsets it by 15 units and uses size 8 (data 9400 / 9404)
static void DR_OvTag( const vec3_t pos, float dz, int id, const byte *rgba ) {
	vec3_t p;

	VectorCopy( pos, p );
	p[2] += dz;
	DR_OvNumber( p, id, 8, rgba );
}

static void DR_OvBox( const vec3_t p1, const vec3_t p2, const byte *rgba ) {		// 0x94c9: the 12 edges
	int		c, k;
	vec3_t	a, b;

	for ( c = 0; c < 8; c++ ) {
		for ( k = 0; k < 3; k++ ) {
			if ( c & ( 1 << k ) ) {
				continue;
			}
			a[0] = ( c & 1 ) ? p2[0] : p1[0];
			a[1] = ( c & 2 ) ? p2[1] : p1[1];
			a[2] = ( c & 4 ) ? p2[2] : p1[2];
			VectorCopy( a, b );
			b[k] = p2[k];
			DR_OvLine( a, b, rgba );
		}
	}
}

static void DR_OvPoly( const vec3_t v0, const vec3_t v1, const vec3_t v2, const vec3_t v3, const float *rgba ) {		// 0x973c
	static const float	st[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 0 } };
	polyVert_t	verts[4];
	const float	*v[4];
	qhandle_t	shader;
	int			i, j;

	v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3;
	for ( i = 0; i < 4; i++ ) {
		VectorCopy( v[i], verts[i].xyz );
		verts[i].st[0] = st[i][0];
		verts[i].st[1] = st[i][1];
		for ( j = 0; j < 4; j++ ) {
			verts[i].modulate[j] = (byte)(int)rgba[j];
		}
	}
	shader = trap_R_RegisterShader( "flatpolygon" );
	if ( shader ) {
		trap_R_AddPolyToScene( shader, 4, verts );
	}
}

static void DR_OvFaces( const vec3_t p1, const vec3_t p2, const float *rgba ) {		// 0x99b7: the six faces of the box
	vec3_t	half, c, A, B, C, D, E, F, G, H;
	int		i;

	for ( i = 0; i < 3; i++ ) {
		half[i] = ( p2[i] - p1[i] ) / 2.0f;
		c[i] = p1[i] + half[i];
	}
#define OV_CORNER( out, sx, sy, sz ) out[0] = c[0] + (sx) * half[0]; out[1] = c[1] + (sy) * half[1]; out[2] = c[2] + (sz) * half[2]
	OV_CORNER( A, -1, -1, -1 ); OV_CORNER( B, -1, -1, 1 ); OV_CORNER( C, 1, -1, 1 ); OV_CORNER( D, 1, -1, -1 );
	OV_CORNER( E, -1, 1, -1 ); OV_CORNER( F, -1, 1, 1 ); OV_CORNER( G, 1, 1, 1 ); OV_CORNER( H, 1, 1, -1 );
#undef OV_CORNER
	DR_OvPoly( A, B, C, D, rgba );
	DR_OvPoly( C, G, H, D, rgba );
	DR_OvPoly( E, F, G, H, rgba );
	DR_OvPoly( A, B, F, E, rgba );
	DR_OvPoly( B, F, G, C, rgba );
	DR_OvPoly( A, E, H, D, rgba );
}

static void DR_OvMarker( const vec3_t pos ) {		// the white sprite on an activator point (medal_gauntlet, radius 10)
	refEntity_t re;

	Com_Memset( &re, 0, sizeof( re ) );
	re.reType = RT_SPRITE;
	re.customShader = cgs.media.medalGauntlet;
	re.radius = 10;
	Com_Memset( re.shaderRGBA, 255, 4 );
	VectorCopy( pos, re.origin );
	trap_R_AddRefEntityToScene( &re );
}

static vec3_t	ovDir;		// the missile direction of the shared overlay entity (DeFRaG leaves it set between uses)

static void DR_OvBall( int weapon, const vec3_t org ) {		// 0x1f3a0 on the overlay entity: a rocket (5), plasma (8) or BFG (9) missile
	centity_t c;

	Com_Memset( &c, 0, sizeof( c ) );
	c.currentState.eType = ET_MISSILE;
	c.currentState.weapon = weapon;
	VectorCopy( ovDir, c.currentState.pos.trDelta );
	VectorCopy( org, c.lerpOrigin );
	CG_DF_AddGhostMissile( &c );
}

// a ball at `p`, its direction taken from `from` to `p`, with the id number above or below it
static void DR_OvEnd( const vec3_t from, const vec3_t p, int id, qboolean below, int mask, const byte *rgba ) {
	vec3_t d;

	VectorSubtract( p, from, d );
	VectorNormalize( d );
	VectorCopy( d, ovDir );
	if ( mask & 16 ) {
		DR_OvBall( 9, p );
	}
	if ( mask & 1 ) {
		DR_OvTag( p, below ? -15.0f : 15.0f, id, rgba );
	}
}

static void DR_Overlay( void ) {		// 0x18873
	int		mask = df_demos_ReplayDrawMask.integer;
	vec3_t	p, q, mid, fwd;
	int		i, k;

	if ( !drEnabled ) {
		return;
	}
	VectorClear( ovDir );
	for ( i = 0; i < DR_MAX_CAM; i++ ) {		// cameras: a rocket along the view, the id above and below
		if ( !DR_InUse( DR_CAM, i ) ) {
			continue;
		}
		VectorCopy( drCams[i].origin, p );
		AngleVectors( drCams[i].angles, fwd, NULL, NULL );
		VectorCopy( fwd, ovDir );
		if ( mask & 2 ) {
			DR_OvBall( 5, p );
		}
		if ( mask & 1 ) {
			DR_OvTag( p, 15.0f, i, drRed );
			DR_OvTag( p, -15.0f, i, drRed );
		}
	}
	for ( i = 0; i < DR_MAX_TRAN; i++ ) {		// camera transitions: a line between the two cameras
		if ( drTrans[i].dtype == DR_CAM && DR_InUse( DR_CAM, drTrans[i].id1 ) && DR_InUse( DR_CAM, drTrans[i].id2 ) && ( mask & 4 ) ) {
			DR_OvLine( drCams[drTrans[i].id1].origin, drCams[drTrans[i].id2].origin, drRed );
		}
	}
	for ( i = 0; i < DR_MAX_LINK; i++ ) {		// links: a line from the activator to the camera it moves to
		const drLink_t *l = &drLinks[i];
		qboolean haveFrom = qfalse, haveTo = qfalse;

		if ( !DR_InUse( DR_LINK, i ) || !DR_InUse( DR_ACT, l->id1 ) ) {
			continue;
		}
		if ( drActs[l->id1].type == 4 ) {
			VectorCopy( drActs[l->id1].pos1, p );
			haveFrom = qtrue;
		} else if ( drActs[l->id1].type == 5 ) {
			for ( k = 0; k < 3; k++ ) {
				p[k] = drActs[l->id1].pos1[k] + 0.5f * ( drActs[l->id1].pos2[k] - drActs[l->id1].pos1[k] );
			}
			haveFrom = qtrue;
		}
		if ( l->dtype2 == DR_CAM ) {
			if ( !DR_InUse( DR_CAM, l->id2 ) ) {
				continue;
			}
			VectorCopy( drCams[l->id2].origin, q );
			haveTo = qtrue;
		} else if ( l->dtype2 == DR_TRAN ) {
			if ( drTrans[l->id2].dtype != DR_CAM || !DR_InUse( DR_CAM, drTrans[l->id2].id1 ) ) {
				continue;
			}
			VectorCopy( drCams[drTrans[l->id2].id1].origin, q );
			haveTo = qtrue;
		}
		if ( haveFrom && haveTo && ( mask & 8 ) ) {
			DR_OvLine( p, q, drGreen );
		}
	}
	for ( i = 0; i < DR_MAX_ACT; i++ ) {
		drAct_t *a = &drActs[i];

		if ( a->type == 4 ) {		// RADIUS: a marker, a plasma ball and BFG balls at the six axis ends
			if ( VectorLength( a->pos1 ) == 0 ) {
				continue;
			}
			if ( mask & 16 ) {
				DR_OvMarker( a->pos1 );
			}
			if ( mask & 16 ) {
				DR_OvBall( 8, a->pos1 );
			}
			if ( mask & 1 ) {
				DR_OvTag( a->pos1, 15.0f, i, drGreen );
				DR_OvTag( a->pos1, -15.0f, i, drGreen );
			}
			for ( k = 0; k < 3; k++ ) {
				VectorCopy( a->pos1, p );
				p[k] += a->value;
				DR_OvEnd( a->pos1, p, i, qfalse, mask, drGreen );
				VectorCopy( a->pos1, p );
				p[k] -= a->value;
				DR_OvEnd( a->pos1, p, i, k == 2, mask, drGreen );
			}
		}
		if ( a->type == 5 ) {		// VOLUME: the box between pos1 and pos2
			static const float faceRgba[4] = { 0, 0, 255, 25 };

			if ( VectorLength( a->pos1 ) == 0 && VectorLength( a->pos2 ) == 0 ) {
				continue;
			}
			if ( mask & 16 ) {
				DR_OvMarker( a->pos1 );
				DR_OvMarker( a->pos2 );
			}
			for ( k = 0; k < 3; k++ ) {
				mid[k] = a->pos1[k] + 0.5f * ( a->pos2[k] - a->pos1[k] );
			}
			if ( mask & 16 ) {
				DR_OvBall( 8, mid );
			}
			if ( mask & 1 ) {
				DR_OvTag( mid, 15.0f, i, drBlue );
				DR_OvTag( mid, -15.0f, i, drBlue );
			}
			for ( k = 0; k < 2; k++ ) {		// the two defining corners (yellow): the number goes to the outside
				const float *pt = k ? a->pos2 : a->pos1;

				VectorSubtract( pt, mid, fwd );
				VectorNormalize( fwd );
				DR_OvEnd( mid, pt, i, fwd[2] >= 0, mask, drYellow );
			}
			for ( k = 0; k < 3; k++ ) {		// the other six corners (green)
				VectorCopy( a->pos1, p );
				p[k] = a->pos2[k];
				VectorSubtract( p, mid, fwd );
				VectorNormalize( fwd );
				DR_OvEnd( mid, p, i, fwd[2] >= 0, mask, drGreen );
				VectorCopy( a->pos2, p );
				p[k] = a->pos1[k];
				VectorSubtract( p, mid, fwd );
				VectorNormalize( fwd );
				DR_OvEnd( mid, p, i, fwd[2] >= 0, mask, drGreen );
			}
			if ( mask & 32 ) {
				DR_OvBox( a->pos1, a->pos2, drBlue );
			}
			if ( mask & 64 ) {
				DR_OvFaces( a->pos1, a->pos2, faceRgba );
			}
		}
	}
}

// 0x1a4ac: the per-mode work table (modes 1 and 2: activators, transitions, camera; 3 adds the edit overlay; 4 is the overlay alone)
static void DR_Table( int mode ) {
	DR_Track( mode );
	if ( mode >= 1 && mode <= 3 ) {
		DR_Activators();
		DR_StepTrans();
		DR_CameraView();
	}
	if ( mode >= 3 ) {
		DR_Overlay();
	}
}

// 0x19156 clamps ReplayMode to 0..4, then 0x1a4ed runs the frame. Called after CG_CalcViewValues.
void CG_DF_ReplayFrame( void ) {
	int mode = df_demos_ReplayMode.integer;

	if ( mode < 0 || mode > 4 ) {
		mode = (int)DR_Clamp( (float)mode, 0, 4 );
		trap_Cvar_Set( "df_demos_ReplayMode", va( "%i", mode ) );
	}
	drFreeCam = qfalse;
	drVerbose = mode >= 2;
	DR_KeyFrame( mode );
	if ( drEnabled && drCleanup ) {		// 0x1a3a6
		DR_RestoreVars();
		DR_ClearLatches();
		drCleanup = 0;
	}
	if ( cg.demoPlayback && DR_CvarF( "df_demos_CameraMode" ) != 0 ) {
		DR_Cleanup( qtrue );
	} else if ( mode == 0 ) {
		DR_Cleanup( qtrue );
	} else if ( mode == 1 && !cg.demoPlayback && drKey == 0 ) {
		return;
	} else {
		qboolean loaded = DR_EnsureLoaded();

		if ( !loaded && mode != 1 ) {
			return;
		}
		if ( mode == 1 && ( !loaded || drKey == 2 ) ) {		// AUTOCHASE: the replay free camera (mode 9)
			CG_DF_ReplayFreeCamera( 9, 0 );
			if ( drKey == 1 ) {
				DR_KeyChange( 2, qtrue );
			}
			drFreeCam = qtrue;
		} else {
			DR_Table( mode );
		}
	}
}

void CG_DF_Replay_f( void ) {			// 0x18006
	char sub[MAX_STRING_CHARS];

	drArg = 1;
	Q_strncpyz( sub, DR_Tok(), sizeof( sub ) );
	if ( !sub[0] || !Q_stricmp( sub, "READ" ) || !Q_stricmp( sub, "LOAD" ) ) {
		DR_CmdLoad();
	} else if ( !Q_stricmp( sub, "WRITE" ) || !Q_stricmp( sub, "SAVE" ) ) {
		DR_CmdSave();
	} else if ( !Q_stricmp( sub, "VALIDATE" ) ) {
		drEnabled = 1;
		DR_Validate( DR_Verbose() );		// DeFRaG discards the result (0x16afb returns 1)
	} else if ( !Q_stricmp( sub, "CLEAR" ) || !Q_stricmp( sub, "DELETE" ) ) {
		DR_CmdClear();
	} else if ( !Q_stricmp( sub, "GET" ) ) {
		DR_CmdGet();
	} else if ( !Q_stricmp( sub, "SET" ) ) {
		DR_CmdSet();
	} else if ( !Q_stricmp( sub, "ACTIVATE" ) ) {
		DR_CmdActivate();
	} else if ( !Q_stricmp( sub, "HELP" ) || !Q_stricmp( sub, "?" ) ) {
		DR_Help();
	} else {
		CG_Printf( "^1REPLAY ERROR:^7 Invalid Command ^4-->^7 ( %s )\n", sub );
		DR_Help();
	}
}
