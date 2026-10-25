/*
 * dfgolden - golden data tool for the oDFs physics tests
 *
 *   dfgolden check [-v] [-r] [--csv] <demo|dir> ...    cleanliness gate + census
 *   dfgolden info  [-n N] <demo>                       settings and first N snapshots
 *
 * "check" decides whether a demo is clean enough to become a golden file.
 * It is deliberately strict: a demo is only CLEAN when its message sequence,
 * snapshot clock and command clock are free of any gap or irregularity.
 *
 *   REJECT  a structural problem (parse error, gaps, clock steps, ...)
 *   DIRTY   structurally fine, but the player's command clock is irregular
 *           somewhere (repeated, skipped or backwards commandTime ...)
 *   CLEAN   neither
 *
 * Respawns and teleports (EF_TELEPORT_BIT toggled, pm_type change) are not
 * irregularities: they only split the run into segments.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#include "demo.h"

#define EF_TELEPORT_BIT		0x00000004
#define PMF_FOLLOW			4096
#define PM_NORMAL			0
#define POS_JUMP_UNITS		64.0f		/* per 8 ms step; 4000 u/s is 32 */

/* ---- reasons ---- */

enum {
	R_PARSE, R_NOEND, R_PROTOCOL, R_GAMESTATE, R_NOSNAPS, R_SEQGAP, R_SEQORDER,
	R_DELTA, R_FULLSNAP, R_SNAPFLAGS, R_MULTISNAP, R_NOSNAPMSG, R_TIMESTEP,
	R_PMOVE, R_CFGCHANGE, R_NONFINITE,
	R_NUMHARD,
	I_CMDREPEAT = R_NUMHARD, I_CMDSKIP, I_CMDSHORT, I_CMDBACK, I_POSJUMP, I_FOLLOW, I_CLIENT,
	R_NUM
};

static const char *reasonName[R_NUM] = {
	"PARSE_ERROR", "NO_END_MARKER", "NOT_PROTOCOL_68", "GAMESTATE_COUNT", "NO_SNAPSHOTS",
	"SEQ_GAP", "SEQ_ORDER", "DELTA_INVALID", "FULL_SNAPSHOT", "SNAPFLAGS", "MULTI_SNAP_MSG",
	"MSG_WITHOUT_SNAP", "TIME_STEP", "PMOVE_NOT_FIXED_8MS", "CONFIG_CHANGED", "NON_FINITE",
	"CMDTIME_REPEAT", "CMDTIME_SKIP", "CMDTIME_SHORT", "CMDTIME_BACK", "POS_JUMP",
	"FOLLOWING", "CLIENT_CHANGE"
};

static int reasonTotal[R_NUM];		/* number of demos with that reason */
static int total, nClean, nDirty, nReject;

typedef struct {
	int		verbose, recursive, csv;
	const char	*exclude[8];		/* case-insensitive substrings of the file name */
	int		numExclude;
} opts_t;

static int skipped;

static int Excluded( const char *name, const opts_t *o ) {
	char lower[512];
	int i;

	Q_strncpyz( lower, name, sizeof( lower ) );
	Q_strlwr( lower );
	for ( i = 0; i < o->numExclude; i++ ) {
		if ( strstr( lower, o->exclude[i] ) ) {
			return 1;
		}
	}
	return 0;
}

/* ---- helpers ---- */

/* Info_ValueForKey returns one of two alternating static buffers, so every result
   has to be copied before the next call */
static void InfoValue( const char *info, const char *key, char *out, int size ) {
	Q_strncpyz( out, Info_ValueForKey( info, key ), size );
}

static void Cfg( const demo_t *d, const char *key, char *out, int size ) {
	InfoValue( d->systeminfo, key, out, size );
	if ( !out[0] ) {
		InfoValue( d->serverinfo, key, out, size );
	}
}

/* keys whose change during a demo can alter the physics */
static const char *physKeys[] = {
	"pmove_fixed", "pmove_msec", "sv_fps", "g_speed", "g_gravity", "g_knockback", "jumpvelocity",
	"g_gametype", "defrag_gametype", "defrag_mode", "df_promode", "timescale", "g_synchronousClients",
	"sv_cheats", "g_cheats", "g_smoothClients", NULL
};

