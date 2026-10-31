// ui_dfmaps.c -- DeFRaG map database of the start-map menu (ui.dis 0x1378 / 0x13a0 / 0x1444 / 0x1716 / 0x17b8).
// Two databases: type 1 "defrag" (scripts/*.defi) and type 2 "quake3" (scripts/arenas.txt + scripts/*.arena).
// Each file is a list of "{ key value ... }" records; entries are kept in a name-sorted linked list.

#include "ui_local.h"

#define DFMAP_MAXFILE	65536		// oDFs: DeFRaG 8192; a map pack's file can list many maps
#define DFMAP_LISTSIZE	262144		// oDFs: DeFRaG 16384, which held about 1000 file names: the rest of scripts/*.defi was never read

typedef struct uiDfMapEntry_s {
	char					map[32];		// +0
	char					longname[32];	// +32
	char					author[32];		// +64
	int						flags;			// +96  see UIDF_MAP_*
	struct uiDfMapEntry_s	*next;			// +100
} uiDfMapEntry_t;

typedef struct {
	int				count;
	uiDfMapEntry_t	*head;
	int				loaded;
} uiDfMapList_t;

typedef struct {
	int				type;
	uiDfMapEntry_t	*entries;
	uiDfMapList_t	*list;
	int				max;
	const char		*name;
	const char		*file;		// fixed file read first (or NULL)
	const char		*ext;		// extension scanned in scripts/
} uiDfMapDb_t;

#define DFMAP_MAX	8192		// oDFs: DeFRaG 4096

static uiDfMapEntry_t	dfmapDefragEntries[DFMAP_MAX];
static uiDfMapEntry_t	dfmapArenaEntries[DFMAP_MAX];
static uiDfMapList_t	dfmapDefragList;
static uiDfMapList_t	dfmapArenaList;

static uiDfMapDb_t dfmapDb[2] = {
	{ UIDF_MAPDB_DEFRAG, dfmapDefragEntries, &dfmapDefragList, DFMAP_MAX, "defrag", NULL, ".defi" },
	{ UIDF_MAPDB_QUAKE3, dfmapArenaEntries, &dfmapArenaList, DFMAP_MAX, "quake3", "scripts/arenas.txt", ".arena" },
};

static uiDfMapDb_t *UI_DF_MapDbFind( int type ) {		// 0x1378
	int i;

	for ( i = 0; i < 2; i++ ) {
		if ( dfmapDb[i].type == type ) {
			return &dfmapDb[i];
		}
	}
	return NULL;
}

// The entry is kept (the array slot it was read into is count): linking in name order is done once per database, by
// UI_DF_MapSort -- a sorted insert of every entry is quadratic and was the slowest part of reading a few thousand maps.
static void UI_DF_MapInsert( uiDfMapDb_t *db, uiDfMapEntry_t *e ) {		// 0x13a0
	if ( db->list->count >= db->max ) {
		return;
	}
	e->next = NULL;
	db->list->count++;
}

// name order, case-insensitive; of equal names the entry read later comes first (as when every entry was inserted before the first
// one that is not smaller)
static int UI_DF_MapCompare( const void *a, const void *b ) {
	const uiDfMapEntry_t *x = *(uiDfMapEntry_t * const *)a;
	const uiDfMapEntry_t *y = *(uiDfMapEntry_t * const *)b;
	int c = Q_stricmp( x->map, y->map );

	return c ? c : (int)( y - x );
}

static void UI_DF_MapSort( uiDfMapDb_t *db ) {
	static uiDfMapEntry_t *sorted[DFMAP_MAX];
	int i, n = db->list->count;

	for ( i = 0; i < n; i++ ) {
		sorted[i] = &db->entries[i];
	}
	qsort( sorted, n, sizeof( sorted[0] ), UI_DF_MapCompare );
	db->list->head = n ? sorted[0] : NULL;
	for ( i = 0; i < n; i++ ) {
		sorted[i]->next = i + 1 < n ? sorted[i + 1] : NULL;
	}
}

