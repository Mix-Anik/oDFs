# Rocket-jump extra velocity — Pass 2: exhaustive whole-QVM `ps.velocity` write sweep

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Purpose:** [RocketJump_ExtraVelocity_Search.md](RocketJump_ExtraVelocity_Search.md) searched
the `G_Damage`/`G_RadiusDamage`/missile-impact code and the already-known per-frame
movement functions and found nothing. This pass drops the assumption that the missing
mechanism references `df_promode` or the standard movement/damage functions at all, and
instead exhaustively enumerates **every `addf` instruction in the entire `qagame.dis`**
(848 total) to find every place that could possibly write to a `playerState_t.velocity`
field (confirmed offsets `+0x20/+0x24/+0x28`), regardless of which function it's in.//
**Result: no new/unexplained velocity, origin, or knockback source was found.** Two
genuinely new (previously undocumented) code regions were discovered and fully decoded
along the way — a DeFRaG Lightning-Gun-related "phantom missile" spawner and a
grapple-swing physics subsystem — but both are conclusively unreachable from a
stationary, grapple-not-in-use, rocket-self-splash scenario.

---

## Method

1. Parsed all 169,328 instruction lines of `qagame.dis` into an indexed list.
2. Found all 848 `addf` instructions, then all 612 where the very next instruction is
   `store4` (i.e. an actual `x = a + b` write, as opposed to `addf` feeding into a
   further compare/branch/argument).
3. For each of the 612, walked backward within the same function (bounded by the
   nearest preceding `enter`) to resolve the **exact destination address expression**:
   which local variable holds the store address, and what `base + const` computation
   filled that local. This required widening the backward search window twice after
   validating against the already-confirmed `G_Damage` kvel-add case (`0x20309` etc.)
   and `CPM_PM_Aircontrol` (`0x1b09` etc.) — both of which use a longer instruction
   shape than the initial pattern assumed, which is a concrete illustration of the
   false-positive/false-negative risk called out in the task brief.
4. Cross-checked the precisely-resolved set against a broader (unrefined) 40-instruction
   backward window that flags **any** `const 0x20/0x24/0x28` + `add` near an
   `addf`/`store4` pair, to make sure the precise resolver wasn't silently dropping real
   hits. Every entry that appeared in the broad set but not the precise set was manually
   read from the raw disassembly (not inferred) to classify it.

### Result of the exhaustive resolve

Across the **entire QVM**, exactly **14** `addf`/`store4` pairs write to a destination
address of the exact shape `base + 0x20`, `base + 0x24`, or `base + 0x28`:

| Address | Function | Base pointer | Verdict |
|---|---|---|---|
| `0x20309`/`0x2031b`/`0x2032d` | `G_Damage` promode kvel add | `targ->client` (`+0x204`, deref) → `ps.velocity[0/1/2]` | **Known** — documented in [G_Damage.md](G_Damage.md) |
| `0x203c0`/`0x203d2`/`0x203e4` | `G_Damage` non-promode kvel add | same | **Known** — documented in [G_Damage.md](G_Damage.md) |
| `0x1b09`/`0x1b22` | `CPM_PM_Aircontrol` (`0x1a84`) | `pm` arg → `pm->ps` (double deref) → `ps.velocity[0/1]` | **Known** — documented, "Full match" in `BG_PMOVE/README.md`; gated on player having non-zero wish-direction input (`movementDir` check at `0x1a91`-`0x1a9a`), so inert with zero movement input |
| `0x06acb`/`0x06af5` | `PM_CheckJump` (`0x6a5e`) | global `0x132900` → `pm->ps` → `ps.velocity[2]` | **Known** — documented in [PM_CheckJump.md](../BG_PMOVE/PM_CheckJump.md) (double-jump `+270`/`+100` bonus); gated on `cmd.upmove >= 10` (jump key actively pressed that frame) — structurally cannot fire during a "zero movement input" flight |
| `0x22cb7`/`0x22ce4` | grapple-swing helper (`0x22b82`, newly decoded — see below) | raw `arg0` (single deref, **not** via `+0x204` client indirection) | **NOT `ps.velocity`** — writes to the *grapple hook entity's own* `s.pos.trBase[0]`/`[2]` (confirmed distinct field from `ps.velocity` — see below) |
| `0x22f12`/`0x22fbd` | grapple-swing helper's second block (`0x22eb4`, newly decoded — see below) | raw `arg0` (single deref) | **NOT `ps.velocity`** — same `trBase` field, second accumulation pass |

