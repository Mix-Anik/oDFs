// cg_dfautorec.c -- DeFRaG autorecord (cgame 0x19e0 per-frame state machine, 0x18ab start, 0x1873/0x1865 stop, 0x1887 keep/discard, 0x17bc copy + naming,
// 0x586d / 0x5543 / 0x5431 / 0x5504 file-name expansion; see disasm/CGAME/autorecord.md)
//
// Records every run into demos/temp/current.dm_<protocol> and, when the run ends, copies it to demos/<df_ar_targetfolder>/<name>.dm_<protocol> if it is
// a new personal best (or a tie with df_tieOverwrites) — or always with df_autorecord 3. States: 0 idle, 1 recording, 2 waiting df_ar_stopdelay ms after
// the finish, 3 stopped (copy after 200 ms).
// Not ported: the "near the start trigger" start condition 0x18fa used on non-DeFRaG servers (we always start from idle, like DeFRaG when its
// gametype index is 1), the speed-capture name format variant (needs the capture gametype, df_ar_speedcapture_format is registered but unused).

#include "cg_local.h"

vmCvar_t	df_autorecord, df_ar_stopdelay, df_ar_format, df_ar_speedcapture_format, df_ar_verbose, df_ar_targetfolder;

#define AR_DEFAULT_FORMAT	"$map.$-route[$gt.$phys.$-mode]$m.$-s.$-ms($pl.$plc)"

enum { AR_EV_START = 1, AR_EV_FINISH = 2, AR_EV_RESPAWN = 4 };

static int		arState, arEvents, arFinishMs, arFinishKeep, arWarned, arDelay;

void CG_DF_ArEvent( int ev, int ms, qboolean keep ) {
	arEvents |= ev;
	if ( ev == AR_EV_FINISH ) {
		arFinishMs = ms;
		arFinishKeep = keep;
	}
}

void CG_DF_ArStartEvent( void ) { CG_DF_ArEvent( AR_EV_START, 0, qfalse ); }
void CG_DF_ArRespawnEvent( void ) { CG_DF_ArEvent( AR_EV_RESPAWN, 0, qfalse ); }
void CG_DF_ArFinishEvent( int ms, qboolean keep ) { CG_DF_ArEvent( AR_EV_FINISH, ms, keep ); }

static void AR_Say( const char *msg ) {
	if ( df_ar_verbose.integer ) {
		CG_Printf( "%s\n", msg );
	}
}

static int AR_CvarInt( const char *name ) {
	char buf[64];

	trap_Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atoi( buf );
}

// ---- file name expansion ----
// 0x5431: append a token value; colour codes are skipped, characters that are not valid in file names become '-'
static void AR_PutToken( char **out, int *left, const char *s, int max ) {
	while ( *s && max > 0 && *left > 0 ) {
		int c = (byte)*s;

		if ( c == '^' && s[1] && s[1] != '^' ) {
			s += 2;
			continue;
		}
		if ( c <= 32 || c >= 126 || strchr( ":*?\"<>|\\/.()[]", c ) ) {
			c = '-';
		}
		*(*out)++ = c;
		( *left )--;
		max--;
		s++;
	}
}

// 0x5504: value unavailable -> `$-x` removes the character written before the token, `$+x` the one after it
static void AR_Missing( char **out, char *start, const char **fmt, qboolean minus, qboolean plus ) {
	if ( minus && *out != start ) {
		( *out )--;
	}
	if ( plus && **fmt && **fmt != '$' ) {
		( *fmt )++;
	}
}

static const char *AR_GtName( int gt ) {
	static const char *names[16] = { "ac", "df", "tm", "fc", "res", "mdf", "mtm", "mfc", "AC", "DF", "TM", "FC", "RES", "DF", "TM", "FC" };

	return names[gt & 15];
}

