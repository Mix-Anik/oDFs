// ui_dfbest.c -- oDFs: the player's best time of every map, read from the names of the demos in demos/ (map browser).
//
// A DeFRaG demo is named "<map>[<game>.<physics>[.<n>]]<mm.ss.mmm>(<player>.<country>).dm_68", e.g.
// "1-run[mdf.cpm]00.03.680(Mixanik3.Estonia).dm_68".  The demos whose player name is the current `name` of the client (colours
// removed, compared without case and without anything but letters and digits) or one of the names in df_ui_maps_playerNames (comma
// separated, e.g. "Mixanikth,Mixanik") are "yours"; for every map the lowest time of each
//   run (df / mdf)  x  vq3 / cpm,    fast caps (fc / mfc)  x  vq3 / cpm,    tricks (tm)  x  vq3 / cpm
// is kept with the demo's path.  Demos of other kinds ([cpm.seg] segments, [..cpm.tr]) are not records and are skipped.
//
// The list of demos comes from the index file of the demo browser (df_demos_index.dat, whatever its age), else from the recursive
// listing of demos/ (oDFe).  The index can be old, so every time the map browser opens, demos/ itself and the autorecord folder
// (df_ar_targetfolder) are read again, which is where new demos appear (the demo browser merges the same two folders into its
// index).  Lists are parsed a few milliseconds per frame (UI_DF_BestRun).

#include "ui_local.h"

#define BEST_SLOTS		6			// kind * 2 + physics: run, fast caps, tricks x vq3, cpm
#define BEST_RUN		0
#define BEST_FC			1
#define BEST_TM			2
#define BEST_MAXMAPS	8192
#define BEST_HASHSIZE	16384		// power of two, twice BEST_MAXMAPS
#define BEST_POOLSIZE	524288		// the paths of the best demos
#define BEST_LISTSIZE	4194304		// the demo list (the index file is ~70 bytes per demo)
#define BEST_PASSSIZE	2097152		// one folder
#define BEST_NAMESIZE	64
#define BEST_INDEX_FILE	"df_demos_index.dat"	// see THE INDEX in ui_demo2.c
#define BEST_INDEX_HEADER	64
#define BEST_INDEX_MAGIC	"DFDI1"

typedef struct {
	char	map[32];				// lower case
	int		ms[BEST_SLOTS];			// 0 = no demo
	int		path[BEST_SLOTS];		// bestPool offset + 1
} bestEntry_t;

static bestEntry_t	bestEntries[BEST_MAXMAPS];
static int			bestCount;
static int			bestHash[BEST_HASHSIZE];	// bestEntries index + 1
static char			bestPool[BEST_POOLSIZE];
static int			bestPoolUsed;
static char			bestList[BEST_LISTSIZE];
static char			bestDirList[BEST_PASSSIZE];		// a folder listing to find a file by name (demos/ alone has ~1 MB of names)
static char			bestPassList[BEST_PASSSIZE];
static char			bestPrefix[96];			// the folder (below demos/) the pass list is of, with a closing slash
static int			bestPass;				// 0 the index / recursive list, 1 demos/, 2 the autorecord folder
static int			bestListCount, bestListPos;
static const char	*bestListPtr;
static int			bestState;					// 0 nothing read yet, 1 parsing the index, 3 parsing a folder, 2 done
#define BEST_MAXNAMES	8
static char			bestNames[BEST_MAXNAMES][BEST_NAMESIZE];	// the names that are "you", Best_Norm'd: `name`, then df_ui_maps_playerNames
static int			bestNumNames;
static char			bestKey[BEST_NAMESIZE + 256];				// the `name` and df_ui_maps_playerNames the entries were made for

// lower case letters and digits only: how names are compared (file names lose characters a player name may have)
static void Best_Norm( const char *s, char *out, int size ) {
	int n = 0;

	for ( ; *s && n < size - 1; s++ ) {
		int c = *s;

		if ( c >= 'A' && c <= 'Z' ) {
			c += 'a' - 'A';
		}
		if ( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) ) {
			out[n++] = c;
		}
	}
	out[n] = 0;
}

static unsigned Best_Hash( const char *s ) {
	unsigned h = 5381;

	for ( ; *s; s++ ) {
		h = h * 33 + *s;
	}
	return h;
}

