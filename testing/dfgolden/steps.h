/*
 * steps.h - what the per-step physics check shares with the full-game replay
 */
#ifndef DFGOLDEN_STEPS_H
#define DFGOLDEN_STEPS_H

#include "q_shared.h"
#include "demo.h"

/* the usercmd found for the step into a snapshot (indexed by that snapshot's number) */
typedef struct {
	signed char		fm, rm, um;
	unsigned char	btn;			/* BUTTON_ATTACK was needed to keep PMF_RESPAWNED */
	unsigned char	regular;		/* the pair is a regular step */
	unsigned char	exact;			/* Pmove() reproduced the step exactly with this command */
} recoveredCmd_t;

/* the collision code the engine that recorded a demo ran: 0 oDFe's (quake3e), 1 the same in x87 registers (32 bit x86 builds),
   2 the ioq3 form with floats (DeFRaG engines built on ioq3); the host has a copy of each (host/coll.h) */
int		Steps_EngineColl( const char *version );
extern int	gHostColl;		/* which copy the host runs */

/* the usercmd of the step A -> B (command time and view angles follow from B) */
void	Steps_BuildCmd( usercmd_t *cmd, const playerState_t *A, const playerState_t *B, int buttons, int fm, int rm,
				int um, int weapon );

extern int	gRecoverDeep;	/* Steps_Recover also runs the exhaustive search for the steps the others do not solve (slow) */

/* a step whose command is searched again: the game replay got simVel where the recording has the demo's velocity;
   the difference between the game's velocity and what Pmove() alone gives is the server's (knockback), and the
   command is the one whose Pmove() result plus that difference is the recorded velocity */
typedef struct {
	int				step;
	signed char		fm, rm, um;		/* the command the file has now */
	float			simVel[3];		/* the game's velocity after the step with that command */
	signed char		nfm, nrm, num;	/* out: the better command */
	unsigned char	found;
} refineReq_t;

int		Steps_Refine( const char *demoPath, const char *qvm, const char *bspPath, const char *mapsDir, refineReq_t *reqs, int num );

/* runs the per-step recovery on a demo and returns the commands it found (caller frees) */
int		Steps_Recover( const char *demoPath, const char *qvm, const char *bspPath, const char *mapsDir, int ulps,
				recoveredCmd_t **cmds, int *num );

/* m:ss.mmm with a leading minus; seconds relative to the race timer */
void	FormatSignedTime( char *out, int size, int ms );
int		TimerRelative( const demo_t *d, int snap, int *relMs );
#endif
