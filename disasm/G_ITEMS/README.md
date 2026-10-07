# G_ITEMS — Defrag QVM disassembly analysis

Analysis of `g_items.c`-equivalent functions from the Defrag qagame.qvm — item pickup dispatch,
per-client pickup gating, item spawn/drop/respawn.

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md). This pass started from the task of explaining a real, reported DeFRaG
practice-mode behavior: **items do not disappear for other players when one player picks them up** —
each player gets an independent view of whether they've grabbed it. Functions were located starting
from the confirmed `Pickup_Armor` anchor (`0x20ecd`, exact hash match to vanilla) and `G_BounceItem`
anchor (`0x21561`, exact hash match), then identified by **content** (field offsets, call targets,
function-pointer assignment sites), not hash-table position alone — see
[Touch_Item.md](Touch_Item.md) for why a prior position-only pass misidentified `0x20f11`.

---

## Headline finding

**DeFRaG's `Touch_Item` never hides the item entity from anyone.** There is no
`SVF_NOCLIENT`/`SVF_SINGLECLIENT`/`SVF_NOTSINGLECLIENT`/`EF_NODRAW`/`contents=0`/`nextthink` scheduling
anywhere in the pickup path. Instead, a **new per-client cooldown array** on `gentity_t`
(`ent+0x32c`, `int[64]`, one slot per `clientNum`) independently gates whether *that specific client*
may currently register a pickup against *this specific item entity*. Picking the item up writes only
the picker's own slot (`level.time + respawnSeconds*1000`, or `-1` for non-respawning items); every
other client's slot — and the entity's actual visibility/solidity — is completely untouched. This is
**unconditional, not gated by `df_promode`, `df_mode`, or any other cvar found in this QVM** (verified
by a full-file grep for the `df_promode` cache address and a full inventory of every `df_*` cvar-name
string literal in the build — no cvar named plainly `df_mode` exists in this QVM at all).

Full mechanism, confidence level, and exact field offsets/values: see [Touch_Item.md](Touch_Item.md).
`RespawnItem`'s fate (merged into the `ent->use` callback, per-activator-scoped): see
[RespawnItem.md](RespawnItem.md).

---

## Functions located and analyzed

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `Touch_Item` | `0x20f11` | `0x19626` | ❌ absent/modified (`-79df7020` not found anywhere in file) | ✅ **Fully decoded** — per-client pickup-cooldown mechanism confirmed, no entity-hide logic of any kind. See [Touch_Item.md](Touch_Item.md) |
| `RespawnItem` (standalone) | — does not exist — | `0x194ee` | ❌ confirmed absent (`60a2eb9d`, zero matches file-wide) | ✅ Confirmed removed; body merged into the `ent->use` callback below. See [RespawnItem.md](RespawnItem.md) |
| `ent->use` handler (RespawnItem body, inlined) | `0x21249` | n/a (vanilla `Use_Item`=`0x1998c`, 9-instr wrapper) | ❌ not matched (95 vs 9 instr) | ✅ Fully decoded — vanilla `RespawnItem` body transplanted, reworked to reset one activator's cooldown slot instead of a global unhide. See [RespawnItem.md](RespawnItem.md) |
| `FinishSpawningItem` | `0x212a7` | `0x19995` | ❌ not matched (352 vs 343 instr, close) | ✅ Spot-confirmed via content (ET_ITEM/mins/maxs/modelindex setup, `touch`/`use` assignment sites) — not fully instruction-decoded line-by-line |
| `LaunchItem` | `0x2111a` | `0x1985d` | ✅ exact hash match (`-3f4ad885`) | ✅ Confirmed unmodified; its `ent->touch = Touch_Item` write independently confirms `Touch_Item`'s address |
| `Drop_Item` | `0x211e6` | `0x19929` | ✅ exact hash match (`63047584`) | ✅ Confirmed unmodified |
| `Pickup_Weapon` | `0x20d2d` | `0x19365` | — | ✅ Already fully decoded — [`../G_ACTIVE/func_0x20d2d.md`](../G_ACTIVE/func_0x20d2d.md). Return value `5` now explained as the per-client respawn-seconds value, resolving that doc's open question |
| `Pickup_Armor` | `0x20ecd` | `0x194aa` | ✅ exact hash match (`7441a372`) | ✅ Confirmed unmodified (pre-existing project anchor) |
| `Pickup_Ammo` | `0x20ced` | `0x19334` | not checked | Identified via switch-case position only (case 2), see [Touch_Item.md](Touch_Item.md) |
| `Pickup_Health` | `0x20e25` | `0x19424` | ❌ not matched (size 169 vs 134) | Identified via switch-case position only (case 4); size suggests modification, not investigated further (confirmed no `svFlags`/`nextthink` involvement regardless, since `Touch_Item` itself has none) |
| `Pickup_Powerup` | `0x20afa` | — | not checked | Identified via switch-case position only (case 5) |
| `Pickup_Holdable` | `0x20c57` | — | not checked | Identified via switch-case position only (case 6) |
| `Pickup_Team` | `0x26e74` | — | not checked | Identified via switch-case position only (case 8) |
| `BG_CanItemBeGrabbed` | `0x4c42` | — | — | Already confirmed in [`../BG_MISC/README.md`](../BG_MISC/README.md); call site re-confirmed in `Touch_Item` at vanilla's exact position |
| `G_UseTargets` | `0x28151` | — | — | Already confirmed in [`../G_UTILS/G_UseTargets.md`](../G_UTILS/G_UseTargets.md); call site re-confirmed in `Touch_Item` at vanilla's exact position. Its own new per-(entity,activator) fields (`ent+0x42c..0x438`) sit **immediately adjacent** to this module's new `ent+0x32c` array — see cross-check in [Touch_Item.md](Touch_Item.md) |

