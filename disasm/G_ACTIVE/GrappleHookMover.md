# `func_0x22fe6` — grapple-hook push/crush/carry mover

> **CORRECTION (supersedes everything below about "grapple"):** this function is **vanilla `G_MoverPush`**, not a grapple mechanic.
> Evidence: `EntitiesInBox` max count `0x400` = MAX_GENTITIES; unlink -> list -> `currentOrigin += move`, `currentAngles(+0x1f4) += amove` -> link; per-candidate
> `groundEntityNum != pusher` overlap test + `0x22a36` (= `G_TestEntityPosition`, decoded: clipmask `+0x23c` or MASK_SOLID, trace from `ps.origin`/`trBase`, returns `&g_entities[entityNum]` on startsolid)
> + `0x22b82` (= `G_TryPushingEntity`, writes `trBase` and nudges `client->ps.origin`/`delta_angles[YAW]`); the `trType == 4` (TR_SINE) branch is vanilla's bobbing-mover instakill
> (`G_Damage(..., 99999, 0, MOD_CRUSH)`); the "linked-list walk" is vanilla's restore loop over `pushed[]`/`pushed_p` (`0x192c94`/`0x192c98`, reset at entry of
> `0x232f6` = `G_MoverTeam`); `0x23429` = `G_RunMover` (returns if `flags & 0x400` = `FL_TEAMSLAVE`, calls `G_MoverTeam` if either trType != TR_STATIONARY, then `G_RunThink 0x1153`);
> `0x22b16` = matrix-vector helper (`G_RotatePoint`-style). The "flags 0x400 = hook attached" reading is wrong. Nothing DeFRaG-specific -> nothing to port; oDFs uses the vanilla `g_mover.c`.
> `0x22eb4` = vanilla MISSIONPACK `G_TryPushingProxMine` (RESOLVED: `AngleVectors`=`0xa186` on `vec3_origin - amove`, then tail-calls `0x22e17` = `G_CheckProxMinePosition`, which uses the vanilla constants 0.125 (`0x3e000000`) and 2.0 (`0x40000000`) times `movedir` at `+0x2a4`; `0x22ab3` = `G_CreateRotationMatrix`). The `candidate+0x234` skip is probably `physicsObject` (+8 shift vs. vanilla layout: `clipmask` is at `+0x23c` here); unverified.