// 0x5543: one `$token`
static void AR_Token( char **out, char *start, int *left, const char **fmt, int ms ) {
	qboolean	minus = qfalse, plus = qfalse;
	char		tmp[64];
	const char	*s = *fmt;
	qtime_t		t;

	while ( *s == '-' || *s == '+' ) {
		if ( *s == '-' ) minus = qtrue; else plus = qtrue;
		s++;
	}
	*fmt = s;
#define TOK(n) ( !Q_strncmp( s, n, strlen( n ) ) ? ( *fmt = s + strlen( n ), qtrue ) : qfalse )
	if ( TOK( "map" ) ) {
		AR_PutToken( out, left, Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ), 64 );
	} else if ( TOK( "phys" ) ) {
		AR_PutToken( out, left, atoi( CG_DF_Serverinfo( "df_promode" ) ) ? "cpm" : "vq3", 3 );
	} else if ( TOK( "gt" ) ) {
		AR_PutToken( out, left, AR_GtName( atoi( CG_DF_Serverinfo( "defrag_gametype" ) ) ), 3 );
	} else if ( TOK( "plc" ) ) {
		trap_Cvar_VariableStringBuffer( "df_country", tmp, sizeof( tmp ) );
		AR_PutToken( out, left, tmp, 20 );
	} else if ( TOK( "pl" ) ) {
		AR_PutToken( out, left, cgs.clientinfo[cg.clientNum].name, 20 );
	} else if ( TOK( "route" ) ) {
		const char *r = CG_DF_Serverinfo( "defrag_route" );

		if ( r[0] ) {
			AR_PutToken( out, left, r, 18 );
		} else {
			AR_Missing( out, start, fmt, minus, plus );
		}
	} else if ( TOK( "mode" ) ) {
		int m = atoi( CG_DF_Serverinfo( "defrag_mode" ) );

		if ( m >= 0 && CG_DF_Serverinfo( "defrag_mode" )[0] ) {
			AR_PutToken( out, left, va( "%i", m ), 1 );
		} else {
			AR_Missing( out, start, fmt, minus, plus );
		}
	} else if ( TOK( "s" ) ) {
		if ( ms ) {
			AR_PutToken( out, left, va( "%02i", ( ms / 1000 ) % 60 ), 2 );
		} else {
			AR_Missing( out, start, fmt, minus, plus );
		}
	} else if ( TOK( "ms" ) ) {
		if ( ms ) {
			AR_PutToken( out, left, va( "%03i", ms % 1000 ), 3 );
		} else {
			AR_Missing( out, start, fmt, minus, plus );
		}
	} else if ( TOK( "m" ) ) {
		if ( ms ) {
			AR_PutToken( out, left, va( "%i", ms / 60000 ), 2 );
		} else {
			AR_Missing( out, start, fmt, minus, plus );
		}
	} else if ( TOK( "datetime" ) ) {
		trap_RealTime( &t );
		AR_PutToken( out, left, va( "%04i%02i%02i%02i%02i%02i", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec ), 14 );
	} else if ( TOK( "date" ) ) {
		trap_RealTime( &t );
		AR_PutToken( out, left, va( "%04i%02i%02i", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday ), 8 );
	} else if ( *s == '$' ) {
		AR_PutToken( out, left, "$", 1 );
		( *fmt )++;
	}
#undef TOK
}

// 0x586d: <dir>/ + expanded format + .<ext>
static void AR_BuildName( char *buf, int size, const char *fmt, const char *dir, const char *ext, int ms ) {
	char		*out = buf, *start;
	int			left = size - 1;

	if ( dir && dir[0] ) {
		Q_strncpyz( buf, dir, size );
		out += strlen( buf );
		left -= strlen( buf );
		if ( dir[strlen( dir ) - 1] != '/' && left > 0 ) {
			*out++ = '/';
			left--;
		}
	}
	if ( ext ) {
		left -= strlen( ext ) + 1;
	}
	start = out;
	while ( *fmt && left > 0 ) {
		int c = (byte)*fmt;

		if ( c == '^' && fmt[1] && fmt[1] != '^' ) {
			fmt += 2;
		} else if ( c == '$' ) {
			fmt++;
			AR_Token( &out, start, &left, &fmt, ms );
		} else {
			if ( c <= 32 || c >= 126 || strchr( ":*?\"<>|\\/", c ) ) {
				c = '-';
			}
			*out++ = c;
			fmt++;
			left--;
		}
	}
	*out = 0;
	if ( ext ) {
		if ( ext[0] != '.' ) {
			*out++ = '.';
		}
		Q_strncpyz( out, ext, size - ( out - buf ) );
	}
}

// 0x12bb: copy a file in chunks
static qboolean AR_CopyFile( const char *src, const char *dst ) {
	fileHandle_t	in, out;
	static byte		chunk[32000];
	int				len, n;

	len = trap_FS_FOpenFile( src, &in, FS_READ );
	if ( !in ) {
		return qfalse;
	}
	trap_FS_FOpenFile( dst, &out, FS_WRITE );
	if ( !out ) {
		trap_FS_FCloseFile( in );
		return qfalse;
	}
	while ( len > 0 ) {
		n = len > (int)sizeof( chunk ) ? (int)sizeof( chunk ) : len;
		trap_FS_Read( chunk, n, in );
		trap_FS_Write( chunk, n, out );
		len -= n;
	}
	trap_FS_FCloseFile( in );
	trap_FS_FCloseFile( out );
	return qtrue;
}

