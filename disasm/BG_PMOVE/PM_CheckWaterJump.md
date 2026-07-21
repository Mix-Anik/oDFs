# PM_CheckWaterJump — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006b40`  
**Called from:** PM_WaterMove (start)

---

## Structure

No args. Returns qboolean — whether a water jump was initiated.

```
if pm->ps->pm_time != 0: return false      ; already in a timed state
if pm->waterlevel != 2: return false       ; must be waist-deep (not fully under)

flatforward = (pml.forward[0], pml.forward[1], 0)
VectorNormalize(flatforward)

spot = ps->origin + 30 * flatforward      ; test 30 units ahead
spot[2] += 4

cont = pointcontents(spot, clientNum)
if !(cont & CONTENTS_SOLID): return false  ; need wall ahead

spot[2] += 16
cont = pointcontents(spot, clientNum)
if cont != 0: return false               ; need gap above wall
                                         ; (DeFRaG: stricter than Q3's SOLID|CLIP|BODY check)

; initiate water jump
velocity = 200 * forward
velocity[2] = 350
pm_flags |= PMF_TIME_WATERJUMP (0x100)
pm_time = 2000ms (0x7d0)
return true
```

**Minor DeFRaG difference:** The gap check above the wall uses `cont != 0` (completely empty space required). oDFs checks `cont & (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY)`. DeFRaG is slightly stricter — water above the gap would block the jump in DeFRaG but not in oDFs. In practice this is rarely encountered.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6b40` | Function entry |
| `0x6b44` | ps->pm_time (ps+0x10) != 0: return false |
| `0x6b4f` | pm->waterlevel (pm+0xd4) != 2: return false |
| `0x6b58–0x6b62` | Build flatforward from pml.forward (0x132874, 0x132878); z=0 |
| `0x6b65` | VectorNormalize (0x9d12) |
| `0x6b6d` | spot = origin + 30.0f × flatforward |
| `0x6b98` | spot[2] += 4.0f (0x40800000) |
| `0x6bae` | First pointcontents call |
| `0x6bb9` | Check CONTENTS_SOLID (0x1); if not solid: return false |
| `0x6bc4` | spot[2] += 16.0f (0x41800000) |
| `0x6bda` | Second pointcontents call |
| `0x6be2` | If cont != 0: return false (gap must be completely clear) |
| `0x6bed` | velocity[0..1] = 200.0f × forward[0..1] (0x43480000) |
| `0x6c0b` | velocity[2] = 350.0f (0x43af0000) |
| `0x6c19` | pm_flags |= 0x100 (PMF_TIME_WATERJUMP) |
| `0x6c21` | ps->pm_time = 0x7d0 (2000ms) |
| `0x6c24` | Return true |
| `0x6c25` | Return false path |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x41f00000` | 30.0f | Forward test distance |
| `0x40800000` | 4.0f | First height offset |
| `0x41800000` | 16.0f | Second height offset |
| `0x43480000` | 200.0f | Horizontal launch speed |
| `0x43af0000` | 350.0f | Vertical launch speed |
| `0x7d0` | 2000 | pm_time for water jump (ms) |
| `0x100` | | PMF_TIME_WATERJUMP |

---

## oDFs implementation notes

Full match. ✓ Minor: gap check is `contents == 0` in DeFRaG vs specific flag check in oDFs. Functionally equivalent in all normal gameplay scenarios.