static int CfgChanged( const demo_t *d, char *which, int whichSize ) {
	int i;

	for ( i = 0; physKeys[i]; i++ ) {
		char a[MAX_INFO_VALUE], b[MAX_INFO_VALUE];

		Q_strncpyz( a, Info_ValueForKey( d->systeminfo, physKeys[i] ), sizeof( a ) );
		Q_strncpyz( b, Info_ValueForKey( d->systeminfoNow, physKeys[i] ), sizeof( b ) );
		if ( strcmp( a, b ) ) {
			Q_strncpyz( which, physKeys[i], whichSize );
			return 1;
		}
		Q_strncpyz( a, Info_ValueForKey( d->serverinfo, physKeys[i] ), sizeof( a ) );
		Q_strncpyz( b, Info_ValueForKey( d->serverinfoNow, physKeys[i] ), sizeof( b ) );
		if ( strcmp( a, b ) ) {
			Q_strncpyz( which, physKeys[i], whichSize );
			return 1;
		}
	}
	return 0;
}

static const char *Basename( const char *path ) {
	const char *s = strrchr( path, '/' );
	const char *t = strrchr( path, '\\' );

	if ( t > s ) {
		s = t;
	}
	return s ? s + 1 : path;
}

/* ---- the check ---- */

typedef struct {
	int		hard[R_NUM];		/* count per reason */
	int		step;				/* snapshot step in ms */
	int		pmoveMsec;
	int		boundaries;			/* respawns / teleports / pm_type changes */
	int		segments;			/* runs of at least 2 regular snapshots */
	int		longest;			/* longest run, in snapshots */
	int		inSegments;			/* snapshots inside runs */
	int		firstCfgBad;
	char	cfgKey[64];
} result_t;

static int Finite3( const vec3_t v ) {
	return isfinite( v[0] ) && isfinite( v[1] ) && isfinite( v[2] );
}

static void Analyse( const demo_t *d, result_t *r ) {
	int		i, run;
	int		step = 0;
	int		mismatch = 0;

	memset( r, 0, sizeof( *r ) );

	if ( d->err[0] ) {
		r->hard[R_PARSE] = 1;
	}
	if ( !d->endMarker && !d->err[0] ) {
		r->hard[R_NOEND] = 1;
	}
	if ( !d->err[0] ) {
		/* vanilla servers write "protocol", ioq3 based ones "com_protocol" */
		char proto[MAX_INFO_VALUE];

		InfoValue( d->serverinfo, "protocol", proto, sizeof( proto ) );
		if ( !proto[0] ) {
			InfoValue( d->serverinfo, "com_protocol", proto, sizeof( proto ) );
		}
		if ( atoi( proto ) != 68 ) {
			r->hard[R_PROTOCOL] = 1;
		}
	}
	if ( d->numGamestates != 1 ) {
		r->hard[R_GAMESTATE] = 1;
	}
	if ( d->numSnaps < 2 ) {
		r->hard[R_NOSNAPS] = 1;
	}
	r->hard[R_SEQGAP] = d->seqGaps;
	r->hard[R_SEQORDER] = d->seqOrder;
	r->hard[R_DELTA] = d->badDeltas;
	r->hard[R_FULLSNAP] = d->fullSnaps;
	r->hard[R_MULTISNAP] = d->multiSnapMsgs;
	r->hard[R_NOSNAPMSG] = d->msgsWithoutSnap;

	/* snapshot flags: the startup window (full snapshots) may legitimately be rate delayed */
	for ( i = ( d->firstDeltaSnap > 1 ? d->firstDeltaSnap : 1 ); i < d->numSnaps; i++ ) {
		if ( d->snaps[i].snapFlags & ( SNAPFLAG_RATE_DELAYED | SNAPFLAG_NOT_ACTIVE ) ) {
			r->hard[R_SNAPFLAGS]++;
		}
		if ( ( d->snaps[i].snapFlags ^ d->snaps[i - 1].snapFlags ) & SNAPFLAG_SERVERCOUNT ) {
			r->hard[R_SNAPFLAGS]++;
		}
	}

	/* the server clock: one constant step */
	if ( d->numSnaps >= 2 ) {
		step = d->snaps[1].serverTime - d->snaps[0].serverTime;
		r->step = step;
		for ( i = 1; i < d->numSnaps; i++ ) {
			if ( d->snaps[i].serverTime - d->snaps[i - 1].serverTime != step ) {
				r->hard[R_TIMESTEP]++;
			}
		}
	}

	/* pmove: the tests need fixed 8 ms steps, one per snapshot */
	{
		char fixed[MAX_INFO_VALUE], msec[MAX_INFO_VALUE];

		Cfg( d, "pmove_fixed", fixed, sizeof( fixed ) );
		Cfg( d, "pmove_msec", msec, sizeof( msec ) );
		r->pmoveMsec = msec[0] ? atoi( msec ) : 8;
		if ( atoi( fixed ) != 1 || r->pmoveMsec != step ) {
			r->hard[R_PMOVE] = 1;
		}
	}

	if ( CfgChanged( d, r->cfgKey, sizeof( r->cfgKey ) ) ) {
		r->hard[R_CFGCHANGE] = 1;
	}

	for ( i = 0; i < d->numSnaps; i++ ) {
		const playerState_t *ps = &d->snaps[i].ps;

		if ( !Finite3( ps->origin ) || !Finite3( ps->velocity ) || !Finite3( ps->viewangles ) ) {
			r->hard[R_NONFINITE]++;
		}
	}

	/* the player's command clock, segment by segment */
	run = 1;
	for ( i = 1; i < d->numSnaps; i++ ) {
		const playerState_t *a = &d->snaps[i - 1].ps;
		const playerState_t *b = &d->snaps[i].ps;
		int		boundary, regular = 1;
		int		dct = b->commandTime - a->commandTime;

		boundary = ( ( a->eFlags ^ b->eFlags ) & EF_TELEPORT_BIT ) || a->pm_type != b->pm_type;
		(void)mismatch;

		if ( boundary ) {
			r->boundaries++;
			regular = 0;
		} else {
			if ( a->clientNum != b->clientNum || b->clientNum != d->clientNum ) {
				r->hard[I_CLIENT]++;
				regular = 0;
			}
			if ( ( a->pm_flags | b->pm_flags ) & PMF_FOLLOW ) {
				r->hard[I_FOLLOW]++;
				regular = 0;
			}
			if ( dct != step ) {
				if ( dct == 0 ) {
					r->hard[I_CMDREPEAT]++;
				} else if ( dct < 0 ) {
					r->hard[I_CMDBACK]++;
				} else if ( dct > step ) {
					r->hard[I_CMDSKIP]++;
				} else {
					r->hard[I_CMDSHORT]++;
				}
				regular = 0;
			} else {
				vec3_t	delta;

				VectorSubtract( b->origin, a->origin, delta );
				if ( VectorLength( delta ) > POS_JUMP_UNITS ) {
					r->hard[I_POSJUMP]++;
					regular = 0;
				}
			}
		}
		if ( b->pm_type != PM_NORMAL || a->pm_type != PM_NORMAL ) {
			regular = 0;
		}

		if ( regular ) {
			run++;
		} else {
			if ( run >= 2 ) {
				r->segments++;
				r->inSegments += run;
				if ( run > r->longest ) {
					r->longest = run;
				}
			}
			run = 1;
		}
	}
	if ( run >= 2 ) {
		r->segments++;
		r->inSegments += run;
		if ( run > r->longest ) {
			r->longest = run;
		}
	}
}

