# `func_0x274db` — `trigger_multiple` touch with the "1 ms timer" correction

**Identity (bytecode + data):** the data-segment spawn table maps `"trigger_multiple"` to `func_0x27868` (`0x2680`), which — after the vanilla `wait`/`random` key parsing and the
`"trigger_multiple has random >= wait"` warning — sets `ent->touch (+0x2c0) = 0x274db` (bytecode `0x278b1`) and `ent->use (+0x2c4) = 0x274cf` (`Use_Multi`-shaped).
So `0x274db(self, other, trace)` replaces vanilla `Touch_Multi`; it is the touch callback of **every** `trigger_multiple`, including the race timer start/stop/checkpoint triggers.

## Flow
```c
void Touch_Multi_1ms( gentity_t *self, gentity_t *other, trace_t *trace ) {
    if ( !other->client ) return;                         // +0x204
    before = func_0x3abc( other->client );                // raceTimer_t.state (+0xc via GetTimerRecord 0x3588)
    func_0x273a5( self, other );                          // the ordinary Touch_Multi behaviour (fires targets -> start/stop/checkpoint)
    after  = func_0x3abc( other->client );
    if ( after != before ) {
        if ( after == 1 /*TIMER_RUNNING: start*/ ) debugPrint( "1mstimer: detected TIMER_START" );
        else if ( after == 2 /*TIMER_FINISHED: stop*/ ) debugPrint( "1mstimer: detected TIMER_STOP" );
        else return;
        /* sub-frame refinement, see below */
    }
}
```
Debug prints are emitted through the engine print trap only when `df_dbg_1mstimer` (cvar `0x130f7c`, `.integer` `0x130f88`) is nonzero. (`state` values as in
[race_timer_system.md](race_timer_system.md): 0 idle, 1 running, 2 finished, 3 checkpoint.)

## The refinement — numerics DECODED (bytecode `0x2752d`-`0x27865`)
Per-client saved state (new `gclient_t` fields, written elsewhere each frame — writer not yet located): `client+0x2d4..0x2dc` = `pos0` (position at the previous command),
`client+0x2e0..0x2e8` = `vel0`, `client+0x2ec..0x2f4` = `vel1`, `client+0x2f8` = previous `commandTime`. Current position `pos1` = `ps.origin` (`client+0x14`).
```c
F      = (float)( ps.commandTime - client->prevCommandTime );          // frametime in ms  (0x2752f..)
// find where the player's motion crossed the trigger: trace pos0 -> pos1 (mask 0x40000001 = CONTENTS_TRIGGER|CONTENTS_SOLID),
// iterating (passEntityNum starts -1, loop at 0x2759c, up to the trigger entity) and, if pos0 is inside a solid/trigger, "stepping up" pos0.z towards pos1.z (debug: "stepping up +%f");
// 'failed to trace for trigger' / 'player inside the trigger' end the function early.
n      = trace plane normal (local3c..44);   postrig = point on the trigger found by the trace (local30..38)
v0     = -( vel0 . n );                       // speed towards the surface before the move   (bytecode 0x276a2.. multiplies by the constant -1.0f)
v1     = -( vel1 . n );                       // after the move
d      = -( (postrig - pos0) . n );            // distance travelled along the normal until the crossing
a      = 1000.0f * (v1 - v0) / F;             // ups/s^2   (0x447a0000 = 1000.0f)
if ( v1 <= 1e-5f ) a = 0;                     // "detected obtructed move, cancelling acceleration" (0x3727c5ac = 1e-5)
if ( fabs(a) < 1e-5f ) {                      // linear:  d = v0 * t
    if ( v0 <= 1e-5f ) return;                // "not moving - defaulting to main timer"
    t = 1000.0f * d / v0;                     // ms
} else {                                      // quadratic: d = v0*t + a*t^2/2
    disc = v0*v0 + 2.0f * a * d;
    if ( disc < 0 ) return;                   // "negative discriminant - bailing out"
    t = 2000.0f * d / ( v0 + sqrt(disc) );    // ms, numerically stable root (sqrt = trap -0x6b)
}
offset = F - t;                               // ms left in the frame after the crossing  (0x27839)
func_0x3aab( client, offset );                // raceTimer_t.fracDelta (+0x10) = offset
```
The "+=" (start) / "-=" (stop) debug text only differs in the *message*; both store `F - t`. `fracDelta` is then consumed by `func_0x37c7`
([func_0x37c7.md](func_0x37c7.md)): on START `fracAccum = fracDelta` (after the reset) and on FINISH `fracAccum -= fracDelta`, with `bestTime = ms + round_half(fracAccum)` — i.e. the
reported time is corrected by the sub-frame start and stop offsets. Constants verified: `1000.0f`, `2000.0f`, `1e-5f`, `-1.0f`, `2.0f`. **Snapshot writer located:** `ClientThink_real` (`0x1a866`, bytecode `0x1aac1`-`0x1aada`), just before `Pmove` (`0x8653`): `client+0x2d4 = ps.origin` (12-byte `block_copy` from `+0x14`), `client+0x2e0 = ps.velocity` (from `+0x20`), `client+0x2f8 = ps.commandTime`.
`vel1` (`client+0x2ec`) is read by the touch but **no store to `0x2ec` exists in `ClientThink_real`** — its writer was not found (the other `0x2ec` constants in the file belong to unrelated `gentity` fields in the `fire_*` code). Still unread: the trace loop's exact iteration/step-up details (`0x2758c`-`0x27690`).

