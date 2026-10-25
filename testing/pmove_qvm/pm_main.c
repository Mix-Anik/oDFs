/*
 * pm_main.c - entry point of pmove.qvm
 *
 * A QVM that contains only the shared movement code (bg_pmove.c, bg_slidemove.c) so the test
 * host can run Pmove() on a given playerState without the rest of the game. vmMain must stay the
 * first function of the first source file (the interpreter enters at instruction 0).
 */
#include "../../code/qcommon/q_shared.h"
#include "../../code/game/bg_public.h"
#include "pmtest.h"

void trap_Print( const char *text );
void trap_Error( const char *text );
void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
int trap_PointContents( const vec3_t point, int passEntityNum );

static pmReq_t	request;
static pmTrajReq_t	trajRequest;

static int PMT_Step( void );

/* must stay the first function: the interpreter enters the module at instruction 0 */
int vmMain( int command, int arg0, int arg1, int arg2 ) {
	switch ( command ) {
	case PMT_STEP:
		return PMT_Step();
	case PMT_ADDR:
		return (int)&request;
	case PMT_TRAJ:
		BG_EvaluateTrajectory( &trajRequest.tr, trajRequest.atTime, trajRequest.result );
		return 0;
	case PMT_TRAJADDR:
		return (int)&trajRequest;
	}
	return -1;
}

static int PMT_Step( void ) {
	pmReq_t *r = &request;
	pmove_t pm;
	int i;

	memset( &pm, 0, sizeof( pm ) );

	pm.ps = &r->ps;
	pm.cmd = r->cmd;
	pm.tracemask = r->tracemask;
	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = 0;
	pm.noFootsteps = 0;
	pm.pmove_fixed = r->pmove_fixed;
	pm.pmove_msec = r->pmove_msec;
	pm.obs_disabled = r->obs_disabled;
	pm.promode = r->promode;
	pm.killWallbug = r->killWallbug;

	Pmove( &pm );

	r->cmd = pm.cmd;
	r->numtouch = pm.numtouch;
	for ( i = 0; i < PMT_MAXTOUCH; i++ ) {
		r->touchents[i] = pm.touchents[i];
	}
	r->waterlevel = pm.waterlevel;
	r->watertype = pm.watertype;
	return 0;
}

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start( argptr, fmt );
	vsprintf( text, fmt, argptr );
	va_end( argptr );

	trap_Print( text );
}
