# G_ACTIVE — Defrag QVM disassembly analysis

Analysis of server-side per-frame client update functions from `g_active.c` in the Defrag qagame.qvm.

For general disassembly methodology, struct layouts, and global addresses see [../README.md](../README.md).

---

## Critical architectural difference from Q3/oDFs

**In Q3/oDFs:** `ClientThink_real` builds a `pmove_t` on the stack, populates it (ps, cmd, tracemask, gauntletHit, obs_disabled, promode, etc.) and calls `Pmove(pmove_t *pm)`.

**In DeFRaG:** `Pmove` takes a `gentity_t *ent` directly. The pm struct is a **static global** — its pointer is stored permanently at `0x132900`. The struct is re-initialized from entity data each frame inside the Pmove outer function.

---

## Functions analyzed

| Function | Address | Status |
|----------|---------|--------|
| **`ClientThink_real`** (was "Pmove outer") | `0x1a866` | ✅ Analyzed — see [Pmove_outer.md](Pmove_outer.md); identity corrected, see [ClientThink.md](ClientThink.md). The real `ClientEndFrame` is a separate function, `0x1adc9` — see below; the "ClientEndFrame merged into Pmove outer" claim in this file's Unresolved table is superseded, though `0x1a866` may still contain its own P_WorldEffects-shaped call (`0x1ab6a`) independent of the per-server-frame `ClientEndFrame` — not reconciled this pass. |
| **`ClientThink`** (real per-command dispatcher) | `0x1ac8f` | ✅ Resolved this pass — see [ClientThink.md](ClientThink.md) |
| func_0x1a394 (pm struct builder) | `0x1a394` | ✅ Analyzed — see [func_0x1a394.md](func_0x1a394.md) |
| func_0x1a1b2 = `G_TouchTriggers` (touch trigger handler) | `0x1a1b2` | ✅ **Resolved — this IS `G_TouchTriggers`**, see [func_0x1a1b2.md](func_0x1a1b2.md). Not race-only: the `sessionTeam`/`eType`/hardcoded-address checks are one nested conditional, byte-for-byte matching vanilla's spectator exception (`Touch_DoorTrigger` identified, confirmed against `g_mover.c`); teleporters are never excluded for normal players; the function's tail has the vanilla jumppad-frame reset. **Fixed in code**: `DeFRaG_TouchTriggers` (`g_active.c`) now faithfully ports this, and the redundant second `G_TouchTriggers()` call in `ClientThink_real` was removed. One unconfirmed detail remains: a `flags & 0x40000000` exclusion bit with no located purpose |
| P_WorldEffects | `0x19f04` | ✅ Analyzed — see [P_WorldEffects.md](P_WorldEffects.md) |
| **`ClientSpawn`** (was misidentified as "ClientThink_real") | `0x1d27b` | ✅ Substantially analyzed — see [ClientThink_real.md](ClientThink_real.md) (identity corrected — this is `ClientSpawn`, see [ClientThink.md](ClientThink.md)) |
| func_0x4152 (DeFRaG input state tracker) | `0x4152` | ✅ Analyzed — see [func_0x4152.md](func_0x4152.md) |
| func_0x421b (per-frame ps flags updater) | `0x421b` | ✅ Analyzed — see [func_0x421b.md](func_0x421b.md) |
| func_0x426b (SetOBS) | `0x426b` | ✅ Analyzed — see [func_0x426b.md](func_0x426b.md) |
| func_0x42b5 (SetGrappleMode) | `0x42b5` | ✅ Analyzed — see [func_0x42b5.md](func_0x42b5.md); consumer fully decoded, see [GrappleMode.md](GrappleMode.md) |
| func_0x4290 (SetLGKnockback) | `0x4290` | ✅ Analyzed — see [func_0x4290.md](func_0x4290.md); consumer fully decoded, see [LG_Knockback.md](LG_Knockback.md) |
| Spectator Pmove wrapper | `0x1accd` | ✅ Analyzed (inline in Pmove_outer.md) |
| func_0x1a44a (inactivity kick handler) | `0x1a44a` | ✅ Analyzed — see [func_0x1a44a.md](func_0x1a44a.md) |
| **`ClientEndFrame`** (was "SpectatorThink / follow loop") | `0x1adc9` | ✅ Resolved this pass — see [ClientEndFrame.md](ClientEndFrame.md) |
| **`SpectatorClientEndFrame`** | `func_0x1acf4` | ✅ Resolved this pass — see [ClientEndFrame.md](ClientEndFrame.md) |
| G_TouchTriggers | ✅ **Fully resolved: `func_0x1a1b2` (`0x1a1b2`)** | Long history of corrections, now closed out. `0x1bda5` (an early wrong guess for this identity) is **not** `G_TouchTriggers` — it's `G_RemoveQueuedBotBegin` (a 16-entry pending-bot-begin queue clear, called from `ClientDisconnect`/`0x1d6dc` — see [`../G_CLIENT/README.md`](../G_CLIENT/README.md)). The real function is `func_0x1a1b2`, called from `0x1a866` ("Pmove outer" = the true `ClientThink_real`, see [ClientThink.md](ClientThink.md)): confirmed to be a **complete** `G_TouchTriggers` — its `sessionTeam`/`eType`/hardcoded-touch-address checks are one nested conditional byte-for-byte matching vanilla's own spectator exception (teleporters are never excluded for normal players; the hardcoded address is `Touch_DoorTrigger`, confirmed against `g_mover.c`), and its tail has vanilla's exact jumppad-frame reset. Full resolution and the oDFs code fix (removed a double-touch bug caused by running both `DeFRaG_TouchTriggers` and `G_TouchTriggers` every frame): see [func_0x1a1b2.md](func_0x1a1b2.md). One minor detail remains unconfirmed (a `flags & 0x40000000` exclusion bit with no located purpose). |
| func_0x22fe6 (grapple-hook push/crush/carry mover) | `0x22fe6` | ✅ Fully decoded — see [GrappleHookMover.md](GrappleHookMover.md); settles a `G_TouchTriggers` candidate as ruled out — **not implemented** |
| ClientRespawn | `0x1cb0b` | ✅ Confirmed — **corrects prior wrong guess of "ClientThink"**, see [ClientRespawn.md](ClientRespawn.md) |
| CopyToBodyQue | `func_0x1c937` | ✅ Strong match — **corrects prior wrong guess of "cmd fetch"**, see [ClientRespawn.md](ClientRespawn.md) |
| ClientBegin | `0x1d18b` | ✅ Confirmed via debug strings — see [ClientRespawn.md](ClientRespawn.md) |
| TeamCount | `0x1cb3b` | ✅ Confirmed — see [PickTeam.md](PickTeam.md) |
| PickTeam | `0x1cbbd` | ✅ Confirmed — **corrects prior wrong guess of "G_RunClient"**, see [PickTeam.md](PickTeam.md) |

