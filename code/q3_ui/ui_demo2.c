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
/*
=======================================================================

DEMOS MENU

=======================================================================
*/


#include "ui_local.h"

/*
DeFRaG demo browser (ui.qvm 0xe999..0xf46c, decoded in iteration 134).

Differences to the stock q3_ui demo menu:
 - the list is directory aware: sub folders of demos/ are listed first as "/name" (and "/.." inside a folder);
   activating one enters it.  The current folder is kept in df_demos_CurrentPath and the list scroll/cursor in
   df_demos_CurrentListVars ("top cursor") so that the menu comes back where it was after a demo ended
 - layout: the list is 65x21 at (10,75), a read-only FOLDER: field, vertical scroll arrows (PgUp/PgDn) and four
   option spinners (Replay / Mod Support / Validation / Time Demo) bound to df_demos_ReplayMode,
   df_demos_ModSupport, df_demos_validation and timedemo
   (oDFs: the arrows and the spinners are gone, a scroll bar sits next to the list instead, and the title is small at the
   top right.  The four settings are plain cvars now; the browser no longer writes them.)
 - double click on a list entry, Enter / Right arrow, middle mouse and Backspace / Left arrow (up one folder)
 - starting a demo also hands "#1 demo <name>" to the cgame ghost system through df_ghosts_Command
 - names are upper-cased like the stock menu (0x1e7ba is Q_strupr: it maps through 0x1c190 = toupper; iteration 134 read it as lower-casing)
 (The DeFRaG flag word at ui 0x12390, bit 0 = keep case, bit 1 = flat list, is never written in the qvm: both bits are 0.)

oDFs additions (not in DeFRaG):
 - SEARCH box (under FOLDER:): case insensitive "contains" filter on the demo / folder names of the current folder.
   Typing a character anywhere in the menu jumps to the box, Enter hands focus back to the list.  Folders are filtered
   too, "/.." always stays.  The text is kept in df_demos_Search so the filtered list is still there when a demo ended;
   entering a folder clears it.  Wildcards make it an anchored glob (Demos_Glob): * ? and **; a '/' or ** in the term
   searches all folders below the current one too (Demos_LoadTree).
 - a demo started from the browser returns to the browser (on top of the main menu) when it is over
   (df_demos_ReturnToBrowser, UI_DemosReturnAfterPlayback).
 - sized for tens of thousands of demos: the full folder listing is kept in alllist[] (qsort, O(n log n)) and the shown
   list is filtered from it; while the term only grows the previous result is narrowed instead of rescanning everything.
*/

#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define SCROLLBAR_X			620			// at the right edge of the screen (the list ends at x = 10 + 65 * 8 = 530)
#define SCROLLBAR_W			16
#define SCROLLBAR_MINTHUMB	20
#define FIELD_PLATE_X		100			// FOLDER / SEARCH: plate left edge; the text starts 4 pixels in (FIELD_X)
#define FIELD_X				104
#define FIELD_CHARS			53			// plate = 53 * 8 + 8 pixels = as wide as the list plus its scroll bar gap
#define BUTTON_W			96
#define BUTTON_H			30
#define BUTTON_Y			438
#define BUTTON_MARGIN		12

#define MAX_DEMOS			65536		// entries of one folder (sub folders included)
#define MAX_DEMODIRS		1024
#define DIRBUFSIZE			65536
#define FILEBUFSIZE			4194304		// ~64 bytes per demo name at MAX_DEMOS
#define NAMEBUFSIZE			( DIRBUFSIZE + FILEBUFSIZE )
#define DEMOPATH_SIZE		256
#define DEMONAME_SIZE		256
#define DOUBLECLICK_MSEC	600
#define SEARCH_MAXCHARS		64
#define TREEBUFSIZE			4194304		// names of the recursive listing (every demo below the current folder)
#define MAX_CRAWLDIRS		2048		// fallback crawl (engines without the "**/" listing): sub folders visited
#define CRAWLQ_SIZE			131072
#define MAX_CRAWLDEPTH		8
#define GLOB_MAXSTEPS		2000		// backtracking budget of one glob match
#define MAX_WORDS			32768		// word completion: distinct words of the names that are searched
#define WORDBUF_SIZE		524288
#define WORD_HASHSIZE		65536		// power of two, at least twice MAX_WORDS
#define WORD_MINLEN			3
#define WORD_MAXLEN			40
#define WORDS_SLICE_MS		6			// build time per frame
#define SUGGEST_MAX			8
#define SUGGEST_MINFRAG		2			// typed letters before a word is proposed
#define SUGGEST_ROW_H		( SMALLCHAR_HEIGHT + 2 )
#define SUGGEST_X			FIELD_PLATE_X
#define SUGGEST_Y			47			// just under the search plate
#define SUGGEST_W			( FIELD_CHARS * SMALLCHAR_WIDTH + 8 )

#define ID_BACK				10
#define ID_GO				11
#define ID_LIST				12
#define ID_SEARCH			15

typedef struct {
	int		listId;			// the id a double click is valid for (ID_LIST)
	int		lastId;			// id of the item that sent the last menu event
	int		lastIndex;
	int		lastTime;
} demoClick_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menutext_s		back;				// BACK / PLAY: drawn by Demos_ButtonDraw
	menutext_s		go;

	menulist_s		list;
	menufield_s		folder;
	menufield_s		search;
	menutext_s		count;

	qboolean		keyNav;				// the last input was a navigation key (Tab / arrows / Enter): the focused button is lit
	int				lastCursorX, lastCursorY;	// the pointer moving ends keyNav (Demos_MenuDraw)
	qboolean		sbDrag;				// the scroll bar thumb is being dragged
	int				sbGrab;				// cursor offset inside the thumb while dragging

	demoClick_t		click;

	int				numDirs;			// folders in demolist (the shown list)
	int				numAll;				// entries of the folder, before the search filter
	int				numAllDirs;
	qboolean		empty;				// demolist only holds a placeholder line
	qboolean		filterValid;		// demolist is the result of filtering alllist with 'applied'
	qboolean		treeMode;			// the shown list is filtered from tlist (all sub folders) instead of alllist (this folder)
	qboolean		treeValid;			// tlist is loaded for the current folder (kept while the menu is open)
	qboolean		crawling;			// fallback listing in progress (Demos_CrawlRun, driven by Demos_MenuDraw)
	int				crawlQi, crawlN, crawlUsed, crawlStart;
	qboolean		crawlTruncated;
	char			loadText[48];		// the "Indexing..." placeholder line
	// the demo index (see THE INDEX): every demo below demos/, sorted; strings in tnames[0..tused)
	qboolean		indexLoaded;		// ilist holds the index
	qboolean		indexFresh;			// it was scanned just now (a file that was read needs Demos_IndexMerge)
	qboolean		indexMerged;		// the demos root and the autorecord folder were merged during this visit
	qboolean		indexDirty;			// changed since it was written
	qboolean		forceScan;			// F5: scan instead of reading the file
	int				indexStamp;			// hour stamp of the scan the index comes from
	int				numIndex;
	int				tused;
	char			dotdot[8];
	int				numTree;
	int				numTreeDirs;
	qboolean		appliedGlob;		// 'applied' was a glob pattern (not a plain "contains" term)
	char			applied[SEARCH_MAXCHARS + 1];	// upper-cased search term demolist was built for
	char			countText[64];
	// word completion of the search text (see WORD COMPLETION)
	qboolean		wordsValid;			// words[] holds the words of the list that is searched (wordKey*)
	qboolean		wordsBuilding;
	int				listGen;			// counts Demos_LoadFolder runs
	int				wordKeyTree, wordKeyGen, wordKeyTotal;
	int				wordPos;			// next entry of the list to take words from
	int				numWords, wordsUsed;
	qboolean		sugValid;			// the suggestions were worked out for sugText / sugCursor
	char			sugText[SEARCH_MAXCHARS + 1];
	int				sugCursor;
	char			sugFrag[WORD_MAXLEN + 1];	// the word being typed, lower case
	int				sugFragLen;
	int				numSug, sugSel;
	qboolean		sugMoved;			// the selection was moved with the arrow keys
	int				sug[SUGGEST_MAX];	// indexes into words[], best first
	int				sugScore[SUGGEST_MAX];
	char			sugCommon[WORD_MAXLEN + 1];	// what all words that start with the typed letters have in common
	int				sugCommonLen;
	char			path[DEMOPATH_SIZE];
	char			dirs[DIRBUFSIZE];
	char			crawlq[CRAWLQ_SIZE];	// folders still to visit by the recursive listing (original case, relative)
	char			*crawl[MAX_CRAWLDIRS];
	char			names[NAMEBUFSIZE];
	char			tnames[TREEBUFSIZE];	// the index strings: "A/B/NAME.DM_68", upper-cased
	char			*ilist[MAX_DEMOS];		// the index: every demo below demos/, sorted (Q_stricmp)
	int				mergePos[MAX_DEMOS];	// Demos_IndexMergeFolder work arrays
	char			*mergeAdds[MAX_DEMOS];
	char			*tlist[MAX_DEMOS];		// the view: "/.." (inside a folder) and every demo below the current folder, sorted
	char			*alllist[MAX_DEMOS];	// every entry of the folder: folders first, then demos, sorted
	char			*demolist[MAX_DEMOS];	// the entries matching the search (what the scroll list shows)
	char			*words[MAX_WORDS];		// word completion: the distinct words, lower case, strings in wordBuf
	int				wordCount[MAX_WORDS];	// how often each one occurs
	int				wordHash[WORD_HASHSIZE];	// open addressing table of word index + 1
	char			wordBuf[WORDBUF_SIZE];
} demos_t;

static demos_t	s_demos;



// 0x4188 / 0x4173 / 0x4197: double click tracker
static void Demos_ClickReset( void ) {
	s_demos.click.lastIndex = -1;
	s_demos.click.lastTime = -1;
}

static qboolean Demos_DoubleClick( int index ) {
	int now = trap_Milliseconds();

	if ( now - s_demos.click.lastTime <= DOUBLECLICK_MSEC && s_demos.click.lastIndex == index
		&& s_demos.click.lastId == s_demos.click.listId && s_demos.click.listId != 0 ) {
		Demos_ClickReset();
		return qtrue;
	}
	s_demos.click.lastIndex = index;
	s_demos.click.lastTime = now;
	return qfalse;
}

// 0xe999: case insensitive compare
static int Demos_Compare( const char *a, const char *b ) {
	while ( ( *a == *b || ( *a ^ 32 ) == *b ) && *a ) {
		a++;
		b++;
	}
	return tolower( *(const unsigned char *)a ) - tolower( *(const unsigned char *)b );
}

// 0xe9e0: folders ("/name", "/.." first) before demos, then alphabetical.
// DeFRaG bubble sorts (O(n^2)); qsort keeps a folder with tens of thousands of demos usable
static int Demos_SortCompare( const void *a, const void *b ) {
	const char	*x = *(const char * const *)a;
	const char	*y = *(const char * const *)b;
	qboolean	fx = ( x[0] == '/' );
	qboolean	fy = ( y[0] == '/' );

	if ( fx != fy ) {
		return fx ? -1 : 1;
	}
	if ( fx ) {
		if ( !strcmp( x, "/.." ) ) {
			return -1;
		}
		if ( !strcmp( y, "/.." ) ) {
			return 1;
		}
	}
	return Demos_Compare( x, y );
}