static bestEntry_t *Best_Lookup( const char *map, qboolean add ) {
	unsigned	slot = Best_Hash( map ) & ( BEST_HASHSIZE - 1 );
	bestEntry_t	*e;
	int			i;

	while ( ( i = bestHash[slot] ) != 0 ) {
		if ( !strcmp( bestEntries[i - 1].map, map ) ) {
			return &bestEntries[i - 1];
		}
		slot = ( slot + 1 ) & ( BEST_HASHSIZE - 1 );
	}
	if ( !add || bestCount >= BEST_MAXMAPS ) {
		return NULL;
	}
	e = &bestEntries[bestCount++];
	Com_Memset( e, 0, sizeof( *e ) );
	Q_strncpyz( e->map, map, sizeof( e->map ) );
	bestHash[slot] = bestCount;
	return e;
}

/*
===============
Best_Parse

path: a path relative to demos/ ("SUB/NAME[MDF.CPM]00.16.640(NAME.COUNTRY).DM_68", any case).  Returns qtrue and the map (lower
case), the slot and the time in ms when it is a run of the current player.
===============
*/
static qboolean Best_Parse( const char *path, char *map, int mapSize, int *slot, int *ms ) {
	const char	*file, *open, *close, *rb, *lb, *p;
	char		name[BEST_NAMESIZE], norm[BEST_NAMESIZE], game[32], timeStr[16];
	char		*tok[3], *q;
	int			len, n, i, kind, phys, t[3];

	file = strrchr( path, '/' );
	file = file ? file + 1 : path;
	open = strrchr( file, '(' );
	close = strrchr( file, ')' );
	if ( !open || !close || close < open ) {
		return qfalse;
	}

	// "(name.country)": the country is after the last dot, a name may have dots itself
	len = close - open - 1;
	if ( len <= 0 || len >= (int)sizeof( name ) ) {
		return qfalse;
	}
	Com_Memcpy( name, open + 1, len );
	name[len] = 0;
	q = strrchr( name, '.' );
	if ( q ) {
		*q = 0;
	}
	Best_Norm( name, norm, sizeof( norm ) );
	if ( !norm[0] ) {
		return qfalse;
	}
	for ( i = 0; i < bestNumNames && strcmp( norm, bestNames[i] ); i++ )
		;
	if ( i == bestNumNames ) {
		return qfalse;
	}

	// "<map>[game.physics]<time>("
	for ( rb = open - 1; rb > file && *rb != ']'; rb-- )
		;
	if ( rb <= file || *rb != ']' ) {
		return qfalse;
	}
	for ( lb = rb - 1; lb > file && *lb != '['; lb-- )
		;
	if ( lb <= file || *lb != '[' || lb - file >= mapSize ) {
		return qfalse;
	}
	Com_Memcpy( map, file, lb - file );
	map[lb - file] = 0;
	Q_strlwr( map );

	len = rb - lb - 1;
	if ( len <= 0 || len >= (int)sizeof( game ) ) {
		return qfalse;
	}
	Com_Memcpy( game, lb + 1, len );
	game[len] = 0;
	Q_strlwr( game );
	n = 0;
	q = game;
	for ( ;; ) {
		tok[n++] = q;
		q = strchr( q, '.' );
		if ( !q || n == 3 ) {
			break;
		}
		*q++ = 0;
	}
	if ( strchr( tok[n - 1], '.' ) || n < 2 ) {
		return qfalse;
	}
	if ( !strcmp( tok[0], "df" ) || !strcmp( tok[0], "mdf" ) ) {
		kind = BEST_RUN;
		if ( n != 2 ) {
			return qfalse;			// [mdf.cpm.tr]
		}
	} else if ( !strcmp( tok[0], "fc" ) || !strcmp( tok[0], "mfc" ) ) {
		kind = BEST_FC;
	} else if ( !strcmp( tok[0], "tm" ) ) {
		kind = BEST_TM;
	} else {
		return qfalse;				// [cpm.seg] and the like
	}
	if ( !strcmp( tok[1], "vq3" ) ) {
		phys = 0;
	} else if ( !strcmp( tok[1], "cpm" ) ) {
		phys = 1;
	} else {
		return qfalse;
	}

	// "mm.ss.mmm"
	len = open - rb - 1;
	if ( len <= 0 || len >= (int)sizeof( timeStr ) ) {
		return qfalse;
	}
	Com_Memcpy( timeStr, rb + 1, len );
	timeStr[len] = 0;
	p = timeStr;
	for ( i = 0; i < 3; i++ ) {
		if ( *p < '0' || *p > '9' ) {
			return qfalse;
		}
		t[i] = 0;
		while ( *p >= '0' && *p <= '9' ) {
			t[i] = t[i] * 10 + *p++ - '0';
		}
		if ( i < 2 ) {
			if ( *p++ != '.' ) {
				return qfalse;
			}
		}
	}
	if ( *p ) {
		return qfalse;
	}
	*ms = ( t[0] * 60 + t[1] ) * 1000 + t[2];
	if ( *ms <= 0 ) {
		return qfalse;
	}
	*slot = kind * 2 + phys;
	return qtrue;
}

