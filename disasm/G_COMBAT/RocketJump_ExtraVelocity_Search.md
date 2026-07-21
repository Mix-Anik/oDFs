# Search for an extra rocket-jump self-splash velocity source (outside G_Damage/G_RadiusDamage)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Purpose:** [G_Damage.md](G_Damage.md)'s third re-examination pass confirmed
that live-server rocket self-splash data implies an additive `knockback`
bonus of roughly +9.4 to +11.3 (avg +10.28), and confirmed — by exhaustive
instruction-level trace of `0x20250`-`0x20361` (`G_Damage`'s entire
knockback-to-kvel range, both promode and non-promode paths) and
`G_RadiusDamage`'s `points` computation — that this bonus is **not** present
in either function. This document extends the search outside those two
functions, per two leads. **Both leads came up empty.** No oDFs source was
changed as a result of this pass.

---

## Lead 1 — the unmapped `0x208f2`–`0x22066` gap between G_RadiusDamage and G_ExplodeMissile

### Method

Used `enter N` instructions (function-entry markers) to enumerate every
function start in the address range `0x20773` (G_RadiusDamage) through
`0x22066` (G_ExplodeMissile). Found **41 function starts** in
`0x20998`–`0x22066` (the actual unmapped span, since `G_RadiusDamage` itself
ends at `0x208f2`). For each function containing at least one `addf`
instruction (the only way `ps.velocity` could receive an additive term), the
destination of every `addf` was traced back to its base pointer/offset. In
addition, the **entire** `0x208f2`–`0x22066` range was grepped for:

- `1317c4` (the `df_promode` cvar cache address) — **zero matches** anywhere in the gap.
- `132900` (the global `ps`/`pm` pointer used throughout `bg_pmove.c`-equivalent code) — **zero matches** anywhere in the gap.

### The specific named lead: `0x20d2d`

`0x20d2d` (called from `0x20fac`, inside `0x20f11`) is the function
previously flagged in `func_0x4290.md`/`func_0x42b5.md` as an unidentified
caller of `SetLGKnockback`/`SetGrappleMode`. **Fully decoded this pass** —
see [G_ACTIVE/func_0x20d2d.md](../G_ACTIVE/func_0x20d2d.md) for the complete
instruction-level writeup. Summary:

- **Identity: `Pickup_Weapon`** — the weapon-item pickup handler, called from
  `func_0x20f11` (`Pickup_Item`, evidenced by a `"Item: %i %s"` debug-log
  string literal and a switch on `itemType` 1–8 matching vanilla
  `itType_t`) when `itemType == IT_WEAPON (1)`.