static void PrintInfoString( const char *label, const char *s ) {
	printf( "%s:\n", label );
	while ( *s == '\\' ) {
		char key[BIG_INFO_STRING], val[BIG_INFO_STRING];
		int n;

		s++;
		n = 0;
		while ( *s && *s != '\\' && n < (int)sizeof( key ) - 1 ) {
			key[n++] = *s++;
		}
		key[n] = '\0';
		if ( *s != '\\' ) {
			break;
		}
		s++;
		n = 0;
		while ( *s && *s != '\\' && n < (int)sizeof( val ) - 1 ) {
			val[n++] = *s++;
		}
		val[n] = '\0';
		printf( "  %-24s %s\n", key, val );
	}
}

static void CheckDemo( const char *path, const opts_t *o ) {
	static demo_t	d;
	result_t		r;
	const char		*verdict;
	int				i, anyHard = 0, anyIrr = 0;
	char			reasons[512];

	total++;
	if ( Demo_Load( path, &d ) ) {
		printf( "%-6s %s: cannot open\n", "REJECT", Basename( path ) );
		nReject++;
		return;
	}

	Analyse( &d, &r );

	for ( i = 0; i < R_NUMHARD; i++ ) {
		if ( r.hard[i] ) {
			anyHard = 1;
		}
	}
	for ( i = R_NUMHARD; i < R_NUM; i++ ) {
		if ( r.hard[i] ) {
			anyIrr = 1;
		}
	}
	verdict = anyHard ? "REJECT" : anyIrr ? "DIRTY" : "CLEAN";
	if ( anyHard ) {
		nReject++;
	} else if ( anyIrr ) {
		nDirty++;
	} else {
		nClean++;
	}

	reasons[0] = '\0';
	for ( i = 0; i < R_NUM; i++ ) {
		if ( r.hard[i] ) {
			char tmp[96];

			reasonTotal[i]++;
			if ( o->verbose || o->csv || strlen( reasons ) < 300 ) {
				if ( i == R_PARSE ) {
					Com_sprintf( tmp, sizeof( tmp ), "%s(%s) ", reasonName[i], d.err );
				} else if ( i == R_CFGCHANGE ) {
					Com_sprintf( tmp, sizeof( tmp ), "%s(%s) ", reasonName[i], r.cfgKey );
				} else {
					Com_sprintf( tmp, sizeof( tmp ), "%s=%d ", reasonName[i], r.hard[i] );
				}
				Q_strcat( reasons, sizeof( reasons ), tmp );
			}
		}
	}

	{
		char map[MAX_INFO_VALUE];

		InfoValue( d.serverinfo, "mapname", map, sizeof( map ) );
		if ( o->csv ) {
			char promode[MAX_INFO_VALUE], gametype[MAX_INFO_VALUE], vers[MAX_INFO_VALUE];

			InfoValue( d.serverinfo, "df_promode", promode, sizeof( promode ) );
			InfoValue( d.serverinfo, "defrag_gametype", gametype, sizeof( gametype ) );
			InfoValue( d.serverinfo, "defrag_vers", vers, sizeof( vers ) );
			printf( "\"%s\",%s,%s,%d,%d,%d,%d,%d,%d,%s,%s,%s,\"%s\"\n", path, verdict, map, d.numSnaps, r.step,
				r.segments, r.longest, r.inSegments, r.boundaries, promode, gametype, vers, reasons );
		} else {
			printf( "%-6s %-28s %-14s snaps=%-6d step=%-3d bound=%-3d segs=%-3d longest=%-6d %s\n",
				verdict, Basename( path ), map, d.numSnaps, r.step, r.boundaries, r.segments, r.longest,
				reasons );
		}
	}
	if ( o->verbose && d.gapBeforeFirstSnap ) {
		printf( "         note: sequence gap of %d between gamestate and first snapshot\n", d.gapBeforeFirstSnap );
	}
	if ( o->verbose && d.seqGaps ) {
		printf( "         first sequence gap at snapshot %d\n", d.firstSeqGapSnap );
	}

	Demo_Free( &d );
}