## Trace loop (decoded in the port pass, bytecode `0x2758c`-`0x27690`)
```c
pos0 = client->pos0;                                   // may be raised below
for ( attempt = 0; attempt < 2; attempt++ ) {
    start = pos0;  passEnt = -1;
    for ( hop = 0; hop < 2; hop++ ) {                  // hop over other triggers/solids
        trace( &tr, start, other->r.mins, other->r.maxs, ps.origin, passEnt, 0x40000001 /* CONTENTS_TRIGGER|SOLID */ );
        if ( &g_entities[tr.entityNum] == self || tr.fraction == 1.0f || tr.entityNum == 0x3fe /*world*/ ) break;
        passEnt = tr.entityNum;  start = tr.endpos;
    }
    if ( tr.entityNum != 0x3fe ) break;
    up = ps.origin.z - pos0.z;  if ( up < 0 || up > 18.0f ) break;      // step-up retry only for 0..18 units
    pos0.z = client->pos0.z = ps.origin.z;               // "stepping up +%f"
}
if ( &g_entities[tr.entityNum] != self ) -> "failed to trace for trigger."
if ( VectorLength(tr.plane.normal) < 1e-5f ) -> "player inside the trigger, no adjustement."
```
Trace-struct layout in the frame: `local24` = `trace_t` (`fraction` `+0x2c`, `endpos` `+0x30`, `plane.normal` `+0x3c`, `entityNum` `+0x58`), so `postrig = tr.endpos`, `n = tr.plane.normal`. A `vel0` normalisation call at `0x27688` has an unused result (dead code).

## oDFs
**Ported** (syntax-check clean, not run in a game): `gclient_t.timerPos0/timerVel0/timerPrevCommandTime` snapshotted before `Pmove` in `ClientThink_real` (`g_active.c`); `Touch_Multi` (`g_trigger.c`) now
records the race-timer state, runs `multi_trigger`, and calls `G_TimerSubFrame` (`g_defrag.c`) which solves the crossing time exactly as above and sets `fracAccum` (start: `= offset`, stop: `-= offset`);
`G_StartRace` zeroes the fractional fields, and `G_TimerTick` reports `bestTime = (level.time - startTime) + round_half(fracAccum)`. Deviation: `vel1` is read from `ps.velocity` at touch time
(the touch runs after `Pmove`, which is what DeFRaG's unlocated `+0x2ec` snapshot should hold). New cvar `df_dbg_1mstimer` (default 0) prints the key quantities via `G_Printf`.
**Behaviour change:** race times become sub-millisecond-corrected (typically ±1 frame of the old whole-ms value) on maps whose start/stop triggers are `trigger_multiple`.