- **Contains zero floating-point instructions of any kind** (`addf`, `subf`,
  `mulf`, `divf` all absent from the full `0x20d2d`–`0x20e24` range) —
  it is pure integer/bitflag logic (ammo bookkeeping, inventory bitmask,
  two conditional calls to `SetLGKnockback`/`SetGrappleMode` gated on the
  picked-up weapon's index being `WP_LIGHTNING(6)`/`WP_GRAPPLING_HOOK(0xa)`).
- **Zero references to `ps.velocity`, `dir`, or `knockback`.**
- **Zero references to the `df_promode` cache (`0x1317c4`).**
- Fires on **item pickup** (walking over a dropped/map weapon), not during
  damage/splash — structurally cannot be the rocket-jump velocity source.

**This specific lead is conclusively ruled out.**

### The other 40 functions in the gap

Every function in `0x20998`–`0x22066` containing an `addf` was traced.
None writes to `ps.velocity` (confirmed offset `ps+0x20/0x24/0x28` per
`disasm/README.md`); all resolve to one of these categories:

| Function(s) | Identity (evidence) | `addf` destination |
|---|---|---|
| `0x20998` (enter `0x10a4`, huge frame) | **Identified in the Pass 2 follow-up** ([RocketJump_ExtraVelocity_Search_Pass2.md](RocketJump_ExtraVelocity_Search_Pass2.md)): gated to `arg0->s.eType == 3` (`ET_MISSILE` only); builds a `trap_EntitiesInBox` query around the missile's `currentOrigin` and dispatches through a per-entity function-pointer field (`other+0x2c0`) for each entity found — a proximity scan/dispatch of unconfirmed purpose. Grepped for `df_promode`/`G_Damage`/`G_RadiusDamage`/the `pm` global — zero matches; calls neither damage function | `ent->currentOrigin(+0x1e8/0x1ec/0x1f0) + global vector constant (0x2450/0x2454/0x2458)` — an origin/trace-point calc, not `ps.velocity` |
| `0x20afa`, `0x20c57`, `0x20ced`, `0x20e25` | Sibling family of `func_0x3c8b` callers (item-pickup-adjacent helpers, same family as `0x20d2d`) | `0x20afa`'s `addf` is a 3-component dot product compared against `0.4` (FOV/angle-style check) — not velocity |
| `0x20f11` | **`Pickup_Item`** — dispatches to `Pickup_Weapon` (`0x20d2d`) by `itemType`; also does ammo-jitter/despawn-timer bookkeeping | `addf` at `0x210c2` is a `crandom()`-style jitter (`(rand&0x7fff)/32767 - 0.5) * 2 * ent+0x320`) feeding an **ammo count**, not velocity |
| `0x2111a`, `0x211e6` | **`TossClientItems`-class** (weapon-drop-on-death): spawns a pickup entity via `G_Spawn` (`0x283de`), sets `trType`/`trTime`/`trBase`/mins/maxs/`think`=`0x20f11`/despawn-timer (`level.time + 30000`) | `addf`'s compute a `crandom()*50+200`-style toss-velocity jitter written into the **new item entity's own trajectory** (`local10+0x24` = its `trDelta`), not the player's `ps.velocity`. Called from `0x1a63a`/`0x1f896` (death/toss path), not from any live rocket-jump path |
| `0x21561`, `0x21f4f` | **Missile bounce/reflection physics** — shares helper `0x4f43` with `G_MissileImpact` (`0x2210f`); implements `v' = v - 2(v·n)n` off a `trace_t`'s `plane.normal` (trace_t field layout `fraction+0x8`, `endpos+0xc/0x10/0x14`, `plane.normal+0x18/0x1c/0x20` matches exactly) | Writes to `arg0+0x24/0x28/0x2c` = the **missile entity's own** `entityState_t.pos.trDelta[0/1/2]` (confirmed via `entityState_t` layout: `trBase` at `+0x18..+0x20`, `trDelta` at `+0x24..+0x2c`) — this is the projectile bouncing off a wall (e.g. grenade bounce), **not** the player's `ps.velocity` |
| `0x217d0` | Simple per-frame counter (`client+0x204+0x1c += 1.0`) | Integer-style float counter increment, unrelated to velocity |
| `0x21b5e` | **`FireWeapon`** — confirmed via its `switch` on weapon index `4`–`9` (`arg0+0xc0`): case `5` calls `0x2276a` = the already-confirmed `fire_rocket` address. Computes a `crandom()`-jittered, `VectorNormalize`d aim-spread direction before dispatching | `addf`'s build the spread-jittered **aim direction** passed to `fire_XXX`, not the shooter's `ps.velocity` — `fire_rocket` itself was already exhaustively re-checked in `G_Damage.md` with no promode velocity term |

**No `addf` anywhere in the `0x208f2`–`0x22066` gap writes to a `ps.velocity`-offset
field, and no reference to the `df_promode` cache or the global `ps`/`pm`
pointer exists anywhere in the gap.** Lead 1 is exhausted with a confirmed
negative result.

---

## Lead 2 — post-`G_Damage` velocity modification in per-frame server/shared movement code

Re-examined the existing G_ACTIVE and BG_PMOVE docs (already "analyzed" for
movement-architecture purposes) specifically for an `addf` on
`ps.velocity[0/1/2]` gated by `PMF_TIME_KNOCKBACK` (bit `0x40`, confirmed in
`G_Damage.md`) or by the `df_promode` cache (`0x1317c4`), and for any
special-case behavior in the single frame where `PMF_TIME_KNOCKBACK` is
freshly set.

### `ClientThink_real` (`0x1d27b`), `Pmove_outer` (`0x1a866`), `P_WorldEffects` (`0x19f04`)

No `PMF_TIME_KNOCKBACK`/`0x1317c4`-gated velocity modification found in any
of the three. `Pmove_outer`'s "speed tracking" block (`0x1ac53`–`0x1ac83`)
reads `ps.velocity` via `VectorLength` to track the highest observed speed
for a display feature — confirmed **read-only**, no `addf`/`store4` to
`ps.velocity`.

### `PM_WalkMove` (`0x726e`) — gravity-for-slick/knockback block

Directly re-decoded at `0x7422`–`0x744e` (previously summarized in
`PM_WalkMove.md` as "Gravity for SURF_SLICK / PMF_TIME_KNOCKBACK only" without
instruction-level detail):

```
if (groundTrace.surfaceFlags(0x1328d4) & 0x2 /* SURF_SLICK */
    || ps->pm_flags(+0xc) & 0x40 /* PMF_TIME_KNOCKBACK */) {
    ps->velocity[2] -= ps->gravity * pml.frametime;   ; confirmed subf, not addf
}
```

Confirmed **`subf`** (standard gravity subtraction), matching vanilla Q3
byte-for-byte. No additive knockback boost.

### `PM_AirMove` (`0x6e71`), `CPM_PM_Aircontrol` (`0x1a84`), `PM_Accelerate` (`0x67f2`)

Re-read against the existing detailed docs. Neither references
`PMF_TIME_KNOCKBACK` nor the promode cache in any velocity-additive context.
`CPM_PM_Aircontrol`'s only DeFRaG-specific deviation from the reference
source is a `dot <= 0` early-skip guard (already documented, reduces rather
than adds velocity in that case). `PM_Accelerate` is a byte-for-byte
vanilla-standard accelerate function.

### `PM_Friction` (`0x66e0`)

`PMF_TIME_KNOCKBACK` gates **whether ground friction is skipped** (so
knockback velocity isn't immediately eaten by ground friction) — this is
vanilla Q3 behavior (`if (... && !PMF_TIME_KNOCKBACK) applyFriction()`), not
an additive bonus. No `addf` on velocity anywhere in the function.

### `PM_DropTimers` (referenced)

Clears `PMF_TIME_KNOCKBACK` (among other flags) once `pm_time` expires — a
flag-clear only, no velocity arithmetic.

**Lead 2 is exhausted with a confirmed negative result:** no function in the
checked set (`ClientThink_real`, `Pmove_outer`, `P_WorldEffects`,
`PM_WalkMove`, `PM_AirMove`, `CPM_PM_Aircontrol`, `PM_Accelerate`,
`PM_Friction`, `PM_DropTimers`) contains an `addf` on `ps.velocity` gated by
`PMF_TIME_KNOCKBACK` or the `df_promode` cache.

---

## Overall conclusion

Both leads specified in the task brief were checked exhaustively and came up
empty:

- **Lead 1:** the entire previously-unmapped `0x208f2`–`0x22066` gap (41
  functions) was enumerated and every `addf` instruction in it traced to a
  non-`ps.velocity` destination. The specific named function (`0x20d2d`) was
  fully decoded and identified as `Pickup_Weapon` — an item-pickup handler
  with **zero floating-point instructions**, unrelated to damage/knockback.
- **Lead 2:** the per-frame server (`g_active.c`-equivalent) and
  client/server-shared movement code (`bg_pmove.c`-equivalent) paths that
  consume `PMF_TIME_KNOCKBACK` were re-examined at instruction level; all
  uses of the flag are either read-only gates (skip friction, apply gravity
  identically to vanilla) or flag-clearing, never an additive velocity term.

**No confirmed source of the reported ~+10-unit additive rocket-jump
knockback bonus was found anywhere in this pass.** Combined with
`G_Damage.md`'s three prior exhaustive passes (which ruled out `G_Damage`,
`G_RadiusDamage`, `fire_rocket`, `G_ExplodeMissile`, `G_MissileImpact`,
`G_RunMissile`'s promode references, and the full knockback-to-kvel
instruction range), the following server-side/shared-movement QVM code has
now been exhaustively checked and confirmed **not** to contain the gap's
source:

- `G_Damage` (`0x2015b`) — full knockback computation range `0x20250`-`0x20361`
- `G_RadiusDamage` (`0x20773`) — full `points`/`damage` computation
- `fire_rocket` (`0x2276a`)
- `G_ExplodeMissile` (`0x22066`), `G_MissileImpact` (`0x2210f`)
- The full `0x208f2`–`0x22066` unmapped gap (41 functions, including
  `Pickup_Weapon`/`Pickup_Item`, `FireWeapon`, missile bounce physics,
  `TossClientItems`)
- `ClientThink_real`, `Pmove_outer`, `P_WorldEffects` (G_ACTIVE)
- `PM_WalkMove`, `PM_AirMove`, `CPM_PM_Aircontrol`, `PM_Accelerate`,
  `PM_Friction`, `PM_DropTimers` (BG_PMOVE)

### Open avenues not covered by this pass (explicitly out of scope, not investigated)

- Client-side prediction/physics consumption of the resulting velocity
  (`cgame.qvm`) — the task brief noted this had been "already independently
  audited" per an earlier pass, but that audit's scope/findings were not
  re-verified here.
- The remaining, not-yet-decoded portions of `G_Damage` past the knockback
  block (`0x2041f` onward — armor/health application, event broadcasting).
- `func_0x20c97` (called from `Pickup_Weapon`, unidentified "give weapon"
  helper) and the exact semantics of `item->item+0x28` — flagged as open
  questions in `func_0x20d2d.md`, but both are pickup-time, not
  damage-time, so cannot be the source of a per-shot velocity bonus.
- Independent re-verification of the live-server test methodology that
  produced the +9.4 to +11.3 additive-gap figures (out of scope for a
  disassembly-only pass — flagged in `G_Damage.md` as well).

---

## oDFs implementation notes

**No source code changed as a result of this pass.** No confirmed
additive/multiplicative velocity term was found outside the already-ported
1.2x horizontal self-splash multiplier (see `G_Damage.md`'s "oDFs
implementation notes"). Per the CRITICAL RULE (no speculation), nothing was
guess-patched.