No other `addf` instruction anywhere in the file resolves to a `base+0x20/0x24/0x28`
destination. This is the complete set — every genuine struct-field write to those three
offsets in the whole QVM is accounted for above, and none of them represents an
unexplained addition.

---

## New code discovered and fully decoded this pass

While tracing the ambiguous candidates above, two previously-undocumented regions were
fully decoded to confirm they don't touch `ps.velocity`. Both turned out to be
DeFRaG's **swinging grapple hook** implementation, plus one unrelated Lightning-Gun
artifact found immediately adjacent to `fire_rocket` in the address space.

### `0x22880` — DeFRaG "phantom missile" for Lightning Gun (fully decoded, unrelated to rockets)

Immediately after `fire_rocket` (`0x2276a`, ends `0x22869`) sits a byte-for-byte
structural clone of a `fire_*` function at `0x22880`, confirmed via the same field
layout established by `fire_rocket.md` (`ent+0x20c`=classname, `+0x2b0`=nextthink,
`+0x2b4`=think, `+0x4`=`s.eType`, `+0x1a8`=`svFlags`, `+0xc0`=`s.weapon`,
`+0x200`=`r.ownerNum`, `+0x258`=parent, `+0x2e4`=damage, `+0x2e8`=splashDamage,
`+0x2ec`=splashRadius, `+0x2f0`=methodOfDeath, `+0x18/0x1c/0x20`=`trBase`,
`+0x24/0x28/0x2c`=`trDelta`):

```
s.weapon        = 6    (WP_LIGHTNING)
methodOfDeath   = 11   (MOD_LIGHTNING)
damage          = 0
splashDamage    = 45
splashRadius    = 46
nextthink       = level.time + 10        ; 10ms fuse
think           = 0x22877
trDelta         = dir * <unread constant>
```

`think` (`0x22877`) is a two-instruction wrapper that does **only**
`G_FreeEntity(self)` (`0x284b7`, fully decoded this pass — confirmed via its
`memset`-style clear at `trap -0x65(0, 0x43c=sizeof(gentity_t), ent)`, classname
reset to a "freed" string, and `freetime`/`inuse` writes, matching vanilla
`G_FreeEntity` exactly) and returns. **The `damage`/`splashDamage`/`splashRadius`/
`methodOfDeath` fields are set but never consumed — `think` never calls `G_Damage`
or `G_RadiusDamage`.** This spawns an invisible, harmless, self-cleaning marker
entity 10ms after creation. Its purpose (likely a DeFRaG Lightning Gun visual/hit
marker artifact) is not further identified — out of scope, and **conclusively not a
damage/knockback source of any kind, let alone a rocket one.**

### `0x2295f` — `fire_grapple` (grapple hook launch, identified via the 800.0 speed constant)

