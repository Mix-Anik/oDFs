/*
 * demo.h - dm_68 demo reader for dfgolden
 *
 * Reads a demo with the engine's own message code (qcommon/msg.c), so the
 * playerState of every snapshot is decoded exactly as the game client does.
 * Entities are not decoded: the playerState comes before them in a snapshot,
 * and every demo message is length framed.
 */
#ifndef DFGOLDEN_DEMO_H
#define DFGOLDEN_DEMO_H

#include "q_shared.h"
#include "qcommon.h"

#define DEMO_CS_SERVERINFO	0
#define DEMO_CS_SYSTEMINFO	1
#define DEMO_MAX_CSCHANGES	256

/* diagnostics: keep every entity state of the snapshots in [gKeepFrom, gKeepTo) */
extern int gKeepFrom, gKeepTo;

/* a solid brush-model entity (door, plat, func_static ...) as the snapshot describes it */
typedef struct {
	int			number;
	int			modelindex;		/* inline model: "*N" */
	trajectory_t	pos;
	trajectory_t	apos;
} demoBmodel_t;

typedef struct {
	int			start, count;	/* range in demo_t.bmodels */
} demoBmList_t;

typedef struct {
	int			seq;			/* demo message sequence */
	int			serverTime;
	int			deltaNum;		/* absolute message number, -1 = full snapshot */
	int			snapFlags;
	int			cmdCount;		/* server commands carried by the message */
	int			csChanges;		/* configstring 0/1 changes seen up to this snapshot */
	int			badDelta;		/* delta source missing / too old */
	int			bmList;			/* solid brush models of this snapshot: index into demo_t.bmLists */
	playerState_t	ps;
} demoSnap_t;

typedef struct {
	int			snap;			/* index of the first snapshot after the change */
	int			index;			/* configstring number (0 or 1) */
} demoCsChange_t;

typedef struct {
	char		err[256];		/* non-empty: parse failure (message + position) */
	int			errMsg;			/* message index of the failure */

	int			numMsgs;
	int			numGamestates;
	int			endMarker;		/* file ends with the -1/-1 marker */
	int			clientNum;

	char		serverinfo[BIG_INFO_STRING];
	char		systeminfo[BIG_INFO_STRING];
	char		serverinfoNow[BIG_INFO_STRING];	/* after the last change */
	char		systeminfoNow[BIG_INFO_STRING];

	demoBmodel_t	*bmodels;
	int			numBmodels, capBmodels;
	demoBmList_t	*bmLists;
	int			numBmLists, capBmLists;

	entityState_t	*keepEnts;		/* entity states of the kept snapshots, back to back */
	int			*keepStart;		/* [snapshot - gKeepFrom] = first index into keepEnts */
	int			*keepCount;
	int			numKeep, capKeep;

	demoSnap_t	*snaps;
	int			numSnaps, capSnaps;

	demoCsChange_t	csChange[DEMO_MAX_CSCHANGES];
	int			numCsChanges;

	/* structural findings */
	int			gapBeforeFirstSnap;	/* sequence gap between gamestate and first snapshot */
	int			seqGaps;			/* gaps after the first snapshot */
	int			firstSeqGapSnap;	/* snapshot index of the first such gap, -1 */
	int			seqOrder;			/* sequence not increasing */
	int			firstDeltaSnap;		/* first snapshot that is a delta, -1 if none */
	int			fullSnaps;			/* full (non delta) snapshots after the startup window */
	int			badDeltas;
	int			msgsWithoutSnap;	/* messages after the gamestate without a snapshot */
	int			multiSnapMsgs;
	int			unknownSvc;
} demo_t;

/* returns 0 when the file could be opened (parse problems are reported in d->err) */
int		Demo_Load( const char *path, demo_t *d );
void	Demo_Free( demo_t *d );

#endif
