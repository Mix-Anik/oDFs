// cg_dfchs.c -- DeFRaG CHS panels ("crosshair stats", df_chs1_* / df_chs2_*): up to 8 configurable info rows around the crosshair.
// Port of cgame 0xf53c (panel draw), 0xbe67 (layout), 0xf4b9 (alignment), 0xc320 (row text, subset of items), see disasm/CGAME/chs.md.
// Item id = decimals * 1000 + item.  Items ported: 1-6, 20-25, 30-38, 100-109, 111, 114-118, 120-139, 171, 172.  Everything else prints nothing.
// Not ported: panel 0 (df_chs0_*, draw function not located), the other ~180 items (need ghosts, OB predictor, jump trackers, ...).

#include "cg_local.h"

dfChsCvars_t	dfChs[3];

#define CHS_BUF			128
#define CHS_MAX_COMBO	10

typedef struct {
	int		x, y, size, shadow, align, id;
	vec4_t	fontCol, labelCol;
} chsRow_t;

// bg_lib has no strcmp-free helpers we need beyond the usual set
static int CG_DF_ChsColorIndex( int c ) {
	return ( c & 15 ) % 10;
}

// 0xbe67: per-row layout from the panel cvars
static void CG_DF_ChsLayout( int p, chsRow_t rows[8] ) {
	dfChsCvars_t	*c = &dfChs[p];
	int		size = c->fontSize.integer, shadow, ox = c->offsetX.integer, oy = c->offsetY.integer, k, cx = 320, cy = 240;
	float	alpha = c->fontTransparency.value;
	int		type = c->displayType.integer;

	if ( size < 4 ) size = 4;
	if ( size > 50 ) size = 50;
	shadow = ( c->fontShadow.integer == 0 || c->fontShadow.integer == 1 ) ? c->fontShadow.integer : 1;
	if ( alpha < 0 || alpha > 1.0f ) {
		alpha = 1.0f;
	}
	if ( ox > 320 ) ox = 320;
	if ( ox < -320 ) ox = -320;
	if ( oy > 240 ) oy = 240;
	if ( oy < -240 ) oy = -240;

	memset( rows, 0, sizeof( chsRow_t ) * 8 );
	switch ( type ) {
	case 1:
	case 2:
	case 3: {
		int pitch = size + 4, y0 = cy - ( size * 4 + 16 );

		for ( k = 0; k < 8; k++ ) {
			rows[k].x = cx;
			rows[k].y = y0 + pitch * k;
			rows[k].align = type;
		}
		break;
	}
	case 4: {
		int pitch = size + 4, y0 = cy - ( size * 2 + 8 );

		for ( k = 0; k < 4; k++ ) {
			rows[k].x = cx - 17;
			rows[k].y = y0 + pitch * k;
			rows[k].align = 3;
			rows[k + 4].x = cx + 17;
			rows[k + 4].y = y0 + pitch * k;
			rows[k + 4].align = 1;
		}
		break;
	}
	default: {
		// ring of eight around the crosshair
		static const int dx[8] = { 0, 15, 18, 15, 0, -15, -18, -15 }, dy[8] = { -18, -10, 0, 10, 18, 10, 0, -10 };
		static const int al[8] = { 2, 1, 1, 1, 2, 3, 3, 3 };

		for ( k = 0; k < 8; k++ ) {
			rows[k].x = cx + dx[k];
			rows[k].y = cy + dy[k] - size / 2;
			rows[k].align = al[k];
		}
		break;
	}
	}
	for ( k = 0; k < 8; k++ ) {
		int ci;

		rows[k].x += ox;
		rows[k].y += oy;
		rows[k].size = size;
		rows[k].shadow = shadow;
		rows[k].id = c->info[k].integer;
		ci = CG_DF_ChsColorIndex( c->fontColor.integer );
		VectorCopy( g_color_table[ci], rows[k].fontCol );
		rows[k].fontCol[3] = alpha;
		ci = CG_DF_ChsColorIndex( c->labelColor.integer );
		VectorCopy( g_color_table[ci], rows[k].labelCol );
		rows[k].labelCol[3] = alpha;
	}
}

// 0xf4b9
static int CG_DF_ChsAlignX( int align, int x, int size, int len ) {
	if ( !len || !size ) {
		return x;
	}
	switch ( align ) {
	case 1:  return x - size / 2;
	case 2:  return ( x - size / 2 ) - ( ( len - 1 ) * size ) / 2;
	case 3:  return ( x - size / 2 ) - ( len - 1 ) * size;
	}
	return x;
}

// ---- row text (subset of 0xc320) ----

static void CG_DF_ChsFloat( char *dst, const char *fmt, float v ) {
	Com_sprintf( dst, CHS_BUF, fmt, v );
}


// date / time (items 100-109)
static void CG_DF_ChsClock( int item, char *label, char *value ) {
	qtime_t	t;
	int		h12, y, mo, d, hh, mm, ss;
	qboolean pm;

	trap_RealTime( &t );
	hh = t.tm_hour; mm = t.tm_min; ss = t.tm_sec; d = t.tm_mday; mo = t.tm_mon + 1; y = t.tm_year + 1900;
	h12 = ( hh == 0 || hh == 12 ) ? 12 : hh % 12;
	pm = hh >= 12;
	switch ( item ) {
	case 100: Q_strncpyz( label, "real time (am/pm)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%2i:%02i:%02i %s", h12, mm, ss, pm ? "PM" : "AM" ); break;
	case 101: Q_strncpyz( label, "real time", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%2i:%02i:%02i", hh, mm, ss ); break;
	case 102: Q_strncpyz( label, "real date (us)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i", mo, d, y ); break;
	case 103: Q_strncpyz( label, "real date (us) - time (am/pm)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i - %2i:%02i:%02i %s", mo, d, y, h12, mm, ss, pm ? "PM" : "AM" ); break;
	case 104: Q_strncpyz( label, "real date (us) - time", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i - %2i:%02i:%02i", mo, d, y, hh, mm, ss ); break;
	case 105: Q_strncpyz( label, "real date (eu)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i", d, mo, y ); break;
	case 106: Q_strncpyz( label, "real date (eu) - time (am/pm)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i - %2i:%02i:%02i %s", d, mo, y, h12, mm, ss, pm ? "PM" : "AM" ); break;
	case 107: Q_strncpyz( label, "real date (eu) - time", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i/%02i/%i - %2i:%02i:%02i", d, mo, y, hh, mm, ss ); break;
	case 108: Q_strncpyz( label, "real date (YYYYMMDD)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%04i%02i%02i", y, mo, d ); break;
	case 109: Q_strncpyz( label, "real time (HHMMSS)", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%02i%02i%02i", hh, mm, ss ); break;
	}
}

// accel meter values (items 120-139)
static void CG_DF_ChsAccel( int item, int dec, char *label, char *value, const char *sep, const char *fmt ) {
	char	w[24], f[96];
	float	a[5];
	int		n = 0, i;
	const char *lab = NULL;

	Com_sprintf( w, sizeof( w ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
	switch ( item ) {
	case 120: Q_strncpyz( label, "AM Start Speed", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.startSpeed ); return;
	case 121: Q_strncpyz( label, "AM Speed", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.cur ); return;
	case 122: Q_strncpyz( label, "AM MaxSpeed", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.max ); return;
	case 123: Q_strncpyz( label, "AM TrueMaxSpeed", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.trueMax ); return;
	case 124: Q_strncpyz( label, "AM Accel Percent", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.accelPct ); return;
	case 125: Q_strncpyz( label, "AM Speed Percent", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.speedPct ); return;
	case 126: Q_strncpyz( label, "AM Accel Percent H", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.hAccel ); return;
	case 127: Q_strncpyz( label, "AM Speed Percent H", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.hSpeed ); return;
	case 128: Q_strncpyz( label, "AM Speed H", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.hCur ); return;
	case 129: Q_strncpyz( label, "AM MaxSpeed H", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.hMax ); return;
	case 138: Q_strncpyz( label, "AM Angle CC", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.angleL ); return;
	case 139: Q_strncpyz( label, "AM Angle C", CHS_BUF ); CG_DF_ChsFloat( value, fmt, dfAm.angleR ); return;
	case 130: lab = "AM Angles (CC%sC)"; a[n++] = dfAm.angleL; a[n++] = dfAm.angleR; break;
	case 131: lab = "AM Percent (A%sS%sstartS"; a[n++] = dfAm.accelPct; a[n++] = dfAm.speedPct; a[n++] = dfAm.startSpeed; break;
	case 132: lab = "AM Speeds (startS%sS%smaxS)"; a[n++] = dfAm.startSpeed; a[n++] = dfAm.cur; a[n++] = dfAm.max; break;
	case 133: lab = "AM Percent H (A%sS%sstartS"; a[n++] = dfAm.hAccel; a[n++] = dfAm.hSpeed; a[n++] = dfAm.startSpeed; break;
	case 134: lab = "AM Speeds H (startS%sS%smaxS)"; a[n++] = dfAm.startSpeed; a[n++] = dfAm.hCur; a[n++] = dfAm.hMax; break;
	case 135: lab = "AM Percent All (hA%sstartS%sA)"; a[n++] = dfAm.hAccel; a[n++] = dfAm.startSpeed; a[n++] = dfAm.accelPct; break;
	case 136: lab = "AM Percent All (hA%shS%sstartS%sA%sS)"; a[n++] = dfAm.hAccel; a[n++] = dfAm.hSpeed; a[n++] = dfAm.startSpeed; a[n++] = dfAm.accelPct; a[n++] = dfAm.speedPct; break;
	case 137: lab = "AM Speeds All (hS%shmaxS%sstartS%sS%smaxS)"; a[n++] = dfAm.hCur; a[n++] = dfAm.hMax; a[n++] = dfAm.startSpeed; a[n++] = dfAm.cur; a[n++] = dfAm.max; break;
	default: return;
	}
	// label has one separator per gap; value = n numbers joined by the separator, each with the wide format
	{
		char *p = label;
		int  k = 0;

		for ( i = 0; lab[i] && p < label + CHS_BUF - 8; i++ ) {
			if ( lab[i] == '%' && lab[i + 1] == 's' ) {
				const char *q = sep;
				while ( *q && p < label + CHS_BUF - 8 ) *p++ = *q++;
				i++;
				k++;
			} else {
				*p++ = lab[i];
			}
		}
		*p = 0;
	}
	f[0] = 0;
	for ( i = 0; i < n; i++ ) {
		Q_strcat( f, sizeof( f ), i ? va( "%s%s", sep, w ) : w );
	}
	switch ( n ) {
	case 2: Com_sprintf( value, CHS_BUF, f, a[0], a[1] ); break;
	case 3: Com_sprintf( value, CHS_BUF, f, a[0], a[1], a[2] ); break;
	case 5: Com_sprintf( value, CHS_BUF, f, a[0], a[1], a[2], a[3], a[4] ); break;
	}
}

// server / map info (items 114-116, 172)
static void CG_DF_ChsServer( int item, char *label, char *value ) {
	char info[MAX_INFO_STRING];

	Q_strncpyz( info, CG_ConfigString( CS_SERVERINFO ), sizeof( info ) );
	switch ( item ) {
	case 114: Q_strncpyz( label, "mapname | server", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%s | %s", Info_ValueForKey( info, "mapname" ), Info_ValueForKey( info, "sv_hostname" ) ); break;
	case 115: Q_strncpyz( label, "mapname", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( info, "mapname" ), CHS_BUF ); break;
	case 116: Q_strncpyz( label, "server", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( info, "sv_hostname" ), CHS_BUF ); break;
	}
}


// 0x4247: distance text; unit 1 = metres, 2 = miles (x.yz), else plain units
static const char *CG_DF_UnitText( float value, int unit ) {
	float v;

	if ( unit == 1 ) {
		return va( "%im", (int)CG_ConvertDistance( value ) );
	}
	if ( unit == 2 ) {
		v = CG_ConvertDistance( value );
		if ( v > 0 ) {
			return va( "%i.%i%imil", (int)v, (int)( 10.0f * v ) % 10, (int)( 100.0f * v ) % 10 );
		} else {
			int i = (int)v;

			return va( "-%i.%i%imil", -1 * i, ( -10 * i ) % 10, ( -100 * i ) % 10 );
		}
	}
	return va( "%iu", (int)value );
}

static void CG_DF_ChsNums( char *value, const char *w, const char *open, const char *between, const char *close, float a, float b, float c ) {
	Com_sprintf( value, CHS_BUF, va( "%s%s%s%s%s%s%s", open, w, between, w, between, w, close ), a, b, c );
}

static void CG_DF_BoundKeyChar( char *out, const char *cmd );

// items 151-153 (cgame 0xe4ff): fixed-width key line  [J|C] [L] [F|B] [R] [W] [A]  with the keys drawn as 151 = arrow glyphs,
// 152 = F/B/L/R letters, 153 = first letter of the bound key (labels "input", "input (FLBR)", "input (binds)")
static void CG_DF_ChsInputLine( int item, char *label, char *value ) {
	char	kF[4] = "F", kB[4] = "B", kL[4] = "L", kR[4] = "R";

	if ( item == 151 ) {
		Q_strncpyz( label, "input", CHS_BUF );
		kF[0] = 0x87 - 256; kB[0] = 0x86 - 256; kL[0] = 0x88 - 256; kR[0] = 0x8d - 256;
		kF[1] = kB[1] = kL[1] = kR[1] = 0;
	} else if ( item == 153 ) {
		Q_strncpyz( label, "input (binds)", CHS_BUF );
		CG_DF_BoundKeyChar( kF, "+forward" );
		CG_DF_BoundKeyChar( kB, "+back" );
		CG_DF_BoundKeyChar( kL, "+moveleft" );
		CG_DF_BoundKeyChar( kR, "+moveright" );
	} else {
		Q_strncpyz( label, "input (FLBR)", CHS_BUF );
	}
	value[0] = 0;
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 32 ) ? "J " : ( CG_DF_Keys( 64 ) ? "C " : "  " ) );
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 8 ) ? kL : " " );
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 1 ) ? kF : ( CG_DF_Keys( 2 ) ? kB : " " ) );
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 16 ) ? va( "%s ", kR ) : "  " );
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 512 ) ? "W " : "  " );
	Q_strcat( value, CHS_BUF, CG_DF_Keys( 256 ) ? "A" : " " );
}