// notes a demo; keeps it when it is the best of its map and kind so far
static void Best_Consider( const char *path ) {
	char		map[32];
	int			slot, ms, len;
	bestEntry_t	*e;

	if ( !Best_Parse( path, map, sizeof( map ), &slot, &ms ) ) {
		return;
	}
	e = Best_Lookup( map, qtrue );
	if ( !e || ( e->ms[slot] && e->ms[slot] <= ms ) ) {
		return;
	}
	e->ms[slot] = ms;
	e->path[slot] = 0;
	len = strlen( path ) + 1;
	if ( bestPoolUsed + len <= BEST_POOLSIZE ) {
		Com_Memcpy( bestPool + bestPoolUsed, path, len );
		e->path[slot] = bestPoolUsed + 1;
		bestPoolUsed += len;
	}
}

// the index file of the demo browser, whatever its age: returns the number of paths in bestList
static int Best_ReadIndex( void ) {
	fileHandle_t	f;
	char			header[BEST_INDEX_HEADER + 1];
	const char		*p;
	int				len, count, bytes;

	len = trap_FS_FOpenFile( BEST_INDEX_FILE, &f, FS_READ );
	if ( !f ) {
		return 0;
	}
	if ( len < BEST_INDEX_HEADER ) {
		trap_FS_FCloseFile( f );
		return 0;
	}
	Com_Memset( header, 0, sizeof( header ) );
	trap_FS_Read( header, BEST_INDEX_HEADER, f );
	if ( Q_strncmp( header, BEST_INDEX_MAGIC " ", strlen( BEST_INDEX_MAGIC ) + 1 ) ) {
		trap_FS_FCloseFile( f );
		return 0;
	}
	p = header + strlen( BEST_INDEX_MAGIC ) + 1;
	count = atoi( COM_Parse( &p ) );
	bytes = atoi( COM_Parse( &p ) );
	if ( count <= 0 || bytes <= 0 || bytes >= BEST_LISTSIZE || len != BEST_INDEX_HEADER + bytes ) {
		trap_FS_FCloseFile( f );
		return 0;
	}
	trap_FS_Read( bestList, bytes, f );
	trap_FS_FCloseFile( f );
	bestList[bytes] = 0;
	return count;
}

// the demo files of one folder as consecutive strings (dm_* of any protocol, else dm_68)
static int Best_ListFolder( const char *dir, const char *prefix, char *buf, int size ) {
	char ext[16];
	int  n;

	Com_sprintf( ext, sizeof( ext ), "%sdm_*", prefix );
	n = trap_FS_GetFileList( dir, ext, buf, size );
	if ( n <= 0 ) {
		Com_sprintf( ext, sizeof( ext ), "%sdm_68", prefix );
		n = trap_FS_GetFileList( dir, ext, buf, size );
	}
	return n < 0 ? 0 : n;
}

