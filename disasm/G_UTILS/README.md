# G_UTILS — Defrag QVM disassembly analysis

Analysis of `g_utils.c`-equivalent functions from the Defrag qagame.qvm —
generic entity-array utilities (`G_Find`/`G_PickTarget`/`G_UseTargets`),
entity lifecycle (`G_Spawn`/`G_InitGentity`/`G_FreeEntity`/`G_TempEntity`),
and misc helpers (`vtos`, `tv`, `G_SetMovedir`, `G_KillBox`, `G_SetOrigin`,
`DebugLine`, shader-remap machinery).

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md). Functions were located via the hash/order
cross-reference method against `baseq3-game-functions.hmap`
(`D:\Other Reverse Engineering\q3vm\baseq3-game-functions.hmap`), the same
methodology used in [../G_WEAPON/](../G_WEAPON/) and [../G_COMBAT/](../G_COMBAT/).

Vanilla's `g_utils.c` functions (`AddRemap` through `DebugLine`) sit
**perfectly contiguously** in both the vanilla hash table and DeFRaG's own
function-hash table (top of `qagame.dis`), anchored against the
already-confirmed `G_FreeEntity` (`0x284b7`), `G_AddEvent` (`0x2861b`), and
`G_SetOrigin` (`0x286ad`) addresses from `disasm/README.md`. The byte-distance
between those three anchors matches vanilla's exactly (`0x164` and `0x92`
respectively), and every function in between was confirmed individually via
the DeFRaG function-hash table (instruction address + size + hash triples at
the top of `qagame.dis`).

**Result: every function in this module hash-matches vanilla exactly (same
instruction-shape) except one — `G_UseTargets`, which is confirmed genuinely
modified (see below).** oDFs's `g_utils.c` is vanilla-based, so this means the
entire module is vanilla-equivalent except for that one function.

---

## Functions located and analyzed

