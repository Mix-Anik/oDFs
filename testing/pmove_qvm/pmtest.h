/*
 * pmtest.h - interface between the test host and pmove.qvm
 *
 * Shared by the QVM (compiled against code/game headers) and the host (engine headers). Only
 * 32 bit ints, floats and the structs q_shared.h defines identically in both are used.
 */
#ifndef PMTEST_H
#define PMTEST_H

#define PMT_STEP	1		/* vmMain( PMT_STEP ) runs one Pmove() on the module's request block */
#define PMT_ADDR	2		/* vmMain( PMT_ADDR ) returns the address of that block (a pmReq_t) */

#define PMT_TRAJ	3		/* vmMain( PMT_TRAJ ) evaluates BG_EvaluateTrajectory() on the trajectory block */
#define PMT_TRAJADDR	4		/* vmMain( PMT_TRAJADDR ) returns the address of that block (a pmTrajReq_t) */

#define PMT_MAXTOUCH	32

typedef struct {
	playerState_t	ps;				/* in / out */
	usercmd_t		cmd;			/* in */

	int				tracemask;		/* in: what ClientThink_real passes to Pmove */
	int				pmove_fixed;
	int				pmove_msec;
	int				obs_disabled;
	int				promode;
	int				killWallbug;

	int				numtouch;		/* out */
	int				touchents[PMT_MAXTOUCH];
	int				waterlevel;
	int				watertype;
} pmReq_t;

/* a mover's position is computed in the QVM so it has the float semantics of the server's qagame */
typedef struct {
	trajectory_t	tr;		/* in */
	int				atTime;	/* in */
	vec3_t			result;	/* out */
} pmTrajReq_t;

#endif
