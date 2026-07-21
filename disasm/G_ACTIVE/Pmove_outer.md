# Pmove outer — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0001a866`  
**Called from:** `0x1ac8f` (Pmove wrapper, called from ClientThink_real 0x1d27b)

**Signature:** `void Pmove(gentity_t *ent)` — takes entity pointer, NOT pmove_t pointer.

---

## Fundamental difference from Q3/oDFs

In Q3/oDFs, `Pmove(pmove_t *pm)` receives an already-built pmove_t struct.  
In DeFRaG, `Pmove(gentity_t *ent)` builds the pm struct internally from entity data.

---

## Structure

### 1. Initialization (0x1a866–0x1a876)

```
local104 = ent->client   (ent+0x204 = gclient_t*)
if *(client + 0x1d4) == 2:   ; spectator with SCOREBOARD state
    return   ; skip Pmove entirely
```

`client + 0x1d4` = some spectator state field in gclient_t.

### 2. Cmd pointer setup (0x1a877–0x1a879)

```
local108 = client + 0x1d8   ; cmd = &client->pers.cmd (usercmd_t at gclient_t+0x1d8)
```

### 3. Cmd input clamping (0x1a880–0x1a8a9)

```
if cmd->forwardmove (cmd+0x15) < -127: cmd->forwardmove = -127
if cmd->rightmove   (cmd+0x16) < -127: cmd->rightmove   = -127
if cmd->upmove      (cmd+0x17) < -127: cmd->upmove      = -127
```

Same clamping as Q3. ✓

### 4. Promode flag update (0x1a8ad–0x1a8af)

```
call func_0x2575(client)   ; sets ps->pm_flags & 0x8000 from df_promode cvar
```

### 5. commandTime clamping (0x1a8b0–0x1a8cd)

```
if cmd->serverTime > level.time + 200:   cmd->serverTime = level.time + 200
if cmd->serverTime < level.time - 1000:  cmd->serverTime = level.time - 1000
```

Same as oDFs. ✓

### 6. pmove_msec validation (0x1a8ce–0x1a8e5)

```
if pmove_msec < 8:   trap_Cvar_Set("pmove_msec", "8")
if pmove_msec > 33:  trap_Cvar_Set("pmove_msec", "33")   ; DeFRaG max = 33 (not 0x21=33 confirmed)
```

Same as oDFs. ✓

### 7. pmove_fixed quantization (0x1a8e6–0x1a906)

```
pmove_fixed = 0
if pmove_fixed_cvar != 0:
    snap cmd->serverTime to pmove_msec boundary
    cmd->serverTime = ((serverTime + msec-1) / msec) * msec
```

### 8. msec calculation (0x1a907–0x1a923)

```
msec = cmd->serverTime - client->ps.commandTime
if msec < 1 AND spectator_type != SPECTATOR_FOLLOW: return
if msec > 200: msec = 200
```

### 9. Spectator check (0x1a924–0x1a929)

```
if some_race_flag_global (0x12c698) != 0:
    call 0x1a5df(client); return   ; race-specific exit early
```

### 10. Main dispatch (0x1a930–0x1a94a)

```
if client->sess.sessionTeam == 3 (TEAM_SPECTATOR):
    if client->spectatorState == 3: return
    call 0x1a394(ent, cmd)   ; spectator physics
    return

; normal player:
call 0x1a44a(client)    ; race timer handling
```

Then continues with the main physics path...

### 11. Subdivision loop call (via func_0x1a394)

The actual Pmove call path goes through `0x1a394` which:
- Builds the pm struct on the stack (see [func_0x1a394.md](func_0x1a394.md))
- Calls subdivision loop `0x8653` → PmoveSingle `0x8331`

### 12. Post-physics write-back (0x1ab20–0x1ab4f)

After PmoveSingle returns, results are written back to the entity:
```
ent+0x1e8 (some position field) += block_copy from local (12 bytes, vec3)
client+0x2ec = ps.velocity (block_copy 12 bytes — saves velocity to gclient_t)
ent+0x1b4 = local velocity/pos (12 bytes)
ent+0x1c0 = local velocity/pos (12 bytes)
ent->waterlevel (ent+0x314) = local
ent->watertype  (ent+0x310) = local
```

`client+0x2ec` is a confirmed gclient_t offset for storing previous-frame velocity.

### 13. World effects (0x1ab57–0x1ab6c)

```
if client->noclip (client+0x2c0) != 0:
    jump past effects   ; noclip players skip damage
else:
    call 0x1a1b2(ent)   ; unknown — large function (483 insns, 4264-byte frame)
    call P_WorldEffects (0x19f04)(ent)
```

This is DeFRaG's equivalent of `ClientEndFrame`. **DeFRaG has no separate ClientEndFrame function** — it is merged into this Pmove outer function.

Function `0x1a1b2` is unidentified. It:
- Requires `client != NULL` and `ps.stats[STAT_HEALTH] > 0` (alive only)
- Computes `ps.origin ± global_vec3` at `0x232c`–`0x2334`
- Too large for P_DamageFeedback; possibly DeFRaG-specific race/position tracking