static qboolean UI_DF_MapBspExists( const char *name ) {	// 0x58a with "maps", "%s.bsp"
	fileHandle_t	f;
	int				len;

	len = trap_FS_FOpenFile( va( "maps/%s.bsp", name ), &f, FS_READ );
	if ( len > 0 ) {
		trap_FS_FCloseFile( f );
	}
	return len > 0;
}

static void UI_DF_MapReadFile( const char *filename, uiDfMapDb_t *db ) {		// 0x1444 MapList_ReadMapDef
	static char		text[DFMAP_MAXFILE];
	fileHandle_t	f;
	int				len;
	const char		*p, *tok;
	uiDfMapEntry_t	*e;

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		trap_Print( va( S_COLOR_RED "MapList_ReadMapDef(): %s not found.\n", filename ) );
		return;
	}
	if ( len >= DFMAP_MAXFILE ) {
		trap_Print( va( S_COLOR_RED "file too large: %s is %i, max allowed is %i\n", filename, len, DFMAP_MAXFILE ) );
		trap_FS_FCloseFile( f );
		return;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	p = text;
	tok = COM_Parse( &p );
	if ( tok[0] != '{' ) {
		Com_Printf( "%s: Missing opening {\n", filename );
		return;
	}
	do {
		qboolean needBsp = qfalse;

		e = &db->entries[db->list->count];
		tok = COM_Parse( &p );
		while ( tok[0] && tok[0] != '}' ) {
			if ( !Q_stricmp( tok, "map" ) ) {
				tok = COM_Parse( &p );
				Q_strncpyz( e->map, tok, 32 );
			} else if ( !Q_stricmp( tok, "longname" ) ) {
				tok = COM_Parse( &p );
				Q_strncpyz( e->longname, tok, 32 );
			} else if ( !Q_stricmp( tok, "revision" ) ) {
				needBsp = qtrue;				// the value token is read as a key and ignored below
			} else if ( !Q_stricmp( tok, "author" ) ) {
				tok = COM_Parse( &p );
				Q_strncpyz( e->author, tok, 32 );
			} else if ( !Q_stricmp( tok, "vq3" ) ) {
				tok = COM_Parse( &p );
				if ( atoi( tok ) ) {
					e->flags |= UIDF_MAP_VQ3;
				}
			} else if ( !Q_stricmp( tok, "cpm" ) ) {
				tok = COM_Parse( &p );
				if ( atoi( tok ) ) {
					e->flags |= UIDF_MAP_CPM;
				}
			} else if ( !Q_stricmp( tok, "type" ) || !Q_stricmp( tok, "style" ) ) {
				tok = COM_Parse( &p );
				if ( !Q_stricmp( tok, "run" ) ) {
					e->flags |= UIDF_MAP_RUN;
				} else if ( !Q_stricmp( tok, "training" ) ) {
					e->flags |= UIDF_MAP_TRAINING;
				} else if ( !Q_stricmp( tok, "level" ) ) {
					e->flags |= UIDF_MAP_LEVEL;
				} else if ( !Q_stricmp( tok, "accuracy" ) ) {
					e->flags |= UIDF_MAP_ACCURACY;
				} else if ( strstr( tok, "ctf" ) ) {
					e->flags |= UIDF_MAP_CTF;
				}
			} else {
				SkipRestOfLine( &p );
			}
			tok = COM_Parse( &p );
		}
		if ( !( e->flags & ( UIDF_MAP_VQ3 | UIDF_MAP_CPM ) ) ) {
			e->flags |= UIDF_MAP_VQ3;
		}
		if ( e->map[0] && ( !needBsp || UI_DF_MapBspExists( e->map ) ) ) {
			UI_DF_MapInsert( db, e );
		} else {
			Com_Memset( e, 0, sizeof( *e ) );
		}
		tok = COM_Parse( &p );
	} while ( tok[0] );
}