/*
===============
Demos_Glob

Glob match of an upper-cased pattern against an upper-cased path ("SUB/NAME", no extension):
  *         any run of characters except '/'
  **        any run of characters including '/'
  ** and a slash: any number of directories, including none, so "two stars, slash, star, NAME, star" finds NAME in
            this folder and in every folder below it
  ?         one character except '/'
The match is anchored: "*X*" contains X, "X*" starts with X, "*X" ends with X.  Backtracking is bounded by
GLOB_MAXSTEPS (a pattern that needs more simply does not match).
===============
*/
static int demosGlobSteps;

static qboolean Demos_Glob( const char *p, const char *s ) {
	while ( *p ) {
		if ( *p == '*' ) {
			qboolean	deep = ( p[1] == '*' );
			const char	*rest = p;
			char		literal;

			while ( *rest == '*' ) {
				rest++;
			}
			if ( deep && *rest == '/' ) {
				// "**/": any number of directories, including none
				rest++;
				for ( ;; ) {
					if ( ++demosGlobSteps > GLOB_MAXSTEPS ) {
						return qfalse;
					}
					if ( Demos_Glob( rest, s ) ) {
						return qtrue;
					}
					while ( *s && *s != '/' ) {
						s++;
					}
					if ( !*s ) {
						return qfalse;
					}
					s++;
				}
			}
			if ( !*rest ) {
				return deep || !strchr( s, '/' );
			}
			// when a plain character follows, only try the positions where it occurs
			literal = ( *rest != '?' && *rest != '/' ) ? *rest : 0;
			for ( ;; ) {
				if ( literal ) {
					while ( *s && *s != literal && ( deep || *s != '/' ) ) {
						s++;
					}
					if ( *s != literal ) {
						return qfalse;
					}
				}
				if ( ++demosGlobSteps > GLOB_MAXSTEPS ) {
					return qfalse;
				}
				if ( Demos_Glob( rest, s ) ) {
					return qtrue;
				}
				if ( !*s || ( !deep && *s == '/' ) ) {
					return qfalse;
				}
				s++;
			}
		}
		if ( *p == '?' ) {
			if ( !*s || *s == '/' ) {
				return qfalse;
			}
		} else if ( *p != *s ) {
			return qfalse;
		}
		p++;
		s++;
	}
	return *s == 0;
}