// items 40-47 (positions / angles / velocity combos), 150 (input), 210-217, 219-239
static void CG_DF_ChsMisc( int item, char *label, char *value, const char *sep, const char *w, const vec3_t pos, const vec3_t vel, const vec3_t ang ) {
	dfStats_t	st;
	vec3_t		vpos;
	char		sys[BIG_INFO_STRING], srv[MAX_INFO_STRING], tmp[MAX_QPATH];
	int			unit = df_unit.integer, ver;

	VectorCopy( cg.refdef.vieworg, vpos );
	vpos[2] += (float)cg.predictedPlayerState.viewheight;
	CG_DF_GetStats( &st );
	Q_strncpyz( srv, CG_ConfigString( CS_SERVERINFO ), sizeof( srv ) );
	Q_strncpyz( sys, CG_ConfigString( CS_SYSTEMINFO ), sizeof( sys ) );
	ver = atoi( Info_ValueForKey( srv, "defrag_vers" ) );
	if ( ver && ver < 1000 ) {
		ver *= 100;
	}

	switch ( item ) {
	case 40:
		Com_sprintf( label, CHS_BUF, "p%sy", sep );
		Com_sprintf( value, CHS_BUF, va( "%s%s%s", w, sep, w ), ang[0], ang[1] );
		break;
	case 41:
		Q_strncpyz( label, "pos", CHS_BUF );
		CG_DF_ChsNums( value, w, "(", " ", ")", pos[0], pos[1], pos[2] );
		break;
	case 42:
		Com_sprintf( label, CHS_BUF, "pos p%sy", sep );
		Com_sprintf( value, CHS_BUF, va( "(%s %s %s) %s%s%s", w, w, w, w, sep, w ), pos[0], pos[1], pos[2], ang[0], ang[1] );
		break;
	case 43:
		Com_sprintf( label, CHS_BUF, "view pos p%sy", sep );
		Com_sprintf( value, CHS_BUF, va( "(%s %s %s) %s%s%s", w, w, w, w, sep, w ), vpos[0], vpos[1], vpos[2], cg.refdefViewAngles[0], cg.refdefViewAngles[1] );
		break;
	case 44: Q_strncpyz( label, "position x y z", CHS_BUF ); CG_DF_ChsNums( value, w, "", " ", "", pos[0], pos[1], pos[2] ); break;
	case 45: Q_strncpyz( label, "view position x y z", CHS_BUF ); CG_DF_ChsNums( value, w, "", " ", "", vpos[0], vpos[1], vpos[2] ); break;
	case 46: Q_strncpyz( label, "angles x y z", CHS_BUF ); CG_DF_ChsNums( value, w, "", " ", "", ang[0], ang[1], ang[2] ); break;
	case 47: Q_strncpyz( label, "velocity x y z", CHS_BUF ); CG_DF_ChsNums( value, w, "", " ", "", vel[0], vel[1], vel[2] ); break;
	case 151: case 152: case 153:
		CG_DF_ChsInputLine( item, label, value );
		break;
	case 150: {
		// movement keys of the viewed player: [J|C] [<] [/\|\/] [>] [W] [A]
		Q_strncpyz( label, "input", CHS_BUF );
		value[0] = 0;
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 32 ) ? "J " : ( CG_DF_Keys( 64 ) ? "C " : "  " ) );
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 8 ) ? "<" : " " );
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 1 ) ? "/\\" : ( CG_DF_Keys( 2 ) ? "\\/" : "  " ) );
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 16 ) ? "> " : "  " );
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 512 ) ? "W " : "  " );
		Q_strcat( value, CHS_BUF, CG_DF_Keys( 256 ) ? "A" : " " );
		break;
	}
	case 210: Q_strncpyz( label, "jump height", CHS_BUF ); Q_strncpyz( value, CG_DF_UnitText( st.jumpHeight, unit == 1 ), CHS_BUF ); break;
	case 211: Q_strncpyz( label, "jump dist", CHS_BUF ); Q_strncpyz( value, CG_DF_UnitText( st.jumpDist, unit == 1 ), CHS_BUF ); break;
	case 212: Q_strncpyz( label, "tallest jump", CHS_BUF ); Q_strncpyz( value, CG_DF_UnitText( st.tallest, unit == 1 ), CHS_BUF ); break;
	case 213: Q_strncpyz( label, "longest jump", CHS_BUF ); Q_strncpyz( value, CG_DF_UnitText( st.longest, unit == 1 ), CHS_BUF ); break;
	case 214: Q_strncpyz( label, "distance", CHS_BUF ); Q_strncpyz( value, CG_DF_UnitText( st.distance, unit ), CHS_BUF ); break;
	case 217: Q_strncpyz( label, "highest speed", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", (int)st.maxSpeed ); break;
	case 219:
		Q_strncpyz( label, "country", CHS_BUF );
		trap_Cvar_VariableStringBuffer( "df_country", value, CHS_BUF );
		break;
	case 190: case 191:		// cgame 0xeac2: only while playing a demo; cvar defrag_servername (set by the ui connect screen); 190 strips path and extension (0x8cfa / 0x8c53)
		Q_strncpyz( label, item == 190 ? "demoname" : "fulldemoname", CHS_BUF );
		if ( cg.demoPlayback ) {
			char	*p;

			trap_Cvar_VariableStringBuffer( "defrag_servername", value, CHS_BUF );
			if ( item == 190 ) {
				if ( ( p = strrchr( value, '/' ) ) != NULL ) {
					memmove( value, p + 1, strlen( p ) );
				}
				if ( ( p = strrchr( value, '.' ) ) != NULL ) {
					*p = 0;
				}
			}
		}
		break;
	case 192: case 193:		// cgame 0xeb02: name of tracked client (0x120e44, assumed = viewed client; its writer is not decoded)
		Q_strncpyz( label, item == 192 ? "tracking player name" : "tracking player name (no color)", CHS_BUF );
		if ( cg.snap && cg.snap->ps.clientNum >= 0 && cg.snap->ps.clientNum < MAX_CLIENTS && cgs.clientinfo[cg.snap->ps.clientNum].name[0] ) {
			Q_strncpyz( value, cgs.clientinfo[cg.snap->ps.clientNum].name, CHS_BUF );
			if ( item == 193 ) {
				Q_CleanStr( value );
			}
		}
		break;
	case 215: Q_strncpyz( label, va( "h%sa spent", sep ), CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i%s%i", st.healthSpent, sep, st.armorSpent ); break;
	case 216: Q_strncpyz( label, "shots fired", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", st.shotsFired ); break;
	case 225: Q_strncpyz( label, "multiplayer", CHS_BUF ); Q_strncpyz( value, ( ver >= 18100 && atoi( Info_ValueForKey( srv, "defrag_gametype" ) ) < 5 ) ? "on" : "off", CHS_BUF ); break;		// 0x11d2ac
	case 227: Q_strncpyz( label, "overbounces", CHS_BUF ); Q_strncpyz( value, ( ver >= 18100 && atoi( Info_ValueForKey( srv, "defrag_obs" ) ) != 0 ) ? "on" : "off", CHS_BUF ); break;		// 0x11d2b4
	case 233: Q_strncpyz( label, "gamepath", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( sys, "fs_game" ), CHS_BUF ); break;		// 0x11d550 = fs_game
	case 220: Q_strncpyz( label, "defrag", CHS_BUF ); Q_strncpyz( value, ver ? "on" : "off", CHS_BUF ); break;
	case 221:
		Q_strncpyz( label, "version", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i.%02i.%02i", ver / 10000, ( ver / 100 ) % 100, ver % 100 );
		break;
	case 222: Q_strncpyz( label, "gametype", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( srv, "g_gametype" ) ) ); break;
	case 223: Q_strncpyz( label, "gametype real", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( srv, "defrag_gametype" ) ) ); break;
	case 224: Q_strncpyz( label, "gamemode", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( srv, "defrag_mode" ) ) ); break;
	case 226: Q_strncpyz( label, "physics", CHS_BUF ); Q_strncpyz( value, atoi( Info_ValueForKey( srv, "df_promode" ) ) ? "cpm" : "vq3", CHS_BUF ); break;
	case 228: Q_strncpyz( label, "mapname", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( srv, "mapname" ), CHS_BUF ); break;
	case 229: Q_strncpyz( label, "route", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( srv, "defrag_route" ), CHS_BUF ); break;
	case 230: Q_strncpyz( label, "competition", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( srv, "df_competition" ), CHS_BUF ); break;
	case 231: Q_strncpyz( label, "server name", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( srv, "sv_hostname" ), CHS_BUF ); break;
	case 232: Q_strncpyz( label, "gamename", CHS_BUF ); Q_strncpyz( value, Info_ValueForKey( srv, "gamename" ), CHS_BUF ); break;
	case 234: Q_strncpyz( label, "cheats", CHS_BUF ); Q_strncpyz( value, atoi( Info_ValueForKey( sys, "sv_cheats" ) ) ? "on" : "off", CHS_BUF ); break;
	case 235: Q_strncpyz( label, "sv_pure", CHS_BUF ); Q_strncpyz( value, atoi( Info_ValueForKey( sys, "sv_pure" ) ) ? "on" : "off", CHS_BUF ); break;
	case 236: Q_strncpyz( label, "sv_fps", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( sys, "sv_fps" ) ) ); break;
	case 237: Q_strncpyz( label, "pmove_msec", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( sys, "pmove_msec" ) ) ); break;
	case 238: Q_strncpyz( label, "g_sync", CHS_BUF ); Com_sprintf( value, CHS_BUF, "%i", atoi( Info_ValueForKey( sys, "g_synchronousClients" ) ) ); break;
	case 239:
		Q_strncpyz( label, "com_maxfps", CHS_BUF );
		trap_Cvar_VariableStringBuffer( "com_maxfps", tmp, sizeof( tmp ) );
		Com_sprintf( value, CHS_BUF, "%i", atoi( tmp ) );
		break;
	}
}