---

## Key findings

### Architecture

- **pm is a static global**: `0x132900` permanently holds a pointer to the pm struct. It is never reassigned. PmoveSingle stores this pointer at function entry (`*(0x132900) = arg0`).
- **`Pmove` takes `gentity_t*`**: DeFRaG's `Pmove(0x1a866)` takes an entity pointer, not a pmove_t pointer. All pm struct setup is done inside Pmove outer and func_0x1a394.
- **Corrected call chain** (see [ClientThink.md](ClientThink.md)): `ClientThink (0x1ac8f)` → `ClientThink_real (0x1a866, "Pmove outer")` → `func_0x1a394` → `subdivision loop (0x8653)` → `PmoveSingle (0x8331)`. Separately, `ClientSpawn (0x1d27b)` (called from `ClientBegin`/`ClientRespawn`) ends by calling `ClientThink (0x1ac8f)` for an immediate post-spawn think cycle.

### pm struct setup (func_0x1a394)

- pm zeroed by `memset(pm, 0, 0xec)` each frame
- `pm->ps = &client->ps` (client pointer)
- `pm->cmd = *ucmd` (24 bytes copied)
- `pm->tracemask = 0x10001` (MASK_PLAYERSOLID)
- `pm->trace = -25` (trap_Trace)
- `pm->pointcontents = -26` (trap_PointContents)
- All other fields: **zeroed** (never set explicitly)

