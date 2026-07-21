# func_0x1a394 — pm struct builder + physics dispatcher

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0001a394`  
**Called from:** `0x1a866` (Pmove outer, normal player physics path)

**Args:** `arg0` = ent (gentity_t*), `arg1` = cmd (usercmd_t*)

---

## Purpose

Builds the `pmove_t` struct on the stack, populates it from entity data, and calls the subdivision loop (0x8653) which iterates PmoveSingle (0x8331). This is the DeFRaG equivalent of oDFs's pmove_t setup in ClientThink_real.

---

## pm struct setup

The pm struct lives at `&local18` (stack offset 0x18 in a 0x120-byte frame).

### Steps (confirmed from disassembly)

```
1. Spectator-follow check:
   if client->spectatorState == SPECTATOR_FOLLOW (2):
       pm_type = PM_SPECTATOR (2); ps->speed = 1000; skip normal setup

2. Zero pm struct:
   call trap -0x65(&local18, 0, 0xec)   ; memset(pm, 0, 0xec=236 bytes)

3. pm->ps = client (local14)             ; pm+0x00 = client ptr (= &client->ps since ps is at client+0)

4. pm->cmd = *arg1                        ; pm+0x04..+0x1b = copy 0x18=24 bytes of usercmd_t

5. pm->tracemask = 0x10001               ; pm+0x1c = MASK_PLAYERSOLID = CONTENTS_SOLID|CONTENTS_PLAYERCLIP

6. pm->trace = -25                       ; pm+0xe4 = trap_Trace number
   pm->pointcontents = -26              ; pm+0xe8 = trap_PointContents number

7. Pass &local18 (= pm) to subdivision loop:
   call 0x8653(&pm)
```

### pm fields NOT set (left as 0 from memset)

| pm field | oDFs value | DeFRaG value | Notes |
|----------|-----------|--------------|-------|
| pm->gauntletHit (pm+0x28?) | CheckGauntletAttack result | **0 always** | No gauntlet check |
| pm->debugLevel (pm+0x20) | g_debugMove.integer | 0 | No debug output |
| pm->noFootsteps (pm+0x24) | g_dmflags & DF_NO_FOOTSTEPS | 0 | Footsteps always on |
| pm->pmove_fixed | pmove_fixed.integer | 0 | Fixed handled in outer |
| pm->pmove_msec | pmove_msec.integer | 0 | msec handled in outer |
| pm->obs_disabled | df_ob_killobs.integer | **not used** | OBS in ps->stats[12] |
| pm->promode | df_promode.integer | **not used** | promode in pm_flags |

### pm->tracemask by pm_type

The disassembly shows `0x10001` for the spectator path. For normal players:
- PM_DEAD: `MASK_PLAYERSOLID & ~CONTENTS_BODY` (not observed in 0x1a394, may be set elsewhere)
- Normal: `MASK_PLAYERSOLID = 0x10001`

This may be handled in the outer Pmove caller, not in 0x1a394.

---

## After Pmove (0x1a3ce–0x1a3e2)

```
; Save ps->origin back to entity
ent->grapplePoint[0..2] = client->ps.origin   ; block_copy 0xc bytes
call 0x1a1b2(ent)   ; trap_LinkEntity equivalent
call trap -0x20(ent)   ; some post-physics trap
```

`0x1a1b2` is likely the entity link function (trap_LinkEntity).

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x1a394` | Function entry |
| `0x1a39c` | spectatorState == SPECTATOR_FOLLOW (2) check |
| `0x1a3a6–0x1a3ae` | Spectator path: pm_type=2, ps->speed=1000 |
| `0x1a3af–0x1a3b7` | `trap -0x65(&pm, 0, 0xec)` = memset pm struct |
| `0x1a3b8–0x1a3bb` | pm->ps = client (pm+0x00 = client ptr) |
| `0x1a3bc–0x1a3bf` | pm->cmd = *arg1 (block_copy 0x18 bytes, pm+0x04) |
| `0x1a3c0–0x1a3c2` | pm->tracemask = 0x10001 (pm+0x1c) |
| `0x1a3c3–0x1a3c5` | pm->trace = -25 = trap_Trace (pm+0xe4) |
| `0x1a3c6–0x1a3c8` | pm->pointcontents = -26 = trap_PointContents (pm+0xe8) |
| `0x1a3ca` | Pass &pm to subdivision loop |
| `0x1a3cb` | Call `0x8653` (subdivision loop) |
| `0x1a3d6` | Save origin back: block_copy(ent->grapplePoint, ps->origin, 12) |
| `0x1a3da` | Call 0x1a1b2(ent) — trap_LinkEntity |
| `0x1a3e0` | Call trap -0x20(ent) |

---

## Resolved: gauntletHit always 0

Since the pm struct is zeroed by memset and gauntletHit is never set:
- `pm->gauntletHit = 0` always
- PM_Weapon at `0x80ec` reads `pm + 0x2c` and checks `!= 0`
- This is always 0, meaning the gauntlet fires in continuous attack mode (no per-frame hit detection)
- This is valid for DeFRaG since gauntlet use is uncommon and the check was for animation switching only

## Resolved: OBS disable mechanism

oDFs uses `pm->obs_disabled` (private pm field). DeFRaG:
- Does NOT have `obs_disabled` in pm struct
- Stores OBS disable state in `ps->stats[12] & 0x200`
- PM_GroundTrace reads `ps->stats[12] & 0x200` directly (not a pm field)
- The bit is presumably set somewhere in game init/player state setup (not yet located in disasm)