| Function | DeFRaG address | Size (instr) | Vanilla address | Vanilla size | Hash match? | Status |
|----------|-----------------|---------------|------------------|---------------|-------------|--------|
| `AddRemap` | `0x27f17` | `0x70` | `0x1f620` | `0x70`* | ✅ `-15dc646d` | ✅ Confirmed unmodified |
| `BuildShaderStateConfig` | `0x27f86` | `0x47` | `0x1f68f` | `0x47`* | ✅ `6156c004` | ✅ Confirmed unmodified |
| `G_FindConfigstringIndex` | `0x27fcc` | `0x6d` | `0x1f6d5` | `0x6d`* | ✅ `-6817f6e` | ✅ Confirmed unmodified |
| `G_ModelIndex` | `0x28038` | `0x14` | `0x1f741` | `0x14`* | ✅ `-5dd4443f` | ✅ Confirmed unmodified |
| `G_SoundIndex` | `0x2804b` | `0x14` | `0x1f754` | `0x14`* | ✅ `-5dd4443f` | ✅ Confirmed unmodified (identical hash to `G_ModelIndex`, expected — same shape) |
| `G_TeamCommand` | `0x2805e` | `0x3f` | `0x1f767` | `0x3f`* | ✅ `-654df3c` | ✅ Confirmed unmodified |
| **`G_Find`** | `0x2809c` | `0x50` | `0x1f7a5` | `0x50`* | ✅ `-19014934` | ✅ Confirmed unmodified — no race-mode/classname special-casing found |
| **`G_PickTarget`** | `0x280eb` | `0x67` | `0x1f7f4` | `0x67`* | ✅ `2b308286` | ✅ Confirmed unmodified |
| **`G_UseTargets`** | `0x28151` | `0x153` | `0x1f85a` | `0x9f` | ❌ `7be8a2b2` vs `-3613e102` | ⚠️ **MODIFIED — 180 extra instructions**: new once-per-server-frame per-(entity,activator) de-dup guard inserted before the target-firing loop — see [G_UseTargets.md](G_UseTargets.md) |
| `tv` | `0x282a3` | `0x2f` | `0x1f8f9` | `0x2f`* | ✅ `375d3aab` | ✅ Confirmed unmodified |
| `vtos` | `0x282d1` | `0x3d` | `0x1f927` | `0x3d`* | ✅ `-40f21f0d` | ✅ Confirmed unmodified |
| **`G_SetMovedir`** | `0x2830d` | `0x56` | `0x1f963` | `0x56`* | ✅ `-51ac3130` | ✅ Confirmed unmodified |
| `vectoyaw` | `0x28362` | `0x5b` | `0x1f9b8` | `0x5a`* | ✅ `b9b610c` | ✅ Confirmed unmodified (1-instr size drift, hash still matches — see hashing caveat) |
| **`G_InitGentity`** | `0x283bc` | `0x23` | `0x1fa12` | `0x22`* | ✅ `23939099` | ✅ Confirmed unmodified |
| **`G_Spawn`** | `0x283de` | `0xaf` | `0x1fa34` | `0xae`* | ✅ `-6afdc104` | ✅ Confirmed unmodified — address independently corroborated by `disasm/README.md`'s `TossClientItems` note ("spawns pickup entity via `G_Spawn` (`0x283de`)") |
| **`G_EntitiesFree`** | `0x2848c` | `0x2c` | `0x1fae2` | `0x2b`* | ✅ `2b86e654` | ✅ Confirmed unmodified |
| `G_FreeEntity` | `0x284b7` | `0x30` | `0x1fb0d` | `0x2f`* | ✅ `-1c3d2019` | ✅ Already confirmed in `disasm/README.md`; hash re-confirmed this pass |
| `G_TempEntity` | `0x284e6` | `0x4f` | `0x1fb3c` | `0x4e`* | ✅ `-74d5b1ce` | ✅ Confirmed unmodified |
| `G_KillBox` | `0x28534` | `0xcd` | `0x1fb8a` | `0xcc`* | ✅ `-3b62582a` | ✅ Fully spot-decoded (not just hash) — damage `100000` (`0x186a0`), `DAMAGE_NO_PROTECTION`=`0x8`, `MOD_TELEFRAG`=`0x12`(18) all confirmed vanilla-identical; calls `G_Damage` at `0x2015b` (DeFRaG's modified `G_Damage`, but called here with vanilla-identical args) |
| `G_AddPredictableEvent` | `0x28600` | `0x1c` | `0x1fc56` | `0x1b`* | ✅ `-2f0277d3` | ✅ Confirmed unmodified; calls `BG_AddPredictableEventToPlayerstate` (`0x503d`, already known) |
| `G_AddEvent` | `0x2861b` | `0x7a` | `0x1fc71` | `0x79`* | ✅ `7769d76b` | ✅ Already confirmed in `disasm/README.md`; hash re-confirmed |
| `G_Sound` | `0x28694` | `0x1a` | `0x1fcea` | `0x19`* | ✅ `-3cad717e` | ✅ Confirmed unmodified |
| `G_SetOrigin` | `0x286ad` | `0x40` | `0x1fd03` | `0x3f`* | ✅ `53f60da0` | ✅ Already confirmed in `disasm/README.md`; hash re-confirmed |
| `DebugLine` | `0x286ec` | `0x104` | `0x1fd42` | `0x103`* | ✅ `669ddb45` | ✅ Confirmed unmodified |

\* Vanilla sizes marked `*` are computed from consecutive hmap address deltas
(the vanilla hmap file does not list instruction counts directly), so they
are off-by-a-small-constant from DeFRaG's own table (which lists true
instruction counts) in a few rows — this is a units/measurement artifact, not
a discrepancy; the **hash** is the authoritative match signal and it is exact
in every row except `G_UseTargets`.

## Not part of `g_utils.c` in oDFs / out of scope

- `TeamCount`/`PickTeam` — live in `g_active.c`/`g_client.c` in oDFs, not
  `g_utils.c`. Already confirmed elsewhere: `TeamCount`=`0x1cb3b`,
  `PickTeam`=`0x1cbbd` — see [../G_ACTIVE/PickTeam.md](../G_ACTIVE/PickTeam.md).
  Not re-derived this pass.