### pm struct layout differences (DeFRaG vs oDFs)

DeFRaG's pm struct offsets shift watertype/waterlevel/xyspeed by +8 compared to oDFs. DeFRaG has 2 extra fields at pm+0xc8 and pm+0xcc (between maxs[2] and watertype). These are likely pmove_fixed and pmove_msec, reordered or inserted. trace/pointcontents remain at pm+0xe4/0xe8 in both.

| Field | oDFs offset | DeFRaG offset |
|-------|------------|---------------|
| watertype | pm+0xc8 | pm+0xd0 |
| waterlevel | pm+0xcc | pm+0xd4 |
| xyspeed | pm+0xd0 | pm+0xd8 |
| obs_disabled | pm+0xdc | **not in pm** |
| promode | pm+0xe0 | **not in pm** |
| trace | pm+0xe4 | pm+0xe4 |
| pointcontents | pm+0xe8 | pm+0xe8 |

### Key fields absent from DeFRaG pm struct

- **`gauntletHit = 0 always`**: memset'd, never set. No `CheckGauntletAttack` call. PM_Weapon's check at pm+0x2c is always 0 — gauntlet fires in continuous mode (animation-only difference).
- **`obs_disabled` not in pm**: OBS disable stored in `ps->stats[12] & 0x200` (networked playerState_t field). oDFs uses `pm->obs_disabled` (private pm field). Both suppress overbounce the same way — difference is that DeFRaG's OBS state is visible to clients for prediction.
- **`noFootsteps`, `debugLevel`**: always 0. DeFRaG footsteps always on; no in-game movement debug.
- **promode in pm**: not used — promode is applied directly to `ps->pm_flags & 0x8000` via `func_0x2575`.
- **pmove_fixed, pmove_msec in pm**: zeroed. These are handled in Pmove outer (0x1a866) before calling func_0x1a394.

### P_WorldEffects

- Full match with oDFs on all damage values: drowning (+=2/frame, max 15, MOD_WATER=14), lava (30×waterlevel, MOD_LAVA=16), slime (10×waterlevel, MOD_SLIME=15)
- Minor difference: noclip check uses a global flag; drowning sound logic differs. No physics impact.

### Global addresses confirmed

| Address | Description |
|---------|-------------|
| `0x1b2f0` | Base of g_entities array |
| `0x43c` | sizeof(gentity_t) = 1084 |
| `0x131820` | Global noclip flag (checked in P_WorldEffects) |
| `0x132550` | `df_ob_KillOBs` cvar integer cache — non-zero = OBS globally disabled |
| `0x13183c` | Grapple mode cvar cache (0–3) — controls stats[12] bits 0x400/0x800 |
| `0x131840` | `df_lg_Knockback` cvar integer cache — controls stats[12] bit 0x1000 |
| `0x1317bc` | Previously labeled OBS-related; actually controls weapon selection behavior in ClientThink_real spawn path — exact purpose uncertain |
| `0x12c698` | Race-mode flag (checked in Pmove outer) |
| `0x131844` | DeFRaG max speed tracker — highest speed seen this frame |
| `0x131848` | clientNum of player who achieved max speed |
| `0x13184c` | Max speed reset flag (1 byte) |
| `0x1318cc` | Max speed expire time = level.time + 5000 |
| `0x1318dc` | Per-client 32-byte array (clientIndex × 32); cleared by 0x3a5f each frame |
| `0x12a2f0` | Base of g_clients array (used in 0x3588 client-index calculation) |
| `0x232c` | EntitiesInBox half-extents = **{40.0, 40.0, 52.0}** (data segment constant, used in func_0x1a1b2) |
| `0x18bac` | pmove_msec cvar value |
| `0x18cbc` | pmove_fixed cvar value |
| `0x1b1ec` | Unknown level global — checked `< 4` at ClientThink_real entry |
| `0x12c6a8` | Race start position (vec3_t) — copied to ent+0x5c and ps.origin in func_0x85a |
| `0x12c6b4` | Race start angles (vec3_t) — copied to ps+0x98 in func_0x85a |
| `0x12c6ec` | Spectator flags (checked in spectator cmd acquisition path) |
| `0x132660` | Unknown global; bit 0x1 gates call to func_0x28534 on spawn |
| `0x19ecc` | Unknown cvar/time multiplier used in ClientThink_real |

