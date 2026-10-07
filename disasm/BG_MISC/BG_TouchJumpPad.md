# `BG_TouchJumpPad` — DeFRaG address `0x5078`

**Vanilla address:** `0x105d7` (per `baseq3-game-functions.hmap`)
**Vanilla size:** `0x70` (112 bytes) — `0x10647 - 0x105d7`
**DeFRaG size:** `0x2bf` (703 bytes) — `0x5337 - 0x5078` (`enter 0x80` / final `leave 0x80` at `0x5337`)

**Verdict: heavily MODIFIED.** DeFRaG's `BG_TouchJumpPad` is ~6.3× the size of
vanilla's. The added code is a flag-driven, per-jumppad-entity custom
push-direction/velocity system that only activates when the jumppad entity's
`entityState_t.weapon` field (see below) is non-zero; when that field is `0`
(i.e. any ordinary, unmodified `trigger_push`/jumppad the way vanilla Q3 maps
build them), the function's behavior reduces to byte-for-byte vanilla logic.

## Identification

Confirmed via direct signature/behavior match, not size/position alone:

- 2 args (`ps`, `jumppad`), matching vanilla's `(playerState_t *ps, entityState_t *jumppad)`.
- Opening checks match vanilla exactly: `if (ps->pm_type(+0x4) != PM_NORMAL) return;` then
  `if (ps->powerups[PW_FLIGHT](+0x150)) return;` (offset `+0x150` confirmed as
  `PW_FLIGHT` via the same 8-slot/`0x20`-byte spacing already established for
  `+0x170`=`PW_INVULNERABILITY` in `disasm/README.md`).
- Computes `p = fabs(AngleNormalize180(vectoangles(jumppad->origin2)[PITCH]))`,
  `effectNum = (p<45.0) ? 0 : 1` (constant `0x42340000` = confirmed **45.0**),
  then calls `BG_AddPredictableEventToPlayerstate(EV_JUMP_PAD(=0xd), effectNum, ps)`
  at `0x50c4` — matches vanilla's `EV_JUMP_PAD` (index 13 in `eventnames[]`,
  cross-checked against the array literal order) exactly.
- Ends by writing `ps->jumppad_ent(+0x1c0) = jumppad->number` and
  `ps->jumppad_frame(+0x1cc) = ps->pmove_framecount(+0x1c8)` — both offsets
  independently confirmed by computing them from oDFs's own (vanilla-shaped)
  `playerState_t` in `code/qcommon/q_shared.h` (jumppad_ent immediately
  follows `generic1`/`loopSound` at the end of `ammo[MAX_WEAPONS]`, landing at
  `0x1c0`; `pmove_frame_count`/`jumppad_frame` follow at `0x1c8`/`0x1cc`).
  These matches, plus all other `playerState_t` offsets checked in this pass
  (`weapon`=`0x90`, `weaponstate`=`0x94`, `viewheight`=`0xa4`, `stats[0]`=`0xb8`,
  `powerups[]` base=`0x138`) landing exactly where they're computed to be from
  oDFs's own struct, is strong evidence **DeFRaG's `playerState_t` header
  layout (through `jumppad_ent`/`jumppad_frame`) is identical to oDFs's
  vanilla-shaped struct** — no hidden struct-layout extension in this region.

## Struct offsets used (entityState_t, `jumppad` argument)

Computed directly from oDFs's own (vanilla) `entityState_t` in
`code/qcommon/q_shared.h` and cross-checked for internal self-consistency
(every offset below independently lines up with the computed field list —
e.g. `origin2` at `0x68/0x6c/0x70` and `angles2` at `0x80/0x84/0x88` are each
read as adjacent 4-byte triples exactly where the struct layout puts them):

| Offset | Field | Used as |
|---|---|---|
| `0x00` | `number` | compared against `ps->jumppad_ent` |
| `0x68/0x6c/0x70` | `origin2` | vanilla push-velocity vector |
| `0x80/0x84/0x88` | `angles2` | **repurposed** as a 3-float parameter vector (X and Z components read; Y never read) for the custom modes below |
| `0xc0` | `weapon` | **repurposed** as a bitflag field selecting which custom mode(s) apply |

`weapon`/`angles2` are ordinary `entityState_t` fields that a `trigger_push`
entity has no normal use for (they only matter for `ET_PLAYER`/`ET_ITEM`
rendering) — DeFRaG piggybacks on them to carry per-jumppad configuration.
**What server-side spawn code (`g_trigger.c`/`g_target.c`, not examined in
this pass) sets these fields to for real DeFRaG maps is not established** —
only the *consumption* side (this function) was decoded.

## Behavior when `jumppad->weapon == 0` (ordinary jumppad)

Byte-for-byte vanilla:
```c
if (ps->jumppad_ent != jumppad->number) {
    // vectoangles/effectNum/BG_AddPredictableEventToPlayerstate as above
}
ps->jumppad_ent = jumppad->number;
VectorCopy(jumppad->origin2, ps->velocity);   // unconditional overwrite
ps->jumppad_frame = ps->pmove_framecount;
```
(Confirmed at `0x50de`–`0x50e6`, jumping straight to the shared tail at `0x5320`.)

## Behavior when `jumppad->weapon != 0` (custom mode — DeFRaG addition)