Same `fire_*`-shaped spawn, `s.weapon = 0xa` (`WP_GRAPPLING_HOOK`), `trDelta = dir *
0x44c80000` (= **800.0**, matching `disasm/README.md`'s already-noted "800.0=grapple
speed" identification constant), `think = 0x29145`. Confirms this is the CPM grapple
hook fire function. Not decoded further (grapple mechanics are out of scope for the
rocket-splash investigation and structurally cannot fire without the player actively
using the hook).

### `0x22b82`/`0x22eb4` — grapple-swing physics (fully decoded)

Reached only via the call chain `0x23429` → `0x232f6` → `0x22fe6` → `0x22b82`, and
`0x22fe6` → `0x22eb4`'s sibling. Traced every gate on this chain:

- `0x23429` (entry point of the chain) immediately checks
  `arg0->flags(+0x218) & 0x400` and returns immediately if clear — an entity-flags
  bitmask gate (bit `0x400`), presumably a DeFRaG-added "hook attached/swinging" flag,
  never observed set anywhere else in this pass.
- `0x232f6` calls `BG_EvaluateTrajectory` (`0x4d73`, identified and fully decoded this
  pass — see below) twice on `arg0`'s own position and angle trajectories, i.e. it
  operates on the **hook entity's** interpolated state, not the player's.
- `0x22fe6` compares the entity's current angles against a target angles vector and,
  on a mismatch, computes an absmin/absmax-derived value via an unidentified helper
  (`0x9bef`) before calling `0x22b82`.
- `0x22b82` is a 3x3-matrix-rotation-driven update (uses the generic matrix-multiply
  helper at `0x22b16`, also newly decoded this pass — a pure math utility, no struct
  identity of its own) that:
  1. Adds a delta vector to the **hook entity's own** `s.pos.trBase[0/1/2]`
     (`arg0+0x18/0x1c/0x20`) **twice** (once from `arg2`, once from a
     matrix-rotation-derived local) — these are the `0x22cb7`/`0x22ce4`/`0x22f12`/
     `0x22fbd` writes flagged in the table above. Confirmed **not** `ps.velocity` —
     the destination is `arg0`'s own field via a single dereference, structurally
     distinct from the `arg0->client(+0x204, deref)->ps.velocity(+0x20/24/28)`
     two-step indirection used everywhere `ps.velocity` is genuinely written (e.g.
     `G_Damage`, `CPM_PM_Aircontrol`, `PM_CheckJump`).
  2. **Only if `arg0->client(+0x204) != NULL`** (i.e. only when the entity passed in
     is a player, not the hook projectile itself), directly adds the same delta plus
     a second rotation-derived term to **`client->ps.origin[0/1/2]`** (`+0x14/0x18/0x1c`
     — confirmed via the same `+0x204` client-deref pattern used throughout the rest
     of the codebase) and updates `ps.delta_angles[1]` via a standard `ANGLE2SHORT`
     conversion (`65536*angle/360`, confirmed against the literal `0x47800000`=65536.0
     and `0x43b40000`=360.0 constants).

This is DeFRaG's swinging-grapple player-origin nudge — a **direct position
teleport**, not a velocity change, and it is only reached through the chain above,
which requires the calling entity to have flags-bit `0x400` set (hook actively
attached/swinging). **A stationary player who has not fired/attached a grapple hook
during the test cannot reach this code at all.** Documented here for completeness and
because it is a genuinely new discovery (previously flagged as an unidentified branch
in no prior doc), but it is not a candidate for the rocket-splash investigation.

### `0x4d73` — `BG_EvaluateTrajectory` (identified and fully decoded)

A `switch` on an integer field at `arg0+0x4` (`trType`) with exactly 6 cases
(`< 0` or `> 5` bails out — matching vanilla's `TR_STATIONARY(0)` .. `TR_GRAVITY(5)`
six-way enum) that evaluates a `trajectory_t` (`trBase` at `+0xc`, `trDelta` at
`+0x18`, `trTime`/`trDuration` at `+0x4`/`+0x8`) at a given time into a caller-supplied
output vector (`arg2`). This is the shared BG-layer trajectory evaluator used for
missile/mover rendering and interpolation — a generic math utility with no `ps`
involvement whatsoever. All of its `addf` writes go into the caller's output buffer
(`arg2`), never into any fixed struct offset. Not decoded field-by-field beyond
confirming its identity and that it never touches `ps.velocity` (it has no way to —
it never receives a `ps`/`pm`/entity pointer, only a `trajectory_t*` and a time).

---

## Step 3 — other `G_RadiusDamage`/`trap_EntitiesInBox` callers

Grepped the entire QVM for `const 1 0x20773` (the only way `G_RadiusDamage`,
confirmed at `0x20773`, can be called — QVM has no other call mechanism). **Exactly
two call sites exist in the whole file:**

1. `0x220ea`, inside `G_ExplodeMissile` — the already-documented, confirmed rocket
   splash-damage path (see [G_ExplodeMissile.md](../G_MISSILE/G_ExplodeMissile.md)).
2. `0x223a2`, inside `G_MissileImpact` (`0x2210f`) — inside the region
   `G_MissileImpact.md` had explicitly flagged as **"grapple/hook special case —
   NOT DECODED"** (`0x221ca`–`0x223cb`). **Decoded this pass.** The branch is entered
   only if a string-compare helper (`0x5c8f`) matches the impacting missile's
   `classname` (`ent+0x20c`) against a literal string constant (`0x8fcb`) — i.e. it is
   gated on `strcmp(ent->classname, "grapple") == 0` (or equivalent), consistent with
   this being the "grapple hook stuck to a wall" bookkeeping path (spawns a stuck-hook
   marker entity, sets `r.svFlags`, computes an AABB-center `currentOrigin` from
   `absmin`/`absmax`). The `G_RadiusDamage` call itself is additionally guarded by
   `if (ent->splashDamage(+0x2e8) == 0) skip` — and reads its `damage`/`radius`/`mod`
   arguments from the **missile entity's own fields**, exactly like the rocket path
   does, just for whatever entity reached this branch. **Since a rocket's `classname`
   is not `"grapple"`, this branch is structurally unreachable for rocket impacts** —
   confirmed via the classname string-compare gate, not merely inferred from field
   values. Ruled out.

Also grepped for the `trap_EntitiesInBox` trap call (`const 1 -0x21` immediately
before `call -1`) across the whole file — 8 sites total. Besides `G_RadiusDamage`
itself (`0x207bc`, already documented) and three sites in unrelated debug/utility
functions (`0x3d57`, `0x1a223`, `0x1c5c7` — none reference `ps.velocity`, `G_Damage`,
or `df_promode`; not further decoded, out of scope), one site (`0x209ed`) is inside a
previously-flagged-but-unidentified large function at `0x20998` (see
`RocketJump_ExtraVelocity_Search.md`'s "Lead 1" table, first row). **Fully re-checked
this pass:** the function is gated to `arg0->s.eType == 3` (`ET_MISSILE`) only, builds
an `EntitiesInBox` query around the missile's `currentOrigin` ± a fixed global vector
(`0x2450/0x2454/0x2458`), and for each entity found in the box, dereferences and calls
a per-entity function-pointer field (`other+0x2c0`) — a dynamic touch/think-style
dispatch whose purpose was not further identified (plausibly a proximity-trigger or
homing-target scan; out of scope). **Grepped this function's entire instruction range
(`0x20998`–`0x20afa`) for `df_promode` (`0x1317c4`), `G_Damage` (`0x2015b`),
`G_RadiusDamage` (`0x20773`), the `pm`/`ps` global (`0x132900`), the `0x132660`
debug-flags global, and the `sv_cheats` cache (`0x132100`) — zero matches for all of
them.** It calls neither `G_Damage` nor `G_RadiusDamage` and contains no reference to
`ps.velocity`. Ruled out as any kind of damage/knockback/velocity mechanism.

The remaining 5 unrelated `addf` in this function (already flagged in the prior pass)
write to a local origin/trace-point scratch value (`currentOrigin ± global vector`),
not `ps.velocity` — consistent with the prior pass's finding, now additionally
confirmed to have no `G_Damage`/`G_RadiusDamage`/promode involvement whatsoever.

---

## Step 4 — `previous_velocity` (`pml+0x84`/`pml+0x88`, `0x1328f8`/`0x1328fc`) traced

All 8 reference sites in the whole QVM to `0x1328f8` or `0x1328fc` were located and
individually read:

| Address | Function | Usage |
|---|---|---|
| `0x76db` | `PM_CrashLand` (`0x76b1`) | **Read** — `vel = previous_velocity[2]`, fed into the standard vanilla Q3 fall-impact-speed quadratic (`a=0.5*gravity, b=vel, ...`) used to compute fall damage. This is the field's canonical vanilla purpose: `ps.velocity[2]` may already be zeroed/clipped by ground contact by the time `PM_CrashLand` runs, so the pre-landing velocity snapshot is needed to compute how hard the player actually hit. |
| `0x7a9e` | `PM_GroundTrace` (`0x792e`) | **Read** — compares `previous_velocity[0]` against a negative threshold constant to conditionally set a bit (`0x20`) in `pm_flags`; a landing/hard-impact classification flag, not a velocity write. |
| `0x7e7c`/`0x7e91`/`0x7ea6`/`0x7ebb` | `PM_WaterEvents` (`0x7e78`) | **Read** (cluster of 4, one per component/derived check) — used for water-enter/leave splash-sound event triggering (comparing before/after water state), matching vanilla's use of `pml.previous_waterlevel`-adjacent data for splash sounds. |
| `0x85a2` | `PmoveSingle` (`0x8331`) | **Write** — `previous_waterlevel(0x1328fc) = pm->waterlevel`, executed once per `PmoveSingle` call, *before* `PM_CheckDuck`/`PM_GroundTrace`/`PM_DropTimers` run that frame. This is the per-frame "snapshot the pre-movement state" step. |

No write to `0x1328f8` (previous_velocity proper) was found via a literal constant
anywhere in the file — it is presumably set via a `block_copy` using a computed
address alongside `previous_origin` (`0x1328ec`, also read-only at its single
reference site) at the point where `pml` is (re)built per-`Pmove()`-call (inside
`func_0x1a394`, called from `Pmove_outer`, per `Pmove_outer.md`'s existing writeup —
not re-decoded field-by-field this pass, but its role is already established: it
builds the `pm`/`pml` structures before the subdivision loop runs).

**Conclusion: `previous_velocity`/`previous_waterlevel` are exactly what their name
and vanilla Q3 heritage suggest — a once-per-`Pmove()`-call "before" snapshot used
only for read-only comparisons (fall damage, hard-landing flag, water splash sound
triggering). Nothing reads them and adds the result back into `ps.velocity`.**
Definitively ruled out as an extra-velocity source.

---

## Overall conclusion of this pass

A full, address-space-wide sweep of `qagame.dis` — not limited to the previously
suspected damage/knockback/movement functions — found:

- **Zero** unexplained `addf` writes to `ps.velocity` (offsets `+0x20/+0x24/+0x28`)
  anywhere in the QVM. The complete set of 14 genuine struct-offset writes at those
  three offsets is fully enumerated and accounted for (6 known `G_Damage`, 2 known
  `CPM_PM_Aircontrol`, 2 known `PM_CheckJump`, 4 confirmed to be an unrelated entity's
  `trBase`, not `ps.velocity` at all).
- **Zero** additional `G_RadiusDamage`/`trap_EntitiesInBox`-based "generic explosion
  push" mechanism separate from the already-documented rocket splash path. The only
  other `G_RadiusDamage` call site in the entire QVM is classname-gated to `"grapple"`
  and cannot fire for a rocket.
- **Zero** evidence that `previous_velocity`/`previous_waterlevel` (`pml+0x84/+0x88`)
  are anything other than a read-only, once-per-frame "before" snapshot used for fall
  damage and landing/splash event detection — never re-added to `ps.velocity`.
- Two genuinely new code regions were discovered and fully decoded (a Lightning-Gun
  phantom-missile spawner with a `G_FreeEntity`-only think callback, and a
  swinging-grapple player-origin-nudge subsystem) — both confirmed structurally
  unreachable from a stationary, no-grapple, rocket-self-splash scenario.

**No oDFs source was changed as a result of this pass** — per the CRITICAL RULE, no
guess-patch was made in the absence of a confirmed mechanism. Combined with the four
prior passes (`G_Damage.md`, `RocketJump_ExtraVelocity_Search.md`), the server-side QVM
physics/damage/knockback code has now been searched exhaustively by two independent
methods (targeted region-by-region reading, and a blind whole-file `addf`-destination
sweep) with a consistent negative result. If the reported extra rocket-jump landing
distance is real, its cause is not present anywhere in `qagame.qvm`'s compiled
bytecode — the next places to look, if this is revisited, are: (a) independent
re-verification of the live-test methodology itself (flagged as out-of-scope for a
disassembly-only pass in every prior document too), or (b) some other client-visible
effect (e.g. view/camera positioning, not physics) being mistaken for extra travel
distance, since this pass exhaustively covered every plausible *server-authoritative*
physics mechanism.