// starts the first folder pass after `after` that has demos; bestState 2 when there is none
static void Best_NextPass( int after ) {
	char dir[160], target[96];
	int  pass, count, i, len;

	for ( pass = after + 1; pass <= 2; pass++ ) {
		bestPrefix[0] = 0;
		Q_strncpyz( dir, "demos", sizeof( dir ) );
		if ( pass == 2 ) {
			// df_ar_targetfolder: "./" (= the root) or a folder name, with or without slashes around it
			trap_Cvar_VariableStringBuffer( "df_ar_targetfolder", target, sizeof( target ) );
			for ( i = 0; target[i] == '.' || target[i] == '/' || target[i] == '\\'; ) {
				i++;
			}
			memmove( target, target + i, strlen( target + i ) + 1 );
			len = strlen( target );
			while ( len > 0 && ( target[len - 1] == '/' || target[len - 1] == '\\' ) ) {
				target[--len] = 0;
			}
			if ( !target[0] || len + 2 >= (int)sizeof( bestPrefix ) ) {
				continue;
			}
			Com_sprintf( dir, sizeof( dir ), "demos/%s", target );
			Com_sprintf( bestPrefix, sizeof( bestPrefix ), "%s/", target );
		}
		count = Best_ListFolder( dir, "", bestPassList, sizeof( bestPassList ) );
		if ( count > 0 ) {
			bestListCount = count;
			bestListPos = 0;
			bestListPtr = bestPassList;
			bestPass = pass;
			bestState = 3;
			return;
		}
	}
	bestState = 2;
}

/*
===============
UI_DF_BestLoad

Starts reading the demos for the current name: everything the first time (or when the name changed), later only the folders where
new demos appear.  Nothing happens while a read is going on.
===============
*/
void UI_DF_BestLoad( void ) {
	char raw[BEST_NAMESIZE], extra[256], key[sizeof( bestKey )], tok[BEST_NAMESIZE];
	const char *p;
	int  count, n;

	trap_Cvar_VariableStringBuffer( "name", raw, sizeof( raw ) );
	Q_CleanStr( raw );
	trap_Cvar_VariableStringBuffer( "df_ui_maps_playerNames", extra, sizeof( extra ) );
	Com_sprintf( key, sizeof( key ), "%s|%s", raw, extra );
	if ( bestState && !strcmp( key, bestKey ) ) {
		if ( bestState == 2 ) {
			Best_NextPass( 0 );
		}
		return;
	}
	bestCount = 0;
	Com_Memset( bestHash, 0, sizeof( bestHash ) );
	bestPoolUsed = 0;
	Q_strncpyz( bestKey, key, sizeof( bestKey ) );
	bestState = 2;

	// the names that are you: the current one, and the comma separated ones of df_ui_maps_playerNames (old nicknames)
	bestNumNames = 0;
	Best_Norm( raw, bestNames[0], sizeof( bestNames[0] ) );
	if ( bestNames[0][0] ) {
		bestNumNames = 1;
	}
	p = extra;
	while ( *p && bestNumNames < BEST_MAXNAMES ) {
		n = 0;
		while ( *p && *p != ',' && n < (int)sizeof( tok ) - 1 ) {
			tok[n++] = *p++;
		}
		tok[n] = 0;
		while ( *p && *p != ',' ) {
			p++;
		}
		if ( *p == ',' ) {
			p++;
		}
		Q_CleanStr( tok );
		Best_Norm( tok, bestNames[bestNumNames], sizeof( bestNames[0] ) );
		if ( bestNames[bestNumNames][0] ) {
			bestNumNames++;
		}
	}
	if ( !bestNumNames ) {
		return;
	}
	count = Best_ReadIndex();
	if ( count <= 0 ) {
		count = Best_ListFolder( "demos", "**/", bestList, sizeof( bestList ) );
	}
	if ( count <= 0 ) {
		Best_NextPass( 0 );		// no index, and an engine without the recursive listing: only the folders
		return;
	}
	bestPass = 0;
	bestPrefix[0] = 0;
	bestListCount = count;
	bestListPos = 0;
	bestListPtr = bestList;
	bestState = 1;
}

qboolean UI_DF_BestLoading( void ) {
	return bestState == 1 || bestState == 3;
}

// parses the demo list for about budgetMs milliseconds
void UI_DF_BestRun( int budgetMs ) {
	char full[200];
	int  begin, n = 0;

	if ( bestState != 1 && bestState != 3 ) {
		return;
	}
	begin = trap_Milliseconds();
	while ( bestListPos < bestListCount ) {
		if ( bestPrefix[0] ) {
			Com_sprintf( full, sizeof( full ), "%s%s", bestPrefix, bestListPtr );
			Best_Consider( full );
		} else {
			Best_Consider( bestListPtr );
		}
		bestListPtr += strlen( bestListPtr ) + 1;
		bestListPos++;
		if ( ( ++n & 63 ) == 0 && trap_Milliseconds() - begin >= budgetMs ) {
			return;
		}
	}
	Best_NextPass( bestState == 1 ? 0 : bestPass );
}

