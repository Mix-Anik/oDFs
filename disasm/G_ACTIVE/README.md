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
| Pmove outer (+ ClientEndFrame merged) | `0x1a866` | ✅ Analyzed — see [Pmove_outer.md](Pmove_outer.md) |
| func_0x1a394 (pm struct builder) | `0x1a394` | ✅ Analyzed — see [func_0x1a394.md](func_0x1a394.md) |
| func_0x1a1b2 (touch trigger handler) | `0x1a1b2` | ✅ Analyzed — see [func_0x1a1b2.md](func_0x1a1b2.md) |
| P_WorldEffects | `0x19f04` | ✅ Analyzed — see [P_WorldEffects.md](P_WorldEffects.md) |
| ClientThink_real | `0x1d27b` | ✅ Substantially analyzed — see [ClientThink_real.md](ClientThink_real.md) |
| func_0x4152 (DeFRaG input state tracker) | `0x4152` | ✅ Analyzed — see [func_0x4152.md](func_0x4152.md) |
| func_0x421b (per-frame ps flags updater) | `0x421b` | ✅ Analyzed — see [func_0x421b.md](func_0x421b.md) |
| func_0x426b (SetOBS) | `0x426b` | ✅ Analyzed — see [func_0x426b.md](func_0x426b.md) |
| func_0x42b5 (SetGrappleMode) | `0x42b5` | ✅ Analyzed — see [func_0x42b5.md](func_0x42b5.md) |
| func_0x4290 (SetLGKnockback) | `0x4290` | ✅ Analyzed — see [func_0x4290.md](func_0x4290.md) |
| Pmove wrapper | `0x1ac8f` | ✅ Analyzed (inline in Pmove_outer.md) |
| Spectator Pmove wrapper | `0x1accd` | ✅ Analyzed (inline in Pmove_outer.md) |
| func_0x1a44a (inactivity kick handler) | `0x1a44a` | ✅ Analyzed — see [func_0x1a44a.md](func_0x1a44a.md) |
| SpectatorThink / follow loop | `0x1adc9` | 🔍 Identified only |
| G_TouchTriggers | `0x1bda5` | 🔍 Identified — iterates trigger list |
| ClientThink | `0x1cb0b` | 🔍 Identified — calls 0x1c937 (cmd fetch) then ClientThink_real |
| G_RunClient | `0x1cbbd` area | 🔍 Identified — synchronous client runner |

---

## Key findings

### Architecture

- **pm is a static global**: `0x132900` permanently holds a pointer to the pm struct. It is never reassigned. PmoveSingle stores this pointer at function entry (`*(0x132900) = arg0`).
- **`Pmove` takes `gentity_t*`**: DeFRaG's `Pmove(0x1a866)` takes an entity pointer, not a pmove_t pointer. All pm struct setup is done inside Pmove outer and func_0x1a394.
- **Pmove call chain**: `ClientThink_real (0x1d27b)` → `Pmove wrapper (0x1ac8f)` → `Pmove outer (0x1a866)` → `func_0x1a394` → `subdivision loop (0x8653)` → `PmoveSingle (0x8331)`

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
| client+0x1d8 | client->pers.cmd (usercmd_t) |
| client+0x2ec | Previous-frame velocity (vec3, saved after each Pmove) |
| client+0x284 | sess.sessionTeam (3 = TEAM_SPECTATOR) |
| ent+0x4 | s.eType (entity state type field) |
| ent+0x1a8 | entity flags (bit 0x8 = player has active touch function) |
| ent+0x1cc | entity flags (bit 0x40000000 = excluded from race trigger check) |
| ent+0x2c0 | touch function pointer — `void (*touch)(self, other, trace*)` |
| ent+0x1d0 | ent->r.absmin[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage, see [../G_COMBAT/G_RadiusDamage.md](../G_COMBAT/G_RadiusDamage.md) |
| ent+0x1dc | ent->r.absmax[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage |
| ent+0x1e8 | ent->r.currentOrigin[0] (per-axis, +4 per component) — confirmed via G_RadiusDamage |
| ent+0x218 | ent->flags (FL_* bitfield) — confirmed `FL_NO_KNOCKBACK = 0x800` matches oDFs `g_local.h` exactly; via G_Damage |
| ent+0x2e0 | ent->takedamage — confirmed via G_Damage / G_RadiusDamage |

---

## Unresolved

| Issue | Details |
|-------|---------|
| stats[12] & 0x200 origin | ✅ Resolved — set by func_0x426b called from func_0x421b, called from ClientThink_real; implemented in g_active.c |
| gauntletHit PM_Weapon check | pm+0x2c: whether it's shifted gauntletHit or framecount — always 0 either way |
| ClientEvents address | 0x273f → calls 0x3a5f(client) → clears 32 bytes of per-client array at 0x1318dc; purpose unclear (DeFRaG-specific?) |
| ClientEndFrame | **No separate function** — merged into 0x1a866 (sections 12–17); P_WorldEffects at 0x1ab6a |
| func_0x1a1b2 purpose | ✅ Identified: **per-frame touch trigger handler** — detects start/finish/checkpoint zone entry each physics tick |