### gclient_t / gentity_t field offsets (confirmed)

| Offset | Field |
|--------|-------|
| ent+0x204 | ent->client (gclient_t*) |
| ent+0x2dc | ent->health |
| ent+0x2e4 | ent->damage |
| ent+0x310 | ent->watertype |
| ent+0x314 | ent->waterlevel |
| client+0x2c0 | client->noclip (DeFRaG per-client flag) |
| client+0x340 | client->airOutTime |
| client+0x378 | client->pain_debounce_time |
| client+0x1d4 | client->pers.connected (int: DISCONNECTED=0/CONNECTING=1/CONNECTED=2) — confirmed via [PickTeam.md](PickTeam.md) |
| client+0x1d8 | client->pers.cmd (usercmd_t) |
| client+0x2ec | Previous-frame velocity (vec3, saved after each Pmove) |
| client+0x284 | sess.sessionTeam (3 = TEAM_SPECTATOR) |
| `0x12a308` | level.maxclients — confirmed via [PickTeam.md](PickTeam.md)'s `TeamCount` loop bound |
| ent+0x4 | s.eType (entity state type field) |
| ent+0x1a8 | entity flags (bit 0x8 = player has active touch function) |
| ent+0x1cc | entity flags; bit 0x40000000 causes exclusion from `func_0x1a1b2`'s touch-dispatch loop — **exact purpose not confirmed**, see correction in [func_0x1a1b2.md](func_0x1a1b2.md) |
| ent+0x2c0 | touch function pointer — `void (*touch)(self, other, trace*)` |
| ent+0x1d0 | ent->r.absmin[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage, see [../G_COMBAT/G_RadiusDamage.md](../G_COMBAT/G_RadiusDamage.md) |
| ent+0x1dc | ent->r.absmax[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage |
| ent+0x1e8 | ent->r.currentOrigin[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage |
| ent+0x218 | ent->flags (FL_* bitfield) — confirmed `FL_NO_KNOCKBACK = 0x800` matches oDFs `g_local.h` exactly; via G_Damage |
| ent+0x2e0 | ent->takedamage — confirmed via G_Damage / G_RadiusDamage |

---

## ClientThink / G_RunClient investigation

Went looking for oDFs's `ClientThink(clientNum)` / `G_RunClient(ent)` equivalents to
close out plumbing feeding `usercmd_t` into Pmove. This turned into a larger
correction pass — full details and evidence in [ClientRespawn.md](ClientRespawn.md)
and [PickTeam.md](PickTeam.md). Summary of what changed:

- **`0x1cbbd` is not `G_RunClient`** — it's **`PickTeam`** (team-balance autojoin
  helper), and its helper `0x1cb3b` is **`TeamCount`** — both structurally identical
  to vanilla Q3's `g_utils.c`.
- **`0x1cb0b` is not "ClientThink"** — it's **`ClientRespawn`**, called from two sites
  *inside Pmove outer* (gated on `health <= 0`), matching oDFs's forcerespawn-timer
  and attack/use-button respawn logic exactly. This is DeFRaG relocating
  `ClientThink_real`'s dead-player-respawn block into Pmove outer — the same pattern
  already seen with `ClientEndFrame` being absorbed into Pmove outer.
- **`func_0x1c937` is not "cmd fetch"** — it's a strong structural match for
  **`CopyToBodyQue`** (spawns the corpse into an 8-slot body-queue pool, matching
  vanilla's `BODY_QUEUE_SIZE = 8` exactly).
- **`0x1d18b` is `ClientBegin`**, confirmed via two decoded debug strings
  (`"ClientBegin: %i"`, `"%s entered the game."`) and the classic
  eFlags-preserving `ps` memset idiom. It calls `0x1d27b` exactly where vanilla
  calls `ClientSpawn`.
- **Real usercmd fetch happens in the Pmove wrapper** (`0x1ac8f`, trap `-0x25` =
  `trap_GetUsercmd`, previously mislabeled "unknown DeFRaG trap") — not in
  `0x1cb0b`/`ClientRespawn` as originally guessed. This means DeFRaG's usercmd
  fetch happens deep inside whatever `0x1d27b` is, not in a `ClientThink()`-shaped
  wrapper the way oDFs does it.
- **Resolved (later pass):** `0x1d27b` (documented at length as `ClientThink_real`)
  is actually **`ClientSpawn`** — its only two callers (`ClientBegin`,
  `ClientRespawn`) are both spawn-related, and it finishes by calling the real
  `ClientThink` (`0x1ac8f`) as an immediate post-spawn think cycle, rather than being
  a merge of the two functions. See [ClientThink.md](ClientThink.md).
- **Resolved (later pass): the real per-command `ClientThink` dispatcher is
  `0x1ac8f`** — structurally an exact match for vanilla's `ClientThink(clientNum)`
  (`trap_GetUsercmd`, `lastCmdTime`, bot/synchronous-clients gate), called directly
  by the engine (shown as caller `0x0` in the disassembly, i.e. outside the QVM via
  `vmMain`'s `GAME_CLIENT_THINK`). It calls `0x1a866` — meaning **"Pmove outer" is
  actually DeFRaG's `ClientThink_real`**, not merely a Pmove wrapper. See
  [ClientThink.md](ClientThink.md) for the full corrected call graph.
- Also fixed two pre-existing doc bugs found along the way: Pmove outer's entry
  guard had its branch direction backwards (see
  [Pmove_outer.md](Pmove_outer.md#1-initialization-0x1a8660x1a876), now confirmed
  to be a `pers.connected == CON_CONNECTED` check), and trap `-0x25` in the Pmove
  wrapper was mislabeled "unknown" when it's `trap_GetUsercmd`.

---

## Unresolved

| Issue | Details |
|-------|---------|
| stats[12] & 0x200 origin | ✅ Resolved — set by func_0x426b called from func_0x421b, called from ClientThink_real; implemented in g_active.c |
| gauntletHit PM_Weapon check | ✅ Resolved — always 0 (pm struct memset every frame, never written); see [func_0x1a394.md](func_0x1a394.md#resolved-gauntlethit-always-0) |
| `0x273f` | ✅ Resolved — NOT ClientEvents (that is `0x1a63a`, [ClientEvents.md](ClientEvents.md)). `0x273f(ent)` = tiny wrapper → `0x3a5f(ent->client)`, which `memset`s that client's 32-byte `raceTimer_t` record; see [../G_CLIENT/func_0x273f.md](../G_CLIENT/func_0x273f.md) |
| ClientEndFrame | ✅ Resolved — is a separate function, `0x1adc9`, see [ClientEndFrame.md](ClientEndFrame.md). Supersedes the earlier "merged into 0x1a866" claim; `0x1a866` may still separately contain its own P_WorldEffects-shaped call (`0x1ab6a`) — not reconciled with `0x1adc9`'s `func_0x19e41` call this pass. |
| func_0x1a1b2 purpose | ✅ Identified: **per-frame touch trigger handler** — detects start/finish/checkpoint zone entry each physics tick |
| func_0x1c937 purpose | ✅ Resolved (strong match) — `CopyToBodyQue`, see [ClientRespawn.md](ClientRespawn.md) |
| Real `G_RunClient` location | Not found — `0x1cbbd` (the prior guess) is actually `PickTeam` |
| Real per-command "ClientThink" dispatcher location | Not found — `0x1d27b` (`ClientThink_real`'s address) has no per-command caller, only spawn-related callers |
| `0x1d27b` identity: `ClientThink_real`, `ClientSpawn`, or a merge of both | Not determined — see [open question](ClientRespawn.md#open-question-what-is-0x1d27b) |
| Why DeFRaG fetches usercmd inside the Pmove wrapper instead of in a `ClientThink()`-shaped wrapper | Architectural difference from oDFs noted, not explained |