/*
===============
Demos_Matches

Case insensitive test of the (upper-cased) search term against one list entry: "contains" for a plain term, an
anchored glob match (Demos_Glob) when the term has wildcards.  The list stores its names upper-cased (Q_strupr in
Demos_LoadFolder), so the term only has to be upper-cased once.  The leading '/' of a folder and the ".DM_NN"
extension of a demo are not searched; "/.." matches every term so a sub folder can always be left.
===============
*/
static qboolean Demos_Matches( const char *name, const char *term, int termLen, qboolean glob ) {
	const char	*p, *last, *dot;
	char		first = term[0];
	int			len;

	if ( name[0] == '/' ) {
		if ( !strcmp( name, "/.." ) ) {
			return qtrue;
		}
		name++;
		len = strlen( name );
	} else {
		len = strlen( name );
		for ( dot = name + len; dot > name && *dot != '.' && name + len - dot < 8; dot-- )
			;
		if ( *dot == '.' && !Q_strncmp( dot, ".DM_", 4 ) ) {
			len = dot - name;
		}
	}

	if ( glob ) {
		// match on the name without its extension: cut the string there for the duration of the match
		// (all list strings live in our own writable buffers)
		char		*end = (char *)name + len;
		char		saved = *end;
		qboolean	result;

		*end = 0;
		demosGlobSteps = 0;
		result = Demos_Glob( term, name );
		*end = saved;
		return result;
	}

	last = name + len - termLen;
	for ( p = name; p <= last; p++ ) {
		if ( *p == first && !Q_strncmp( p, term, termLen ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
===============
Demos_ApplyFilter

Rebuilds demolist (what the scroll list shows) from alllist for the current search text.  Returns qfalse when the text
did not change since the last call.  The selection goes back to the top of the list.

The term decides what is searched:
  - plain text ("asd")                  contains, on the names of the current folder
  - wildcards ("*asd*", "a?d*")         anchored glob on the names of the current folder
  - a '/' or "**" in the term           the demos of the current folder AND all folders below it (filtered from tlist, the
                                        recursive listing: entries are relative paths "SUB/NAME.DM_68"); plain text
                                        with a '/' is a "contains" on that path, otherwise it is a glob on the path
Both lists stay loaded while the menu is open, terms that match everything skip the per entry test, and a plain term
that only grows is filtered from the previous result.
===============
*/
static void Demos_LoadFolder( void );
static void Demos_LoadTree( void );

static qboolean Demos_ApplyFilter( void ) {
	char		term[SEARCH_MAXCHARS + 1];
	int			termLen, appliedLen, i, j, n, dirs;
	char		**src;
	char		*e;
	int			total, totalDirs, start = trap_Milliseconds();
	qboolean	narrow, glob, hasSlash, needTree, matchAll;

	Q_strncpyz( term, s_demos.search.field.buffer, sizeof( term ) );
	Q_strupr( term );
	termLen = strlen( term );

	if ( s_demos.filterValid && !strcmp( term, s_demos.applied ) ) {
		return qfalse;
	}

	glob = qfalse;
	hasSlash = qfalse;
	for ( i = 0; i < termLen; i++ ) {
		if ( term[i] == '*' || term[i] == '?' ) {
			glob = qtrue;
		} else if ( term[i] == '/' ) {
			hasSlash = qtrue;
		}
	}
	needTree = hasSlash || strstr( term, "**" ) != NULL;
	if ( needTree && !s_demos.treeValid && !s_demos.crawling ) {
		// the view of everything below this folder: kept while the menu is open (until the folder changes), the
		// index behind it comes from the index file
		Demos_LoadTree();
		s_demos.filterValid = qfalse;
	}
	if ( needTree != s_demos.treeMode ) {
		s_demos.treeMode = needTree;
		s_demos.filterValid = qfalse;
	}
	appliedLen = strlen( s_demos.applied );

	if ( s_demos.treeMode && s_demos.crawling ) {
		// the recursive list is still being built (engine without the one call listing): a placeholder line until it is done
		Com_sprintf( s_demos.loadText, sizeof( s_demos.loadText ), "Indexing sub folders... %i%%", s_demos.crawlQi * 100 / s_demos.crawlN );
		s_demos.demolist[0] = s_demos.loadText;
		s_demos.list.numitems = 1;
		s_demos.numDirs = 0;
		s_demos.list.top = 0;
		s_demos.list.curvalue = 0;
		s_demos.empty = qtrue;
		s_demos.go.generic.flags |= ( QMF_INACTIVE | QMF_HIDDEN );
		s_demos.countText[0] = 0;
		Q_strncpyz( s_demos.applied, term, sizeof( s_demos.applied ) );
		s_demos.appliedGlob = glob;
		s_demos.filterValid = qtrue;
		trap_Cvar_Set( "df_demos_Search", s_demos.search.field.buffer );
		return qtrue;
	}

	// terms that match every entry need no per entry work: "*" ("**") on names, "**", "**" and "/*", "**" and "/**" on paths
	matchAll = ( termLen == 0 );
	if ( !matchAll ) {
		if ( s_demos.treeMode ) {
			matchAll = !strcmp( term, "**" ) || !strcmp( term, "**/*" ) || !strcmp( term, "**/**" );
		} else {
			matchAll = glob && !hasSlash;
			for ( i = 0; i < termLen && matchAll; i++ ) {
				if ( term[i] != '*' ) {
					matchAll = qfalse;
				}
			}
		}
	}

	// a plain term that extends the applied plain term can only match entries of the current result
	narrow = s_demos.filterValid && !glob && !s_demos.appliedGlob && appliedLen > 0 && termLen > appliedLen
		&& !Q_strncmp( term, s_demos.applied, appliedLen );
	if ( narrow && s_demos.empty ) {
		Q_strncpyz( s_demos.applied, term, sizeof( s_demos.applied ) );
		trap_Cvar_Set( "df_demos_Search", s_demos.search.field.buffer );
		return qtrue;
	}
	if ( s_demos.treeMode ) {
		total = s_demos.numTree;
		totalDirs = s_demos.numTreeDirs;
	} else {
		total = s_demos.numAll;
		totalDirs = s_demos.numAllDirs;
	}
	if ( narrow ) {
		src = s_demos.demolist;
		n = s_demos.list.numitems;
	} else {
		src = s_demos.treeMode ? s_demos.tlist : s_demos.alllist;
		n = total;
	}

	j = 0;
	dirs = 0;
	if ( matchAll && !narrow ) {
		for ( i = 0; i < n; i++ ) {
			s_demos.demolist[i] = src[i];
		}
		j = n;
		dirs = totalDirs;
	} else {
		for ( i = 0; i < n; i++ ) {
			e = src[i];
			if ( termLen && !Demos_Matches( e, term, termLen, glob ) ) {
				continue;
			}
			s_demos.demolist[j++] = e;		// j <= i, so filtering demolist in place is safe
			if ( e[0] == '/' ) {
				dirs++;
			}
		}
	}

	s_demos.empty = ( j == 0 );
	if ( s_demos.empty ) {
		s_demos.demolist[0] = (char *)( total ? "No Matches." : "No Demos Found." );
		j = 1;
		dirs = 0;
		s_demos.go.generic.flags |= ( QMF_INACTIVE | QMF_HIDDEN );
		Com_sprintf( s_demos.countText, sizeof( s_demos.countText ), "%s", total ? "0 matches" : "" );
	} else {
		s_demos.go.generic.flags &= ~( QMF_INACTIVE | QMF_HIDDEN );
		if ( termLen ) {
			Com_sprintf( s_demos.countText, sizeof( s_demos.countText ), "%i of %i%s", j - dirs, total - totalDirs,
				s_demos.treeMode ? " (with sub folders, F5 rescans)" : "" );
		} else {
			Com_sprintf( s_demos.countText, sizeof( s_demos.countText ), "%i demos", j - dirs );
		}
	}
	s_demos.list.numitems = j;
	s_demos.numDirs = dirs;
	s_demos.list.top = 0;
	s_demos.list.curvalue = 0;

	Q_strncpyz( s_demos.applied, term, sizeof( s_demos.applied ) );
	s_demos.appliedGlob = glob;
	s_demos.filterValid = qtrue;
	trap_Cvar_Set( "df_demos_Search", s_demos.search.field.buffer );
	if ( trap_Cvar_VariableValue( "developer" ) ) {
		Com_Printf( "Demos: search '%s' -> %i entries (%s) in %i ms\n", term, s_demos.empty ? 0 : j, s_demos.treeMode ? "with sub folders" : "this folder",
			trap_Milliseconds() - start );
	}
	return qtrue;
}

// returns the offset behind the last of n consecutive strings
static int Demos_SkipStrings( const char *p, int n ) {
	const char *start = p;

	while ( n-- > 0 ) {
		p += strlen( p ) + 1;
	}
	return p - start;
}

/*
===============
Demos_ListDemoFiles

The demo files of one folder, as consecutive strings in buf: "dm_*" (any protocol) or "dm_<protocol>" below protocol 68.
The engine here lists exact extensions only, so when "dm_*" finds nothing the stock ioq3 lookup (com_protocol, then
com_legacyprotocol) is used instead.  Returns the number of names.

recursive: oDFe extension (see FS_GetFileList): an extension that starts with two stars and a slash lists the folder AND
everything below it in one call, names relative to dirpath ("SUB/DEEP/NAME.DM_68", '/' separated, sorted).  Other
engines match nothing for such an extension, so 0 means "empty or not supported" and the caller falls back to
the crawl (Demos_CrawlStart).
===============
*/
static int Demos_ListDemoFiles( const char *dirpath, char *buf, int bufsize, qboolean recursive ) {
	char		ext[32];
	const char	*pre = recursive ? "**/" : "";
	int			numFiles, protocol, legacy;

	protocol = (int)trap_Cvar_VariableValue( "protocol" );
	if ( protocol >= 68 ) {
		Com_sprintf( ext, sizeof( ext ), "%sdm_*", pre );
	} else {
		Com_sprintf( ext, sizeof( ext ), "%sdm_%d", pre, protocol );
	}
	numFiles = trap_FS_GetFileList( dirpath, ext, buf, bufsize );
	if ( numFiles <= 0 && protocol >= 68 ) {
		protocol = (int)trap_Cvar_VariableValue( "com_protocol" );
		legacy = (int)trap_Cvar_VariableValue( "com_legacyprotocol" );
		numFiles = 0;
		if ( protocol > 0 ) {
			Com_sprintf( ext, sizeof( ext ), "%sdm_%d", pre, protocol );
			numFiles = trap_FS_GetFileList( dirpath, ext, buf, bufsize );
		}
		if ( legacy > 0 && legacy != protocol && numFiles >= 0 ) {
			int used = Demos_SkipStrings( buf, numFiles );
			Com_sprintf( ext, sizeof( ext ), "%sdm_%d", pre, legacy );
			numFiles += trap_FS_GetFileList( dirpath, ext, buf + used, bufsize - used );
		}
	}
	return numFiles < 0 ? 0 : numFiles;
}

/*
===============
Demos_LoadFolder (0xeb9f)

alllist = the entries of the current folder: "/.." (inside a folder), the sub folders as "/NAME" (from
FS_GetFileList( "demos/<path>", "/" )), then the demos, sorted.
===============
*/
static void Demos_LoadFolder( void ) {
	char	dirpath[DEMOPATH_SIZE + 8];
	const char	*p;
	char	*d;
	int		i, len, found, off, numFiles;

	s_demos.listGen++;
	s_demos.treeMode = qfalse;
	s_demos.treeValid = qfalse;			// the recursive listing belongs to the folder, load it again when it is needed
	s_demos.crawling = qfalse;
	s_demos.numTree = 0;
	s_demos.numTreeDirs = 0;
	s_demos.numAll = 0;
	s_demos.numAllDirs = 0;
	s_demos.filterValid = qfalse;
	s_demos.list.numitems = 0;
	s_demos.list.curvalue = 0;
	s_demos.list.top = 0;

	Com_sprintf( dirpath, sizeof( dirpath ), "%s/%s", "demos", s_demos.path );
	found = trap_FS_GetFileList( dirpath, "/", s_demos.dirs, sizeof( s_demos.dirs ) );
	if ( found <= 0 && s_demos.path[0] ) {
		// the remembered folder is gone: back to the top
		s_demos.path[0] = 0;
		Com_sprintf( dirpath, sizeof( dirpath ), "%s/%s", "demos", s_demos.path );
		found = trap_FS_GetFileList( dirpath, "/", s_demos.dirs, sizeof( s_demos.dirs ) );
	}
	if ( found > MAX_DEMODIRS ) {
		found = MAX_DEMODIRS;
	}

	off = 0;
	if ( s_demos.path[0] ) {
		Q_strncpyz( s_demos.names, "/..", 4 );
		off = 4;
		s_demos.numAllDirs++;
	}
	d = s_demos.dirs;
	for ( i = 0; i < found; i++, d += len + 1 ) {
		len = strlen( d );
		if ( !len || !strcmp( d, "." ) || !strcmp( d, ".." ) ) {
			continue;
		}
		if ( off + len + 2 > DIRBUFSIZE ) {
			break;
		}
		Q_strncpyz( s_demos.names + off, "/", 2 );
		off += 1;
		Q_strupr( d );
		Q_strncpyz( s_demos.names + off, d, len + 1 );
		off += len + 1;
		s_demos.numAllDirs++;
	}

	numFiles = Demos_ListDemoFiles( dirpath, s_demos.names + off, FILEBUFSIZE, qfalse );
	if ( numFiles > MAX_DEMOS - s_demos.numAllDirs ) {
		numFiles = MAX_DEMOS - s_demos.numAllDirs;
	}
	if ( numFiles >= MAX_DEMOS - s_demos.numAllDirs ) {
		Com_Printf( "^3Demos: only the first %i demos of this folder are listed\n", numFiles );
	}
	s_demos.numAll = numFiles + s_demos.numAllDirs;

	p = s_demos.names;
	for ( i = 0; i < s_demos.numAllDirs; i++ ) {
		s_demos.alllist[i] = (char *)p;
		p += strlen( p ) + 1;
	}
	d = s_demos.names + off;
	for ( ; i < s_demos.numAll; i++ ) {
		s_demos.alllist[i] = d;
		len = strlen( d );
		if ( len >= 4 && !strcmp( d + len - 4, ".dm3" ) ) {
			d[len - 4] = 0;
		}
		Q_strupr( d );
		d += len + 1;
	}

	qsort( s_demos.alllist, s_demos.numAll, sizeof( char * ), Demos_SortCompare );
}

/*
===============
THE INDEX

The recursive listing (every demo below demos/) is kept in a file, df_demos_index.dat in the game folder, so opening the
browser does not scan the whole tree again.  In memory it is ilist[numIndex] (sorted, case insensitive, upper-cased paths
relative to demos/, strings in tnames[0..tused)); the part below the current folder is the view tlist (Demos_BuildView).

 - Demos_IndexRead: loads the file when it is there, well formed and not older than df_demos_indexMaxAge hours.
 - Demos_IndexScan: lists the whole tree (one engine call on oDFe, the sliced crawl elsewhere) and writes the file.
 - Demos_IndexMerge: a loaded file is brought up to date for the places demos normally appear, with a listing that is
   cheap anyway: the folder that is being browsed (its list is already loaded), the demos root and the autorecord target
   folder (df_ar_targetfolder).  New demos are added, demos that are gone are dropped; the file is rewritten if that
   changed anything.  Demos recorded somewhere else are picked up by a rescan: F5 in the browser, or automatically when
   the file gets older than df_demos_indexMaxAge hours (0 = scan every time).
===============
*/
#define DEMOS_INDEX_FILE	"df_demos_index.dat"
#define DEMOS_INDEX_HEADER	64
#define DEMOS_INDEX_MAGIC	"DFDI1"

static int Demos_HourStamp( void ) {
	qtime_t	t;

	trap_RealTime( &t );
	return ( t.tm_year * 366 + t.tm_yday ) * 24 + t.tm_hour;
}

static int Demos_SortCompareCI( const void *a, const void *b ) {
	return Q_stricmp( *(const char * const *)a, *(const char * const *)b );
}

static void Demos_IndexWrite( void ) {
	fileHandle_t	f;
	char			header[DEMOS_INDEX_HEADER];
	char			*stage = s_demos.dirs;		// 64 KB write buffer (not in use between listings)
	int				i, len, fill = 0, bytes = 0;

	for ( i = 0; i < s_demos.numIndex; i++ ) {
		bytes += strlen( s_demos.ilist[i] ) + 1;
	}
	trap_FS_FOpenFile( DEMOS_INDEX_FILE, &f, FS_WRITE );
	if ( !f ) {
		return;
	}
	memset( header, 0, sizeof( header ) );
	Com_sprintf( header, sizeof( header ), "%s %i %i %i\n", DEMOS_INDEX_MAGIC, s_demos.numIndex, bytes, s_demos.indexStamp );
	trap_FS_Write( header, sizeof( header ), f );
	for ( i = 0; i < s_demos.numIndex; i++ ) {
		len = strlen( s_demos.ilist[i] ) + 1;
		if ( fill + len > DIRBUFSIZE ) {
			trap_FS_Write( stage, fill, f );
			fill = 0;
		}
		memcpy( stage + fill, s_demos.ilist[i], len );
		fill += len;
	}
	if ( fill ) {
		trap_FS_Write( stage, fill, f );
	}
	trap_FS_FCloseFile( f );
	s_demos.indexDirty = qfalse;
}

static qboolean Demos_IndexRead( void ) {
	fileHandle_t	f;
	char			header[DEMOS_INDEX_HEADER + 1];
	const char		*p;
	char			*d;
	int				len, count, bytes, stamp, age, maxAge, i;

	len = trap_FS_FOpenFile( DEMOS_INDEX_FILE, &f, FS_READ );
	if ( !f ) {
		return qfalse;
	}
	if ( len < DEMOS_INDEX_HEADER ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	memset( header, 0, sizeof( header ) );
	trap_FS_Read( header, DEMOS_INDEX_HEADER, f );
	if ( Q_strncmp( header, DEMOS_INDEX_MAGIC " ", strlen( DEMOS_INDEX_MAGIC ) + 1 ) ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	p = header + strlen( DEMOS_INDEX_MAGIC ) + 1;
	count = atoi( COM_Parse( &p ) );
	bytes = atoi( COM_Parse( &p ) );
	stamp = atoi( COM_Parse( &p ) );

	maxAge = (int)trap_Cvar_VariableValue( "df_demos_indexMaxAge" );
	age = Demos_HourStamp() - stamp;
	if ( count < 0 || count > MAX_DEMOS || bytes < 0 || bytes >= TREEBUFSIZE || len != DEMOS_INDEX_HEADER + bytes
		|| maxAge <= 0 || age < 0 || age > maxAge ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	}

	trap_FS_Read( s_demos.tnames, bytes, f );
	trap_FS_FCloseFile( f );
	s_demos.tnames[bytes] = 0;

	d = s_demos.tnames;
	for ( i = 0; i < count; i++ ) {
		if ( d >= s_demos.tnames + bytes ) {
			return qfalse;			// fewer strings than the header says
		}
		s_demos.ilist[i] = d;
		d += strlen( d ) + 1;
	}
	if ( d != s_demos.tnames + bytes ) {
		return qfalse;
	}
	s_demos.numIndex = count;
	s_demos.tused = bytes;
	s_demos.indexStamp = stamp;
	return qtrue;
}

/*
===============
Demos_IndexMergeFolder

Brings the index entries that lie directly in the folder rel ("" = demos/, else upper-case "A/B") in line with cur[0..n):
the demos the folder really holds (sorted case insensitively, upper-cased, relative to the folder).  limit = end of the
free space in tnames for new strings.
===============
*/
static void Demos_IndexMergeFolder( const char *rel, char **cur, int n, int limit ) {
	char		prefix[DEMOPATH_SIZE];
	const char	*e;
	char		*str;
	int			plen, i, j, k, m, nadds = 0, nrem = 0, cmp, len, dest;

	if ( rel[0] ) {
		Com_sprintf( prefix, sizeof( prefix ), "%s/", rel );
	} else {
		prefix[0] = 0;
	}
	plen = strlen( prefix );

	// the index entries that are files of this very folder (not of folders below it)
	m = 0;
	for ( i = 0; i < s_demos.numIndex; i++ ) {
		e = s_demos.ilist[i];
		if ( plen && Q_stricmpn( e, prefix, plen ) ) {
			continue;
		}
		if ( strchr( e + plen, '/' ) ) {
			continue;
		}
		s_demos.mergePos[m++] = i;
	}

	// walk both sorted lists: only in the index = gone, only in the folder = new
	i = 0;
	j = 0;
	while ( i < m || j < n ) {
		if ( i < m && j < n ) {
			cmp = Q_stricmp( s_demos.ilist[s_demos.mergePos[i]] + plen, cur[j] );
		} else {
			cmp = ( i < m ) ? -1 : 1;
		}
		if ( cmp == 0 ) {
			i++;
			j++;
		} else if ( cmp < 0 ) {
			s_demos.ilist[s_demos.mergePos[i]] = NULL;
			nrem++;
			i++;
		} else {
			len = strlen( cur[j] );
			if ( s_demos.tused + plen + len + 1 <= limit && s_demos.numIndex - nrem + nadds < MAX_DEMOS ) {
				str = s_demos.tnames + s_demos.tused;
				memcpy( str, prefix, plen );
				memcpy( str + plen, cur[j], len + 1 );
				s_demos.tused += plen + len + 1;
				s_demos.mergeAdds[nadds++] = str;
			}
			j++;
		}
	}
	if ( !nrem && !nadds ) {
		return;
	}

	// drop the vanished entries, then merge the new ones in from the back (both runs are sorted)
	for ( i = 0, k = 0; i < s_demos.numIndex; i++ ) {
		if ( s_demos.ilist[i] ) {
			s_demos.ilist[k++] = s_demos.ilist[i];
		}
	}
	s_demos.numIndex = k;
	i = s_demos.numIndex - 1;
	j = nadds - 1;
	dest = s_demos.numIndex + nadds - 1;
	while ( j >= 0 ) {
		if ( i >= 0 && Q_stricmp( s_demos.ilist[i], s_demos.mergeAdds[j] ) > 0 ) {
			s_demos.ilist[dest--] = s_demos.ilist[i--];
		} else {
			s_demos.ilist[dest--] = s_demos.mergeAdds[j--];
		}
	}
	s_demos.numIndex += nadds;
	s_demos.indexDirty = qtrue;
	if ( trap_Cvar_VariableValue( "developer" ) ) {
		Com_Printf( "Demos: index '%s': %i new, %i gone\n", rel, nadds, nrem );
	}
}

// merges one folder that is not the browsed one: lists it (into the free end of tnames) and sorts the names
static void Demos_IndexMergeListed( const char *dirpath, const char *rel ) {
	char	*scratch, *d;
	int		n, i, len, scratchSize;

	scratchSize = ( TREEBUFSIZE - s_demos.tused ) / 2;
	scratch = s_demos.tnames + TREEBUFSIZE - scratchSize;
	n = Demos_ListDemoFiles( dirpath, scratch, scratchSize, qfalse );
	if ( n > MAX_DEMOS ) {
		n = MAX_DEMOS;
	}
	d = scratch;
	for ( i = 0; i < n; i++ ) {
		len = strlen( d );
		s_demos.tlist[i] = d;
		if ( len >= 4 && !strcmp( d + len - 4, ".dm3" ) ) {
			d[len - 4] = 0;
		}
		Q_strupr( d );
		d += len + 1;
	}
	qsort( s_demos.tlist, n, sizeof( char * ), Demos_SortCompareCI );
	Demos_IndexMergeFolder( rel, s_demos.tlist, n, scratch - s_demos.tnames );
}

/*
===============
Demos_IndexMerge: the places where new demos usually appear (see THE INDEX)
===============
*/
static void Demos_IndexMerge( void ) {
	char		target[DEMOPATH_SIZE], targetUpper[DEMOPATH_SIZE], dirpath[DEMOPATH_SIZE + 8];
	const char	*cur = s_demos.path[0] == '/' ? s_demos.path + 1 : s_demos.path;
	char		**files = s_demos.alllist + s_demos.numAllDirs;
	int			nfiles = s_demos.numAll - s_demos.numAllDirs, i, len;
	qboolean	sorted = qtrue;
	int			start = trap_Milliseconds();

	// the browsed folder: its listing is loaded already.  Its order is DeFRaG's (Demos_Compare), the index's is Q_stricmp
	for ( i = 1; i < nfiles; i++ ) {
		if ( Q_stricmp( files[i - 1], files[i] ) > 0 ) {
			sorted = qfalse;
			break;
		}
	}
	if ( !sorted ) {
		for ( i = 0; i < nfiles; i++ ) {
			s_demos.tlist[i] = files[i];
		}
		qsort( s_demos.tlist, nfiles, sizeof( char * ), Demos_SortCompareCI );
		files = s_demos.tlist;
	}
	Demos_IndexMergeFolder( cur, files, nfiles, TREEBUFSIZE );

	if ( s_demos.indexMerged ) {
		return;			// the demos root and the autorecord folder only once per visit
	}
	s_demos.indexMerged = qtrue;

	if ( cur[0] ) {
		Demos_IndexMergeListed( "demos", "" );
	}

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
	Q_strncpyz( targetUpper, target, sizeof( targetUpper ) );
	Q_strupr( targetUpper );
	if ( target[0] && Q_stricmp( targetUpper, cur ) ) {
		Com_sprintf( dirpath, sizeof( dirpath ), "demos/%s", target );
		Demos_IndexMergeListed( dirpath, targetUpper );
	}
	if ( trap_Cvar_VariableValue( "developer" ) ) {
		Com_Printf( "Demos: index merge took %i ms\n", trap_Milliseconds() - start );
	}
}

/*
===============
Demos_BuildView

tlist = the part of the index below the current folder: "/.." (inside a folder) and the demos as paths relative to the
current folder, i.e. pointers into the index strings behind the folder's prefix.
===============
*/
static void Demos_BuildView( void ) {
	char		prefix[DEMOPATH_SIZE];
	const char	*e;
	int			plen, i;

	s_demos.numTree = 0;
	s_demos.numTreeDirs = 0;
	if ( s_demos.path[0] ) {
		Q_strncpyz( s_demos.dotdot, "/..", sizeof( s_demos.dotdot ) );
		s_demos.tlist[s_demos.numTree++] = s_demos.dotdot;
		s_demos.numTreeDirs = 1;
		Com_sprintf( prefix, sizeof( prefix ), "%s/", s_demos.path[0] == '/' ? s_demos.path + 1 : s_demos.path );
	} else {
		prefix[0] = 0;
	}
	plen = strlen( prefix );

	for ( i = 0; i < s_demos.numIndex && s_demos.numTree < MAX_DEMOS; i++ ) {
		e = s_demos.ilist[i];
		if ( plen && Q_stricmpn( e, prefix, plen ) ) {
			continue;
		}
		s_demos.tlist[s_demos.numTree++] = (char *)e + plen;
	}
	s_demos.treeValid = qtrue;
}

/*
===============
Demos_IndexFinish: a full listing is complete (scan or crawl): stamp it, store it, show it
===============
*/
static void Demos_IndexFinish( void ) {
	s_demos.indexStamp = Demos_HourStamp();
	s_demos.indexLoaded = qtrue;
	s_demos.indexFresh = qtrue;
	s_demos.indexMerged = qtrue;
	Demos_IndexWrite();
}

/*
===============
Demos_CrawlStart / Demos_CrawlRun

Fallback of Demos_IndexScan for engines that cannot list recursively (slow: two engine calls per folder, and every call
scans all pk3s: seconds for a big tree).  So it runs in slices from the menu's draw function (Demos_MenuDraw) and the menu
stays usable while it works.  Breadth first over the sub folders of demos/ (MAX_CRAWLDEPTH levels, MAX_CRAWLDIRS folders).
The engine lists the demos of a folder into the upper half of the free space of tnames and the prefixed names are written
behind the used part, so nothing needs a second buffer.

Demos_CrawlRun( ms ) works for about ms milliseconds (at least one folder).  When the queue is empty the index is sorted,
stored and the search is applied again.
===============
*/
static void Demos_CrawlStart( void ) {
	s_demos.numIndex = 0;
	s_demos.tused = 0;
	s_demos.crawlq[0] = 0;
	s_demos.crawl[0] = s_demos.crawlq;
	s_demos.crawlN = 1;
	s_demos.crawlUsed = 1;
	s_demos.crawlQi = 0;
	s_demos.crawlTruncated = qfalse;
	s_demos.crawlStart = trap_Milliseconds();
	s_demos.crawling = qtrue;
	s_demos.treeValid = qfalse;
}

static qboolean Demos_ApplyFilter( void );

static void Demos_CrawlRun( int budgetMs ) {
	char	dirpath[DEMOPATH_SIZE * 2 + 16];
	const char	*rel;
	char	*d, *rd, *dest, *scratch;
	int		i, k, qi, qn, qused, off, found, n, len, plen, depth, scratchSize;
	int		stop = trap_Milliseconds() + budgetMs;
	qboolean	truncated;

	qi = s_demos.crawlQi;
	qn = s_demos.crawlN;
	qused = s_demos.crawlUsed;
	off = s_demos.tused;
	truncated = s_demos.crawlTruncated;

	do {
		if ( qi >= qn ) {
			break;
		}
		rel = s_demos.crawl[qi];
		plen = rel[0] ? strlen( rel ) + 1 : 0;
		Com_sprintf( dirpath, sizeof( dirpath ), "demos%s%s", rel[0] ? "/" : "", rel );

		// the demos of this folder: listed into the upper half of the free space, copied (with the folder prefix) behind off
		scratchSize = ( TREEBUFSIZE - off ) / 2;
		scratch = s_demos.tnames + TREEBUFSIZE - scratchSize;
		n = Demos_ListDemoFiles( dirpath, scratch, scratchSize, qfalse );
		rd = scratch;
		for ( k = 0; k < n; k++ ) {
			len = strlen( rd );
			if ( s_demos.numIndex >= MAX_DEMOS || off + plen + len + 1 > rd - s_demos.tnames ) {
				truncated = qtrue;
				break;
			}
			dest = s_demos.tnames + off;
			if ( plen ) {
				memcpy( dest, rel, plen - 1 );
				dest[plen - 1] = '/';
			}
			memcpy( dest + plen, rd, len + 1 );
			if ( len >= 4 && !strcmp( dest + plen + len - 4, ".dm3" ) ) {
				dest[plen + len - 4] = 0;
			}
			Q_strupr( dest );
			s_demos.ilist[s_demos.numIndex++] = dest;
			off += plen + len + 1;
			rd += len + 1;
		}

		// the sub folders of this folder join the queue
		for ( depth = 0, i = 0; rel[i]; i++ ) {
			if ( rel[i] == '/' ) {
				depth++;
			}
		}
		found = 0;
		if ( depth + 1 < MAX_CRAWLDEPTH ) {
			found = trap_FS_GetFileList( dirpath, "/", s_demos.dirs, sizeof( s_demos.dirs ) );
		}
		d = s_demos.dirs;
		for ( i = 0; i < found; i++, d += len + 1 ) {
			len = strlen( d );
			if ( !len || !strcmp( d, "." ) || !strcmp( d, ".." ) ) {
				continue;
			}
			if ( qn >= MAX_CRAWLDIRS || qused + plen + len + 1 > CRAWLQ_SIZE ) {
				truncated = qtrue;
				break;
			}
			dest = s_demos.crawlq + qused;
			if ( plen ) {
				memcpy( dest, rel, plen - 1 );
				dest[plen - 1] = '/';
			}
			memcpy( dest + plen, d, len + 1 );
			s_demos.crawl[qn++] = dest;
			qused += plen + len + 1;
		}
		qi++;
	} while ( trap_Milliseconds() < stop );

	s_demos.crawlQi = qi;
	s_demos.crawlN = qn;
	s_demos.crawlUsed = qused;
	s_demos.tused = off;
	s_demos.crawlTruncated = truncated;

	if ( qi >= qn ) {
		// done
		if ( truncated ) {
			Com_Printf( "^3Demos: the demo list is incomplete (too many demos or folders)\n" );
		}
		qsort( s_demos.ilist, s_demos.numIndex, sizeof( char * ), Demos_SortCompareCI );
		s_demos.crawling = qfalse;
		Demos_IndexFinish();
		Demos_BuildView();
		if ( trap_Cvar_VariableValue( "developer" ) ) {
			Com_Printf( "Demos: crawled %i demos in %i ms\n", s_demos.numIndex, trap_Milliseconds() - s_demos.crawlStart );
		}
		if ( s_demos.treeMode ) {
			// the search was waiting for this list
			s_demos.filterValid = qfalse;
			Demos_ApplyFilter();
		}
	}
}

/*
===============
Demos_IndexScan: the whole tree of demos/, from scratch.  One engine call on oDFe, otherwise the crawl is started
(s_demos.crawling) and finishes later.
===============
*/
static void Demos_IndexScan( void ) {
	char	*d;
	int		i, n, len, start = trap_Milliseconds();

	s_demos.numIndex = 0;
	s_demos.tused = 0;
	n = Demos_ListDemoFiles( "demos", s_demos.tnames, TREEBUFSIZE, qtrue );
	if ( n <= 0 ) {
		// empty, or an engine without the recursive listing: crawl the folders, a slice per frame
		Demos_CrawlStart();
		return;
	}
	if ( n > MAX_DEMOS ) {
		n = MAX_DEMOS;
		Com_Printf( "^3Demos: only the first %i demos are indexed\n", n );
	}

	d = s_demos.tnames;
	for ( i = 0; i < n; i++ ) {
		s_demos.ilist[s_demos.numIndex++] = d;
		len = strlen( d );
		if ( len >= 4 && !strcmp( d + len - 4, ".dm3" ) ) {
			d[len - 4] = 0;
		}
		Q_strupr( d );
		d += len + 1;
	}
	s_demos.tused = d - s_demos.tnames;
	if ( s_demos.tused > TREEBUFSIZE - 512 ) {
		Com_Printf( "^3Demos: the name buffer is full, the demo list may be incomplete\n" );
	}
	Demos_IndexFinish();
	if ( trap_Cvar_VariableValue( "developer" ) ) {
		Com_Printf( "Demos: scanned %i demos in %i ms\n", s_demos.numIndex, trap_Milliseconds() - start );
	}
}

/*
===============
Demos_LoadTree

Makes tlist (the view of everything below the current folder) available: from the index file when it is there, else
from a scan.  The index stays loaded while the menu is open; going into another folder only rebuilds the view.
===============
*/
static void Demos_LoadTree( void ) {
	int		start = trap_Milliseconds();

	s_demos.treeValid = qfalse;
	s_demos.numTree = 0;
	s_demos.numTreeDirs = 0;

	if ( !s_demos.indexLoaded ) {
		if ( s_demos.forceScan || !Demos_IndexRead() ) {
			s_demos.forceScan = qfalse;
			Demos_IndexScan();
			if ( s_demos.crawling ) {
				return;			// finished by Demos_CrawlRun
			}
		} else {
			s_demos.indexLoaded = qtrue;
			s_demos.indexFresh = qfalse;
			if ( trap_Cvar_VariableValue( "developer" ) ) {
				Com_Printf( "Demos: index file read, %i demos in %i ms\n", s_demos.numIndex, trap_Milliseconds() - start );
			}
		}
	}
	if ( !s_demos.indexFresh ) {
		Demos_IndexMerge();
	}
	Demos_BuildView();
	if ( s_demos.indexDirty ) {
		Demos_IndexWrite();
	}
}

/*
===============
Demos_BuildList: list the current folder and show it (through the search filter)
===============
*/
static void Demos_BuildList( void ) {
	char		vars[32];
	const char	*vp;
	int			start = trap_Milliseconds();

	Demos_LoadFolder();
	if ( trap_Cvar_VariableValue( "developer" ) ) {
		Com_Printf( "Demos: folder '%s' listed (%i entries) in %i ms\n", s_demos.path, s_demos.numAll, trap_Milliseconds() - start );
	}
	Demos_ClickReset();

	// the search text applies to the new folder's list too (Demos_EnterFolder clears it first)
	Demos_ApplyFilter();
	Com_sprintf( s_demos.folder.field.buffer, sizeof( s_demos.folder.field.buffer ), "%s", s_demos.path[0] ? s_demos.path : "/" );

	// back to the scroll position / selection the menu was left with
	trap_Cvar_VariableStringBuffer( "df_demos_CurrentListVars", vars, sizeof( vars ) );
	vp = vars;
	s_demos.list.top = Com_Clamp( 0, s_demos.list.numitems - 1, atoi( COM_Parse( &vp ) ) );
	s_demos.list.curvalue = Com_Clamp( 0, s_demos.list.numitems - 1, atoi( COM_Parse( &vp ) ) );
}

/*
===============
Demos_EnterFolder (0xea70): the selected entry is a folder ("/name" or "/..")
===============
*/
static void Demos_EnterFolder( void ) {
	char	sel[DEMOPATH_SIZE], cur[DEMOPATH_SIZE], up[DEMOPATH_SIZE];
	int		i, n, slashes, seen;

	Q_strncpyz( sel, s_demos.demolist[s_demos.list.curvalue], sizeof( sel ) );
	Q_strncpyz( cur, s_demos.path, sizeof( cur ) );

	if ( !strcmp( sel, "/.." ) ) {
		// drop the last path component
		memset( up, 0, sizeof( up ) );
		slashes = 0;
		for ( i = 0; cur[i]; i++ ) {
			if ( cur[i] == '/' ) {
				slashes++;
			}
		}
		n = 0;
		seen = 0;
		for ( i = 0; i < (int)strlen( cur ); i++ ) {
			if ( seen == slashes ) {
				break;
			}
			if ( cur[i] == '/' ) {
				seen++;
				if ( seen < slashes ) {
					up[n++] = cur[i];
				}
			} else {
				up[n++] = cur[i];
			}
		}
		Com_sprintf( s_demos.path, sizeof( s_demos.path ), "%s", up );
	} else if ( strlen( cur ) + strlen( sel ) + 1 < sizeof( s_demos.path ) ) {
		Com_sprintf( s_demos.path, sizeof( s_demos.path ), "%s%s", cur, sel );
	}

	MField_Clear( &s_demos.search.field );		// the search text belongs to the folder it was typed in
	s_demos.list.top = 0;
	s_demos.list.curvalue = 0;
	trap_Cvar_Set( "df_demos_CurrentPath", s_demos.path );
	trap_Cvar_Set( "df_demos_CurrentListVars", va( "%i %i", s_demos.list.top, s_demos.list.curvalue ) );
}

/*
===============
Demos_Play (0xee6e)
===============
*/
static void Demos_Play( void ) {
	char	name[DEMONAME_SIZE];
	const char	*item = s_demos.demolist[s_demos.list.curvalue];
	int		len = strlen( item );

	if ( len >= DEMONAME_SIZE ) {
		Com_Printf( "^1ERROR:^7 Demo filename too long, maximum length = %i, filename length = %i.\n", DEMONAME_SIZE, len );
		return;
	}
	Q_strncpyz( name, item, sizeof( name ) );
	len = strlen( name );
	if ( len > 6 ) {
		name[len - 6] = 0;		// ".dm_NN"
	}
	trap_Cvar_Set( "df_ghosts_Command", va( "#1 demo %s", name ) );
	{
		// UI_DemosReturnAfterPlayback: back to this menu when the demo is over.  The flag is the demo's file name (without
		// folders): the engine keeps it as the "server name" of the session it plays
		const char *slash = strrchr( name, '/' );

		trap_Cvar_Set( "df_demos_ReturnToBrowser", slash ? slash + 1 : name );
	}

	if ( s_demos.path[0] ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s/%s\"\n", s_demos.path, name ) );
	} else {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s\"\n", name ) );
	}
}

/*
===============
Demos_Activate (0xef16): enter the folder or play the demo
===============
*/
static void Demos_Activate( void ) {
	if ( s_demos.empty ) {
		return;		// only the "No Demos Found." / "No Matches." placeholder (the original has no such guard)
	}
	trap_Cvar_Set( "df_demos_CurrentListVars", va( "%i %i", s_demos.list.top, s_demos.list.curvalue ) );
	if ( s_demos.list.curvalue < s_demos.numDirs ) {
		Demos_EnterFolder();
		Demos_BuildList();
	} else {
		UI_ForceMenuOff();
		Demos_Play();
	}
}

/*
===============
Demos_MenuEvent (0xef43)
===============
*/
static void Demos_MenuEvent( void *ptr, int event ) {
	s_demos.click.lastId = ((menucommon_s*)ptr)->id;
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GO:
		Demos_Activate();
		break;

	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
===============
Demos_MenuKey (0xef84)
===============
*/
static qboolean Demos_SearchFocused( void ) {
	return Menu_ItemAtCursor( &s_demos.menu ) == (void *)&s_demos.search;
}

/*
===============
The scroll bar (oDFs): a track right of the list with a thumb whose size is the visible share of the list.  Click the
track or drag the thumb with the left mouse button; the mouse wheel and the keys scroll as before.  The menu system gets
key presses but no releases, so a drag is followed from Demos_MenuDraw with trap_Key_IsDown.
===============
*/
static qboolean Demos_ScrollbarGeometry( int *trackY, int *trackH, int *thumbY, int *thumbH ) {
	int	total = s_demos.list.numitems;
	int	rows = s_demos.list.height;
	int	range, top;

	*trackY = s_demos.list.generic.y;
	*trackH = rows * SMALLCHAR_HEIGHT;
	if ( total <= rows ) {
		*thumbY = *trackY;
		*thumbH = *trackH;
		return qfalse;			// everything is visible, nothing to scroll
	}
	range = total - rows;
	*thumbH = *trackH * rows / total;
	if ( *thumbH < SCROLLBAR_MINTHUMB ) {
		*thumbH = SCROLLBAR_MINTHUMB;
	}
	top = s_demos.list.top;
	if ( top < 0 ) {
		top = 0;
	} else if ( top > range ) {
		top = range;
	}
	*thumbY = *trackY + ( *trackH - *thumbH ) * top / range;
	return qtrue;
}

// sets the list position from the cursor while the thumb is dragged
static void Demos_ScrollbarDrag( void ) {
	int	trackY, trackH, thumbY, thumbH, range, top;

	if ( !Demos_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	range = s_demos.list.numitems - s_demos.list.height;
	top = ( uis.cursory - s_demos.sbGrab - trackY ) * range / ( trackH - thumbH );
	s_demos.list.top = Com_Clamp( 0, range, top );
}

static void Demos_ScrollbarDraw( void ) {
	static vec4_t	trackColor = { 0.15f, 0.0f, 0.0f, 0.6f };
	static vec4_t	thumbColor = { 0.70f, 0.08f, 0.08f, 0.95f };
	static vec4_t	hotColor = { 1.0f, 0.30f, 0.15f, 1.0f };
	int				trackY, trackH, thumbY, thumbH;
	qboolean		hot;

	if ( !Demos_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH ) ) {
		return;
	}
	hot = s_demos.sbDrag || UI_CursorInRect( SCROLLBAR_X, trackY, SCROLLBAR_W, trackH );
	UI_FillRect( SCROLLBAR_X, trackY, SCROLLBAR_W, trackH, trackColor );
	UI_FillRect( SCROLLBAR_X + 1, thumbY, SCROLLBAR_W - 2, thumbH, hot ? hotColor : thumbColor );
}

/*
===============
WORD COMPLETION (oDFs)

While the search box has the focus, the word at the end of its text ("luc" in "cpm luc") is completed from the words of
the names that the search looks at: the folder, or everything below it once the text is a path or a "**" search.  A word
is a run of letters, digits, '_' and '-' with a letter in it, WORD_MINLEN or more long; the extension is left out, so
"lucy_cup04[df.cpm]00.32.640(MoskaL-th.TILTOSTAN).dm_68" gives lucy_cup04, cpm, moskal-th and tiltostan.  The words are
collected the first time the box is used (a few milliseconds per frame, Demos_WordsRun) and again when the folder changes.

The best SUGGEST_MAX words are listed under the box: words that start with the typed letters first, then words that only
contain them, each by how many times they occur.  The grey text after the cursor shows what Tab will do: complete up to
what all words that start with the typed letters have in common ("luc" gives "lucy_cup0" when there are lucy_cup04 and
lucy_cup05), and once nothing more is common, the whole selected word.  Up / Down select a word (Enter then takes it),
a click on a row takes that word.  Escape clears the search text, which closes the list.
===============
*/
static qboolean Demos_IsWordChar( int c ) {
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-';
}

// adds one word (tok, len characters, not terminated) or counts it again
static void Demos_WordAdd( const char *tok, int len ) {
	char		word[WORD_MAXLEN + 1];
	unsigned	h = 5381;
	int			i, c, slot;
	qboolean	hasAlpha = qfalse;

	if ( len < WORD_MINLEN || len > WORD_MAXLEN ) {
		return;
	}
	for ( i = 0; i < len; i++ ) {
		c = tok[i];
		if ( c >= 'A' && c <= 'Z' ) {
			c += 'a' - 'A';
		}
		if ( c >= 'a' && c <= 'z' ) {
			hasAlpha = qtrue;
		}
		word[i] = c;
		h = h * 33 + c;
	}
	if ( !hasAlpha ) {
		return;				// times, numbers
	}
	word[len] = 0;

	slot = h & ( WORD_HASHSIZE - 1 );
	while ( ( i = s_demos.wordHash[slot] ) != 0 ) {
		if ( !strcmp( s_demos.words[i - 1], word ) ) {
			s_demos.wordCount[i - 1]++;
			return;
		}
		slot = ( slot + 1 ) & ( WORD_HASHSIZE - 1 );
	}
	if ( s_demos.numWords >= MAX_WORDS || s_demos.wordsUsed + len + 1 > WORDBUF_SIZE ) {
		return;
	}
	memcpy( s_demos.wordBuf + s_demos.wordsUsed, word, len + 1 );
	s_demos.words[s_demos.numWords] = s_demos.wordBuf + s_demos.wordsUsed;
	s_demos.wordCount[s_demos.numWords] = 1;
	s_demos.wordsUsed += len + 1;
	s_demos.numWords++;
	s_demos.wordHash[slot] = s_demos.numWords;
}

// the words of one list entry: a folder "/NAME", a demo "NAME.DM_68", or a path "A/B/NAME.DM_68"
static void Demos_WordsAddEntry( const char *e ) {
	const char	*p, *end, *start;

	end = e + strlen( e );
	if ( e[0] != '/' ) {
		for ( p = end; p > e; p-- ) {
			if ( p[-1] == '.' ) {
				end = p - 1;		// leave out the extension
				break;
			}
		}
	}
	start = NULL;
	for ( p = e; p <= end; p++ ) {
		if ( p < end && Demos_IsWordChar( *p ) ) {
			if ( !start ) {
				start = p;
			}
		} else if ( start ) {
			Demos_WordAdd( start, p - start );
			start = NULL;
		}
	}
}

// collects the words of the list that is searched, for about budgetMs milliseconds at a time
static void Demos_WordsRun( int budgetMs ) {
	char		**src;
	int			total, n = 0, begin;
	qboolean	tree = s_demos.treeMode;

	if ( tree && ( !s_demos.treeValid || s_demos.crawling ) ) {
		s_demos.wordsValid = qfalse;			// the list is not there yet
		s_demos.wordsBuilding = qfalse;
		return;
	}
	src = tree ? s_demos.tlist : s_demos.alllist;
	total = tree ? s_demos.numTree : s_demos.numAll;

	if ( s_demos.wordsValid && s_demos.wordKeyTree == tree && s_demos.wordKeyGen == s_demos.listGen && s_demos.wordKeyTotal == total ) {
		return;
	}
	if ( !s_demos.wordsBuilding || s_demos.wordKeyTree != tree || s_demos.wordKeyGen != s_demos.listGen || s_demos.wordKeyTotal != total ) {
		memset( s_demos.wordHash, 0, sizeof( s_demos.wordHash ) );
		s_demos.numWords = 0;
		s_demos.wordsUsed = 0;
		s_demos.wordPos = 0;
		s_demos.wordKeyTree = tree;
		s_demos.wordKeyGen = s_demos.listGen;
		s_demos.wordKeyTotal = total;
		s_demos.wordsBuilding = qtrue;
		s_demos.wordsValid = qfalse;
		s_demos.sugValid = qfalse;
		s_demos.numSug = 0;
	}

	begin = trap_Milliseconds();
	while ( s_demos.wordPos < total ) {
		Demos_WordsAddEntry( src[s_demos.wordPos++] );
		if ( ( ++n & 63 ) == 0 && trap_Milliseconds() - begin >= budgetMs ) {
			break;
		}
	}
	if ( s_demos.wordPos >= total ) {
		s_demos.wordsBuilding = qfalse;
		s_demos.wordsValid = qtrue;
		s_demos.sugValid = qfalse;
		if ( trap_Cvar_VariableValue( "developer" ) ) {
			Com_Printf( "Demos: %i words for completion\n", s_demos.numWords );
		}
	}
}

// the character index where the word being typed starts, -1 when there is none (cursor not at the end, too short)
static int Demos_SuggestFragStart( void ) {
	const char	*buf = s_demos.search.field.buffer;
	int			len = strlen( buf ), start = len;

	if ( s_demos.search.field.cursor != len ) {
		return -1;
	}
	while ( start > 0 && Demos_IsWordChar( buf[start - 1] ) ) {
		start--;
	}
	if ( len - start < SUGGEST_MINFRAG || len - start > WORD_MAXLEN ) {
		return -1;
	}
	return start;
}

// works out the suggestions for the text in the box
static void Demos_SuggestUpdate( void ) {
	const char	*buf = s_demos.search.field.buffer;
	const char	*word;
	int			start, fragLen, i, k, n, w, score;
	qboolean	havePrefix = qfalse;

	Q_strncpyz( s_demos.sugText, buf, sizeof( s_demos.sugText ) );
	s_demos.sugCursor = s_demos.search.field.cursor;
	s_demos.sugValid = qtrue;
	s_demos.numSug = 0;
	s_demos.sugSel = 0;
	s_demos.sugMoved = qfalse;
	s_demos.sugCommon[0] = 0;
	s_demos.sugCommonLen = 0;
	s_demos.sugFragLen = 0;

	start = Demos_SuggestFragStart();
	if ( start < 0 || !s_demos.wordsValid ) {
		return;
	}
	fragLen = strlen( buf ) - start;
	for ( i = 0; i < fragLen; i++ ) {
		s_demos.sugFrag[i] = buf[start + i];
		if ( s_demos.sugFrag[i] >= 'A' && s_demos.sugFrag[i] <= 'Z' ) {
			s_demos.sugFrag[i] += 'a' - 'A';
		}
	}
	s_demos.sugFrag[fragLen] = 0;
	s_demos.sugFragLen = fragLen;

	n = 0;
	for ( w = 0; w < s_demos.numWords; w++ ) {
		word = s_demos.words[w];
		if ( !Q_strncmp( word, s_demos.sugFrag, fragLen ) ) {
			if ( !word[fragLen] ) {
				continue;			// exactly what is typed
			}
			if ( !havePrefix ) {
				havePrefix = qtrue;
				Q_strncpyz( s_demos.sugCommon, word, sizeof( s_demos.sugCommon ) );
			} else {
				for ( k = 0; s_demos.sugCommon[k] && s_demos.sugCommon[k] == word[k]; k++ )
					;
				s_demos.sugCommon[k] = 0;
			}
			score = 0x40000000 + s_demos.wordCount[w];
		} else {
			if ( n == SUGGEST_MAX && s_demos.sugScore[SUGGEST_MAX - 1] >= s_demos.wordCount[w] ) {
				continue;			// cannot make the list
			}
			if ( !strstr( word, s_demos.sugFrag ) ) {
				continue;
			}
			score = s_demos.wordCount[w];
		}
		if ( n < SUGGEST_MAX ) {
			k = n++;
		} else if ( score > s_demos.sugScore[SUGGEST_MAX - 1] ) {
			k = SUGGEST_MAX - 1;
		} else {
			continue;
		}
		while ( k > 0 && s_demos.sugScore[k - 1] < score ) {
			s_demos.sug[k] = s_demos.sug[k - 1];
			s_demos.sugScore[k] = s_demos.sugScore[k - 1];
			k--;
		}
		s_demos.sug[k] = w;
		s_demos.sugScore[k] = score;
	}
	s_demos.numSug = n;
	s_demos.sugCommonLen = strlen( s_demos.sugCommon );
}

static int Demos_SuggestRowAt( void ) {
	int	row;

	if ( !UI_CursorInRect( SUGGEST_X, SUGGEST_Y, SUGGEST_W, s_demos.numSug * SUGGEST_ROW_H + 2 ) ) {
		return -1;
	}
	row = ( uis.cursory - SUGGEST_Y - 1 ) / SUGGEST_ROW_H;
	return Com_Clamp( 0, s_demos.numSug - 1, row );
}

// the list is shown while the box has the focus (the pointer over the list moves the focus to the scroll list that lies
// below it, which must not close it)
static qboolean Demos_SuggestVisible( void ) {
	if ( !s_demos.numSug || !s_demos.sugValid ) {
		return qfalse;
	}
	if ( s_demos.sugCursor != s_demos.search.field.cursor || strcmp( s_demos.sugText, s_demos.search.field.buffer ) ) {
		return qfalse;
	}
	return Demos_SearchFocused() || Demos_SuggestRowAt() >= 0;
}

static void Demos_SuggestRefresh( void ) {
	if ( !Demos_SearchFocused() && Demos_SuggestRowAt() < 0 ) {
		return;
	}
	Demos_WordsRun( WORDS_SLICE_MS );
	if ( !s_demos.wordsValid ) {
		s_demos.sugValid = qfalse;
		s_demos.numSug = 0;
		return;
	}
	if ( s_demos.sugValid && s_demos.sugCursor == s_demos.search.field.cursor && !strcmp( s_demos.sugText, s_demos.search.field.buffer ) ) {
		return;
	}
	Demos_SuggestUpdate();
}

// what Tab puts in place of the typed letters
static const char *Demos_SuggestTarget( void ) {
	if ( !s_demos.numSug ) {
		return NULL;
	}
	if ( !s_demos.sugMoved && s_demos.sugCommonLen > s_demos.sugFragLen ) {
		return s_demos.sugCommon;
	}
	return s_demos.words[s_demos.sug[s_demos.sugSel]];
}

// replaces the word being typed with text, and applies the new search
static void Demos_SuggestAccept( const char *text ) {
	mfield_t	*f = &s_demos.search.field;
	int			start = Demos_SuggestFragStart();

	if ( start < 0 || start + (int)strlen( text ) > SEARCH_MAXCHARS ) {
		return;
	}
	Q_strncpyz( f->buffer + start, text, SEARCH_MAXCHARS + 1 - start );
	f->cursor = strlen( f->buffer );
	f->scroll = f->cursor - f->widthInChars + 1;
	if ( f->scroll < 0 ) {
		f->scroll = 0;
	}
	s_demos.sugValid = qfalse;
	if ( Demos_ApplyFilter() ) {
		Demos_ClickReset();
		trap_Cvar_Set( "df_demos_CurrentListVars", "0 0" );
	}
}

static sfxHandle_t Demos_MenuKey( int key ) {
	sfxHandle_t	sound;
	qboolean	typing;
	int			trackY, trackH, thumbY, thumbH;

	// left click on the scroll bar: grab the thumb (a click on the track puts the middle of the thumb there)
	if ( key == K_MOUSE1 && Demos_ScrollbarGeometry( &trackY, &trackH, &thumbY, &thumbH )
		&& UI_CursorInRect( SCROLLBAR_X, trackY, SCROLLBAR_W, trackH ) ) {
		if ( uis.cursory >= thumbY && uis.cursory < thumbY + thumbH ) {
			s_demos.sbGrab = uis.cursory - thumbY;
		} else {
			s_demos.sbGrab = thumbH / 2;
		}
		s_demos.sbDrag = qtrue;
		Demos_ScrollbarDrag();
		return menu_null_sound;
	}

	// a click on a word of the completion list takes it
	if ( key == K_MOUSE1 && Demos_SuggestVisible() && Demos_SuggestRowAt() >= 0 ) {
		Demos_SuggestAccept( s_demos.words[s_demos.sug[Demos_SuggestRowAt()]] );
		Menu_SetCursorToItem( &s_demos.menu, &s_demos.search );
		return menu_move_sound;
	}

	if ( key == K_TAB || key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW
		|| key == K_ENTER || key == K_KP_ENTER || key == K_PGUP || key == K_PGDN ) {
		s_demos.keyNav = qtrue;
	}

	// mouse wheel: scroll the list wherever the focus is, and leave the highlighted entry where it is (DeFRaG moved the
	// selection one entry per notch on top of the scrolling)
	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		ScrollList_Key( &s_demos.list, key );
		return menu_null_sound;
	}

	// a printable character anywhere outside of the search box starts a search
	if ( ( key & K_CHAR_FLAG ) && !Demos_SearchFocused() ) {
		int ch = key & ~K_CHAR_FLAG;

		if ( ch > ' ' && ch < 127 ) {
			Menu_SetCursorToItem( &s_demos.menu, &s_demos.search );
		}
	}
	typing = Demos_SearchFocused();

	// Escape with a search typed clears it (the first press); the next one leaves the menu.  This also closes the
	// completion list
	if ( key == K_ESCAPE && s_demos.search.field.buffer[0] ) {
		MField_Clear( &s_demos.search.field );
		s_demos.sugValid = qfalse;
		if ( Demos_ApplyFilter() ) {
			Demos_ClickReset();
			trap_Cvar_Set( "df_demos_CurrentListVars", "0 0" );
		}
		return menu_null_sound;
	}

	// word completion: Tab takes the proposal, Up / Down move through the list, Enter takes the word that was moved to.
	// Without a list these keys do what they always did
	if ( typing && ( key == K_TAB || key == K_UPARROW || key == K_KP_UPARROW || key == K_DOWNARROW || key == K_KP_DOWNARROW
		|| key == K_ENTER || key == K_KP_ENTER ) ) {
		Demos_SuggestRefresh();
		if ( Demos_SuggestVisible() ) {
			if ( key == K_TAB ) {
				Demos_SuggestAccept( Demos_SuggestTarget() );
				return menu_move_sound;
			}
			if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
				s_demos.sugSel = ( s_demos.sugSel + 1 ) % s_demos.numSug;
				s_demos.sugMoved = qtrue;
				return menu_move_sound;
			}
			if ( key == K_UPARROW || key == K_KP_UPARROW ) {
				s_demos.sugSel = ( s_demos.sugSel + s_demos.numSug - 1 ) % s_demos.numSug;
				s_demos.sugMoved = qtrue;
				return menu_move_sound;
			}
			if ( s_demos.sugMoved ) {
				Demos_SuggestAccept( s_demos.words[s_demos.sug[s_demos.sugSel]] );
				return menu_move_sound;
			}
		}
	}

	// F5: list everything again, the folder and (when a search below it is shown) the whole demo index
	if ( key == K_F5 ) {
		s_demos.forceScan = qtrue;
		s_demos.indexLoaded = qfalse;
		s_demos.crawling = qfalse;
		s_demos.treeValid = qfalse;
		trap_Cvar_Set( "df_demos_CurrentListVars", va( "%i %i", s_demos.list.top, s_demos.list.curvalue ) );
		Demos_BuildList();
		return menu_move_sound;
	}

	// Enter confirms the search: back to the list
	if ( typing && ( key == K_ENTER || key == K_KP_ENTER ) ) {
		Menu_SetCursorToItem( &s_demos.menu, &s_demos.list );
		return menu_move_sound;
	}

	sound = Menu_DefaultKey( &s_demos.menu, key );

	// the box may have changed: refilter the list
	if ( Demos_ApplyFilter() ) {
		Demos_ClickReset();
		trap_Cvar_Set( "df_demos_CurrentListVars", "0 0" );
	}

	if ( key == K_MOUSE1 && s_demos.click.lastId == ID_LIST ) {
		if ( Demos_DoubleClick( s_demos.list.curvalue ) ) {
			Demos_Activate();
		}
	}
	if ( typing ) {
		// Backspace / arrows / Enter belong to the text box
		return sound;
	}
	if ( key == K_MOUSE3 && s_demos.path[0] ) {
		s_demos.list.curvalue = 0;
		Demos_Activate();
	}
	if ( ( key == K_LEFTARROW || key == K_BACKSPACE ) && s_demos.path[0] ) {
		s_demos.list.curvalue = 0;
		Demos_Activate();
	}
	if ( key == K_RIGHTARROW || key == K_ENTER || key == K_KP_ENTER ) {
		Demos_Activate();
	}
	return sound;
}

/*
===============
Demos_MenuDraw: advances the fallback folder crawl a little every frame, follows a scroll bar drag, draws the bar
===============
*/
static void Demos_SuggestDraw( void );

static void Demos_MenuDraw( void ) {
	if ( s_demos.crawling ) {
		Demos_CrawlRun( 12 );
		if ( s_demos.crawling && s_demos.demolist[0] == s_demos.loadText ) {
			Com_sprintf( s_demos.loadText, sizeof( s_demos.loadText ), "Indexing sub folders... %i%%", s_demos.crawlQi * 100 / s_demos.crawlN );
		}
	}
	if ( uis.cursorx != s_demos.lastCursorX || uis.cursory != s_demos.lastCursorY ) {
		s_demos.lastCursorX = uis.cursorx;
		s_demos.lastCursorY = uis.cursory;
		s_demos.keyNav = qfalse;		// the mouse is in charge again
	}
	if ( s_demos.sbDrag ) {
		if ( trap_Key_IsDown( K_MOUSE1 ) ) {
			Demos_ScrollbarDrag();
		} else {
			s_demos.sbDrag = qfalse;
		}
	}
	Demos_SuggestRefresh();
	Menu_Draw( &s_demos.menu );
	Demos_ScrollbarDraw();
	Demos_SuggestDraw();
}

/*
===============
Look of the controls (oDFs).  The stock widgets draw everything in the DeFRaG list colours (yellow labels, red values), so
FOLDER / SEARCH / BACK / PLAY are drawn here: dark red plates with a red outline (the red of the old BACK / PLAY art), a
yellow outline while focused, white labels.
===============
*/
static vec4_t	demosPlate = { 0.14f, 0.02f, 0.02f, 0.90f };
static vec4_t	demosPlateFocus = { 0.26f, 0.04f, 0.03f, 0.95f };
static vec4_t	demosBorder = { 0.62f, 0.10f, 0.08f, 1.0f };
static vec4_t	demosBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	demosLabel = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	demosFolderText = { 1.0f, 0.78f, 0.62f, 1.0f };
static vec4_t	demosSearchText = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	demosHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	demosButtonFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	demosButtonFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	demosButtonBorder = { 0.80f, 0.14f, 0.10f, 1.0f };
static vec4_t	demosButtonText = { 1.0f, 0.82f, 0.76f, 1.0f };

static void Demos_DrawOutline( int x, int y, int w, int h, const float *border ) {
	UI_FillRect( x, y, w, 1, border );
	UI_FillRect( x, y + h - 1, w, 1, border );
	UI_FillRect( x, y, 1, h, border );
	UI_FillRect( x + w - 1, y, 1, h, border );
}

static void Demos_DrawFrame( int x, int y, int w, int h, const float *fill, const float *border ) {
	UI_FillRect( x, y, w, h, fill );
	Demos_DrawOutline( x, y, w, h, border );
}

static vec4_t	demosDropFill = { 0.10f, 0.015f, 0.015f, 1.0f };
static vec4_t	demosDropSel = { 0.42f, 0.07f, 0.05f, 1.0f };

// the completion list under the search box
static void Demos_SuggestDraw( void ) {
	int		i, y, hot, sel;
	char	num[16];

	if ( !Demos_SuggestVisible() ) {
		return;
	}
	hot = Demos_SuggestRowAt();
	sel = hot >= 0 ? hot : s_demos.sugSel;
	Demos_DrawFrame( SUGGEST_X, SUGGEST_Y, SUGGEST_W, s_demos.numSug * SUGGEST_ROW_H + 2, demosDropFill, demosBorder );
	for ( i = 0; i < s_demos.numSug; i++ ) {
		y = SUGGEST_Y + 1 + i * SUGGEST_ROW_H;
		if ( i == sel ) {
			UI_FillRect( SUGGEST_X + 1, y, SUGGEST_W - 2, SUGGEST_ROW_H, demosDropSel );
		}
		UI_DrawString( SUGGEST_X + 4, y + 1, s_demos.words[s_demos.sug[i]], UI_SMALLFONT, i == sel ? demosLabel : demosFolderText );
		Com_sprintf( num, sizeof( num ), "%i", s_demos.wordCount[s_demos.sug[i]] );
		UI_DrawString( SUGGEST_X + SUGGEST_W - 4, y + 1, num, UI_RIGHT | UI_SMALLFONT, demosHint );
	}
}

// grey text after the cursor: what Tab would add
static void Demos_SuggestGhost( void ) {
	mfield_t	*f = &s_demos.search.field;
	const char	*target;
	char		ghost[WORD_MAXLEN + 1];
	int			prestep, avail;

	if ( !Demos_SuggestVisible() ) {
		return;
	}
	target = Demos_SuggestTarget();
	if ( !target || Q_strncmp( target, s_demos.sugFrag, s_demos.sugFragLen ) ) {
		return;				// a word that only contains the letters: nothing to append
	}
	prestep = ( (int)strlen( f->buffer ) + 1 <= f->widthInChars ) ? 0 : f->scroll;
	avail = f->widthInChars - ( f->cursor - prestep );
	if ( avail <= 0 ) {
		return;
	}
	Q_strncpyz( ghost, target + s_demos.sugFragLen, sizeof( ghost ) );
	if ( (int)strlen( ghost ) > avail ) {
		ghost[avail] = 0;
	}
	UI_DrawString( FIELD_X + ( f->cursor - prestep ) * SMALLCHAR_WIDTH, s_demos.search.generic.y, ghost, UI_SMALLFONT, demosHint );
}

// FOLDER (read only) and SEARCH: label in the proportional font, the text on a plate
static void Demos_FieldDraw( void *self ) {
	menufield_s	*f = (menufield_s *)self;
	qboolean	focus = ( Menu_ItemAtCursor( f->generic.parent ) == self );
	qboolean	isSearch = ( f == &s_demos.search );
	int			y = f->generic.y;

	UI_DrawProportionalString( 12, y - 3, f->generic.name, UI_SMALLFONT | UI_DROPSHADOW, demosLabel );
	Demos_DrawFrame( FIELD_PLATE_X, y - 2, FIELD_CHARS * SMALLCHAR_WIDTH + 8, SMALLCHAR_HEIGHT + 4,
		focus ? demosPlateFocus : demosPlate, focus ? demosBorderFocus : demosBorder );
	if ( isSearch && !f->field.buffer[0] && !focus ) {
		UI_DrawString( FIELD_X, y, "type to search - wildcards * ? and ** for sub folders", UI_SMALLFONT, demosHint );
	} else {
		MField_Draw( &f->field, FIELD_X, y, UI_SMALLFONT | ( focus ? UI_PULSE : 0 ), isSearch ? demosSearchText : demosFolderText );
		if ( isSearch && focus ) {
			Demos_SuggestGhost();
		}
	}
}

// BACK / PLAY: a faint red fill, a 2 pixel red outline and plain text.  Lit while the mouse is over it, or while it has
// the keyboard focus after a navigation key (the menu keeps its focus when the pointer leaves an item, which must not
// keep it lit): the outline turns yellow, the fill gets stronger and breathes a little
static void Demos_ButtonDraw( void *self ) {
	menutext_s	*b = (menutext_s *)self;
	qboolean	lit = ( b->generic.flags & QMF_HASMOUSEFOCUS ) || ( s_demos.keyNav && Menu_ItemAtCursor( b->generic.parent ) == self );
	int			x = b->generic.left, y = b->generic.top;
	vec4_t		fill;
	const float	*border = lit ? demosBorderFocus : demosButtonBorder;

	Vector4Copy( lit ? demosButtonFillLit : demosButtonFill, fill );
	if ( lit ) {
		fill[3] *= 0.85f + 0.15f * sin( uis.realtime * 0.006f );
	}
	UI_FillRect( x, y, BUTTON_W, BUTTON_H, fill );
	Demos_DrawOutline( x, y, BUTTON_W, BUTTON_H, border );
	Demos_DrawOutline( x + 1, y + 1, BUTTON_W - 2, BUTTON_H - 2, border );
	UI_DrawProportionalString( x + BUTTON_W / 2, y + ( BUTTON_H - 20 ) / 2, b->string, UI_CENTER | UI_SMALLFONT, lit ? color_white : demosButtonText );
}

/*
===============
Demos_MenuInit (0xf023)
===============
*/
static void Demos_MenuInit( void ) {
	memset( &s_demos, 0 ,sizeof(demos_t) );

	Demos_Cache();

	s_demos.menu.key = Demos_MenuKey;
	s_demos.menu.draw = Demos_MenuDraw;
	s_demos.menu.wrapAround = qtrue;
	s_demos.menu.fullscreen = qtrue;

	// DeFRaG: the big "DEMOS" banner font at the top centre.  Small, top right, so the rows below can start higher
	s_demos.banner.generic.type		= MTYPE_PTEXT;
	s_demos.banner.generic.flags	= QMF_INACTIVE;
	s_demos.banner.generic.x		= 632;
	s_demos.banner.generic.y		= 8;
	s_demos.banner.string			= "DEMOS";
	s_demos.banner.color			= color_red;
	s_demos.banner.style			= UI_RIGHT|UI_SMALLFONT;

	// BACK and PLAY: plates with text (the DeFRaG buttons were 128x64 bitmaps), bottom left and right
	s_demos.back.generic.type		= MTYPE_PTEXT;
	s_demos.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.back.generic.id			= ID_BACK;
	s_demos.back.generic.callback	= Demos_MenuEvent;
	s_demos.back.generic.ownerdraw	= Demos_ButtonDraw;
	s_demos.back.generic.x			= BUTTON_MARGIN;
	s_demos.back.generic.y			= BUTTON_Y;
	s_demos.back.string				= "BACK";

	s_demos.go.generic.type			= MTYPE_PTEXT;
	s_demos.go.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.go.generic.id			= ID_GO;
	s_demos.go.generic.callback		= Demos_MenuEvent;
	s_demos.go.generic.ownerdraw	= Demos_ButtonDraw;
	s_demos.go.generic.x			= 640 - BUTTON_MARGIN - BUTTON_W;
	s_demos.go.generic.y			= BUTTON_Y;
	s_demos.go.string				= "PLAY";

	s_demos.list.generic.type		= MTYPE_SCROLLLIST;
	s_demos.list.generic.flags		= QMF_PULSEIFFOCUS;
	s_demos.list.generic.callback	= Demos_MenuEvent;
	s_demos.list.generic.id			= ID_LIST;
	s_demos.list.generic.x			= 10;
	s_demos.list.generic.y			= 50;		// DeFRaG: 75 / 21 rows; FOLDER: and SEARCH: sit above it now, the banner is small
	s_demos.list.width				= 65;
	s_demos.list.height				= 22;		// ends at y = 402, above the BACK / PLAY buttons (y = 416)
	s_demos.list.columns			= 1;
	s_demos.list.itemnames			= (const char **)s_demos.demolist;

	s_demos.folder.generic.type		= MTYPE_FIELD;
	s_demos.folder.generic.name		= "FOLDER";
	s_demos.folder.generic.flags	= QMF_SMALLFONT|QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	s_demos.folder.generic.callback	= Demos_MenuEvent;
	s_demos.folder.generic.ownerdraw = Demos_FieldDraw;
	s_demos.folder.generic.x		= FIELD_X;
	s_demos.folder.generic.y		= 7;
	s_demos.folder.field.widthInChars = FIELD_CHARS;		// DeFRaG: 64; as wide as the list (longer paths scroll)
	s_demos.folder.field.maxchars	= DEMOPATH_SIZE - 1;

	// under the FOLDER: line, same left edge, as wide as the list
	s_demos.search.generic.type		= MTYPE_FIELD;
	s_demos.search.generic.name		= "SEARCH";
	s_demos.search.generic.flags	= QMF_SMALLFONT|QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.search.generic.callback	= Demos_MenuEvent;
	s_demos.search.generic.id		= ID_SEARCH;
	s_demos.search.generic.ownerdraw = Demos_FieldDraw;
	s_demos.search.generic.x		= FIELD_X;
	s_demos.search.generic.y		= 29;
	s_demos.search.field.widthInChars = FIELD_CHARS;
	s_demos.search.field.maxchars	= SEARCH_MAXCHARS;

	s_demos.count.generic.type		= MTYPE_TEXT;
	s_demos.count.generic.flags		= QMF_SMALLFONT|QMF_INACTIVE;
	s_demos.count.generic.x			= 320;
	s_demos.count.generic.y			= 408;		// under the list, between BACK and PLAY
	s_demos.count.string			= s_demos.countText;
	s_demos.count.style				= UI_CENTER|UI_SMALLFONT;
	s_demos.count.color				= color_white;

	s_demos.click.listId = ID_LIST;
	s_demos.click.lastId = ID_LIST;
	Demos_ClickReset();

	Menu_AddItem( &s_demos.menu, &s_demos.banner );
	Menu_AddItem( &s_demos.menu, &s_demos.list );
	Menu_AddItem( &s_demos.menu, &s_demos.search );
	Menu_AddItem( &s_demos.menu, &s_demos.back );
	Menu_AddItem( &s_demos.menu, &s_demos.go );
	Menu_AddItem( &s_demos.menu, &s_demos.folder );
	Menu_AddItem( &s_demos.menu, &s_demos.count );

	// the widgets worked out their mouse areas for their stock look: use the plates
	s_demos.folder.generic.left = s_demos.search.generic.left = 8;
	s_demos.folder.generic.right = s_demos.search.generic.right = FIELD_PLATE_X + FIELD_CHARS * SMALLCHAR_WIDTH + 8;
	s_demos.folder.generic.top = s_demos.folder.generic.y - 2;
	s_demos.search.generic.top = s_demos.search.generic.y - 2;
	s_demos.folder.generic.bottom = s_demos.folder.generic.top + SMALLCHAR_HEIGHT + 4;
	s_demos.search.generic.bottom = s_demos.search.generic.top + SMALLCHAR_HEIGHT + 4;
	s_demos.back.generic.left = s_demos.back.generic.x;
	s_demos.back.generic.right = s_demos.back.generic.x + BUTTON_W - 1;
	s_demos.go.generic.left = s_demos.go.generic.x;
	s_demos.go.generic.right = s_demos.go.generic.x + BUTTON_W - 1;
	s_demos.back.generic.top = s_demos.go.generic.top = BUTTON_Y;
	s_demos.back.generic.bottom = s_demos.go.generic.bottom = BUTTON_Y + BUTTON_H - 1;

	// MenuField_Init (Menu_AddItem) clears the fields: fill them after the items are added.  An empty folder or search
	// result is handled by Demos_ApplyFilter (placeholder line, "go" button hidden)
	trap_Cvar_VariableStringBuffer( "df_demos_CurrentPath", s_demos.path, sizeof( s_demos.path ) );
	trap_Cvar_VariableStringBuffer( "df_demos_Search", s_demos.search.field.buffer, SEARCH_MAXCHARS + 1 );
	s_demos.search.field.cursor = strlen( s_demos.search.field.buffer );
	Demos_BuildList();
}

/*
=================
Demos_Cache (0xf2e2)
=================
*/
void Demos_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
}

