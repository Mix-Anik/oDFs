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

## The refinement (algorithm as named by the function's own debug strings, in code order)
`frametime` ms; `pos0` = player position at the start of the frame, `pos1` = at the end; if `pos0` is inside the trigger already, "stepping up +%f" probes upward
(`STEPSIZE`-style) before tracing; a trace against the trigger finds the crossing point `postrig` and surface `normal` ("failed to trace for trigger." /
"player inside the trigger, no adjustement." are the early exits). With `vel0` / `vel1` the velocities before / after the move:
- `v0 = -vel0 · normal`, `v1 = -vel1 · normal` (speed towards the surface, ups), `a = (v1 - v0) / frametime`, `d = -(postrig - pos0) · normal` (distance to the surface);
- if the move was obstructed ("detected obtructed move, cancelling acceleration") `a = 0`; `a == 0` → linear: `t = d / v0`; `v0 == 0` → "not moving - defaulting to main timer";
- else quadratic `d = v0*t + 0.5*a*t²` (negative discriminant → "bailing out");
- finally the timer offset is adjusted: `"timer offset += %f ms"` for a start, `"-= %f ms"` for a stop (the sub-millisecond correction lands in the `raceTimer_t` fractional fields,
  `fracAccum`/`fracDelta` — exact stores not traced this pass).
The numeric details (which of `t`/`frametime - t` is applied, sign conventions, clamps) were NOT decoded; the above is the structure the strings and float ops make visible.

## oDFs
Not ported. oDFs's `G_TimerTick` derives elapsed time from `level.time` (whole-millisecond), so it has no sub-frame correction; porting needs the numeric half decoded.
`df_dbg_1mstimer` (default `0`, flags 0) and the cvar-table entry are listed in [../G_UTILS/cvar_table.md](../G_UTILS/cvar_table.md).
