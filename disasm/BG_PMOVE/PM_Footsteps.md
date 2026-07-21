# PM_Footsteps — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007d06`  
**Called from:** PmoveSingle (after PM_TorsoAnimation)

---

## Structure

No args. Computes xyspeed, sets leg animations, advances bob cycle, fires footstep events.

### 1. XY speed (0x7d06–0x7d34)

```
pm->xyspeed = sqrt(velocity[0]² + velocity[1]²)   ; stored at pm+0xd8
```

### 2. Airborne path (0x7d35–0x7d57)

```
if ps->groundEntityNum == ENTITYNUM_NONE (0x3ff):
    if ps->powerups[PW_INVULNERABILITY] (ps+0x170) != 0:
        PM_ContinueLegsAnim(LEGS_IDLECR = 0x17)
    if pm->waterlevel > 1:
        PM_ContinueLegsAnim(LEGS_SWIM = 0x11)
    return
```

### 3. No-movement idle (0x7d58–0x7d96)

```
if cmd.forwardmove == 0 AND cmd.rightmove == 0:
    if pm->xyspeed >= 5.0f: return   ; coasting, no idle anim
    ps->bobCycle = 0
    if PMF_DUCKED: PM_ContinueLegsAnim(LEGS_IDLECR = 0x17)
    else:          PM_ContinueLegsAnim(LEGS_IDLE = 0x16)
    return
```

### 4. Moving animations + bob rate (0x7d97–0x7dfe)

```
footstep = false

if PMF_DUCKED:
    bobmove = 0.5f
    if PMF_BACKWARDS_RUN: PM_ContinueLegsAnim(LEGS_BACKCR = 0x20)
    else:                 PM_ContinueLegsAnim(LEGS_WALKCR = 0x0d)
    ; ducked never plays footsteps

elif NOT BUTTON_WALKING (cmd.buttons & 0x10 == 0):
    bobmove = 0.4f
    if PMF_BACKWARDS_RUN: PM_ContinueLegsAnim(LEGS_BACK = 0x10)
    else:                 PM_ContinueLegsAnim(LEGS_RUN = 0x0f)
    footstep = true

else:   ; BUTTON_WALKING
    bobmove = 0.3f
    if PMF_BACKWARDS_RUN: PM_ContinueLegsAnim(LEGS_BACKWALK = 0x21)
    else:                 PM_ContinueLegsAnim(LEGS_WALK = 0x0e)
```

### 5. Bob cycle advancement (0x7dff–0x7e1e)

```
old = ps->bobCycle (ps+0x08)
ps->bobCycle = int(float(old) + bobmove × pml.msec) & 0xff
```

### 6. Footstep event dispatch (0x7e1f–0x7e76)

```
if ((old+64) XOR (bobCycle+64)) & 128:   ; crossed cycle boundary
    if waterlevel == 0:
        if footstep AND NOT pm->noFootsteps:
            PM_AddEvent(PM_FootstepForSurface())
    elif waterlevel == 1:
        PM_AddEvent(EV_FOOTSPLASH = 3)
    elif waterlevel == 2:
        PM_AddEvent(EV_SWIM = 5)
    ; waterlevel == 3: no sound
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7d06` | Function entry |
| `0x7d10–0x7d2d` | Compute xyspeed = sqrt(vel[0]²+vel[1]²) via trap −0x6b |
| `0x7d34` | Store xyspeed to pm+0xd8 |
| `0x7d3b` | ps->groundEntityNum (ps+0x44) == 0x3ff (airborne check) |
| `0x7d3c` | Airborne: jump to 0x7d3d |
| `0x7d3d` | ps+0x170 = PW_INVULNERABILITY check |
| `0x7d4a` | pm->waterlevel (pm+0xd4) > 1 → LEGS_SWIM |
| `0x7d58` | On-ground: cmd.forwardmove/rightmove == 0 check |
| `0x7d76` | xyspeed < 5.0f (0x40a00000) check for idle |
| `0x7d97` | footstep = 0; PMF_DUCKED (& 0x1) check |
| `0x7da5` | bobmove = 0.5f (PMF_DUCKED path) |
| `0x7dc9` | bobmove = 0.4f (running path) (0x3ecccccd) |
| `0x7de7` | bobmove = 0.3f (walking path) (0x3e99999a) |
| `0x7dff–0x7e19` | bob cycle: `(old + bobmove×msec) & 0xff` |
| `0x7e1a` | local34 = 64 (0x40) — cycle boundary offset |
| `0x7e2c` | `band 0x80` — boundary detection |
| `0x7e34` | waterlevel == 0 path |
| `0x7e48` | PM_FootstepForSurface (0x7698) call |
| `0x7e53` | waterlevel == 1 → EV_FOOTSPLASH (3) |
| `0x7e61` | waterlevel == 2 → EV_SWIM (5) |
| `0x7e76` | Function return |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x40a00000` | 5.0f | Minimum xyspeed for coasting (no idle anim) |
| `0x3f000000` | 0.5f | Duck bobmove rate |
| `0x3ecccccd` | 0.4f | Run bobmove rate |
| `0x3e99999a` | 0.3f | Walk bobmove rate |
| `0x40` = 64 | | Cycle boundary offset |
| `0x80` = 128 | | Cycle boundary mask |
| `3` | EV_FOOTSPLASH | Shallow water footstep event |
| `5` | EV_SWIM | Medium water footstep event |

---

## oDFs implementation notes

Full match. ✓ Same logic, same constants. Animation index values (0x0d, 0x0e, etc.) are DeFRaG's internal model animation indices — logic matches Q3/oDFs but exact numbers reflect DeFRaG's custom model.

Note: `pm->noFootsteps` is checked at pm+0x28. In oDFs, `pm->noFootsteps` is at the same relative position.