static int AR_Protocol( void ) {
	int p = AR_CvarInt( "protocol" );

	return p ? p : AR_CvarInt( "com_protocol" );
}

// 0x17bc: name and copy the finished demo
static void AR_Save( void ) {
	char	fmt[256], dir[128], name[256], src[96], ext[16];
	int		proto = AR_Protocol();

	trap_Cvar_VariableStringBuffer( "df_ar_format", fmt, sizeof( fmt ) );
	if ( !fmt[0] ) {
		Q_strncpyz( fmt, AR_DEFAULT_FORMAT, sizeof( fmt ) );
		trap_Cvar_Set( "df_ar_format", fmt );
	}
	if ( !strcmp( df_ar_targetfolder.string, "./" ) || !strcmp( df_ar_targetfolder.string, "." ) || !df_ar_targetfolder.string[0] ) {
		Q_strncpyz( dir, "demos", sizeof( dir ) );		// the default "./" would give demos/./name
	} else {
		Com_sprintf( dir, sizeof( dir ), "demos/%s", df_ar_targetfolder.string );
	}
	Com_sprintf( ext, sizeof( ext ), "dm_%i", proto );
	AR_BuildName( name, sizeof( name ), fmt, dir, ext, arFinishMs );
	Com_sprintf( src, sizeof( src ), "demos/temp/current.dm_%i", proto );
	if ( AR_CopyFile( src, name ) ) {
		if ( df_ar_verbose.integer ) {
			CG_Printf( "Autorecord: demo written to %s^7\n", name );
		}
	} else {
		CG_Printf( "^3Autorecord: demo copy failed.^7\n" );
	}
	arState = 0;
	arFinishMs = 0;
}

static void AR_Stop( void ) {		// 0x1873
	trap_SendConsoleCommand( "silent stoprecord\n" );
	arState = 0;
	AR_Say( "Autorecord: demo discarded." );
}

static void AR_Start( void ) {		// 0x18ab
	int sp = AR_CvarInt( "ui_recordSPDemo" ), sync = AR_CvarInt( "g_synchronousClients" );

	if ( !sp ) {
		trap_SendConsoleCommand( "set ui_recordSPDemo 1\n" );
	}
	if ( !sync ) {
		trap_SendConsoleCommand( "set g_synchronousClients 1\n" );
	}
	trap_SendConsoleCommand( "silent stoprecord\nsilent record temp/current\n" );
	if ( !sync ) {
		trap_SendConsoleCommand( "set g_synchronousClients 0\n" );
	}
	if ( !sp ) {
		trap_SendConsoleCommand( "set ui_recordSPDemo 0\n" );
	}
	arState = 1;
	arWarned = 0;
	AR_Say( "Autorecord: started recording." );
}

void CG_DF_AutoRecFrame( void ) {		// 0x19e0
	if ( cg.demoPlayback || !cg.snap ) {
		arEvents = 0;
		return;
	}
	arDelay -= cg.frametime;
	if ( arDelay < 0 ) {
		arDelay = 0;
	}
	if ( arState == 1 ) {
		if ( arEvents & AR_EV_RESPAWN ) {
			AR_Stop();
		}
		if ( arState == 1 && ( arEvents & AR_EV_FINISH ) ) {
			if ( arFinishKeep || df_autorecord.integer == 3 ) {		// 0x1887
				arState = 2;
				arDelay = df_ar_stopdelay.integer;
				AR_Say( "Autorecord: demo going to be saved." );
			} else {
				AR_Stop();
			}
		}
	}
	if ( arState == 2 ) {
		if ( arDelay == 0 || ( arEvents & ( AR_EV_RESPAWN | AR_EV_START ) ) ) {		// 0x1865: stop now, copy after 200 ms
			trap_SendConsoleCommand( "silent stoprecord\n" );
			arState = 3;
			arDelay = 200;
		}
	}
	if ( arState == 3 && arDelay == 0 ) {
		AR_Save();
	}
	if ( arState == 0 && df_autorecord.integer && cg.snap->ps.stats[STAT_HEALTH] > 0
		&& cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR ) {
		if ( !( arEvents & AR_EV_START ) && CG_DF_RunActive() ) {		// the run began before we were recording
			if ( !arWarned ) {
				CG_Printf( "^3Autorecord: couldn't record from the beginning, please restart your run.^7\n" );
				arWarned = 1;
			}
		} else {
			AR_Start();
		}
	}
	arEvents = 0;
}