/* ---- info ---- */

static void InfoDemo( const char *path, int count ) {
	static demo_t	d;
	int				i;

	if ( Demo_Load( path, &d ) ) {
		fprintf( stderr, "cannot open %s\n", path );
		return;
	}
	printf( "file: %s\nmessages: %d  snapshots: %d  gamestates: %d  end marker: %d  clientNum: %d\n",
		path, d.numMsgs, d.numSnaps, d.numGamestates, d.endMarker, d.clientNum );
	if ( d.err[0] ) {
		printf( "PARSE ERROR in message %d: %s\n", d.errMsg, d.err );
	}
	printf( "config changes (serverinfo/systeminfo) during the demo: %d\n", d.numCsChanges );
	PrintInfoString( "serverinfo", d.serverinfo );
	PrintInfoString( "systeminfo", d.systeminfo );

	for ( i = 0; i < d.numSnaps && i < count; i++ ) {
		const demoSnap_t *s = &d.snaps[i];
		const playerState_t *ps = &s->ps;

		printf( "snap %d seq=%d time=%d delta=%d flags=%d | cmdTime=%d pm_type=%d pm_flags=0x%x pm_time=%d "
			"origin=(%g %g %g) vel=(%g %g %g) ang=(%g %g %g) delta_ang=(%d %d %d) speed=%d gravity=%d "
			"ground=%d eFlags=0x%x stats12=0x%x\n",
			i, s->seq, s->serverTime, s->deltaNum, s->snapFlags, ps->commandTime, ps->pm_type, ps->pm_flags,
			ps->pm_time, ps->origin[0], ps->origin[1], ps->origin[2], ps->velocity[0], ps->velocity[1],
			ps->velocity[2], ps->viewangles[0], ps->viewangles[1], ps->viewangles[2], ps->delta_angles[0],
			ps->delta_angles[1], ps->delta_angles[2], ps->speed, ps->gravity, ps->groundEntityNum,
			ps->eFlags, ps->stats[12] );
	}
	Demo_Free( &d );
}

/* ---- file walking ---- */

static int IsDemo( const char *name ) {
	size_t n = strlen( name );

	return n > 6 && !Q_stricmp( name + n - 6, ".dm_68" );
}