- `G_BounceProjectile` (vanilla `0x1fe45`, DeFRaG `0x287ef`, hash
  `-7a21224e`, exact match) sits immediately after `DebugLine` and before
  `Weapon_Gauntlet` (`0x288ae`) in both vanilla and DeFRaG. It is **not**
  present in oDFs's `g_utils.c` (or anywhere in oDFs) and was not
  investigated further — out of scope for this pass, flagged only to close
  out the address range.

---

## `G_TouchTriggers` open question — two candidates ruled out, both now fully decoded

Per the prior `G_ACTIVE` pass, the true general-purpose `G_TouchTriggers`
(handling items/teleporters, as opposed to `func_0x1a1b2`'s race-trigger-only
scanner) was never conclusively located; two candidates were flagged:
`0x23158` and `0x285b2`.

**Both candidates are ruled out, and both are now fully decoded (not just
narrowed):**

- **`0x285b2` is not a separate function.** It is the `trap_EntitiesInBox`
  call (trap `-0x21`) *inside* `G_KillBox` (`0x28534`, decoded in full above,
  confirmed vanilla-identical `G_KillBox`). Not `G_TouchTriggers`.
- **`0x23158` is not a function start either.** It is a `trap_EntitiesInBox`
  call site *inside* the function starting at `0x22fe6` (`enter 0x10b8`,
  confirmed via its matching `leave` at `0x232f5` to be a full 784-instruction
  body). **This function is now fully decoded — see
  [`../G_ACTIVE/GrappleHookMover.md`](../G_ACTIVE/GrappleHookMover.md).**
  It is definitively **NOT** `G_TouchTriggers`: it never reads a `->touch`
  function pointer anywhere. It's a grapple-hook-specific push/crush/carry
  mover — it moves the hook entity, then either damages (`G_Damage`,
  `MOD_CRUSH`) or repositions (via a separate linked-list walk, not the
  `EntitiesInBox` result) entities caught in its new bounding box. Reached
  via the call chain `0x23429 → 0x232f6 → 0x22fe6`, confirming it belongs to
  the mover subsystem (`G_RunMover`->`G_MoverTeam`->`G_MoverPush`; the earlier "grapple-swing" label was wrong, see G_ACTIVE/GrappleHookMover.md), not a per-frame client-touch dispatcher.

**Net result: the true general-purpose `G_TouchTriggers` remains
unlocated.** Both previously-flagged candidate addresses are now positively
and fully identified as *not* being it (one is inside `G_KillBox`, the other
is the grapple-hook mover documented in `GrappleHookMover.md`) — this closes
out both leads rather than just narrowing them.

**Follow-up (same pass): a full sweep of every `trap_EntitiesInBox` call
site in the entire QVM** (grepped the whole file for the trap constant,
verified each hit is actually followed by a `call` and not a coincidental
bitmask `band`) found exactly **6 real call sites** total:
`0x1a223` (`func_0x1a1b2`, race-trigger scanner, implemented), `0x1c5c7`
(`func_0x1c56f`, an "is another player near this position" proximity
check — see [`../G_COMBAT/func_0x1c56f.md`](../G_COMBAT/func_0x1c56f.md)),
`0x207bc` (`G_RadiusDamage`),
`0x209ed` (`func_0x20998`, a missile-only trigger-touch dispatcher — see
[`../G_MISSILE/func_0x20998.md`](../G_MISSILE/func_0x20998.md)), `0x23158`
(the grapple-hook mover above), and `0x285b2` (`G_KillBox`). **None of the
6 is a confirmed generic player-side `G_TouchTriggers`** under the name/shape
originally being searched for. The search continues; this sweep at least
conclusively eliminates "it's some not-yet-opened `EntitiesInBox` call site"
as a possibility — there are no more unexamined ones.