### 14. Other post-effects calls (0x1ab6d–0x1abb7)

```
call 0x37c7(ent, local13c)
// check: if commandTime changed → subdivision loop re-entry at 0x1aa8f
call 0x1a7ba(client)
call 0x4324(ent, local18)
call 0x1a63a(ent, local110)
// save old origin: ent+0x1e8 = block_copy 12 bytes from ps.origin (client+0x14)
call 0x14751(ent+0x1e8)
call 0x1a10f(ent, local18)
```

### 15. eFlags update (0x1abda–0x1ac06)

```
ent+0x2cc = old_eFlags (save previous)
ent+0x2c8 = *(local108+0x10)   ; new eFlags from ps
*(ent+0x2d0) = (*(ent+0x2d0) & ~ent->2cc) | new_eFlags   ; merge changed bits
```

Tracks which eFlags bits changed this frame.

### 16. Speed tracking — DeFRaG-specific (0x1ac53–0x1ac83)

```
if ps.stats[STAT_HEALTH] (client+0xb8) > 0:
    local_vel = block_copy ps.velocity (client+0x20, 12 bytes)
    speed = (int)VectorLength(local_vel)
    if speed > 800 AND speed > *(0x131844):
        *(0x131844) = speed              ; global max speed
        *(0x131848) = ps.clientNum       ; who achieved it
        *(0x13184c) = 0 (byte)           ; reset flag
        *(0x1318cc) = level.time + 5000  ; expire in 5s
```

DeFRaG tracks the highest player speed observed each frame (threshold: 800 ups) for display. Not present in oDFs.

### 17. Final call (0x1ac84–0x1ac8e)

```
call 0x1a4ec(ent, local10c)
leave 0x160
```

`0x1a4ec` is unidentified.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x1a866` | Function entry |
| `0x1a86e` | Load ent->client (ent+0x204) |
| `0x1a873` | Spectator scoreboard check → early return 0x1ac8d |
| `0x1a877` | local108 = client+0x1d8 = cmd pointer |
| `0x1a880–0x1a8a9` | forwardmove/rightmove/upmove clamp to -127 |
| `0x1a8ad` | Call func_0x2575(client) — promode flag setter |
| `0x1a8b0–0x1a8cd` | commandTime clamping ±200ms/1000ms |
| `0x1a8ce–0x1a8e5` | pmove_msec validation 8–33 |
| `0x1a8e6–0x1a906` | pmove_fixed serverTime quantization |
| `0x1a907–0x1a923` | msec calculation + spectator-follow check |
| `0x1a924` | Race timer global 0x12c698 check |
| `0x1a930` | sessionTeam == 3 (spectator) check |
| `0x1a940–0x1a948` | Spectator: call 0x1a394(ent, cmd) |
| `0x1ab20–0x1ab4f` | Post-physics write-back (watertype, waterlevel, origins) |
| `0x1ab57–0x1ab6c` | Noclip guard → 0x1a1b2 + P_WorldEffects |
| `0x1ab6a` | Call P_WorldEffects (0x19f04) |
| `0x1abda–0x1ac06` | eFlags update |
| `0x1ac53–0x1ac83` | Speed tracking (max speed > 800 ups) |
| `0x1ac84` | Call 0x1a4ec(ent, local10c) |
| `0x1ac8d` | Common return point |
| `0x1ac8e` | leave 0x160 |

---

## Related DeFRaG-specific wrappers

### Pmove wrapper — `0x1ac8f`
Called from ClientThink_real (0x1d27b) with clientNum.

```
ent = clientNum * 0x43c + 0x1b2f0   ; compute gentity_t* from clientNum
call trap −0x25 (arg: clientNum, client+0x1d8)   ; unknown DeFRaG trap
*(client+0x2c4) = level.time        ; store level time to client field
if ent->flag & 0x8: call Pmove(ent)
else: call 0x3d7e                   ; alternate path (dead/inactive?)
```

`0x43c` = sizeof(gentity_t) in DeFRaG.  
`0x1b2f0` = base address of g_entities array.

### Spectator Pmove wrapper — `0x1accd`
Called from 0x1adc9 (spectator-follow loop) with ent.

```
if client->sess.sessionTeam == TEAM_SPECTATOR:
    call 0x1acf4(ent)   ; spectator+pmove_fixed path
else:
    quantize cmd->serverTime to pmove_msec boundary
    loop over followed clients:
        if followed client changed: break
    call Pmove(ent)
```

---

## oDFs comparison

| oDFs | DeFRaG |
|------|--------|
| `Pmove(pmove_t *pm)` | `Pmove(gentity_t *ent)` |
| Builds pmove_t in ClientThink_real | Builds pm struct inside Pmove outer |
| `pm.obs_disabled = df_ob_killobs.integer` | OBS stored in `ps->stats[12] & 0x200` |
| `pm.gauntletHit = CheckGauntletAttack(ent)` | gauntletHit always 0, never called |
| `pm.noFootsteps`, `pm.debugLevel` set | Always 0 (zeroed by memset) |
| `pm.promode = df_promode.integer` | func_0x2575 sets `ps->pm_flags & 0x8000` |
