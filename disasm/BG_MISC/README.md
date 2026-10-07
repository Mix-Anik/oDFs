# BG_MISC — Defrag QVM disassembly analysis

Analysis of `bg_misc.c`-equivalent functions from the Defrag qagame.qvm.
`bg_misc.c` is compiled into **both** `qagame.qvm` (server) and `cgame.qvm`
(client) in real Q3/DeFRaG — "bg" = "both game". We only have the
server-side `qagame.qvm` disassembly (`D:\Other\Reverse Engineering\q3vm\qagame.dis`)
available, **not** a `cgame.qvm` disassembly, so only the server-compiled
copy of this shared code could be verified here. Any cgame-only consumption
of these functions (e.g. client-side prediction/HUD use of
`BG_PlayerStateToEntityState`) is out of scope and unverified.

For general disassembly methodology, struct layouts, and global addresses
see [../README.md](../README.md). Functions were located primarily via the
**hash/order cross-reference method** against `baseq3-game-functions.hmap`
(`D:\Other\Reverse Engineering\q3vm\baseq3-game-functions.hmap`), the same
methodology used in [../G_WEAPON/](../G_WEAPON/) and [../G_COMBAT/](../G_COMBAT/).
Vanilla's relevant functions sit contiguously in the hash table
(`BG_FindItemForPowerup` at vanilla `0x10066` through
`BG_PlayerStateToEntityStateExtraPolate` at vanilla `0x10841`, immediately
followed by `bg_pmove.c`'s `PM_AddEvent` at `0x10a48`).

**Important scope note:** the vanilla `hmap` for this Q3 SDK revision (and
oDFs's current `bg_misc.c`, which mirrors it) does **not** contain
`BG_EntityStateToPlayerState`, `BG_AnimParseFile`, or a `bg_weaponlist[]`
table — these simply do not exist in this codebase generation. This is not a
DeFRaG removal; there was nothing to compare (see "Not present" section
below).

## Locating DeFRaG's addresses

Two already-confirmed anchors from prior passes — `BG_EvaluateTrajectory`
(`0x4d73`) and `BG_AddPredictableEventToPlayerstate` (`0x503d`) — sit at a
**constant offset from their vanilla addresses** (`vanilla_addr - 0xb55f`),
and the previously-confirmed `BG_EvaluateTrajectoryDelta` (`0x4f43`)
independently satisfies the same formula, which let the earlier part of the
module (`BG_FindItemForPowerup` through `BG_AddPredictableEventToPlayerstate`)
be located and verified by exact byte-size match against vanilla. **The
formula breaks down after `BG_AddPredictableEventToPlayerstate`** — DeFRaG's
`BG_TouchJumpPad` is substantially larger than vanilla's, shifting everything
after it. Functions from `BG_TouchJumpPad` onward were located by walking
`enter`/`leave` boundaries directly and confirming identity via decoded
content (struct offsets, call targets, literal constants), not
address-formula extrapolation. `PM_AddEvent` (`0x655d`, per
`../BG_PMOVE/README.md`) — the first `bg_pmove.c` function — is ~4.5 KB
further into the file than the naive formula would predict, entirely
accounted for by `BG_TouchJumpPad`'s growth plus a cluster of small
general-purpose math/string helper functions (`abs`, `fabs`, `strcmp`-style
compare, etc. — already independently identified in
`disasm/README.md`/`G_MISSILE/README.md`) that sit between `bg_misc.c` and
`bg_pmove.c` in this build's link order.

---

## Functions located and analyzed

| Function | DeFRaG address | Vanilla address | Size match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `BG_FindItemForPowerup` | `0x4aed` | `0x10066` | ✅ exact (67 B) | Located, not decoded (breadth-first priority; simple linear search, low signal) |
| `BG_FindItemForHoldable` | `0x4b30` | `0x100a9` | ✅ exact (58 B) | Located, not decoded |
| `BG_FindItemForWeapon` | `0x4b6a` | `0x100e3` | ✅ exact (51 B) | Located, not decoded |
| `BG_FindItem` | `0x4b9d` | `0x10116` | ✅ exact (43 B) | Located, not decoded |
| `BG_PlayerTouchesItem` | `0x4bc8` | `0x10141` | ❌ **+26 B** (122 vs 96) | ✅ **Fully decoded — MODIFIED**: promode-gated upper-Z pickup-tolerance relaxation (36→66) — see [BG_PlayerTouchesItem.md](BG_PlayerTouchesItem.md) |
| `BG_CanItemBeGrabbed` | `0x4c42` | `0x101a1` | ✅ exact (305 B) | Spot-checked: identity confirmed via error-string cross-reference (`"BG_CanItemBeGrabbed: IT_BAD"`/`"...index out of range"` literals found in data segment) and exact size match; no `pm_flags`/promode or global-cvar-address reference found anywhere in its instruction range. Not fully instruction-decoded (breadth-first priority; no evidence of modification found) |
| `BG_EvaluateTrajectory` | `0x4d73` | `0x102d2` | ✅ exact (464 B) | ✅ Confirmed unmodified — already fully decoded in a prior pass, see [../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md); re-confirmed this pass via size match |
| `BG_EvaluateTrajectoryDelta` | `0x4f43` | `0x104a2` | ✅ exact (250 B) | ✅ Address already confirmed in `disasm/README.md`; re-confirmed this pass via size match and the anchor-offset formula. Not independently re-decoded (out of this pass's focus; `BG_EvaluateTrajectory`'s decode covers the same switch-on-trType shape) |
| `BG_AddPredictableEventToPlayerstate` | `0x503d` | `0x1059c` | ✅ exact (59 B) | ✅ Address already confirmed in `disasm/README.md` (called by `PM_AddEvent`); re-confirmed this pass via size match and content read (`EV_JUMP_PAD`=13 call site in `BG_TouchJumpPad` matches `eventnames[]` array order) |
| `BG_TouchJumpPad` | `0x5078` | `0x105d7` | ❌ **+591 B** (703 vs 112, ~6.3×) | ✅ **Fully decoded — heavily MODIFIED**: flag-driven custom push-direction/velocity-blend system, opt-in per jumppad via `entityState_t.weapon != 0`; falls back to exact vanilla behavior when `weapon==0` — see [BG_TouchJumpPad.md](BG_TouchJumpPad.md) |
| `BG_PlayerStateToEntityState` | `0x5338` | `0x10647` | ✅ near-exact (505 vs 506 B, -1) | ✅ **Fully decoded — one CONFIRMED delta**: `ET_INVISIBLE`/`GIB_HEALTH` gate compares health against literal `0`, not `-40` (oDFs's `bg_public.h` has `GIB_HEALTH -40`, vanilla value) — everything else byte/offset-identical to vanilla — see [BG_PlayerStateToEntityState.md](BG_PlayerStateToEntityState.md) |
| `BG_PlayerStateToEntityStateExtraPolate` | `0x5532` | `0x10841` | ✅ near-exact (518 vs 519 B, -1) | ✅ Spot-checked (same opening gate/constant confirmed identical to the non-extrapolate version above, `TR_LINEAR_STOP`/`trDuration=50` confirmed matching vanilla) — see [BG_PlayerStateToEntityState.md](BG_PlayerStateToEntityState.md) |

## Not present in this codebase (nothing to compare — not a DeFRaG change)

- **`BG_EntityStateToPlayerState`** — absent from both oDFs's `bg_misc.c` and
  the vanilla `baseq3-game-functions.hmap`. Not implemented in this Q3 SDK
  generation at all.
- **`BG_AnimParseFile`/animation-config parsing** — no such function exists
  in oDFs's `bg_misc.c` or the vanilla hash map for this build. Nothing to
  check.
- **`bg_weaponlist[]`** — does not exist as a symbol in either oDFs's source
  or the vanilla function map; weapon metadata in this codebase generation
  lives elsewhere (not in `bg_misc.c`).

## `bg_itemlist[]` / `eventnames[]` data tables — confirmed present as data, NOT independently verifiable field-by-field

These are static initialized data, not instructions, and — as expected —
the disassembler exposes only a flat, address-sorted **string dump** of the
data segment (not a reconstructed struct-array view). Findings:

- All spot-checked `classname`/`pickup_sound`/model/icon/`pickup_name`
  strings from oDFs's `bg_itemlist[]` were found present in the DeFRaG data
  segment, in the expected reverse-of-declaration address order (e.g. the
  `"team_CTF_blueflag"`/`"icons/iconf_blu1"`/`"models/flags/b_flag.md3"`/
  `"Blue Flag"` cluster appears together, matching that one `gitem_t` entry's
  4 string fields).
- `eventnames[]`: all entries spot-checked present, including the ones after
  the source's commented-out `//#ifdef MISSIONPACK`/`//#endif` (which is
  **not** a real `#ifdef` — those entries, e.g. `EV_KAMIKAZE`,
  `EV_PROXIMITY_MINE_STICK`, `EV_LIGHTNINGBOLT`, are always compiled) — all
  confirmed present, consistent with oDFs's source.
- The genuinely `#ifdef MISSIONPACK`-gated `bg_itemlist[]` entries
  (`holdable_kamikaze`, `weapon_nailgun`, `weapon_prox_launcher`,
  `weapon_chaingun`, `holdable_portal`, `holdable_invulnerability`,
  `item_scout`, `item_ammoregen`, etc.) were searched for and **confirmed
  absent** from the data segment — consistent with `G_WEAPON/README.md`'s
  established finding that MISSIONPACK is not compiled into this DeFRaG
  build.
- **Not verifiable from this disassembly:** the *numeric* `gitem_t` fields
  (`quantity`, `giType`, `giTag`) between the string pointers. The
  disassembler does not expose raw data-segment integer words in a labeled
  struct-array form, only extracted strings. **Per the task's ground rules,
  this is reported as "cannot verify" rather than assumed vanilla-correct** —
  if any DeFRaG quantity/tag value differs from oDFs's table, this pass
  cannot detect it.
- `BG_EvaluateTrajectory: unknown trType: %i"` / `"BG_EvaluateTrajectoryDelta: unknown trType: %i"`
  / `"BG_CanItemBeGrabbed: IT_BAD"` / `"BG_CanItemBeGrabbed: index out of range"`
  error-format strings were all found in the data segment, independently
  corroborating those functions' identifications above.

---

## Headline findings

1. **`BG_TouchJumpPad` is ~6.3× larger than vanilla** and implements a
   substantial DeFRaG-only flag-driven custom jumppad physics system
   (direction-from-player-velocity push mode, auto side-flip, independent
   horizontal/vertical additive-vs-hard-set apply modes, an
   anti-velocity-reversal safety clamp), opt-in per jumppad entity via
   `entityState_t.weapon != 0` and configured via `entityState_t.angles2`.
   Falls back to byte-identical vanilla behavior when unused. See
   [BG_TouchJumpPad.md](BG_TouchJumpPad.md).
2. **`BG_PlayerTouchesItem` relaxes the upper vertical item-pickup tolerance
   from 36 to 66 units, but only in promode** (`pm_flags & 0x8000`) — all
   other bounds (X, Y, Z-lower) unchanged. See
   [BG_PlayerTouchesItem.md](BG_PlayerTouchesItem.md).
3. **`BG_PlayerStateToEntityState`/`...ExtraPolate`'s `GIB_HEALTH` gate
   compares against `0`, not vanilla's `-40`** — everything else in both
   functions is confirmed byte/offset-identical to vanilla, including a
   fairly exhaustive cross-check of the entire touched `playerState_t`
   header region's field offsets. oDFs's `bg_public.h` currently has
   `GIB_HEALTH -40` (the vanilla value) — a confirmed divergence worth
   reviewing. See [BG_PlayerStateToEntityState.md](BG_PlayerStateToEntityState.md).
4. **`BG_EvaluateTrajectory`/`BG_EvaluateTrajectoryDelta`/
   `BG_AddPredictableEventToPlayerstate`** — re-confirmed unmodified
   (addresses, exact-byte size matches); no re-decode needed, already
   covered by prior passes.
5. **`BG_FindItemForPowerup`/`...Holdable`/`...Weapon`/`BG_FindItem`/
   `BG_CanItemBeGrabbed`** — all match vanilla size exactly (byte-for-byte
   for the four `Find*` functions); no promode/cvar-address references found
   in `BG_CanItemBeGrabbed`'s instruction range. Not fully instruction-decoded
   per this task's breadth-first-first guidance — no evidence of
   modification found, but not exhaustively proven line-by-line the way
   `BG_PlayerTouchesItem`/`BG_TouchJumpPad`/`BG_PlayerStateToEntityState`
   were.
6. **`BG_EntityStateToPlayerState`, `BG_AnimParseFile`, `bg_weaponlist[]`
   do not exist in this codebase generation at all** (confirmed absent from
   both oDFs's source and vanilla's own hash map) — nothing to compare,
   not a DeFRaG-specific removal.
7. **`bg_itemlist[]`/`eventnames[]` data**: string content spot-confirmed
   matching (including confirming MISSIONPACK items are absent); the
   numeric `gitem_t` fields (`quantity`/`giType`/`giTag`) are **not
   verifiable** from this disassembly format — reported honestly as
   unchecked, not assumed correct.