/*
===============
UI_DemosReturnAfterPlayback

Called right after the main menu was brought up by UI_SetActiveMenu( UIMENU_MAIN ), which is what the engine does when
a demo is over (and when it is left with Disconnect).  A demo that was started from the browser opens the browser on top
of the main menu again, on the folder / search / list position it was left with.  Returns qtrue when it did.

df_demos_ReturnToBrowser holds the name of the demo the browser started.  It only counts when the last session the
client had was that demo: the engine keeps the demo's file name as the session's "server name" (uiClientState_t
servername), so a flag left over from a demo that was followed by a real server is dropped.  (defrag_demoPlayback cannot
tell: the cgame writes it once, during its init, when it is still 0.)  Engines that clear the name get the browser too.
===============
*/
qboolean UI_DemosReturnAfterPlayback( void ) {
	char				want[MAX_QPATH], error[8];
	uiClientState_t		cs;

	trap_Cvar_VariableStringBuffer( "df_demos_ReturnToBrowser", want, sizeof( want ) );
	if ( !want[0] ) {
		return qfalse;
	}
	trap_Cvar_Set( "df_demos_ReturnToBrowser", "" );

	trap_Cvar_VariableStringBuffer( "com_errorMessage", error, sizeof( error ) );
	if ( error[0] ) {
		return qfalse;		// the main menu is showing an error message that must stay visible
	}
	trap_GetClientState( &cs );
	if ( cs.servername[0] && Q_stricmpn( cs.servername, want, strlen( want ) ) ) {
		return qfalse;		// the last session was something else
	}
	UI_DemosMenu();
	return qtrue;
}

/*
===============
UI_DemosMenu
===============
*/
void UI_DemosMenu( void ) {
	Demos_MenuInit();
	UI_PushMenu( &s_demos.menu );
}
