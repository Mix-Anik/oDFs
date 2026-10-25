/*
 * goldrun.h - building golden files from demos and replaying them through the game
 */
#ifndef DFGOLDEN_GOLDRUN_H
#define DFGOLDEN_GOLDRUN_H

#include "golden.h"

typedef struct {
	const char	*qvm;			/* pmove.qvm: the per-step command search */
	const char	*bsp, *mapsDir;	/* the demo's map: one file or a directory of <map>.bsp */
	int			ulps;			/* float tolerance of the comparison stored in the file */
	int			recoverUlps;	/* tolerance while searching the commands (0: bit exact) */
	int			deep;			/* exhaustive search for the commands of the hard steps */
} goldMakeOpts_t;

/* reads a demo and finds the commands; ~ a minute for a long demo */
int		Gold_Build( const char *demoPath, const goldMakeOpts_t *o, golden_t *g, char *err, int errSize );

int		Gold_RacePlain( const char *serverinfo, const char *systeminfo );

typedef struct {
	const char	*game;			/* qagame.qvm */
	const char	*offsets;		/* offsets.qvm: where the game keeps its own copy of the health and the decay clock */
	int			maxReport;		/* print this many mismatching steps */
	int			ignoreHealth;	/* do not compare the health (running a game whose internals are unknown) */
	int			listAll;		/* list the known mismatches too */
	int			markKnown;		/* store the result in the file's GS_KNOWN_MISS bits (when making a file) */
	int			quiet;
	struct goldMiss_s	*miss;		/* receives the mismatching steps (refining the commands) */
	int			missCap;
} goldRunOpts_t;

typedef struct goldMiss_s {
	int			step;
	float		simVel[3];		/* the game's velocity after the step */
} goldMiss_t;

typedef struct {
	int			steps;			/* compared steps */
	int			exact;
	int			knownMiss;		/* mismatches the file already lists */
	int			regressions;	/* mismatches the file does not list */
	int			fixed;			/* listed mismatches that now match */
	int			raceChecked, raceBad;
	int			finishWant, finishGot;	/* race time at the finish (-1 none): the demo's, and the game's */
	int			fire[16];		/* steps in which a weapon was fired, by weapon */
	int			knock, pad, damage;	/* steps with server knockback, a jump pad, damage */
	int			finishBad;		/* the game's finish time differs and the file does not list that */
	int			finishFrom;		/* finishWant is the time in the demo's file name (else the last time the recording publishes) */
	int			error;			/* the game could not run */
	int			missCount;		/* mismatching steps stored in opts->miss */
} goldRunResult_t;

int		Gold_Run( golden_t *g, const goldRunOpts_t *o, goldRunResult_t *res );

#endif