## Not independently investigated this pass

`Pickup_Ammo`/`Pickup_Health`/`Pickup_Powerup`/`Pickup_Holdable`/`Pickup_Team` were identified only by
their switch-case position inside `Touch_Item`'s dispatch (strong evidence — exact case-value match to
vanilla `itType_t` ordering — but not independently hash-checked or instruction-decoded). None of them
needed to be, since the question this pass answers (where/how is the item hidden from other clients)
is fully answered by `Touch_Item` itself, which calls them only for their integer return value.

A `G_CheckTeamItems`-hash-matching entry was found at DeFRaG `0x21406` (hash `63e564ed`, matching
vanilla's `G_CheckTeamItems` hash exactly) but its size (`0x53`=83 instructions) does not match
vanilla's `G_CheckTeamItems` size (`0x8e`=142 instructions) — a discrepancy **not resolved**, flagged
as an open question for any future CTF/team-items investigation, out of scope here (not called from
anywhere in the per-client pickup mechanism).

---

## New `gentity_t` fields confirmed (DeFRaG-only, no vanilla equivalent)

| Offset | Field | Notes |
|--------|-------|-------|
| `ent+0x32c` | `int perClientPickupTime[64]` | Per-client pickup cooldown. `0`=eligible now (default), future `level.time` value = blocked until then, `-1` = never again for that client. Indexed by `clientNum` (`activator->client+0x8c`). Spans exactly to `ent+0x42c`. |
| `ent+0x42c`–`0x438` | (not this module's fields) | `G_UseTargets`'s own de-dup fields, documented in [`../G_UTILS/G_UseTargets.md`](../G_UTILS/G_UseTargets.md) — tile exactly against the end of `perClientPickupTime` above, ending at `ent+0x43c` = the project's already-documented `sizeof(gentity_t)`. |
| `ent+0x54` / `ent+0x58` | 64-bit per-client bitmask (likely repurposed unused `entityState_t` ints, not a struct-end addition) | Set by `Touch_Item` and `FinishSpawningItem` (helper `0x1629`), cleared by the `ent->use` handler (helper `0x15fa`). Purpose beyond "companion per-client flag" not confirmed — no consumer located. |

## oDFs comparison

oDFs's `code/game/g_items.c` previously implemented the unmodified vanilla global-hide-plus-
respawn-timer model. **Implemented**: added `perClientPickupTime[MAX_CLIENTS]` to `gentity_t`
(`g_local.h`), gated `Touch_Item` on it, replaced the global hide/respawn-schedule tail with the
per-client write, and reworked `Use_Item` to reset only the activator's own slot — see each
function's own doc for the exact diff. The core mechanism was independently re-verified against the
raw bytecode (not just taken on the investigating agent's word) before porting. `RespawnItem()`
itself was left unmodified since it has unrelated callers in `g_team.c`/`g_client.c` outside this
investigation's scope.