static void Walk( const char *path, const opts_t *o ) {
	struct stat st;

	if ( stat( path, &st ) ) {
		fprintf( stderr, "cannot stat %s\n", path );
		return;
	}
	if ( S_ISDIR( st.st_mode ) ) {
		DIR *dir = opendir( path );
		struct dirent *e;

		if ( !dir ) {
			return;
		}
		while ( ( e = readdir( dir ) ) != NULL ) {
			char sub[1024];

			if ( !strcmp( e->d_name, "." ) || !strcmp( e->d_name, ".." ) ) {
				continue;
			}
			snprintf( sub, sizeof( sub ), "%s/%s", path, e->d_name );
			if ( stat( sub, &st ) ) {
				continue;
			}
			if ( S_ISDIR( st.st_mode ) ) {
				if ( o->recursive ) {
					Walk( sub, o );
				}
			} else if ( IsDemo( e->d_name ) ) {
				if ( Excluded( e->d_name, o ) ) {
					skipped++;
				} else {
					CheckDemo( sub, o );
				}
			}
		}
		closedir( dir );
	} else {
		CheckDemo( path, o );
	}
}

int Steps_Main( int argc, char **argv );
int GameRun_Main( int argc, char **argv );
int GameSteps_Main( int argc, char **argv );
int Gold_Main( int argc, char **argv );

static void Usage( void ) {
	fprintf( stderr,
		"usage:\n"
		"  dfgolden check [-v] [-r] [--csv] [--exclude <name substring>] <demo|dir> ...\n"
		"  dfgolden info [-n N] <demo>\n"
		"  dfgolden make --game qagame.qvm --qvm pmove.qvm --offsets offsets.qvm (--bsp map.bsp | --maps dir)\n"
		"                [--allow-map-mismatch] (-o file.dfg | --out-dir dir) <demo> ...\n"
		"  dfgolden test --game qagame.qvm --offsets offsets.qvm [--max N] <file.dfg | dir> ...\n"
		"  dfgolden show <file.dfg> ...\n" );
}

int main( int argc, char **argv ) {
	opts_t	o;
	int		i;

	memset( &o, 0, sizeof( o ) );
	if ( argc < 3 ) {
		Usage();
		return 1;
	}

	if ( !strcmp( argv[1], "check" ) ) {
		int any = 0;

		for ( i = 2; i < argc; i++ ) {
			if ( !strcmp( argv[i], "-v" ) ) {
				o.verbose = 1;
			} else if ( !strcmp( argv[i], "-r" ) ) {
				o.recursive = 1;
			} else if ( !strcmp( argv[i], "--csv" ) ) {
				o.csv = 1;
			} else if ( !strcmp( argv[i], "--exclude" ) && i + 1 < argc && o.numExclude < 8 ) {
				char *s = strdup( argv[++i] );

				Q_strlwr( s );
				o.exclude[o.numExclude++] = s;
			}
		}
		if ( o.csv ) {
			printf( "file,verdict,map,snaps,step,segments,longest,inSegments,boundaries,promode,gametype,vers,reasons\n" );
		}
		for ( i = 2; i < argc; i++ ) {
			if ( !strcmp( argv[i], "--exclude" ) ) {
				i++;
				continue;
			}
			if ( argv[i][0] == '-' ) {
				continue;
			}
			any = 1;
			Walk( argv[i], &o );
		}
		if ( !any ) {
			Usage();
			return 1;
		}
		fprintf( stderr, "\n%d demos: %d CLEAN, %d DIRTY, %d REJECT  (%d excluded by name)\n", total, nClean, nDirty,
			nReject, skipped );
		for ( i = 0; i < R_NUM; i++ ) {
			if ( reasonTotal[i] ) {
				fprintf( stderr, "  %-22s %d demos\n", reasonName[i], reasonTotal[i] );
			}
		}
		return 0;
	}

	if ( !strcmp( argv[1], "make" ) || !strcmp( argv[1], "test" ) || !strcmp( argv[1], "show" ) ) {
		int r = Gold_Main( argc - 1, argv + 1 );

		if ( r < 0 ) {
			Usage();
			return 2;
		}
		return r;
	}

	if ( !strcmp( argv[1], "gamesteps" ) ) {
		return GameSteps_Main( argc - 2, argv + 2 );
	}

	if ( !strcmp( argv[1], "gamerun" ) ) {
		return GameRun_Main( argc - 2, argv + 2 );
	}

	if ( !strcmp( argv[1], "steps" ) ) {
		return Steps_Main( argc - 2, argv + 2 );
	}

	if ( !strcmp( argv[1], "info" ) ) {
		int count = 10;

		for ( i = 2; i < argc; i++ ) {
			if ( !strcmp( argv[i], "-n" ) && i + 1 < argc ) {
				count = atoi( argv[++i] );
			} else {
				InfoDemo( argv[i], count );
			}
		}
		return 0;
	}

	Usage();
	return 1;
}