// items 170, 173, 174 (times), 180-185 (view direction), 200-202 (race times)
static void CG_DF_ChsTimeVec( int item, char *label, char *value, const char *w, const vec3_t ang, const char *fmt ) {
	int		t, s, m, ts, ms, ref;
	vec3_t	f;

	switch ( item ) {
	case 170:
	case 173:
	case 174:
		t = cg.time;
		if ( item != 170 ) {
			t -= cgs.levelStartTime;
		}
		s = t / 1000;
		m = s / 60;
		s %= 60;
		ts = s / 10;
		s %= 10;
		ms = t % 1000;
		if ( item == 170 ) {
			Q_strncpyz( label, "server time", CHS_BUF );
		} else if ( item == 173 ) {
			Q_strncpyz( label, "cg.time - lvlstart (min:sec:msec)", CHS_BUF );
		} else {
			Q_strncpyz( label, "cg.time - lvlstart (min.sec)", CHS_BUF );
		}
		if ( item == 174 ) {
			Com_sprintf( value, CHS_BUF, "%i.%i%i", m, ts, s );
		} else {
			Com_sprintf( value, CHS_BUF, "%i:%i%i:%03i", m, ts, s, ms );
		}
		break;
	case 180: case 181: case 182: case 183: case 184: case 185:
		AngleVectors( ang, f, NULL, NULL );
		if ( item >= 184 ) {
			f[2] = 0;
		}
		VectorNormalize( f );
		if ( item == 183 ) {
			Q_strncpyz( label, "Forward X Y Z", CHS_BUF );
			CG_DF_ChsNums( value, w, "", " ", "", f[0], f[1], f[2] );
		} else {
			static const char *names[6] = { "Forward X", "Forward Y", "Forward Z", NULL, "Forward X (NoZ)", "Forward Y (NoZ)" };
			static const int  comp[6] = { 0, 1, 2, 0, 0, 1 };

			Q_strncpyz( label, names[item - 180], CHS_BUF );
			CG_DF_ChsFloat( value, fmt, f[comp[item - 180]] );
		}
		break;
	case 200:
		Q_strncpyz( label, "time", CHS_BUF );
		Q_strncpyz( value, CG_DF_FormatTime( dfLastRunMs ), CHS_BUF );
		break;
	case 201:
		Q_strncpyz( label, "best time", CHS_BUF );
		ref = CG_DF_ReferenceTime();
		Q_strncpyz( value, ref ? CG_DF_FormatTime( ref ) : "unknown", CHS_BUF );
		break;
	case 202:
		Q_strncpyz( label, "time difference", CHS_BUF );
		ref = CG_DF_ReferenceTime();
		Q_strncpyz( value, CG_DF_FormatTime( ref ? ref - dfLastRunMs : dfLastRunMs ), CHS_BUF );
		break;
	}
}


// items 110 (fps), 112, 113 (client/server settings): 0x108a9 = 4-frame average of the wall-clock frame time
static float CG_DF_ChsFps( void ) {
	static int	times[4], index, previous;
	int			t = trap_Milliseconds(), total = 0, i;

	times[index % 4] = t - previous;
	previous = t;
	index++;
	if ( index <= 4 ) {
		return 0;
	}
	for ( i = 0; i < 4; i++ ) {
		total += times[i];
	}
	if ( !total ) {
		total = 1;
	}
	return (float)( 4000 / total );
}

static void CG_DF_ChsSettings( int item, char *label, char *value, const char *sep, const char *fmt ) {
	char	tmp[MAX_CVAR_VALUE_STRING];
	int		cfps, svfps, sync, pm = 0;
	float	ts, fps;

	if ( item == 110 ) {
		Q_strncpyz( label, "fps", CHS_BUF );
		CG_DF_ChsFloat( value, fmt, CG_DF_ChsFps() );
		return;
	}
	fps = CG_DF_ChsFps();
	trap_Cvar_VariableStringBuffer( "com_maxfps", tmp, sizeof( tmp ) );
	cfps = atoi( tmp );
	trap_Cvar_VariableStringBuffer( "sv_fps", tmp, sizeof( tmp ) );
	svfps = atoi( tmp );
	sync = cg_synchronousClients.integer;
	if ( pmove_fixed.integer ) {
		pm = pmove_msec.integer;
	}
	trap_Cvar_VariableStringBuffer( "timescale", tmp, sizeof( tmp ) );
	ts = atof( tmp );
	if ( item == 112 ) {
		Com_sprintf( label, CHS_BUF, "cfps%ssfps%ssynch%spm%sts", sep, sep, sep, sep );
		Com_sprintf( value, CHS_BUF, va( "%%i%s%%i%s%%i%s%%i%s%%1.1f", sep, sep, sep, sep ), cfps, svfps, sync, pm, ts );
	} else {
		Com_sprintf( label, CHS_BUF, "fps%sfps%ssfps%ssynch%spm%sts", sep, sep, sep, sep, sep );
		Com_sprintf( value, CHS_BUF, va( "%%3i%s%%i%s%%i%s%%i%s%%i%s%%1.1f", sep, sep, sep, sep, sep ), (int)fps, cfps, svfps, sync, pm, ts );
	}
}


// items 10-16: distance to what the player looks at.  0x8444: trace from the eye (+14 units forward, as a muzzle) along the view
// direction, 128000 units; the end point's z is raised by 24.  0x83e3 (VIEW variant): same trace from the eye without offset.
static int CG_DF_ChsTraceMask( void ) {
	return ( cg.snap->ps.stats[14] & 1 ) ? 65537 : 33619969;		// STAT_INTERFERENCE bit 0: players do not block
}

static void CG_DF_ChsLookTrace( const vec3_t pos, const vec3_t ang, vec3_t hit ) {
	vec3_t	fwd, start, end;
	trace_t	tr;

	AngleVectors( ang, fwd, NULL, NULL );
	VectorCopy( pos, start );
	start[2] += cg.snap->ps.viewheight;
	VectorMA( start, 14.0f, fwd, start );
	VectorMA( start, 128000.0f, fwd, end );
	CG_Trace( &tr, start, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CG_DF_ChsTraceMask() );
	VectorCopy( tr.endpos, hit );
	hit[2] += 24.0f;
}

static void CG_DF_ChsViewTrace( const vec3_t eye, const vec3_t ang, vec3_t hit ) {
	vec3_t	fwd, end;
	trace_t	tr;

	AngleVectors( ang, fwd, NULL, NULL );
	VectorMA( eye, 128000.0f, fwd, end );
	CG_Trace( &tr, eye, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CG_DF_ChsTraceMask() );
	VectorCopy( tr.endpos, hit );
}

static void CG_DF_ChsDist( int item, char *label, char *value, const char *sep, const char *w, const char *fmt, const vec3_t pos, const vec3_t ang ) {
	vec3_t	hit, d, eye, vhit, vd;
	float	xy, z, xyz, vxyz;

	CG_DF_ChsLookTrace( pos, ang, hit );
	VectorSubtract( hit, pos, d );
	xy = sqrt( d[0] * d[0] + d[1] * d[1] );
	z = d[2];
	xyz = VectorLength( d );
	VectorCopy( pos, eye );
	eye[2] += cg.snap->ps.viewheight;
	CG_DF_ChsViewTrace( eye, ang, vhit );
	VectorSubtract( vhit, eye, vd );
	vxyz = VectorLength( vd );

	switch ( item ) {
	case 10: Q_strncpyz( label, "dist XY", CHS_BUF ); CG_DF_ChsFloat( value, fmt, xy ); break;
	case 11: Q_strncpyz( label, "dist Z", CHS_BUF ); CG_DF_ChsFloat( value, fmt, z ); break;
	case 12: Q_strncpyz( label, "dist XYZ", CHS_BUF ); CG_DF_ChsFloat( value, fmt, xyz ); break;
	case 13: Q_strncpyz( label, "dist VIEWXYZ", CHS_BUF ); CG_DF_ChsFloat( value, fmt, vxyz ); break;
	case 14:
		Com_sprintf( label, CHS_BUF, "dist XY%sZ%sXYZ", sep, sep );
		Com_sprintf( value, CHS_BUF, va( "%s%s%s%s%s", w, sep, w, sep, w ), xy, z, xyz );
		break;
	case 15:
		Com_sprintf( label, CHS_BUF, "dist XY%sZ%sVIEWXYZ", sep, sep );
		Com_sprintf( value, CHS_BUF, va( "%s%s%s%s%s", w, sep, w, sep, w ), xy, z, vxyz );
		break;
	case 16:
		Q_strncpyz( label, "look pos", CHS_BUF );
		CG_DF_ChsNums( value, w, "(", " ", ")", hit[0], hit[1], hit[2] );
		break;
	}
}


// items 160-163: distance to the position stored by `savepos` (the cvar named by `saveposname` holds  placeplayer x y z ...)
static void CG_DF_ChsSavePosDist( int item, char *label, char *value, const char *sep, const char *w, const vec3_t pos ) {
	char		name[1024], def[1024], *dummy = NULL;
	const char	*p, *tok;
	vec3_t		d;
	float		xy, z, xyz;

	(void)dummy;
	VectorClear( d );
	trap_Cvar_VariableStringBuffer( "saveposname", name, sizeof( name ) );
	trap_Cvar_VariableStringBuffer( name, def, sizeof( def ) );
	p = def;
	tok = COM_Parse( &p );
	if ( !Q_stricmp( tok, "placeplayer" ) ) {
		d[0] = atoi( COM_Parse( &p ) ) - pos[0];		// DeFRaG parses the coordinates with atoi
		d[1] = atoi( COM_Parse( &p ) ) - pos[1];
		d[2] = atoi( COM_Parse( &p ) ) - pos[2];
	}
	xy = sqrt( d[0] * d[0] + d[1] * d[1] );
	z = d[2];
	xyz = VectorLength( d );
	switch ( item ) {
	case 160: Q_strncpyz( label, "savepos dist XY", CHS_BUF ); Com_sprintf( value, CHS_BUF, va( "[ %%s ] = %s", w ), name, xy ); break;
	case 161: Q_strncpyz( label, "savepos dist Z", CHS_BUF ); Com_sprintf( value, CHS_BUF, va( "[ %%s ] = %s", w ), name, z ); break;
	case 162: Q_strncpyz( label, "savepos dist XYZ", CHS_BUF ); Com_sprintf( value, CHS_BUF, va( "[ %%s ] = %s", w ), name, xyz ); break;
	case 163:
		Com_sprintf( label, CHS_BUF, "savepos dist XY%sZ%sXYZ", sep, sep );
		Com_sprintf( value, CHS_BUF, va( "[ %%s ] = %s%s%s%s%s", w, sep, w, sep, w ), name, xy, z, xyz );
		break;
	}
}


// items 50-79, 300-310: overbounce predictor output (cg_dfob.c)
static const int obItemSlot[17] = { 0, 1, 2, 11, 12, 3, 4, 5, 6, 13, 14, 15, 16, 7, 8, 9, 10 };		// items 61..77
static const int obList[11][18] = {
	{ 3, 2, 1, 0 }, { 2, 12, 11 }, { 4, 3, 4, 5, 6 }, { 4, 13, 14, 15, 16 }, { 4, 7, 8, 9, 10 },
	{ 13, 2, 1, 0, 3, 4, 5, 6, 12, 11, 13, 14, 15, 16 }, { 17, 2, 1, 0, 3, 4, 5, 6, 12, 11, 13, 14, 15, 16, 7, 8, 9, 10 },
	{ 7, 2, 1, 0, 4, 5, 2, 6 }, { 11, 2, 1, 0, 4, 5, 6, 12, 11, 14, 15, 16 }, { 5, 2, 1, 0, 12, 11 }, { 2, 1, 0 }
};		// [0] = count, then the slots, items 50..60 (corrected: first entry holds the length)

