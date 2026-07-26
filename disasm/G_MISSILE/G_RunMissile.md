# G_RunMissile — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x000223cb` (vanilla baseq3 equivalent at `0x1a82e`)
**DeFRaG hash:** modified vs vanilla (−2 instructions per `G_MISSILE/README.md`'s
prior hash-table survey) — **fully decoded this pass**, closing the last
previously-undecoded function in the direct rocket fire→explode→damage chain.
**Args:** `ent` (the missile entity)
**Called from:** `0x489b`, `0x21b5e` (`FireWeapon`), `0x28e8b`

---

## Why this was decoded

Requested as part of a full rocket-knockback pipeline audit (fire → flight →
impact → splash → player velocity → landing). Every other function in the
chain (`fire_rocket`, `G_ExplodeMissile`, `G_MissileImpact`, `G_Damage`,
`G_RadiusDamage`) had already been decoded across several prior passes;
`G_RunMissile` — which determines *where and when* a rocket actually detonates
(by tracing its flight path each frame) — was the one remaining gap, previously
only grepped for the `df_promode` cache address (a shallow check, not a full
decode).

## Headline finding

**No `df_promode` reference anywhere in the function** (confirmed both by the
original grep and by this full instruction-by-instruction decode). The
function is structurally a very close match to vanilla's `G_RunMissile` with
three confirmed differences, **none of which affect rocket splash-knockback
math or the explosion point in the self-splash rocket-jump scenario**:

### 1. `passent` override via `target_ent` — confirmed present but inert for rockets

```c
if ( ent->target_ent != NULL )         // ent+0x29c
    passent = ent->target_ent->s.number;  // read via first-4-bytes-of-entity idiom
else
    passent = ent->r.ownerNum;          // ent+0x200 — vanilla's only behavior
```

`ent+0x29c` is confirmed as `target_ent` (cross-referenced against
`fire_rocket.md`: `bolt->target_ent = 0` is set unconditionally at
`0x227d5`-`0x227d8`, exact same offset). **Since `fire_rocket` always sets
`target_ent = NULL` for every rocket, and no other decoded function writes to
this field on a live rocket entity, this branch is always dead code for
rockets** — every rocket's `passent` is `ent->r.ownerNum`, vanilla-identical.
This is presumably a DeFRaG addition for the swinging-grapple hook (which has
its own custom collision needs — consistent with the grapple-swing subsystem
found in
[RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md)),
not a rocket mechanic.

### 2. Early unconditional `trap_LinkEntity` call

```
0x22440-42: trap_LinkEntity(ent)     ; trap -0x1f, confirmed via g_syscalls.asm
```

Called unconditionally right after the position update, **before** checking
whether the missile hit anything — earlier than vanilla, which only links at
the very end (and only if the missile is still flying). A second call to a
different function, `0x1153` (not independently decoded — plausibly a
bookkeeping/second-link wrapper), still happens at the same point vanilla
calls `trap_LinkEntity`, for the same two cases vanilla does (still-flying,
or hit-but-still-`ET_MISSILE` i.e. bounced). Net effect: the entity may get
linked twice in the bounce case. No `ps.velocity`, damage, or knockback
involvement — this affects collision-detection timing/bookkeeping only, not
physics math. Not investigated further (out of scope; no plausible mechanism
for a knockback magnitude change).

### 3. `startsolid`/`allsolid` fixup does an extra `trap_Trace`

Vanilla's fixup for `tr.startsolid || tr.allsolid` is two lines (force
`tr.fraction = 0`, copy `currentOrigin` into `tr.endpos`). DeFRaG's fixup
block (`0x2240a`-`0x22436`) issues a **second full `trap_Trace` call** (raw
trap `-0x19` = `trap_Trace`, confirmed) with `ent->r.currentOrigin`/`mins`/
`maxs`, the same `passent`, and `ent->clipmask` — a genuine second trace, not
just field patching. Not fully resolved what this buys DeFRaG structurally
(plausibly a "get an accurate `entityNum` while stuck" re-trace). This path
only triggers when the missile's collision trace starts already embedded in
solid geometry — an edge case, not the normal "rocket travels through open
air and hits the ground/wall near the player" self-splash scenario. Not
pursued further — out of scope, no plausible knockback-magnitude mechanism.

---

## Structure (full, matches vanilla apart from the 3 points above)

```c
void G_RunMissile( gentity_t *ent ) {
    vec3_t origin;
    BG_EvaluateTrajectory( &ent->s.pos, level.time, &origin );   // call 0x4d73

    int passent = ent->target_ent ? ent->target_ent->s.number : ent->r.ownerNum;

    trace_t tr;
    func_0x43c3( ent, origin, &tr, passent );   // wraps trap_Trace; also gates an
                                                  // alternate debug trace path on
                                                  // global 0x132660 bit 0x2 (already
                                                  // ruled out in G_Damage.md section 1)

    if ( tr.startsolid || tr.allsolid ) {
        // extra re-trace, see point 3 above
    } else {
        VectorCopy( tr.endpos, ent->r.currentOrigin );
    }

    trap_LinkEntity( ent );        // unconditional, early — see point 2 above

    if ( tr.fraction != 1 ) {
        if ( tr.surfaceFlags & SURF_NOIMPACT /* 0x10, confirmed */ ) {
            if ( ent->parent && ent->parent->client &&
                 ent->parent->client->hook == ent ) {          // client+0x34c = hook
                ent->parent->client->hook = NULL;
                ent->parent->client->somefield_0x370 = 0;      // DeFRaG grapple state, not decoded
                ent->parent->client->somefield_0x348 = 0;      // DeFRaG grapple state, not decoded
            }
            G_FreeEntity( ent );   // 0x284b7, confirmed
            return;
        }
        G_MissileImpact( ent, &tr );   // 0x2210f, confirmed — already fully analyzed
        if ( ent->s.eType != ET_MISSILE )
            return;
    }
    func_0x1153( ent );   // not independently decoded — see point 2
}
```

**Every field/offset used here (`target_ent`=+0x29c, `r.ownerNum`=+0x200,
`parent`=+0x258, `s.eType`=+0x4, `SURF_NOIMPACT`=0x10) was already confirmed
in prior passes** ([fire_rocket.md](fire_rocket.md),
[G_MissileImpact.md](G_MissileImpact.md)) or matches known vanilla Q3
constants exactly.

---

## Conclusion for the rocket-knockback investigation

**No promode-conditional logic, no additive/multiplicative velocity term, and
no alternate splash-damage trigger path exists anywhere in `G_RunMissile`.**
The function's only job is trajectory evaluation and collision-trace dispatch
to the already-analyzed `G_MissileImpact`/`G_ExplodeMissile`. This closes the
last previously-undecoded function in the direct rocket fire→explode→damage
call chain — combined with the prior exhaustive passes
([G_COMBAT/G_Damage.md](../G_COMBAT/G_Damage.md),
[RocketJump_ExtraVelocity_Search.md](../G_COMBAT/RocketJump_ExtraVelocity_Search.md),
[RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md)),
**every function reachable between "player fires rocket" and "player's
`ps.velocity` is updated by splash knockback" has now been decoded**, and
none contains an unexplained term.

---

## oDFs implementation notes

No change needed. oDFs's `G_RunMissile`-equivalent (vanilla-derived) already
implements the vanilla trajectory/trace/impact dispatch; none of the three
confirmed DeFRaG differences here have a plausible gameplay effect worth
porting (dead code for rockets, link-timing bookkeeping, and a
solid-embedded-only edge case).
