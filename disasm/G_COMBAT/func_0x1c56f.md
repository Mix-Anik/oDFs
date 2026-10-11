# `func_0x1c56f` — "is another player near this position" proximity check

**Address:** `0x1c56f`
**Stack frame:** `enter 0x1048`
**Called from:** `0x1c65e`, `0x1c6be`, `0x1c860`, `0x26fbf` (per this
project's established convention, "called from" lists the entry address of
each *calling function*, not the exact call-instruction address — `0x1c6be`
is itself a separate, 4-arg "alt cmd fetch" function per
[`../G_ACTIVE/ClientThink_real.md`](../G_ACTIVE/ClientThink_real.md), which
internally calls this one somewhere in its own body).
**Args:** 8 declared, only `arg0` (an entity pointer) is actually read.

## Correction to this function's prior characterization

[`G_Damage.md`](G_Damage.md)'s `0x132660`-cvar-bit section previously
described this function as "a no-op returning `0`/false when the bit is
clear (called from 4 sites, shape consistent with a debug-print/log
formatter that's compiled in but inert unless the bit is set)". That
characterization only looked at the function's entry gate and got the
branch direction **backwards**, and never read past it. Re-decoded in full
this pass (while sweeping every `trap_EntitiesInBox` call site in the QVM
looking for `G_TouchTriggers` — see
[`../G_UTILS/README.md`](../G_UTILS/README.md)): it's actually the opposite
— the function does its real work when the `0x132660` bit is **clear**
(the normal/default case, since that global is never written anywhere in
game bytecode), and is the no-op **only when the bit is set**. It is not a
debug-print formatter; it performs a real entity-box query.

## Full decode

```c
int func_0x1c56f( gentity_t *arg0 ) {
    if ( *(int*)0x132660 & 1 )       // bit 0x1 — see G_Damage.md's survey of this global
        return 0;

    // box: arg0->s.origin (+0x5c/+0x60/+0x64, confirmed offset — matches
    // vanilla entityState_t.origin's position after pos/apos trajectories)
    // asymmetric: x/y ±15, z: -24 (below) / +32 (above) — a player-sized
    // touch volume, NOT the generic vanilla G_TouchTriggers {40,40,52} box
    vec3_t mins = { origin[0]-15.0f, origin[1]-15.0f, origin[2]-24.0f };
    vec3_t maxs = { origin[0]+15.0f, origin[1]+15.0f, origin[2]+32.0f };

    count = trap_EntitiesInBox( mins, maxs, list, 0x400 );   // trap -0x21, confirmed

    for ( i = 0; i < count; i++ ) {
        gentity_t *candidate = &g_entities[ list[i] ];        // base 0x1b2f0, stride 0x43c
        if ( candidate->client /* +0x204, confirmed */ != NULL )
            return 1;    // found another player-having entity in the box — bail out immediately
        // (client == NULL: not a player, keep scanning)
    }
    return 0;   // scanned every candidate, none had a client — no player nearby
}
```

No `->touch` dispatch anywhere — this is a pure boolean proximity test, not
a touch-trigger handler. Confirms it is **not** `G_TouchTriggers` either
(ruling out `0x23158`'s... no, wait: this function's own `EntitiesInBox`
call site is `0x1c5c7`, a different, previously-unaccounted-for site found
during the same QVM-wide sweep that also resolved
[`func_0x20998`](../G_MISSILE/func_0x20998.md) and
[`GrappleHookMover`](../G_ACTIVE/GrappleHookMover.md)).

## Purpose (not fully confirmed, but a reasonable fit)

Given its callers are all cmd-fetch/input-adjacent functions reached from
`ClientThink_real`'s spectator/follow-cam paths, a boolean "is a player
near me" check in that context is plausibly used for a
follow-cam/spectator-targeting heuristic (e.g. "don't auto-follow into an
empty area") or a similar proximity gate — **not independently confirmed
this pass**, flagged rather than asserted.

## oDFs implementation status

**Not implemented.** Low-value, narrow-purpose helper; not behavior-critical
to movement/knockback/triggers. Documented here mainly to correct the prior
wrong characterization and close out its `EntitiesInBox` call site as part
of the `G_TouchTriggers` search.
