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

## Touch side — `trigger_push_touch` also has a DeFRaG addition

Found while tracing the spawn side below: `trigger_push_touch` itself (the
shared `touch` callback used by both `SP_trigger_push` and
`SP_trigger_push_velocity`) is **not** vanilla-identical either — it gates
an instant-kill `G_Damage` call on cvar `df_ndm_nopad`. Fully decoded and
**implemented**; see
[`../G_TRIGGER/trigger_push_touch.md`](../G_TRIGGER/trigger_push_touch.md)
for the full writeup (this applies to *every* jump pad, not just the custom
ones below).

## Spawn side — RESOLVED: new classname `trigger_push_velocity`

A later pass traced this fully. `SP_trigger_push` itself (`0x27a39`, 62
bytes) is **hash-identical to vanilla** (`23e9115c`) — confirmed completely
unmodified, so it does not set `weapon`/`angles2`. The spawn-table data
segment (walked directly: entries are 8-byte `{char *name; void
(*spawn)(gentity_t*);}` pairs, e.g. `0x2684`→`{"trigger_push", 0x27a39}`)
contains an entry immediately after it: `0x268c`→`{"trigger_push_velocity",
0x27a76}` — a brand-new DeFRaG-only classname, string-confirmed at `0x96e3`
in the data segment. Its spawn function (`SP_trigger_push_velocity`,
`0x27a76`, 119 bytes, no vanilla hash counterpart) is **byte-for-byte
identical to `SP_trigger_push`** (`InitTrigger`, same sound precache, same
`ET_PUSH_TRIGGER`/`touch`/`think`/`nextthink` assignments) plus four extra
field copies inserted right before `trap_LinkEntity`:

```c
self->s.weapon     = self->spawnflags;        // *(self+0x210) -> *(self+0xc0)
self->s.angles2[0] = self->random;            // *(self+0x2a0) -> *(self+0x80)
self->s.angles2[1] = self->random;            // *(self+0x2a0) -> *(self+0x84) (same value, duplicated)
self->s.angles2[2] = (float)self->count;      // *(self+0x2f8) -> *(self+0x88), cvif
```

`self+0x210` = `spawnflags` was independently confirmed via `SP_trigger_hurt`
(`0x27df3`, hash-identical to vanilla, `-7f009cb7`): it reads `*(self+0x210)
& 2` and `& 1` to gate `hurt_use`/deferred-link exactly where vanilla checks
`self->spawnflags`. `self+0x2a0` = `random` and `self+0x2f8` = `count` were
already independently confirmed elsewhere in this project
(`G_ACTIVE/func_0x20d2d.md` and `G_TRIGGER/race_timer_system.md`
respectively) — both cross-checks agree with zero drift.

**Practical effect:** a DeFRaG map author places a `trigger_push_velocity`
entity (instead of `trigger_push`), still pointing "target" at a
`target_position` the same way, and uses the entity's **Spawnflags
checkboxes** to pick the custom mode bits below, its **"random" key** as the
bit-`0x1`/bit-`0x1`-clear-with-`0x10` magnitude, and its **"count" key**
(integer) as the bit-`0x4` vertical magnitude.

## CORRECTION — a 6th mode bit was missed: `0x20` (vertical auto-flip)

A full line-by-line re-decode of the vertical-magnitude computation
(`0x51c3`–`0x521d`, the `mode & 0x4` **clear** path) found a second,
previously undocumented auto-flip bit, structurally identical to bit `0x10`
but applied to the vertical (`origin2[2]`) magnitude instead of the
horizontal direction:

```c
vertBoost = jumppad->origin2[2];
if ( mode & 0x20 ) {
    // same "which side gives more/less total speed" comparison as bit 0x10,
    // applied to vertBoost instead of the horizontal pushDir
    if ( |velocity_with_vertBoost_subtracted| > |velocity_with_vertBoost_added| )
        vertBoost = -vertBoost;
}
```
Only meaningful when bit `0x4` is **clear** (i.e. using the pad's configured
`origin2[2]` magnitude rather than the velocity-sign-derived one from bit
`0x4`), exactly paralleling how bit `0x10` only matters when bit `0x1` is
clear.

## CORRECTION — the "`local2c` may be uninitialized" caveat was wrong

The original pass's caveat about bit `0x8`'s hard-overwrite path
(`ps->velocity.z = local2c`) possibly reading an uninitialized value when bit
`0x4` is clear was **incorrect** — a full decode shows `local2c` (this doc's
`vertBoost`) is unconditionally assigned by exactly one of the two mutually
exclusive branches above (`mode & 0x4` set → velocity-sign-derived; clear →
`origin2[2]`, optionally flipped by `0x20`) before any apply-mode code runs.
There is no reachable path where it is read uninitialized.

## CORRECTION — the anti-reversal (`0x40`) comparison is a true cosine, not a bare length compare

Re-derived precisely: for both the horizontal (`0x521d`+) and vertical
(`0x52ac`+) additive-blend paths, the candidate blended vector's cosine
similarity against the player's *original, unmodified* velocity
(`DotProduct(candidate, original) / (|candidate| * |original|)`) is computed
via two `VectorLength` calls (`0x8d63`) and a manual 3-term dot product —
confirmed exactly matching a textbook cosine-of-angle formula, not an
approximation. If bit `0x40` is set and this cosine is negative, the
corresponding velocity component(s) are zeroed instead of applied. Note the
vertical check's "original velocity" is read *after* the horizontal
apply-mode block has already run, so if that block modified
`ps->velocity.x`/`.y`, the vertical cosine check sees the *already-updated*
horizontal components — this is a direct, intentional consequence of the
two blocks running sequentially and both reading live `ps->velocity`, not a
bug.

## Summary

This is a genuine, substantial DeFRaG-specific jumppad feature: a
per-trigger_push-entity opt-in (new classname `trigger_push_velocity`, via
`entityState_t.weapon` != 0, set from the entity's own `spawnflags`)
alternate physics mode supporting direction-from-player-velocity pushes,
automatic push-side flipping on **both** axes (bits `0x10`/`0x20`),
separately configurable horizontal/vertical apply modes (additive-blend vs
hard-set), and an anti-reversal safety clamp (true cosine check) — all
falling back to exact vanilla behavior for any jumppad that leaves `weapon`
at its default `0` (i.e. any ordinary `trigger_push`).

**Implemented** (`code/game/bg_misc.c`'s `BG_TouchJumpPad`, and
`code/game/g_trigger.c`'s new `SP_trigger_push_velocity` + its
`code/game/g_spawn.c` spawn-table registration): full port of all seven mode bits
(`0x1`, `0x2`, `0x4`, `0x8`, `0x10`, `0x20`, `0x40` — `0x40` modifies the
behavior of the `0x2`/`0x8` apply-mode branches rather than acting
independently) and the new map entity. No DeFRaG cvar is involved anywhere in
this feature — it is purely per-entity map data (spawnflags/"random"/"count"
keys on `trigger_push_velocity`), so there was no gating-condition ambiguity
blocking the port this time.