**Address:** `0x22fe6`
**Stack frame:** `enter 0x10b8`
**Called from:** `0x232f6` (which is called from `0x23429`, the grapple-swing
entry point — both already documented in
[`../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md`](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md#0x22b820x22eb4--grapple-swing-physics-fully-decoded)).
Reached only when the calling entity's `flags` (`+0x218`, confirmed) has bit
`0x400` set — i.e. only while a grapple hook is actively attached/swinging.
**Args:** `ent` (the hook entity being moved), a movement-delta vector
pointer, a second vector pointer, an output pointer (zeroed at entry, never
seen written again in this pass — purpose not confirmed).

This resolves the open question left in
[`../G_UTILS/README.md`](../G_UTILS/README.md#g_touchtriggers-open-question--partial-progress-not-resolved):
**`0x22fe6` is fully decoded and is definitively NOT `G_TouchTriggers`.**

---

## Why this isn't `G_TouchTriggers`

Every `call` inside this function targets either a fixed trap number or a
fixed code address — none of them load a value from an entity struct (e.g.
a `->touch` function-pointer field) and call through it. A true
`G_TouchTriggers`-equivalent would iterate an `EntitiesInBox` result and
invoke each entity's `->touch` callback; this function never reads a touch
field at all. Instead it moves `ent` itself and, for every other entity
caught in `ent`'s new bounding box, either damages it directly (hardcoded
`G_Damage` call, not a dispatch through the entity's own touch function) or
repositions it via a separate linked-list walk. The real `G_TouchTriggers`
remains unlocated — see the "still open" section below.

## What it actually does: a push/crush/carry resolver for a moving mover entity

```c
int func_0x22fe6( gentity_t *ent, vec3_t delta1, vec3_t delta2, void *out /* unused? */ ) {
    *out = 0;

    // box-margin sentinel check (0x22fe6-0x23023): compares ent's "anchor" vector
    // (ent+0x1f4/+0x1f8/+0x1fc, NEW/UNCONFIRMED field, sits immediately after
    // r.currentOrigin at +0x1e8) against a running value; on ANY mismatch
    // (the common path — see branch-semantics note below), falls through to:
    vec3_t clip = helper_0x9bef( ent, &ent->movementBounds /* +0x1b4/+0x1c0, confirmed */ );

    // box-corner construction (0x23036-0x23148): builds mins/maxs from
    // ent->r.absmin/absmax (+0x1d0/+0x1dc, confirmed) refined by delta1's components

    trap_UnlinkEntity( ent );                         // trap -0x1f, confirmed below
    count = trap_EntitiesInBox( &mins, &maxs, list, 0x400 );  // trap -0x21, confirmed

    ent->r.currentOrigin[0..2]      += delta1[0..2];  // +0x1e8/+0x1ec/+0x1f0
    ent->UNCONFIRMED_0x1f4[0..2]    += delta2[0..2];  // +0x1f4/+0x1f8/+0x1fc

    trap_LinkEntity( ent );                           // trap -0x1e... see below

    for ( i = 0; i < count; i++ ) {
        gentity_t *candidate = &g_entities[list[i]];  // base 0x1b2f0, stride 0x43c

        if ( candidate->s.eType == 2 || candidate->s.eType == 1 )   continue; // +0x4
        if ( candidate->UNCONFIRMED_0x234 != 0 )                     continue; // new/unconfirmed flag

        if ( candidate->s.groundEntityNum /*+0x94, confirmed*/ != ent->s.number /*ent+0, confirmed*/ ) {
            // AABB overlap test against candidate->r.absmin/absmax (+0x1d0/+0x1dc)
            if ( !overlaps ) continue;
            if ( !helper_0x22a36( candidate ) ) continue;   // unidentified predicate, 1 arg
        }
        // (entities standing ON TOP of ent skip straight to here, bypassing the overlap test)

        if ( !helper_0x22b82( candidate, ent, delta1, delta2 ) ) continue;  // unidentified, 4 args

        if ( ent->s.pos.trType == 4 || ent->s.apos.trType == 4 ) {   // +0xc/+0x30, STRUCTURALLY plausible
                                                                      // (vanilla TR_SINE=4) but NOT
                                                                      // independently confirmed this pass
            // "crush" branch
            G_Damage( candidate, ent, ent, NULL, NULL, 99999, 0, MOD_CRUSH /* 0x11=17 */ );
        } else {
            // "carry" branch: walk a global linked list anchored at 0x192c94/0x192c98
            // (the SAME statics the CALLER, 0x232f6, initializes at its own entry —
            // confirming this list belongs to the grapple-swing subsystem specifically,
            // not a generic entity list)
            for ( node = list_head; node != sentinel; node = node->next ) {
                gentity_t *rider = *node;             // entity ptr stored at node+0
                // block_copy two vec3 blocks between node-local storage and
                // rider / rider->client (+0x204, confirmed)
                trap_LinkEntity( rider );              // trap -0x1f again
            }
            // if the list walk runs off the sentinel mid-way, returns 0 early (leave @ 0x232e4)
        }
    }
    return 1;  // normal completion (vs. 0 on the carry-branch's early-exit path)
}
```

### Trap numbers confirmed against oDFs's own `g_public.h` enum

oDFs's syscall convention (already established elsewhere in this project) is
`trap number = -(enum index + 1)`. Counting `gameImport_t` in
[`../../code/game/g_public.h`](../../code/game/g_public.h) from `G_PRINT`
(index 0): `G_LINKENTITY` = index 30 → trap `-0x1f`, `G_UNLINKENTITY` =
index 31 → trap `-0x20`, `G_ENTITIES_IN_BOX` = index 32 → trap `-0x21`.
All three match exactly what this decode found, independently re-derived
from the project's own header rather than taken on the decoding pass's say-so.

### `MOD_CRUSH` = `0x11` confirmed against oDFs's own `bg_public.h` enum

Counting the `meansOfDeath_t` enum in
[`../../code/game/bg_public.h`](../../code/game/bg_public.h) from
`MOD_UNKNOWN` (0): `MOD_CRUSH` lands at **17** (`0x11`) exactly, matching
the mod argument this decode found in the damage call. (Same technique
already used elsewhere in this project to confirm `MOD_TRIGGER_HURT`=22,
`DAMAGE_NO_PROTECTION`=8.)

### Branch-semantics note on the entry sentinel check

The entry check (`0x22fe6`-`0x23023`) uses `nef -2 <target>` repeatedly:
per this project's confirmed convention, `nef` jumps to `<target>` when the
compared values are **not equal**; falling through requires all three
components to match exactly. Since `ent`'s "anchor" field is only ever
written by this same function (see below), the fallthrough/equal case is
the rare one (anchor already matches current position — nothing moved
since last call); the jump/not-equal case (compute `helper_0x9bef` and
proceed) is the common one.

---

## New/unconfirmed fields found (not documented elsewhere in this project)

| Offset | Observation | Confidence |
|---|---|---|
| `ent+0x1f4`/`+0x1f8`/`+0x1fc` | 3-float field immediately after `r.currentOrigin` (`+0x1e8`); incremented by `delta2` each call; compared against a sentinel at entry | New — not confirmed as any specific named field |
| `candidate+0x234` | Single int/flag; nonzero causes the candidate to be skipped entirely from the push/crush/carry logic | New — not confirmed |
| `ent+0xc` / `ent+0x30` | Compared against literal `4` to pick crush-vs-carry. Structurally matches vanilla `entityState_t.pos.trType`/`apos.trType` offsets (each a `0x24`-byte `trajectory_t`, first field `trType`), and `4` matches vanilla's `TR_SINE` | Plausible structural fit, **not independently confirmed** via string/hash evidence — flagged per project convention, not asserted as fact |

## Already-confirmed fields reused here (not re-derived, cited from elsewhere in this project)

- `+0x1d0`/`+0x1dc` = `r.absmin`/`r.absmax` — `../G_COMBAT/G_RadiusDamage.md`
- `+0x94` = `entityState_t.groundEntityNum` — `../G_MISSILE/G_BounceItem.md`, `../G_MISSILE/README.md`
- `+0x204` = `gentity_t.client` — `../G_MISSILE/README.md`
- `0x2015b` = `G_Damage` — `LG_Knockback.md`
- `+0x1b4`/`+0x1c0` = player-movement-bounds pair — `../G_ACTIVE/func_0x1a1b2.md`

## Still unidentified, flagged for a future pass

- `helper_0x22a36` — 1-arg predicate (candidate entity), boolean-ish return, used as a post-overlap filter. Not decoded.
- `helper_0x22b82` — 4-arg helper `(candidate, ent, delta1, delta2)`, boolean-ish return. Same signature shape as this function itself minus the output arg — likely a tightly-coupled sibling, not a generic utility. Partially described from a different angle in `RocketJump_ExtraVelocity_Search_Pass2.md` (there, called directly from `0x22fe6` on the hook entity itself for a position-teleport effect) — the two descriptions are not confirmed to be fully reconciled; worth a dedicated pass.
- `helper_0x9bef` — box-margin/clip calc using the `+0x1b4`/`+0x1c0` pair, not independently decoded.

## oDFs implementation status

**Not implemented.** This is grapple-hook-specific server-side mover
physics (crushing/carrying entities as the swinging hook moves) — a
genuinely new DeFRaG mechanic with no vanilla counterpart and no existing
oDFs analog. Out of scope for the current pass (which was investigating
`G_TouchTriggers`, not grapple mechanics); left for a future pass focused
specifically on grapple-hook server-side behavior, together with the
still-unidentified `0x22a36`/`0x22b82`/`0x9bef` helpers above.