/*
===============
UI_DF_BestFor

The best time in ms of the map in a mode (0 VQ3, 1 CPM, 2 Tricks, 3 Fast Caps), 0 when there is no demo.  Tricks and Fast Caps
take the better of the two physics.  path: the demo's path relative to demos/ (upper case when it comes from the index file).
===============
*/
int UI_DF_BestFor( const char *map, int mode, const char **path ) {
	char		lower[32];
	bestEntry_t	*e;
	int			a, b, slot;

	if ( bestState == 0 ) {
		return 0;
	}
	Q_strncpyz( lower, map, sizeof( lower ) );
	Q_strlwr( lower );
	e = Best_Lookup( lower, qfalse );
	if ( !e ) {
		return 0;
	}
	if ( mode < 2 ) {
		a = b = BEST_RUN * 2 + mode;
	} else {
		a = ( mode == 2 ? BEST_TM : BEST_FC ) * 2;
		b = a + 1;
	}
	slot = ( e->ms[a] && ( !e->ms[b] || e->ms[a] <= e->ms[b] ) ) ? a : b;
	if ( !e->ms[slot] ) {
		return 0;
	}
	if ( path ) {
		*path = e->path[slot] ? bestPool + e->path[slot] - 1 : NULL;
	}
	return e->ms[slot];
}

void UI_DF_BestFormat( int ms, char *buf, int size ) {
	int m = ms / 60000, s = ( ms / 1000 ) % 60, f = ms % 1000;

	if ( m ) {
		Com_sprintf( buf, size, "%i:%02i.%03i", m, s, f );
	} else {
		Com_sprintf( buf, size, "%i.%03i", s, f );
	}
}

/*
===============
UI_DF_BestDemo

The name to give the `demo` command for the best demo of the map in a mode: its path below demos/ without the extension, with
the case of the real file (the index keeps upper case names).  Returns qfalse when there is none or the file is gone.
===============
*/
qboolean UI_DF_BestDemo( const char *map, int mode, char *out, int size ) {
	const char		*path, *slash, *file, *s;
	char			dir[128], full[200], *dot;
	fileHandle_t	f;
	int				count, i, len, prefixLen;

	if ( !UI_DF_BestFor( map, mode, &path ) || !path ) {
		return qfalse;
	}
	Q_strncpyz( out, path, size );

	// the file opens under the name it has here (a path from a folder listing, or any case where the file system ignores it)
	Com_sprintf( full, sizeof( full ), "demos/%s", path );
	len = trap_FS_FOpenFile( full, &f, FS_READ );
	if ( f ) {
		trap_FS_FCloseFile( f );
	}
	if ( len <= 0 ) {
		// the index keeps upper case names: find the file in its folder by name
		slash = strrchr( path, '/' );
		prefixLen = slash ? slash + 1 - path : 0;			// "SUB/"
		file = path + prefixLen;
		Q_strncpyz( dir, "demos", sizeof( dir ) );
		if ( prefixLen > 1 && prefixLen + 6 < (int)sizeof( dir ) && prefixLen < size ) {
			dir[5] = '/';
			Com_Memcpy( dir + 6, path, prefixLen - 1 );
			dir[6 + prefixLen - 1] = 0;
		}
		count = Best_ListFolder( dir, "", bestDirList, sizeof( bestDirList ) );
		s = bestDirList;
		for ( i = 0; i < count; i++, s += strlen( s ) + 1 ) {
			if ( !Q_stricmp( s, file ) ) {
				Com_Memcpy( out, path, prefixLen );
				Q_strncpyz( out + prefixLen, s, size - prefixLen );
				break;
			}
		}
		if ( i == count ) {
			return qfalse;		// the file is gone (an old index)
		}
	}
	dot = strrchr( out, '.' );
	if ( dot ) {
		*dot = 0;			// ".dm_68"
	}
	return qtrue;
}