/*
===============
UI_DF_MapsLoadStep (0x1716, in slices)

Reads the map files of both databases for about budgetMs milliseconds, then returns: qtrue when everything is in. The map browser
calls it once per frame, so it can show itself (and stay usable) while a few thousand small files are read.
===============
*/
static struct {
	qboolean	started;
	int			dbIndex;			// the database being read, 2 = both done
	qboolean	reading;			// the file list of dbIndex is made
	int			fileCount, filePos;
	const char	*filePtr;
	int			startTime;
	char		names[DFMAP_LISTSIZE];
} dfmapLoad;

qboolean UI_DF_MapsLoadStep( int budgetMs ) {
	int begin = trap_Milliseconds(), n = 0;

	if ( !dfmapLoad.started ) {
		dfmapLoad.started = qtrue;
		dfmapLoad.startTime = begin;
	}
	while ( dfmapLoad.dbIndex < 2 ) {
		uiDfMapDb_t	*db = &dfmapDb[dfmapLoad.dbIndex];

		if ( !dfmapLoad.reading ) {
			if ( db->file ) {
				UI_DF_MapReadFile( db->file, db );
			}
			dfmapLoad.fileCount = trap_FS_GetFileList( "scripts", db->ext, dfmapLoad.names, sizeof( dfmapLoad.names ) );
			dfmapLoad.filePos = 0;
			dfmapLoad.filePtr = dfmapLoad.names;
			dfmapLoad.reading = qtrue;
		}
		while ( dfmapLoad.filePos < dfmapLoad.fileCount && db->list->count < db->max ) {
			char name[128];
			int len = strlen( dfmapLoad.filePtr );

			Com_sprintf( name, sizeof( name ), "scripts/%s", dfmapLoad.filePtr );
			UI_DF_MapReadFile( name, db );
			dfmapLoad.filePtr += len + 1;
			dfmapLoad.filePos++;
			if ( ( ++n & 3 ) == 0 && trap_Milliseconds() - begin >= budgetMs ) {
				return qfalse;
			}
		}
		UI_DF_MapSort( db );
		Com_Printf( "Found %i %s maps.\n", db->list->count, db->name );
		db->list->loaded = 1;
		dfmapLoad.reading = qfalse;
		dfmapLoad.dbIndex++;
		if ( trap_Milliseconds() - begin >= budgetMs ) {
			break;
		}
	}
	if ( dfmapLoad.dbIndex < 2 ) {
		return qfalse;
	}
	if ( dfmapLoad.startTime ) {
		Com_Printf( "Map files read in %i ms (over several frames).\n", trap_Milliseconds() - dfmapLoad.startTime );
		dfmapLoad.startTime = 0;
	}
	return qtrue;
}

// how many maps are in so far, for the progress text
int UI_DF_MapsLoaded( void ) {
	return dfmapDefragList.count + dfmapArenaList.count;
}

// Iterates a map database (0x17b8): returns the first/next entry after `prev` whose flags match both masks
// (flags & include) != 0 and (flags & exclude2) != 0 -- DeFRaG passes a mode mask and a map-type mask.
const uiDfMapEntry_t *UI_DF_MapNext( int type, int maskA, int maskB, const uiDfMapEntry_t *prev ) {
	uiDfMapDb_t		*db;
	uiDfMapEntry_t	*e;

	db = UI_DF_MapDbFind( type );
	if ( !db ) {
		return NULL;
	}
	if ( !db->list->loaded ) {
		while ( !UI_DF_MapsLoadStep( 1000000 ) ) {
		}
	}
	e = prev ? prev->next : db->list->head;
	for ( ; e; e = e->next ) {
		if ( ( e->flags & maskA ) && ( e->flags & maskB ) ) {
			return e;
		}
	}
	return NULL;
}