static void CG_DF_ChsOb( int item, char *label, char *value, const char *sep, const char *w ) {
	int i, slot = -1;

	if ( item >= 50 && item <= 60 ) {
		const int *l = obList[item - 50];
		int n = l[0];

		Q_strncpyz( label, "OB(", CHS_BUF );
		value[0] = 0;
		for ( i = 1; i <= n; i++ ) {
			CG_DF_ObAppend( label, CHS_BUF, 0, l[i] );
			CG_DF_ObAppend( value, CHS_BUF, 1, l[i] );
		}
		Q_strcat( label, CHS_BUF, ")" );
		return;
	}
	if ( item >= 61 && item <= 77 ) {
		slot = obItemSlot[item - 61];
		Com_sprintf( label, CHS_BUF, "OB(%s)", dfObNames[slot] );
		value[0] = 0;
		CG_DF_ObAppend( value, CHS_BUF, 1, slot );
		return;
	}
	if ( item == 79 ) {
		Q_strncpyz( label, "OB VPosOffsets", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%s | %s", w, w ), df_ob_OffsetStartPosZ.value, df_ob_OffsetStopPosZ.value );
		return;
	}
	if ( item >= 300 && item <= 308 ) {
		slot = obItemSlot[item - 300];
	} else if ( item >= 310 && item <= 317 ) {
		slot = obItemSlot[item - 301];		// 310 -> 13, 311..313 -> 14..16, 314..317 -> 7..10
	}
	if ( slot >= 0 ) {
		Com_sprintf( label, CHS_BUF, "OB(%s) offsets", dfObNames[slot] );
		Com_sprintf( value, CHS_BUF, "%.3f | %.3f", dfObOut1[slot], dfObOut2[slot] );
	}
}

// items 26..28 (cgame 0xc908..0xcaf2): speed along the view's right vector (S), with F and XY: a '<' in front of |S| when it is negative, a '>' behind it
// when it is positive (the pad is a blank otherwise). 26 uses the narrow number format, 27 / 28 the wide one
static void CG_DF_ChsSideSpeed( int item, char *label, char *value, const char *sep, const char *fmt, const char *w, const vec3_t vel, const vec3_t ang ) {
	vec3_t	f, r;
	char	pre[2] = " ", post[2] = " ";
	float	xy, F, S;

	AngleVectors( ang, f, r, NULL );
	f[2] = 0;
	VectorNormalize( f );
	r[2] = 0;
	VectorNormalize( r );
	F = vel[0] * f[0] + vel[1] * f[1];
	S = vel[0] * r[0] + vel[1] * r[1];
	xy = sqrt( vel[0] * vel[0] + vel[1] * vel[1] );
	if ( S < 0 ) {
		pre[0] = '<';
	}
	if ( S > 0 ) {
		post[0] = '>';
	}
	if ( S < 0 ) {
		S = -S;
	}
	switch ( item ) {
	case 26:
		Q_strncpyz( label, "speed S", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%%s%s%%s", fmt ), pre, S, post );
		break;
	case 27:
		Com_sprintf( label, CHS_BUF, "speed F%sS", sep );
		Com_sprintf( value, CHS_BUF, va( "%s%s%%s%s%%s", w, sep, w ), F, pre, S, post );
		break;
	case 28:
		Com_sprintf( label, CHS_BUF, "speed XY%sF%sS", sep, sep );
		Com_sprintf( value, CHS_BUF, va( "%s%s%s%s%%s%s%%s", w, sep, w, sep, w ), xy, F, pre, S, post );
		break;
	}
}

// items 320..324 (cgame 0xf153..0xf2d3): the demo camera's pose
static void CG_DF_ChsDemoCam( int item, char *label, char *value, const char *sep, const char *w ) {
	vec3_t	pos, ang;

	CG_DF_DemoCamState( pos, ang );
	switch ( item ) {
	case 320:
		Q_strncpyz( label, "dcam pos", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%s %s %s", w, w, w ), pos[0], pos[1], pos[2] );
		break;
	case 321:
		Q_strncpyz( label, "dcam ang", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%s %s %s", w, w, w ), ang[0], ang[1], ang[2] );
		break;
	case 322:
		Q_strncpyz( label, "dcam pos ang", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%s %s %s %s %s %s", w, w, w, w, w, w ), pos[0], pos[1], pos[2], ang[0], ang[1], ang[2] );
		break;
	case 323:
		Q_strncpyz( label, "dcam pos ang", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "%s,%s,%s,%s,%s,%s", w, w, w, w, w, w ), pos[0], pos[1], pos[2], ang[0], ang[1], ang[2] );
		break;
	case 324:
		Q_strncpyz( label, "dcam pos ang", CHS_BUF );
		Com_sprintf( value, CHS_BUF, va( "(%s %s %s) %s%s%s%s%s", w, w, w, w, sep, w, sep, w ), pos[0], pos[1], pos[2], ang[0], ang[1], ang[2] );
		break;
	}
}

static void CG_DF_ChsItem( int id, char *label, char *value, const char *sep ) {
	const playerState_t *ps;
	vec3_t	pos, vel, ang;
	int		dec = id / 1000, item = id % 1000, health, armor;
	char	fmt[16];
	float	v;

	label[0] = value[0] = 0;
	if ( !df_chs_UsePredicted.integer ) {
		ps = &cg.snap->ps;
	} else {
		ps = &cg.predictedPlayerState;
	}
	if ( dec > 6 ) {
		dec = 6;
	}
	Com_sprintf( fmt, sizeof( fmt ), "%%%i.%if", dec + ( dec > 0 ? 1 : 0 ), dec );
	VectorCopy( ps->origin, pos );
	VectorCopy( ps->velocity, vel );
	VectorCopy( ps->viewangles, ang );
	health = cg.snap->ps.stats[STAT_HEALTH];
	armor = cg.snap->ps.stats[STAT_ARMOR];
	CG_DF_GhostTracked( pos, vel, ang );		// 0xc447: a ghost with a camera mode replaces position / velocity / angles

	switch ( item ) {
	case 1:
		Q_strncpyz( label, "speed", CHS_BUF );
		Q_strncpyz( value, CG_DF_SpeedText( sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) ), CHS_BUF );
		break;
	case 2:
		Q_strncpyz( label, "health", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", health );
		break;
	case 3:
		Q_strncpyz( label, "armor", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", armor );
		break;
	case 4:
		Q_strncpyz( label, "ammo", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", cg.snap->ps.ammo[cg.snap->ps.weapon] );
		break;
	case 5:
		Com_sprintf( label, CHS_BUF, "h%sa", sep );
		Com_sprintf( value, CHS_BUF, "%i%s%i", health, sep, armor );
		break;
	case 6:
		Com_sprintf( label, CHS_BUF, "h%sa%sa", sep, sep );
		Com_sprintf( value, CHS_BUF, "%i%s%i%s%i", health, sep, armor, sep, cg.snap->ps.ammo[cg.snap->ps.weapon] );
		break;
	case 20: case 21: case 22: case 23: case 24: {
		static const char *names[5] = { "speed X", "speed Y", "speed Z", "speed XY", "speed XYZ" };

		Q_strncpyz( label, names[item - 20], CHS_BUF );
		if ( item < 23 ) {
			v = vel[item - 20];
		} else if ( item == 23 ) {
			v = sqrt( vel[0] * vel[0] + vel[1] * vel[1] );
		} else {
			v = sqrt( vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2] );
		}
		CG_DF_ChsFloat( value, fmt, v );
		break;
	}
	case 25: {
		vec3_t f;

		Q_strncpyz( label, "speed F", CHS_BUF );
		AngleVectors( ang, f, NULL, NULL );
		f[2] = 0;
		VectorNormalize( f );
		CG_DF_ChsFloat( value, fmt, vel[0] * f[0] + vel[1] * f[1] );
		break;
	}
	case 26: case 27: case 28: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsSideSpeed( item, label, value, sep, fmt, wf, vel, ang );
		break;
	}
	case 30: case 31: case 32: {
		static const char *names[3] = { "pitch", "yaw", "roll" };

		Q_strncpyz( label, names[item - 30], CHS_BUF );
		CG_DF_ChsFloat( value, fmt, ang[item - 30] );
		break;
	}
	case 33: case 34: case 35: {
		static const char *names[3] = { "X pos", "Y pos", "Z pos" };

		Q_strncpyz( label, names[item - 33], CHS_BUF );
		CG_DF_ChsFloat( value, fmt, pos[item - 33] );
		break;
	}
	case 36: case 37: case 38: {
		static const char *names[3] = { "view X pos", "view Y pos", "view Z pos" };

		Q_strncpyz( label, names[item - 36], CHS_BUF );
		v = cg.refdef.vieworg[item - 36];
		if ( item == 38 ) {
			v += (float)cg.predictedPlayerState.viewheight;		// 0xcbd2: vieworg[2] + viewheight
		}
		CG_DF_ChsFloat( value, fmt, v );
		break;
	}
	case 140: case 141: case 142: case 143: {
		static const char *jl[4] = { "jumps speed", "jumps accel", "jumps dstnc", "jumps hght." };
		static const int jt[4] = { 2, 3, 0, 1 };

		Q_strncpyz( label, jl[item - 140], CHS_BUF );
		CG_DF_JumpHistoryText( jt[item - 140], value, CHS_BUF );
		break;
	}
	case 145: case 146: case 147: case 148: {
		static const char *ml[4] = { "max speed", "max accel", "max dstnc", "max hght." };

		Q_strncpyz( label, ml[item - 145], CHS_BUF );
		CG_DF_ChsFloat( value, fmt, CG_DF_JumpHistoryMax( item - 145 ) );
		break;
	}
	case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 150: case 151: case 152: case 153:
	case 210: case 211: case 212: case 213: case 214: case 190: case 191: case 192: case 193: case 215: case 216: case 217: case 219: case 220: case 221: case 222: case 223: case 224:
	case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsMisc( item, label, value, sep, wf, pos, vel, ang );
		break;
	}
	case 170: case 173: case 174: case 180: case 181: case 182: case 183: case 184: case 185: case 200: case 201: case 202: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsTimeVec( item, label, value, wf, ang, fmt );
		break;
	}
	case 110: case 112: case 113:
		CG_DF_ChsSettings( item, label, value, sep, fmt );
		break;
	case 10: case 11: case 12: case 13: case 14: case 15: case 16: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsDist( item, label, value, sep, wf, fmt, pos, ang );
		break;
	}
	case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 98: case 99: {		// ghost items (cgame 0xc320 cases at 0xd870..0xdac9)
		char wf[24];
		vec3_t gv;
		int gs, k;
		float gd[3];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		switch ( item ) {
		case 90:
			Q_strncpyz( label, "ghost speed", CHS_BUF );
			CG_DF_GhostVelocity( CG_DF_GhostNearest( 2 ), gv );
			CG_DF_ChsFloat( value, fmt, sqrt( gv[0] * gv[0] + gv[1] * gv[1] ) );
			break;
		case 91: case 92: case 93:
			Q_strncpyz( label, item == 91 ? "ghost dist XY" : ( item == 92 ? "ghost dist Z" : "ghost dist XYZ" ), CHS_BUF );
			k = item - 91;
			CG_DF_ChsFloat( value, fmt, CG_DF_GhostDist( CG_DF_GhostNearest( k ), k ) );
			break;
		case 94:
			Com_sprintf( label, CHS_BUF, "ghost dist XY%sZ%sXYZ", sep, sep );
			gs = CG_DF_GhostNearest( 2 );
			for ( k = 0; k < 3; k++ ) {
				gd[k] = CG_DF_GhostDist( gs, k );
			}
			Com_sprintf( value, CHS_BUF, va( "%s%s%s%s%s", wf, sep, wf, sep, wf ), gd[0], gd[1], gd[2] );
			break;
		case 95:
			Q_strncpyz( label, "ghost pos", CHS_BUF );
			CG_DF_GhostOrigin( CG_DF_GhostNearest( 2 ), gv );
			CG_DF_ChsNums( value, wf, "(", " ", ")", gv[0], gv[1], gv[2] );
			break;
		case 96:
			Q_strncpyz( label, "nearest ghost", CHS_BUF );
			gs = CG_DF_GhostNearest( 2 );
			Com_sprintf( value, CHS_BUF, "%i", gs + ( CG_DF_GhostPlaying( gs ) ? 1 : 0 ) );		// 0xda5a: index, +1 when that slot is playing
			break;
		case 98:
			Q_strncpyz( label, "ghost memory size (snap snapbuf ghosts)", CHS_BUF );
			Com_sprintf( value, CHS_BUF, "%i %i %i", 624, 312000, 1560000 );		// DeFRaG's own constants (frame, ring, 5 rings)
			break;
		case 99:
			Q_strncpyz( label, "version ghost / server", CHS_BUF );
			Com_sprintf( value, CHS_BUF, "%i / %i", CG_DF_GhostHeader( CG_DF_GhostNearest( 2 ), 0 ), CG_DF_GhostHeader( CG_DF_GhostNearest( 2 ), 1 ) );
			break;
		}
		break;
	}
	case 80:
		Q_strncpyz( label, "SET [all ghosts] (Auto NoDrawR)", CHS_BUF );
		CG_DF_GhostInfo( 0, 3, NULL, value, CHS_BUF );
		break;
	case 81: case 82: case 83: case 84: case 85: case 86:
		CG_DF_GhostInfo( item > 83, ( item - 81 ) % 3, label, value, CHS_BUF );
		break;
	case 320: case 321: case 322: case 323: case 324: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsDemoCam( item, label, value, sep, wf );
		break;
	}
	case 900:
		Q_strncpyz( label, "Verbose OB Result", CHS_BUF );
		break;
	case 901:
		Q_strncpyz( label, "OSP Offset (1=X,2=Y,3=Z,4=X*,5=Y*,6=Z*,7=bOut,8=bOn)", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%.3f %.3f %.3f %.3f %.3f %.3f", 670.0f, 400.0f, 245.0f, 7.0f, 10.0f, 2.0f );		// the constants at data 5216
		break;
	case 999:
		Q_strncpyz( label, "O3J VERSION", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%s | %s", "1016 (29-DEC-2003)", "OzoneJunkie@hotmail.com" );
		break;
	case 160: case 161: case 162: case 163: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsSavePosDist( item, label, value, sep, wf, pos );
		break;
	}
	case 50: case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73: case 74: case 75: case 76: case 77: case 79: case 300: case 301: case 302: case 303: case 304: case 305: case 306: case 307: case 308: case 310: case 311: case 312: case 313: case 314: case 315: case 316: case 317: {
		char wf[24];

		Com_sprintf( wf, sizeof( wf ), "%%%i.%if", 4 + dec + ( dec > 0 ? 1 : 0 ), dec );
		CG_DF_ChsOb( item, label, value, sep, wf );
		break;
	}
	case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108: case 109:
		CG_DF_ChsClock( item, label, value );
		break;
	case 114: case 115: case 116:
		CG_DF_ChsServer( item, label, value );
		break;
	case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127: case 128: case 129:
	case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137: case 138: case 139:
		CG_DF_ChsAccel( item, dec, label, value, sep, fmt );
		break;
	case 172:
		Q_strncpyz( label, "level start time", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", cgs.levelStartTime );
		break;
	case 111:
		Q_strncpyz( label, "ping", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", cg.scores[cg.snap->ps.clientNum].ping );
		break;
	case 117:
	case 118:
		Q_strncpyz( label, item == 117 ? "player name" : "player name (no color)", CHS_BUF );
		Q_strncpyz( value, cgs.clientinfo[cg.snap->ps.clientNum].name, CHS_BUF );
		if ( item == 118 ) {
			Q_CleanStr( value );
		}
		break;
	case 171:
		Q_strncpyz( label, "client game time", CHS_BUF );
		Com_sprintf( value, CHS_BUF, "%i", cg.time );
		break;
	}
}

// ---- "$variable" expansion (cgame 0x8a85 / 0x8887, named variables table at data 0x19f8) --------------------------------------------
static const struct { const char *name; int id; } chsVars[25] = {
	{ "date", 108 }, { "time", 109 }, { "mapname", 115 }, { "playername", 118 }, { "cgtime", 171 },
	{ "viewposX", 36 }, { "viewposY", 37 }, { "viewposZ", 38 }, { "viewposXYZ", 45 },
	{ "posX", 33 }, { "posY", 34 }, { "posZ", 35 }, { "posXYZ", 44 },
	{ "angX", 30 }, { "angY", 31 }, { "angZ", 32 }, { "angXYZ", 46 },
	{ "velX", 20 }, { "velY", 21 }, { "velZ", 22 }, { "velXYZ", 47 },
	{ "speedXY", 23 }, { "speedZ", 22 }, { "demoname", 190 }, { "fulldemoname", 191 }
};

// text between the first '(' and the following ')' ; returns its length
static int CG_DF_VarArg( const char *s, char *out, int size ) {
	const char *p = strchr( s, '(' ), *q;
	int n = 0;

	out[0] = 0;
	if ( !p ) {
		return 0;
	}
	for ( q = p + 1; *q && *q != ')' && n < size - 1; q++ ) {
		out[n++] = *q;
	}
	out[n] = 0;
	return n;
}

// cgame 0x7fd5: next/previous client slot with valid clientinfo, wrapping 0..63; skips the local client unless inclSelf, spectators
// (team >= 3) unless inclSpect, other teams when teamOnly; returns `start` when nothing qualifies
static int CG_DF_NextPlayerNum( int start, qboolean backward, qboolean inclSelf, qboolean inclSpect, qboolean teamOnly ) {
	int		n = start, i;

	if ( n < 0 ) n = 0;
	if ( n > 63 ) n = 63;
	for ( i = 0; i <= 64; i++ ) {
		n += backward ? -1 : 1;
		if ( n < 0 || n >= 64 ) {
			n = backward ? 63 : 0;
		}
		if ( n == start ) {
			break;
		}
		if ( !cgs.clientinfo[n].name[0] ) {
			continue;
		}
		if ( !inclSelf && n == cg.clientNum ) {
			continue;
		}
		if ( !inclSpect && cgs.clientinfo[n].team >= TEAM_SPECTATOR ) {
			continue;
		}
		if ( teamOnly && cgs.clientinfo[cg.clientNum].team != cgs.clientinfo[n].team ) {
			continue;
		}
		return n;
	}
	return start;
}

// tok: in = "$name" or plain text, out = the expansion.  Returns qtrue when the token is usable (plain text, or a non-empty expansion).
// Supported: $chsInfo(id), $chsLabel(id), the named CHS variables, $space, $empty, otherwise the cvar `name`.
// Also $Previous/NextPlayerNum and $Previous/NextTeamPlayerNum (cgame 0x89ac..).
// Not ported: $pstatNN / $bpstatNN (debug-password gated), user variables set by varMath & co.
qboolean CG_DF_ExpandVar( char *tok, int size ) {
	char	label[CHS_BUF], value[CHS_BUF], arg[CHS_BUF], name[CHS_BUF];
	int		i;

	if ( !tok[0] ) {
		return qfalse;
	}
	if ( tok[0] != '$' ) {
		return qtrue;
	}
	if ( strlen( tok ) <= 1 ) {
		return qfalse;
	}
	Q_strncpyz( name, tok + 1, sizeof( name ) );
	if ( !Q_stricmpn( name, "chsInfo", 7 ) || !Q_stricmpn( name, "chsLabel", 8 ) ) {
		CG_DF_VarArg( name, arg, sizeof( arg ) );
		CG_DF_ChsItem( atoi( arg ), label, value, "" );
		Q_strncpyz( tok, !Q_stricmpn( name, "chsInfo", 7 ) ? value : label, size );
		return tok[0] ? qtrue : qfalse;
	}
	for ( i = 0; i < 25; i++ ) {
		if ( !Q_stricmp( name, chsVars[i].name ) ) {
			CG_DF_ChsItem( chsVars[i].id + 3000, label, value, "" );
			Q_strncpyz( tok, value, size );
			return tok[0] ? qtrue : qfalse;
		}
	}
	{
		static const struct { const char *name; int backward, team; } pn[4] = {
			{ "PreviousPlayerNum", 1, 0 }, { "NextPlayerNum", 0, 0 }, { "PreviousTeamPlayerNum", 1, 1 }, { "NextTeamPlayerNum", 0, 1 }
		};

		for ( i = 0; i < 4; i++ ) {
			if ( !Q_stricmp( name, pn[i].name ) ) {
				// start = 0x120e44 (tracked client; assumed = viewed client, writer not decoded)
				Com_sprintf( tok, size, "%i", CG_DF_NextPlayerNum( cg.snap ? cg.snap->ps.clientNum : cg.clientNum, pn[i].backward, qfalse, qfalse, pn[i].team ) );
				return qtrue;
			}
		}
	}
	if ( !Q_stricmp( name, "space" ) ) {
		Q_strncpyz( tok, " ", size );
		return qtrue;
	}
	if ( !Q_stricmp( name, "empty" ) ) {
		tok[0] = 0;
		return qfalse;
	}
	trap_Cvar_VariableStringBuffer( name, tok, size );
	return tok[0] ? qtrue : qfalse;
}

static void CG_DF_ChsDrawText( float *col, int x, int y, const char *s, int size, int shadow, int total ) {
	if ( s[0] ) {
		CG_DrawStringExt( x, y, s, col, qfalse, shadow, size, size, total );
	}
}


// ---- CHS panel 0 (cgame 0xfde7): movement-key display around the crosshair ------------------------------------------------------------
// DisplayType: 0 = diamond (F top, B bottom, L left, R right) with arrow glyphs, 3 = same with the letters F B L R, 5 = same with the
// first letter of the key bound to +forward/+back/+moveleft/+moveright; 2 / 4 / 6 = keyboard layout (F above B, L B R in a row, J _ C
// below) with arrow glyphs / letters / bound keys; 1 = "/\", "\/", "<", ">" drawn with slash characters.  J = jump, C = crouch, _ = walk.
// Pressed keys use df_chs0_FontColor, released ones df_chs0_LabelColor (only drawn when the two colours differ).
static void CG_DF_Chs0Key( int mask, float x, float y, const char *s, const float *on, const float *off, int differ, int shadow, int size ) {
	if ( CG_DF_Keys( mask ) ) {
		CG_DrawStringExt( (int)x, (int)y, s, on, qfalse, shadow, size, size, 1 );
	} else if ( differ ) {
		CG_DrawStringExt( (int)x, (int)y, s, off, qfalse, shadow, size, size, 1 );
	}
}

static void CG_DF_BoundKeyChar( char *out, const char *cmd ) {
	int key = trap_Key_GetKey( cmd );

	out[0] = key;
	out[1] = 0;
	Q_strupr( out );
}

void CG_DF_DrawChs0( void ) {
	dfChsCvars_t	*c = &dfChs[0];
	int		size = c->fontSize.integer, shadow, type = c->displayType.integer, differ, ox = c->offsetX.integer, oy = c->offsetY.integer;
	float	alpha = c->fontTransparency.value, cx, cy, x, y;
	vec4_t	on, off;
	char	kF[4] = "F", kB[4] = "B", kL[4] = "L", kR[4] = "R", kJ[4] = "J", kC[4] = "C", kA[4] = "_";
	int		ci;

	if ( !cg.snap || !CG_DF_DrawAllowed( c->draw.integer ) ) {
		return;
	}
	if ( size < 4 ) size = 4;
	if ( size > 50 ) size = 50;
	shadow = ( c->fontShadow.integer == 0 || c->fontShadow.integer == 1 ) ? c->fontShadow.integer : 1;
	if ( alpha < 0 || alpha > 1.0f ) {
		alpha = 1.0f;
	}
	if ( ox > 320 ) ox = 320;
	if ( ox < -320 ) ox = -320;
	if ( oy > 240 ) oy = 240;
	if ( oy < -240 ) oy = -240;
	differ = c->fontColor.integer != c->labelColor.integer;
	ci = CG_DF_ChsColorIndex( c->fontColor.integer );
	VectorCopy( g_color_table[ci], on );
	on[3] = alpha;
	ci = CG_DF_ChsColorIndex( c->labelColor.integer );
	VectorCopy( g_color_table[ci], off );
	off[3] = alpha;
	cx = 320 + ox - size / 2;
	cy = 240 + oy - size / 2;

	if ( type == 5 || type == 6 ) {
		CG_DF_BoundKeyChar( kF, "+forward" );
		CG_DF_BoundKeyChar( kB, "+back" );
		CG_DF_BoundKeyChar( kL, "+moveleft" );
		CG_DF_BoundKeyChar( kR, "+moveright" );
	}
	if ( type < 0 || type > 6 ) {
		return;
	}
	if ( type == 1 ) {
		float q = size / 2 / 2;

		// forward "/\"
		CG_DF_Chs0Key( 1, cx - q, cy - ( size + 1 ), "/", on, off, differ, shadow, size );
		CG_DF_Chs0Key( 1, cx + q + 1, cy - ( size + 1 ), "\\", on, off, differ, shadow, size );
		// back "\/"
		CG_DF_Chs0Key( 2, cx - q, cy + size, "\\", on, off, differ, shadow, size );
		CG_DF_Chs0Key( 2, cx + q + 1, cy + size, "/", on, off, differ, shadow, size );
		// left "<"
		CG_DF_Chs0Key( 8, cx - size, cy - ( q + 1 ), "/", on, off, differ, shadow, size );
		CG_DF_Chs0Key( 8, cx - size, cy + q, "\\", on, off, differ, shadow, size );
		// right ">"
		CG_DF_Chs0Key( 16, cx + ( size + 1 ), cy - ( q + 1 ), "\\", on, off, differ, shadow, size );
		CG_DF_Chs0Key( 16, cx + ( size + 1 ), cy + q, "/", on, off, differ, shadow, size );
		CG_DF_Chs0Key( 512, cx, cy, kA, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 32, cx, cy, kJ, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 64, cx, cy, kC, on, off, differ, shadow, size );
		return;
	}
	if ( type == 0 || type == 2 ) {		// arrow glyphs of the bigchars font
		Q_strncpyz( kF, "", sizeof( kF ) );		// bigchars arrow glyphs 135 / 134 / 136 / 141
		Q_strncpyz( kB, "", sizeof( kB ) );
		Q_strncpyz( kL, "", sizeof( kL ) );
		Q_strncpyz( kR, "", sizeof( kR ) );
	}
	x = cx;
	y = cy;
	if ( type == 0 || type == 3 || type == 5 ) {
		CG_DF_Chs0Key( 1, x, y - size, kF, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 2, x, y + size, kB, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 8, x - size, y, kL, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 16, x + size, y, kR, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 512, x, y, kA, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 32, x, y, kJ, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 64, x, y, kC, on, off, differ, shadow, size );
	} else {
		CG_DF_Chs0Key( 1, x, y - size, kF, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 2, x, y, kB, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 8, x - size, y, kL, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 16, x + size, y, kR, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 32, x - 2 * size, y + size, kJ, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 512, x, y + size, kA, on, off, differ, shadow, size );
		CG_DF_Chs0Key( 64, x + 2 * size, y + size, kC, on, off, differ, shadow, size );
	}
}

void CG_DF_DrawChs( int p ) {
	dfChsCvars_t	*c = &dfChs[p];
	chsRow_t		rows[8];
	char			label[CHS_BUF], value[CHS_BUF], extra[CHS_BUF];
	char			sep[8] = "";
	int				k;

	if ( p == 0 ) {
		CG_DF_DrawChs0();
		return;
	}
	if ( !cg.snap || p < 1 || p > 2 || !CG_DF_DrawAllowed( c->draw.integer ) ) {
		return;
	}
	if ( strlen( c->comboSeparator.string ) < 6 ) {
		Q_strncpyz( sep, c->comboSeparator.string, sizeof( sep ) );
	}
	CG_DF_ChsLayout( p, rows );

	for ( k = 0; k < 8; k++ ) {
		int		lt = c->labelType.integer, total, x, y;
		chsRow_t *r = &rows[k];

		label[0] = value[0] = extra[0] = 0;
		if ( !c->enableCombos.integer ) {
			if ( !r->id ) {
				continue;
			}
			CG_DF_ChsItem( abs( r->id ), label, value, sep );
			if ( r->id < 0 ) {
				lt = 0;
			}
		} else {
			const char	*s = c->info[k].string;
			const char	*tok;
			int			n = 0;

			if ( !s[0] ) {
				continue;
			}
			while ( n < CHS_MAX_COMBO ) {
				char	t1[CHS_BUF], t2[CHS_BUF], l[CHS_BUF], v[CHS_BUF], e[CHS_BUF];

				tok = COM_Parse( &s );
				if ( !tok[0] ) {
					break;
				}
				Q_strncpyz( t1, tok, sizeof( t1 ) );
				Q_strncpyz( t2, tok, sizeof( t2 ) );
				if ( t1[0] == '-' ) {
					Q_strncpyz( t2, t1 + 1, sizeof( t2 ) );
					Q_strncpyz( t1, t2, sizeof( t1 ) );
					lt = 0;
				}
				CG_DF_ExpandVar( t2, sizeof( t2 ) );
				if ( strcmp( t1, t2 ) != 0 ) {
					if ( !t2[0] ) {
						continue;
					}
					Q_strncpyz( l, t1, sizeof( l ) );
					Q_strncpyz( v, t2, sizeof( v ) );
				} else {
					int item = abs( atoi( t1 ) );

					if ( !item ) {
						continue;
					}
					CG_DF_ChsItem( item, l, v, sep );
				}
				e[0] = 0;
				if ( n == 0 ) {
					Q_strncpyz( label, l, sizeof( label ) );
					Q_strncpyz( value, v, sizeof( value ) );
				} else {
					if ( strlen( label ) + strlen( l ) + strlen( sep ) + 1 < CHS_BUF ) {
						Q_strcat( label, sizeof( label ), va( "%s%s", sep, l ) );
					}
					if ( strlen( value ) + strlen( v ) + strlen( sep ) + 1 < CHS_BUF ) {
						Q_strcat( value, sizeof( value ), va( "%s%s", sep, v ) );
					}
				}
				n++;
			}
		}
		if ( !value[0] && !label[0] ) {
			continue;
		}

		y = r->y;
		{
			const char *colon = ": ", *colonL = " :";
			int ll = strlen( label ), lv = strlen( value );

			switch ( lt ) {
			default:
			case 0:
				total = lv;
				x = CG_DF_ChsAlignX( r->align, r->x, r->size, total );
				CG_DF_ChsDrawText( r->fontCol, x, y, value, r->size, r->shadow, total );
				break;
			case 1:
				total = ll + 2 + lv;
				x = CG_DF_ChsAlignX( r->align, r->x, r->size, total );
				CG_DF_ChsDrawText( r->labelCol, x, y, label, r->size, r->shadow, total );
				x += ll * r->size;
				CG_DF_ChsDrawText( r->labelCol, x, y, colon, r->size, r->shadow, total );
				x += 2 * r->size;
				CG_DF_ChsDrawText( r->fontCol, x, y, value, r->size, r->shadow, total );
				break;
			case 2:
			case 3:
			case 4: {
				// value :label  for the rows of the "value first" half (type 2: rows 0-3, type 3: rows 4-7, type 4: all), else label: value
				qboolean valueFirst = ( lt == 4 ) || ( lt == 2 && k < 4 ) || ( lt == 3 && k >= 4 );

				total = ll + 2 + lv;
				x = CG_DF_ChsAlignX( r->align, r->x, r->size, total );
				if ( valueFirst ) {
					CG_DF_ChsDrawText( r->fontCol, x, y, value, r->size, r->shadow, total );
					x += lv * r->size;
					CG_DF_ChsDrawText( r->labelCol, x, y, colonL, r->size, r->shadow, total );
					x += 2 * r->size;
					CG_DF_ChsDrawText( r->labelCol, x, y, label, r->size, r->shadow, total );
				} else {
					CG_DF_ChsDrawText( r->labelCol, x, y, label, r->size, r->shadow, total );
					x += ll * r->size;
					CG_DF_ChsDrawText( r->labelCol, x, y, colon, r->size, r->shadow, total );
					x += 2 * r->size;
					CG_DF_ChsDrawText( r->fontCol, x, y, value, r->size, r->shadow, total );
				}
				break;
			}
			}
		}
	}
}

// DeFRaG 0xb0ad: log line of a var* change, selected by df_varHelpers_DrawOutput (0 off; 1-4 "[ name ] Old Value = x New Value = y",
// 5-8 "name --> y"; 1/5 white, 2/6 df_chs0, 3/7 df_chs1, 4/8 df_chs2 colours: name = FontColor, values = LabelColor)
vmCvar_t	df_varHelpers_DrawOutput;

static void CG_DF_VarSetLogged( const char *name, const char *newVal ) {
	char	oldVal[MAX_STRING_CHARS];
	int		mode = df_varHelpers_DrawOutput.integer;
	int		nameCol = 7, valCol = 7;

	trap_Cvar_VariableStringBuffer( name, oldVal, sizeof( oldVal ) );
	trap_Cvar_Set( name, newVal );
	if ( mode < 1 || mode > 8 ) {
		return;
	}
	if ( ( mode - 1 ) % 4 ) {
		int c = ( mode - 1 ) % 4 - 1;	// df_chs0..2

		nameCol = dfChs[c].fontColor.integer;
		valCol = dfChs[c].labelColor.integer;
	}
	if ( mode <= 4 ) {
		CG_Printf( "^7[ ^%i%s ^7] Old Value = ^%i%s ^7New Value = ^%i%s\n", nameCol, name, valCol, oldVal, valCol, newVal );
	} else {
		CG_Printf( "^%i%s ^4--> ^%i%s\n", nameCol, name, valCol, newVal );
	}
}

// ---- varMath (cgame 0xa4cc): "varMath <var> <op> <value> [<op> <value> ...]" evaluated left to right, operators + - * / % = ----
// Numbers keep integer or float flavour (a '.' in either operand selects "%.4f" output); strings concatenate with + ("<s>" = a space,
// "<0>" = empty); "=" assigns.  Arguments may use $variables.  The result is written with trap_Cvar_Set (DeFRaG's 0x80bf also logs
// "ISSUING ... VAR CHANGE" lines and routes protected names through `set`).
static qboolean CG_DF_VarNonNumeric( const char *s ) {
	for ( ; *s; s++ ) {
		if ( ( *s >= 'a' && *s <= 'z' ) || ( *s >= 'A' && *s <= 'Z' ) || *s == ' ' ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean CG_DF_VarHasDot( const char *s ) {
	return strchr( s, '.' ) ? qtrue : qfalse;
}

void CG_DF_VarMath_f( void ) {
	char	target[1024], cur[1024], acc[1024], res[1024], op[1024], val[1024];
	int		i, first = 1, accNon, valNon, accDot, valDot, i1, i2, r44 = 0, valid;
	float	f1, f2, r48, r4c, r50;

	trap_Argv( 1, target, sizeof( target ) );
	CG_DF_ExpandVar( target, sizeof( target ) );
	trap_Cvar_VariableStringBuffer( target, cur, sizeof( cur ) );
	res[0] = 0;
	if ( !target[0] || !strcmp( target, "?" ) ) {
		CG_Printf( "\n" );
		CG_Printf( "^1varMath: ^3Performs math functions on cvars and vstrs.\n" );
		CG_Printf( "^5Usage:   ^2varMath targetVariableName operator: +-*/%%= value (+-*/%%= value)\n" );
		CG_Printf( "^5Example: ^2varMath timescale * .2\n" );
		CG_Printf( "^5Example: ^2varMath user_num = 1 + 3 * 5\n" );
		CG_Printf( "^5Example: ^2varMath user_demoname = $mapname + _ + $date + _ + 001\n" );
		CG_Printf( "\n" );
		CG_Printf( "^3Note:    ^2for immediate variable update, prefix the targetVariableName with \"user_\"\n" );
		CG_Printf( "\n" );
		return;
	}
	for ( i = 2; i < 25 + 2; i += 2 ) {
		trap_Argv( i, op, sizeof( op ) );
		trap_Argv( i + 1, val, sizeof( val ) );
		CG_DF_ExpandVar( op, sizeof( op ) );
		CG_DF_ExpandVar( val, sizeof( val ) );
		if ( first ) {
			if ( !op[0] ) {
				CG_Printf( "*** Error - missing operator\n" );
				return;
			}
			if ( !val[0] && strcmp( op, "=" ) ) {
				CG_Printf( "*** Error - missing value\n" );
				return;
			}
			Q_strncpyz( acc, cur, sizeof( acc ) );
			first = 0;
		} else {
			if ( !op[0] || !val[0] ) {
				break;
			}
			Q_strncpyz( acc, res, sizeof( acc ) );
		}
		f1 = atof( acc ); f2 = atof( val ); i1 = atoi( acc ); i2 = atoi( val );
		accNon = CG_DF_VarNonNumeric( acc );
		accDot = CG_DF_VarHasDot( acc );
		valNon = CG_DF_VarNonNumeric( val ) || !val[0] || !strcmp( val, "<0>" );
		valDot = CG_DF_VarHasDot( val );

		if ( accNon || valNon ) {
			if ( !strcmp( op, "+" ) ) {
				if ( !strcmp( val, "<s>" ) ) {
					Com_sprintf( res, sizeof( res ), "%s ", acc );
				} else {
					Com_sprintf( res, sizeof( res ), "%s%s", acc, val );
				}
				continue;
			}
			if ( !strcmp( op, "=" ) ) {
				if ( !strcmp( val, "<s>" ) ) {
					Q_strncpyz( res, " ", sizeof( res ) );
				} else if ( !strcmp( val, "<0>" ) ) {
					res[0] = 0;
				} else {
					Q_strncpyz( res, val, sizeof( res ) );
				}
				continue;
			}
		}
		valid = 0;
		r48 = r4c = r50 = 0;
		if ( !strcmp( op, "+" ) ) {
			r48 = f1 + f2; r4c = f1 + i2; r50 = i1 + f2; r44 = i1 + i2; valid = 1;
		}
		if ( !strcmp( op, "-" ) ) {
			r48 = f1 - f2; r4c = f1 - i2; r50 = i1 - f2; r44 = i1 - i2; valid = 1;
		}
		if ( !strcmp( op, "*" ) ) {
			r48 = f1 * f2; r4c = f1 * i2; r50 = i1 * f2; r44 = i1 * i2; valid = 1;
		}
		if ( !strcmp( op, "/" ) && f2 != 0 && i2 != 0 ) {
			r48 = f1 / f2; r4c = f1 / i2; r50 = i1 / f2; r44 = i1 / i2; valid = 1;
		}
		if ( !strcmp( op, "%" ) && i2 != 0 ) {
			r44 = i1 % i2; valid = 1;
		}
		if ( !strcmp( op, "=" ) ) {
			r48 = f2; r4c = i2; r50 = f2; r44 = i2; accDot = 0; valid = 1;
		}
		if ( !valid ) {
			CG_Printf( "*** Error - invalid operator: %s\n", op );
			return;
		}
		if ( accDot && valDot ) {
			Com_sprintf( res, sizeof( res ), "%.4f", r48 );
		} else if ( accDot && !valDot ) {
			Com_sprintf( res, sizeof( res ), "%.4f", r4c );
		} else if ( !accDot && valDot ) {
			Com_sprintf( res, sizeof( res ), "%.4f", r50 );
		} else {
			Com_sprintf( res, sizeof( res ), "%i", r44 );
		}
	}
	CG_DF_VarSetLogged( target, res );
}

// ---- varCommand / varCycle / varCycleOnce / varJoin / varFormat / varSave (cgame 0xa42d / 0xa897 / 0xa9bb / 0xaaab / 0xafda) ----
static qboolean CG_DF_VarUsage( const char *arg1 ) {
	return ( !arg1[0] || !strcmp( arg1, "?" ) ) ? qtrue : qfalse;
}

void CG_DF_VarCommand_f( void ) {
	char	cmd[1024], arg[1024];
	int		i;

	trap_Argv( 1, arg, sizeof( arg ) );
	CG_DF_ExpandVar( arg, sizeof( arg ) );
	Q_strncpyz( cmd, arg, sizeof( cmd ) );
	if ( CG_DF_VarUsage( cmd ) ) {
		CG_Printf( "\n^1varCommand: ^3Does a $StringSubstitution, then issues the specified command.\n^5Usage:      ^2varCommand command <...>\n^5Example:    ^2varCommand record $user_demoname\n\n" );
		return;
	}
	for ( i = 2; ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		CG_DF_ExpandVar( arg, sizeof( arg ) );
		if ( !arg[0] ) {
			break;
		}
		Com_sprintf( cmd, sizeof( cmd ), "%s %s", cmd, arg );
	}
	if ( cmd[0] ) {
		Com_sprintf( cmd, sizeof( cmd ), "%s\n", cmd );
		CG_Printf( "^3Issuing Command:^7 %s\n", cmd );
		trap_SendConsoleCommand( cmd );
	}
}

// varCycle <var> <default> <v1> <v2> ... : sets var to the value after its current one; unknown current -> default.
// wrap: varCycle wraps from the last value to v1, varCycleOnce leaves var unchanged at the end of the list.
static void CG_DF_VarCycle( qboolean wrap ) {
	char	name[1024], cur[1024], def[1024], arg[1024], next[1024], out[1024];
	int		i, found = 0;

	trap_Argv( 1, name, sizeof( name ) );
	trap_Cvar_VariableStringBuffer( name, cur, sizeof( cur ) );
	if ( CG_DF_VarUsage( name ) ) {
		CG_Printf( "\n^1varCycle: ^3Cycles through values and sets cvars and vstrs to new value.\n^5Usage:    ^2varCycle targetVariableName defaultValue <value1> <value2> <value3>...\n^5Example:  ^2varCycle com_maxfps 125 111 125 142\n\n^3Note:     ^2for immediate variable update, prefix the targetVariableName with \"user_\"\n\n" );
		return;
	}
	trap_Argv( 2, def, sizeof( def ) );
	CG_DF_ExpandVar( def, sizeof( def ) );
	if ( !def[0] ) {
		CG_Printf( "*** Error - missing default value\n" );
		return;
	}
	Q_strncpyz( out, def, sizeof( out ) );
	for ( i = 3; i < 25 + 3; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		CG_DF_ExpandVar( arg, sizeof( arg ) );
		if ( !strcmp( cur, arg ) && arg[0] ) {
			trap_Argv( i + 1, next, sizeof( next ) );
			CG_DF_ExpandVar( next, sizeof( next ) );
			if ( !next[0] ) {
				if ( !wrap ) {
					return;
				}
				trap_Argv( 3, next, sizeof( next ) );
				CG_DF_ExpandVar( next, sizeof( next ) );
			}
			Q_strncpyz( out, next, sizeof( out ) );
			found = 1;
			break;
		}
	}
	(void)found;
	CG_DF_VarSetLogged( name, out );
}
void CG_DF_VarCycle_f( void ) { CG_DF_VarCycle( qtrue ); }
void CG_DF_VarCycleOnce_f( void ) { CG_DF_VarCycle( qfalse ); }

// varJoin <var> <s1> <s2> ... : appends the strings to the current value; "<s>" = a space, "<0>" clears the string.
void CG_DF_VarJoin_f( void ) {
	char	name[1024], acc[1024], arg[1024];
	int		i;

	trap_Argv( 1, name, sizeof( name ) );
	if ( CG_DF_VarUsage( name ) ) {
		CG_Printf( "\n^1varJoin: ^3Joins up to %i vstrs.\n^1varJoin: ^3Use %s to indicate a space.  Use %s to clear a string.\n^5Usage:   ^2varJoin targetVariableName string1 string2 string3\n^5Example: ^2varJoin user_string Hello %s There\n^5Example: ^2varJoin user_demoname %s $mapname _ 001\n\n^3Note:    ^2for immediate variable update, prefix the targetVariableName with \"user_\"\n\n",
		           25, "<s>", "<0>", "<s>", "<0>" );
		return;
	}
	trap_Cvar_VariableStringBuffer( name, acc, sizeof( acc ) );
	for ( i = 2; i < 25 + 2; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		CG_DF_ExpandVar( arg, sizeof( arg ) );
		if ( !arg[0] ) {
			break;
		}
		if ( !strcmp( arg, "<0>" ) ) {
			acc[0] = 0;
		} else {
			if ( !strcmp( arg, "<s>" ) ) {
				Q_strncpyz( arg, " ", sizeof( arg ) );
			}
			Com_sprintf( acc, sizeof( acc ), "%s%s", acc, arg );
		}
	}
	CG_DF_VarSetLogged( name, acc );
}

// varFormat <var> <spec1: s|i|f> <value1> [<spec2> <value2>] : printf-style formatting, e.g. varFormat user_f 2.2f 987.654
void CG_DF_VarFormat_f( void ) {
	char	name[1024], v1[1024], s1[1024], v2[1024], s2[1024], fmt[1024], out[1024];
	int		t1 = 0, t2 = 0, i1, i2;
	float	f1, f2;
	const char *p;

	trap_Argv( 1, name, sizeof( name ) );
	trap_Argv( 2, s1, sizeof( s1 ) );
	trap_Argv( 3, v1, sizeof( v1 ) );
	trap_Argv( 4, s2, sizeof( s2 ) );
	trap_Argv( 5, v2, sizeof( v2 ) );
	if ( CG_DF_VarUsage( name ) ) {
		CG_Printf( "\n^1varFormat: ^3Performs string formatting on cvars and vstrs.\n^5Usage:     ^2varFormat targetVariableName formatstring: s|i|f varValue <variableName formatstring: s|i|f varValue>\n^5Example:   ^2varFormat user_MyFloat 2.2f 987.654\n\n^3Note:      ^2for immediate variable update, prefix the targetVariableName with \"user_\"\n\n" );
		return;
	}
	CG_DF_ExpandVar( v1, sizeof( v1 ) );
	CG_DF_ExpandVar( v2, sizeof( v2 ) );
	CG_DF_ExpandVar( s1, sizeof( s1 ) );
	CG_DF_ExpandVar( s2, sizeof( s2 ) );
	i1 = atoi( v1 ); i2 = atoi( v2 ); f1 = atof( v1 ); f2 = atof( v2 );
	if ( s2[0] ) {
		Com_sprintf( fmt, sizeof( fmt ), "%%%s%%%s", s1, s2 );
	} else {
		Com_sprintf( fmt, sizeof( fmt ), "%%%s", s1 );
	}
	for ( p = s1; *p; p++ ) {
		if ( *p == 's' ) { t1 = 1; break; }
		if ( *p == 'i' ) { t1 = 2; break; }
		if ( *p == 'f' ) { t1 = 3; break; }
	}
	for ( p = s2; *p; p++ ) {
		if ( *p == 's' ) { t2 = 1; break; }
		if ( *p == 'i' ) { t2 = 2; break; }
		if ( *p == 'f' ) { t2 = 3; break; }
	}
	out[0] = 0;
	if ( t2 == 0 ) {
		if ( t1 == 1 ) Com_sprintf( out, sizeof( out ), fmt, v1 );
		if ( t1 == 2 ) Com_sprintf( out, sizeof( out ), fmt, i1 );
		if ( t1 == 3 ) Com_sprintf( out, sizeof( out ), fmt, f1 );
	} else {
		if ( t1 == 1 && t2 == 1 ) Com_sprintf( out, sizeof( out ), fmt, v1, v2 );
		if ( t1 == 2 && t2 == 2 ) Com_sprintf( out, sizeof( out ), fmt, i1, i2 );
		if ( t1 == 3 && t2 == 3 ) Com_sprintf( out, sizeof( out ), fmt, f1, f2 );
		if ( t1 == 1 && t2 == 2 ) Com_sprintf( out, sizeof( out ), fmt, v1, i2 );
		if ( t1 == 2 && t2 == 1 ) Com_sprintf( out, sizeof( out ), fmt, i1, v2 );
		if ( t1 == 1 && t2 == 3 ) Com_sprintf( out, sizeof( out ), fmt, v1, f2 );
		if ( t1 == 3 && t2 == 1 ) Com_sprintf( out, sizeof( out ), fmt, f1, v2 );
		if ( t1 == 2 && t2 == 3 ) Com_sprintf( out, sizeof( out ), fmt, i1, f2 );
		if ( t1 == 3 && t2 == 2 ) Com_sprintf( out, sizeof( out ), fmt, f1, i2 );
	}
	CG_DF_VarSetLogged( name, out );
}

// varSave <var> <file> : appends  set <var> "<value>"  to the file
void CG_DF_VarSave_f( void ) {
	char			name[1024], file[1024], val[1024], line[1024];
	fileHandle_t	f;
	int				len;

	trap_Argv( 1, name, sizeof( name ) );
	trap_Argv( 2, file, sizeof( file ) );
	if ( CG_DF_VarUsage( name ) ) {
		CG_Printf( "\n^1VarSave: ^3Saves a vstr to a file.\n^5Usage:   ^2varSave vstrname filename\n^5Example: ^2varSave pos1 mysavepositions.cfg\n\n" );
		return;
	}
	if ( !name[0] ) {
		CG_Printf( "*** Error - missing vstr/varname\n" );
		return;
	}
	if ( !file[0] ) {
		CG_Printf( "*** Error - missing filename\n" );
		return;
	}
	CG_DF_ExpandVar( name, sizeof( name ) );
	CG_DF_ExpandVar( file, sizeof( file ) );
	trap_Cvar_VariableStringBuffer( name, val, sizeof( val ) );
	if ( !val[0] ) {
		CG_Printf( "*** Error - vstr/var %s is empty\n", name );
		return;
	}
	len = trap_FS_FOpenFile( file, &f, FS_APPEND );
	if ( !f ) {
		CG_Printf( "*** Error: could not open file (%s)\n", file );
		return;
	}
	Com_sprintf( line, sizeof( line ), "set %s \"%s\"\n", name, val );
	trap_FS_Write( line, strlen( line ), f );
	trap_FS_FCloseFile( f );
	(void)len;
	CG_Printf( "^3FILE ^2(%s)^3 Appended Var:^7\n%s\n", file, line );
}

// ---- help / commands / ? (cgame 0xa30b), read (0xad01), df_ndm_timer_aliases (0x415a) ------------------------------------------------

#define DF_SEP "\n^1==================================================\n"

// help [topic]: without argument the command overview; with one it runs  read system/help/<topic>.txt
void CG_DF_Help_f( void ) {
	char	arg[1024];

	trap_Argv( 1, arg, sizeof( arg ) );
	if ( arg[0] ) {
		trap_SendConsoleCommand( va( "read system/help/%s.txt", arg ) );
		return;
	}
	CG_Printf( "\n" );
	CG_Printf( "^1Available ^1Defrag ^1Game-Commands:\n" );
	CG_Printf( "^1..........^1.......^1..............\n" );
	CG_Printf( "^3help/commands/?                                        \n" );
	CG_Printf( "^3varCommand       info            arena                 \n" );
	CG_Printf( "^3varCycle         read            placeplayer           \n" );
	CG_Printf( "^3varFormat        ghost           respawnpoint          \n" );
	CG_Printf( "^3varJoin          replay          firemissile           \n" );
	CG_Printf( "^3varMath          savepos         stats                 \n" );
	CG_Printf( "^3varSave          displaymessage  svinfo_report         \n" );
	CG_Printf( "^3                 resetjumpmeter  svinfo_summary        \n" );
	CG_Printf( "^3                                 df_ndm_timer_aliases  \n" );
	CG_Printf( "\n" );
	CG_Printf( "Type: ^3\\^2command_name ^3?    ^7for more information on a command\n" );
	CG_Printf( "Type: ^3\\help ^2command_name ^7for extended information on a command\n" );
	CG_Printf( "\n" );
	CG_Printf( "Type: ^3\\help ^2cvar_name ^7for cvar help\n" );
	CG_Printf( "\n" );
	CG_Printf( "Type: ^3\\help cvars ^7for full cvars list\n" );
	CG_Printf( "Type: ^3\\help vstrs ^7for special vstrs list\n" );
	CG_Printf( "\n" );
	CG_Printf( "^2*** Note: you may substitute ^3help ^2with ^3?\n" );
	CG_Printf( "\n" );
	CG_Printf( "^1Defrag ^1for Quake3 ^1v%s\n", va( "%i.%02i.%02i", 1, 91, 31 ) );
	CG_Printf( "^1http://www.planetquake.com/defrag\n" );
	CG_Printf( "\n" );
}

// read <file>: prints a text file, trying  <file>, system/docs/<file>.txt, system/docs/<file>, <file>.txt  (at most 10 chunks of 1023 bytes)
void CG_DF_Read_f( void ) {
	char			arg[128], name[1024], chunk[1024];
	fileHandle_t	f = 0;
	int				len = 0, n, i;

	trap_Argv( 1, arg, sizeof( arg ) );
	if ( !strcmp( arg, "?" ) ) {
		CG_Printf( "\n^1read:    ^3Reads a text file.\n^5Usage:   ^2read <foldername/>filename<extension>\n^5Example: ^2read myrecords.txt\n\n" );
		return;
	}
	if ( !arg[0] ) {
		trap_SendConsoleCommand( "dir docs\n" );
		return;
	}
	Q_strncpyz( name, arg, sizeof( name ) );
	len = trap_FS_FOpenFile( name, &f, FS_READ );
	if ( !f ) {
		Com_sprintf( name, sizeof( name ), "system/docs/%s.%s", arg, "txt" );
		len = trap_FS_FOpenFile( name, &f, FS_READ );
	}
	if ( !f ) {
		Com_sprintf( name, sizeof( name ), "system/docs/%s", arg );
		len = trap_FS_FOpenFile( name, &f, FS_READ );
	}
	if ( !f ) {
		Com_sprintf( name, sizeof( name ), "%s.%s", arg, "txt" );
		len = trap_FS_FOpenFile( name, &f, FS_READ );
	}
	if ( !f ) {
		CG_Printf( "*** Error: file [ %s ] not found\n", arg );
		return;
	}
	CG_Printf( DF_SEP );
	CG_Printf( "^2File:^3 %s", name );
	CG_Printf( DF_SEP );
	CG_Printf( "\n" );
	for ( i = 1; len > 0 && i < 10; i++ ) {
		n = len < 1023 ? len : 1023;
		memset( chunk, 0, sizeof( chunk ) );
		trap_FS_Read( chunk, n, f );
		CG_Printf( "%s", chunk );
		len -= n;
		if ( !n || !chunk[0] ) {
			break;
		}
	}
	trap_FS_FCloseFile( f );
	CG_Printf( "\n" );
	CG_Printf( DF_SEP );
	CG_Printf( "\n" );
}

void CG_DF_TimerAliases_f( void ) {
	CG_Printf( "Timer aliases:\n" );
	CG_Printf( " Defrag Triggers:   df\n" );
	CG_Printf( " Player respawn:    resp\n" );
	CG_Printf( " Items - Weapons:   gt mg sg gl rl lg rg pg bfg hook\n" );
	CG_Printf( " Items - Ammo:      amg asg agl arl alg arg apg abfg\n" );
	CG_Printf( " Items - Health:    h5 h25 h50 mh\n" );
	CG_Printf( " Items - Armor:     as ya ra\n" );
	CG_Printf( " Items - Powerups:  quad bs hast inv reg fli\n" );
	CG_Printf( " Items - Holdables: tp med\n" );
}

// ---- info (cgame 0xba1d): console lookup through the CHS item system ----------------------------------------------------------------
// info            list every CHS item (3 decimals)       info <n>        one item        info $name   expand a string substitution
// info Players    client list                            info StringSubs the substitution list      info ?  usage
static void CG_DF_InfoPrintItem( int id ) {
	char label[CHS_BUF], value[CHS_BUF];

	CG_DF_ChsItem( id + 3000, label, value, "/" );
	if ( label[0] ) {
		CG_Printf( "^3[%03i] %s : ^5%s\n", id, label, value );
	}
}

static void CG_DF_InfoPlayers( void ) {
	int		i;

	CG_Printf( "\n^1Players Information\n^2--------------------------------------------------\n^2ID | TEAM NAME\n^2--------------------------------------------------\n" );
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		char	clean[MAX_QPATH];
		const char *team;

		if ( !cgs.clientinfo[i].name[0] ) {
			break;
		}
		switch ( cgs.clientinfo[i].team ) {
		case TEAM_FREE:  team = "----"; break;
		case TEAM_RED:   team = "^1 RED^7"; break;
		case TEAM_BLUE:  team = "^4BLUE^7"; break;
		case TEAM_SPECTATOR: team = "SPEC"; break;
		default: team = "    "; break;
		}
		Q_strncpyz( clean, cgs.clientinfo[i].name, sizeof( clean ) );
		Q_CleanStr( clean );
		CG_Printf( "^3%02i^7 ^2|^7 %s %s\n", i, team, clean );
	}
	CG_Printf( "^2--------------------------------------------------\n\n" );
}

static void CG_DF_InfoStringSubs( void ) {
	int		i;
	char	label[CHS_BUF], value[CHS_BUF];

	CG_Printf( "\n^1Crosshair Stats String Substitutions List\n^2--------------------------------------------------\n^2CHSNUM |  STRINGNAME  | VALUE\n^2--------------------------------------------------\n" );
	for ( i = 0; i < 25; i++ ) {
		CG_DF_ChsItem( chsVars[i].id + 3000, label, value, "/" );
		CG_Printf( "   ^3%03i^7 ^2|^7 ^3%12s ^2|^5 %s\n", chsVars[i].id, chsVars[i].name, value );
	}
	CG_Printf( "^2--------------------------------------------------\n\n^1Other String Substitutions List\n^2--------------------------------------------------\n" );
	CG_Printf( "^3space\n^3empty\n^3PreviousPlayerNum      NextPlayerNum\n^3PreviousTeamPlayerNum  NextTeamPlayerNum\n^3chsinfo(#)             chslabel(#)\n^2--------------------------------------------------\n\n" );
	CG_Printf( "^5Usage:   ^2Use $ to indicate a string substitution.\n^5Example: ^2varCommand say My name is $playername\n^5Example: ^2varCommand say My speed is $chsinfo(23)\n\nType: ^3\\help vstrs ^7for complete special vstrs list\n\n" );
}

void CG_DF_Info_f( void ) {
	char	arg[1024], exp[1024];
	int		num, i, unknown = 0;

	trap_Argv( 1, arg, sizeof( arg ) );
	num = atoi( arg );
	if ( !Q_stricmp( arg, "Players" ) ) {
		CG_DF_InfoPlayers();
		return;
	}
	if ( !Q_stricmp( arg, "StringSubs" ) ) {
		CG_DF_InfoStringSubs();
		return;
	}
	if ( !Q_stricmp( arg, "debug" ) ) {
		for ( i = 899; i < 910; i++ ) {
			if ( i % 10 == 0 ) {
				CG_Printf( "\n" );
			}
			CG_DF_InfoPrintItem( i );
		}
		CG_DF_InfoPrintItem( 999 );
		return;
	}
	Q_strncpyz( exp, arg, sizeof( exp ) );
	if ( CG_DF_ExpandVar( exp, sizeof( exp ) ) && strcmp( arg, exp ) ) {
		CG_Printf( "^3%s : ^5%s\n", arg, exp );
		return;
	}
	if ( !num && arg[0] && strcmp( arg, "?" ) ) {
		unknown = 1;
	}
	if ( !strcmp( arg, "?" ) || unknown ) {
		if ( unknown ) {
			CG_Printf( "^1*** ERROR: Unrecognized ^2info^1 command: ^3%s\n", arg );
		}
		CG_Printf( "\n^1info:    ^3Performs an info lookup through the crosshair stats display system.\n^5Usage:   ^2info <infonum>\n^5Example: ^2info           (shows entire CHS info list)\n^5Example: ^2info 23\n^5Example: ^2info $posXYZ\n^5Example: ^2info $chsinfo(23)\n\n" );
		CG_Printf( "^2--------------------------------------------------\n^1Special Info Items:\n^2--------------------------------------------------\n^3Players\n^2--------------------------------------------------\n^3StringSubs\n^2--------------------------------------------------\n^5Example: ^2info Players\n^5Example: ^2info StringSubs\n\n" );
		return;
	}
	if ( num ) {
		CG_DF_InfoPrintItem( num );
		return;
	}
	CG_Printf( "\n" );
	for ( i = 1; i < 330; i++ ) {
		if ( i % 10 == 0 ) {
			CG_Printf( "\n" );
		}
		CG_DF_InfoPrintItem( i );
	}
	CG_Printf( "\n" );
}