All of the following is gated behind `local14 = jumppad->weapon` being
non-zero (checked at `0x50dd`). Individual bits of `local14` are tested
independently and are **not mutually exclusive** — multiple can combine.
Bit names below are descriptive (functional), not DeFRaG's actual source
names, which are unknown:

### Push-direction source (bit `0x1`)

- **Bit `0x1` clear** (`0x5124`): push direction = `jumppad->origin2.xy`
  (vanilla's vector), Z forced to 0. If bit `0x10` (below) is also set, this
  direction can be sign-flipped.
- **Bit `0x1` set** (`0x50ef`): push direction = `Normalize(ps->velocity.xy, 0)`
  scaled by `jumppad->angles2[0]` (X component) — i.e. **push along the
  player's own current horizontal movement direction**, with the pad's
  `angles2.x` field supplying the magnitude instead of a fixed direction
  vector.

### Direction auto-flip (bit `0x10`, only meaningful when bit `0x1` is clear)

At `0x5139`: computes `|ps->velocity.xy + origin2.xy|` vs
`|ps->velocity.xy - origin2.xy|`. If the "minus" form is strictly larger
(i.e. the player's current velocity is more than 90° opposed to the pad's
configured push direction), the push direction is negated
(confirmed via literal `-1.0`, **not** `-4.0** — the raw operand is
`-0x40800000`, whose two's-complement bit pattern `0xBF800000` decodes to
`-1.0`, not the naive negation of `0x40800000`=4.0; this project's
established "negative hex constant" pitfall was specifically re-verified
here). Net effect: **a pad pushes correctly regardless of which side the
player approaches from**, instead of vanilla's fixed one-way `origin2` vector.

### Vertical push component (bit `0x4`)

At `0x518f`: if set, computes `Normalize(0, 0, ps->velocity.z)` (i.e. a pure
`±1` vertical unit vector matching the player's existing vertical sign) scaled
by `jumppad->angles2[2]` (Z component) — a configurable vertical boost
magnitude, stored for later use as `local2c`. If this bit is **clear**,
`local2c` is never initialized by this function before it may still be
consumed by the bit-`0x8`-clear path below (see caveat).

### Horizontal apply mode (bit `0x2`)

At `0x521d` (skipped entirely, going straight to the bit-`0x8` check, if
`ps->jumppad_ent == jumppad->number`, i.e. the pad was already touched last
frame):
- **Set**: `ps->velocity.xy += pushdir.xy` (additive blend with existing
  velocity), **unless** bit `0x40` is also set and the resulting blended
  vector's cosine-similarity with the player's original velocity is negative
  (see bit `0x40` below), in which case `ps->velocity.xy` is zeroed instead.
- **Clear** (`0x529e`): `ps->velocity.xy = pushdir.xy` (hard overwrite, same
  as vanilla's single-vector-copy style but using the computed `pushdir`
  instead of always `origin2`).

### Vertical apply mode (bit `0x8`)

At `0x52ac` (also skipped if `ps->jumppad_ent == jumppad->number`):
- **Set**: `ps->velocity.z += local2c` (additive), with the same bit-`0x40`
  "zero if opposing" safety check as the horizontal case.
- **Clear** (`0x5319`): `ps->velocity.z = local2c` (hard overwrite,
  unconditional). **Caveat:** if bit `0x4` was never set, `local2c` was never
  assigned by this function's own code before this write — its value would
  come from whatever was already on the QVM stack frame at function entry.
  This is a structural observation about the bytecode as written; whether
  real DeFRaG maps ever set bit `0x8` without also setting bit `0x4` (which
  would make this reachable) is a map-content question outside the scope of
  this disassembly-only pass — **not confirmed as an exploitable bug, only
  flagged as a code-shape oddity**.

### "Don't reverse the player" safety clamp (bit `0x40`)

Used identically in both the horizontal (`0x5278`) and vertical (`0x5300`)
additive-blend paths: after computing the candidate blended velocity, its
normalized dot product (`cos θ`) against the player's *original* (pre-blend)
velocity is computed via two `VectorLength` calls (`0x8d63`, previously
confirmed) and a manual dot-product/divide. If bit `0x40` is set and this
cosine is negative (candidate result opposes original motion by more than
90°), the corresponding velocity component(s) are zeroed instead of applied
— an anti-jank safeguard against a "smooth" jumppad flinging the player
backward relative to their approach.

## Shared tail (`0x5320`, all paths converge here)

```c
ps->jumppad_ent   = jumppad->number;      // 0x5320
ps->jumppad_frame = ps->pmove_framecount; // 0x532c
```
Matches vanilla's trailing two statements exactly (only reordered relative to
the `VectorCopy` velocity write, which now happens earlier/branch-specific
instead of always-last — no observable behavioral effect from the reorder
itself).

## Summary

This is a genuine, substantial DeFRaG-specific jumppad feature: a
per-trigger_push opt-in (via `entityState_t.weapon` != 0) alternate physics
mode supporting direction-from-player-velocity pushes, automatic push-side
flipping, separately configurable horizontal/vertical apply modes
(additive-blend vs hard-set), and an anti-reversal safety clamp — all
falling back to exact vanilla behavior for any jumppad that leaves `weapon`
at its default `0`. **Not yet connected to any known DeFRaG cvar or mapping
entity-key name** — that would require examining `g_trigger.c`/`g_target.c`
spawn code, out of scope for this `bg_misc.c`-focused pass.