**RESOLVED (same pass, final): `func_0x1a1b2` (item 1 above) *is*
`G_TouchTriggers`.** It was labeled "race-trigger-only" in an earlier pass
(justifying oDFs's split into a generic `G_TouchTriggers` plus a race-only
`DeFRaG_TouchTriggers`), then re-read as "probably the sole scanner, but
unconditionally excludes teleporters via a still-mysterious mechanism." A
third, closer read of the exact branch targets around the
`sessionTeam`/`eType`/hardcoded-address checks found that reasoning was
still wrong: those three checks form **one single nested conditional**,
byte-for-byte identical to vanilla `G_TouchTriggers`'s own spectator
exception (`if (sessionTeam==SPECTATOR) { if (eType != ET_TELEPORT_TRIGGER
&& touch != Touch_DoorTrigger) continue; }`) — not a flat, unconditional
exclusion list. Teleporters are **never** excluded for normal players; the
earlier "eType==9 universal exclusion" claim was a misread of the branch
structure. The hardcoded address is now identified as `Touch_DoorTrigger`
(confirmed by matching its full decode against oDFs's own vanilla-derived
`Touch_DoorTrigger`/`Touch_DoorTriggerSpectator`/`Use_BinaryMover` in
`g_mover.c`, instruction for instruction). The function's previously-unread
tail also contains the exact vanilla jumppad-frame reset. **Full writeup:
[`../G_ACTIVE/func_0x1a1b2.md`](../G_ACTIVE/func_0x1a1b2.md).**

**Fixed in code this pass:** oDFs's `DeFRaG_TouchTriggers` (`g_active.c`)
now implements the correct nested spectator/`Touch_DoorTrigger` exception
and the jumppad-frame reset, making it a complete, faithful port of
`func_0x1a1b2`. The now-fully-redundant second `G_TouchTriggers(ent)` call
inside `ClientThink_real`'s per-frame tail has been removed (it was
double-touching every item/trigger/door every frame); `G_TouchTriggers` as
a function is kept, since `SpectatorThink` still legitimately calls it
separately for spectator physics. Compiled clean (zero errors/warnings).

**The only genuinely unresolved detail left**: one DeFRaG-added
`trigger->flags & 0x40000000` exclusion bit with no located spawn-time
writer and no confirmed purpose — flagged, not guessed at, not ported.
Not behavior-relevant to `g_utils.c` itself — recorded here only because
the task asked for a final status check.

---

## Headline findings

1. **`G_UseTargets` is the only modified function in this entire module.**
   Every other function (23 of 24) hash-matches vanilla exactly. DeFRaG
   inserts a ~180-instruction once-per-server-frame de-duplication guard,
   keyed on `(target-holding entity, activator)`, that skips firing the
   `target`/`targetname` chain a second time if the same activator already
   triggered the same entity this frame. Uses four new `gentity_t` fields
   (`+0x42c` timestamp, `+0x430`/`+0x434` per-clientNum bitmasks,
   `+0x438` non-client-activator flag) with no vanilla counterpart. See
   [G_UseTargets.md](G_UseTargets.md).
2. **`G_Find` and `G_PickTarget` are confirmed unmodified** — no
   race-mode-specific classname special-casing was found in `G_Find`.
3. **`G_KillBox` fully spot-decoded** (not just hash-matched) to rule out a
   hidden telefrag-damage constant change — confirmed `100000`/
   `DAMAGE_NO_PROTECTION`/`MOD_TELEFRAG` all vanilla.
4. **`G_TouchTriggers` open question: both prior candidates fully decoded and
   ruled out, real function still unlocated.** `0x285b2` is inside
   `G_KillBox`; `0x23158` is inside a fully-decoded grapple-hook
   push/crush/carry mover (`0x22fe6`, see
   [`../G_ACTIVE/GrappleHookMover.md`](../G_ACTIVE/GrappleHookMover.md)) —
   neither is a touch dispatcher.
- [G_Printf_G_Error.md](G_Printf_G_Error.md) — `0x96`/`0xb1` wrappers
- [vanilla_identified_survey.md](vanilla_identified_survey.md) — survey results: vanilla functions that only differ by hash
- [func_0x3ccd.md](func_0x3ccd.md) — frame-time/clock-drift sticky flags (writer of 0x1320f4/f8)
- [CheckTournament.md](CheckTournament.md) — func_0xd1f, vanilla-equivalent warmup state machine
