# PM_CheckDuck — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007bb5`  
**Called from:** PmoveSingle (twice per frame — before movement dispatch and after)

---

## Structure

No args. Sets pm->mins, pm->maxs, ps->viewheight and manages PMF_DUCKED.

### 1. Invulnerability powerup path (0x7bb8–0x7c11)

```
if ps->field_0x170 (powerups[PW_INVULNERABILITY]) != 0:
    clear pm_flags & ~PMF_INVULEXPAND (~0x4001)
    pm->mins = (−15, −15, −24)
    pm->maxs = (15, 15, 15)
    pm->mins[0] = pm->mins[1] = −15; pm->maxs[0..1] = 15
    pm->mins[2] = −24; pm->maxs[2] = 15 (crouch box)
    if pm_flags & PMF_INVULEXPAND (0x4000):
        pm->mins = (−42, −42, −42)   ; sphere
        pm->maxs = (42, 42, 42)
    pm_flags |= PMF_DUCKED
    ps->viewheight = 12 (CROUCH_VIEWHEIGHT)
    return
```

`PMF_INVULEXPAND = 0x4000` in DeFRaG.

### 2. Normal path — init box x/y (0x7c2a–0x7c56)

```
pm_flags &= ~PMF_INVULEXPAND (~0x4001)
pm->mins[0] = pm->mins[1] = −15.0f (PLAYER_WIDTH)
pm->maxs[0] = pm->maxs[1] = 15.0f
pm->mins[2] = −24.0f (MINS_Z)
```

Box base addresses: pm+0xb8=mins[0], pm+0xbc=mins[1], pm+0xc0=mins[2], pm+0xc4=maxs[0], pm+0xc8=maxs[1], pm+0xcc=maxs[2].

### 3. PM_DEAD check (0x7c57–0x7c6c)

```
if ps->pm_type == PM_DEAD (3):
    pm->maxs[2] = −8.0f (DEAD_HEIGHT)
    ps->viewheight = −16 (DEAD_VIEWHEIGHT = −0x10)
    return
```

### 4. Duck / stand-up logic (0x7c6e–0x7cdd)

```
if cmd.upmove (pm+0x1b) < 0:
    pm_flags |= PMF_DUCKED
else:
    if PMF_DUCKED set:
        pm->maxs[2] = 32.0f (DEFAULT_HEIGHT)
        trace to see if can stand up
        if !trace.allsolid: pm_flags &= ~PMF_DUCKED
```

### 5. Final height (0x7cde–0x7d04)

```
if PMF_DUCKED:
    pm->maxs[2] = 16.0f (CROUCH_HEIGHT)
    ps->viewheight = 12 (CROUCH_VIEWHEIGHT)
else:
    pm->maxs[2] = 32.0f (DEFAULT_HEIGHT)
    ps->viewheight = 26 (DEFAULT_VIEWHEIGHT = 0x1a)
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7bb5` | Function entry |
| `0x7bb8` | Load ps+0x170 (PW_INVULNERABILITY check) |
| `0x7bbd` | If 0 (no invuln): skip to 0x7c2a |
| `0x7bc4` | pm_flags & 0x4000 (PMF_INVULEXPAND) |
| `0x7bc7` | If not invulexpand: jump to 0x7bee (non-sphere path) |
| `0x7bc8–0x7bed` | Invulexpand sphere: mins/maxs = ±42 |
| `0x7bee–0x7c11` | Non-sphere invuln: mins=(−15,−15,−24), maxs=(15,15,16) |
| `0x7c12–0x7c28` | Set PMF_DUCKED, viewheight=12, return |
| `0x7c2a` | Normal path start: clear invuln flags, set base box |
| `0x7c5e` | pm_type == 3 (PM_DEAD) check |
| `0x7c6f` | cmd.upmove (pm+0x1b) < 0: duck |
| `0x7c87` | PMF_DUCKED check for stand-up attempt |
| `0x7c95` | pm->maxs[2] = 32.0f for stand-up trace |
| `0x7cc9` | trace call via pm->trace (pm+0xe4) |
| `0x7ccd` | trace.allsolid check |
| `0x7cde` | Final PMF_DUCKED check |
| `0x7cec` | maxs[2] = 16 (CROUCH_HEIGHT) + viewheight=12 |
| `0x7cfb` | maxs[2] = 32 (DEFAULT_HEIGHT) + viewheight=26 |
| `0x7d04` | Function return |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0xC2280000` (-0x3dd80000) | −42.0f | INVUL_RADIUS |
| `0x42280000` | 42.0f | INVUL_RADIUS |
| `0xC1700000` (-0x3e900000) | −15.0f | PLAYER_WIDTH |
| `0x41700000` | 15.0f | PLAYER_WIDTH |
| `0xC1C00000` (-0x3e400000) | −24.0f | MINS_Z |
| `0xC1000000` (-0x3f000000) | −8.0f | DEAD_HEIGHT |
| `0x41800000` | 16.0f | CROUCH_HEIGHT |
| `0x42000000` | 32.0f | DEFAULT_HEIGHT |
| `0xc` = 12 | | CROUCH_VIEWHEIGHT |
| `0x1a` = 26 | | DEFAULT_VIEWHEIGHT |
| `−0x10` = −16 | | DEAD_VIEWHEIGHT |
| `0x4000` | | PMF_INVULEXPAND |

---

## oDFs implementation notes

Full match. ✓ All bounding box constants and viewheight values confirmed identical.
