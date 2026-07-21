# PM_CrashLand — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000076b1`  
**Called from:** PM_GroundTrace (on landing transition)

---

## Structure

No args. Calculates fall damage and dispatches sound/pain events on landing.

### 1. Landing animation (0x76b2–0x76c7)

```
if pm_flags & PMF_BACKWARDS_JUMP (0x8):
    PM_ForceLegsAnim(LEGS_LANDB)   ; backward landing
else:
    PM_ForceLegsAnim(LEGS_LAND)    ; forward landing
ps->legsTimer = 0x82 = 130ms (TIMER_LAND)
```

### 2. Physics velocity calculation (0x76cf–0x7712)

Computes exact landing velocity using quadratic kinematics:
```
dist = ps->origin[2] - pml.previous_origin[2]  ; (0x1328ec)
v0 = pml.previous_velocity[2]                  ; (0x1328f8)
a = -ps->gravity / 2
discriminant = v0² - 4*a*(-dist)
if discriminant < 0: return
t = (-v0 - sqrt(discriminant)) / (2*a)
delta = v0 + t * (-ps->gravity)
delta = delta * delta * 0.0001  ; (0x38d1b717 ≈ 0.0001f)
```

### 3. Modifiers

```
if PMF_DUCKED: delta *= 2.0     ; ducking while landing doubles damage
if waterlevel == 3: return      ; completely underwater, no damage
if waterlevel == 2: delta *= 0.25
if waterlevel == 1: delta *= 0.5
if delta < 1.0: return
```

### 4. Event dispatch (0x7766–0x77a4)

```
if surfaceFlags & SURF_NODAMAGE (checked via pml.groundTrace):
    skip
else:
    if delta > 60: PM_AddEvent(EV_FALL_FAR=12)
    elif delta > 40 AND health > 0: PM_AddEvent(EV_FALL_MEDIUM=11)
    elif delta > 7: PM_AddEvent(EV_FALL_SHORT=10)
    else: PM_AddEvent(PM_FootstepForSurface())
```

Then: `ps->bobCycle = 0`

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x76b1` | Function entry |
| `0x76b2–0x76bb` | pm_flags & 0x8 check for landing animation |
| `0x76c8` | ps->legsTimer = 0x82 (TIMER_LAND=130ms) |
| `0x76cf` | dist = ps->origin[2] − pml.previous_origin[2] |
| `0x76da` | v0 = pml.previous_velocity[2] (0x1328f8) |
| `0x76de` | acc = −float(ps->gravity); gravity at ps+0x30 |
| `0x76e8` | a = acc/2 (divf by 2.0f = 0x40000000) |
| `0x76f7–0x7705` | discriminant = v0² − 4·a·(−dist) |
| `0x7706` | If discriminant < 0: return (no damage) |
| `0x770c–0x7712` | sqrt(discriminant) via trap -0x6b |
| `0x7713–0x771f` | t = (−v0 − sqrt) / (2a) |
| `0x7720–0x7729` | delta = v0 + t×acc (impact velocity) |
| `0x772a` | delta = 0x38d1b717 × delta² (= delta²×0.0001f) |
| `0x7739` | PMF_DUCKED check: delta×=2.0 if ducked |
| `0x7743` | waterlevel (via pm+0xd4) checks |
| `0x7766` | delta ≥ 1.0 check |
| `0x776c` | SURF_NODAMAGE check via pml.groundTrace.surfaceFlags |
| `0x7774` | delta > 60 → EV_FALL_FAR |
| `0x777d–0x778f` | delta > 40 → EV_FALL_MEDIUM (with health check) |
| `0x7790–0x779a` | delta > 7 → EV_FALL_SHORT |
| `0x779b–0x77a4` | else → PM_FootstepForSurface() |
| `0x77ac` | Function return |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x38d1b717` | ≈ 0.0001f | Fall damage multiplier (delta²×0.0001) |
| `0x40000000` | 2.0f | Divisor for a = acc/2 |
| `0x42800000` | 64.0f | Not used here (used in PM_GroundTraceMissed) |
| `0x42700000` | 60.0f | EV_FALL_FAR threshold |
| `0x42200000` | 40.0f | EV_FALL_MEDIUM threshold |
| `0x40e00000` | 7.0f | EV_FALL_SHORT threshold |
| `0x3f800000` | 1.0f | Minimum delta to cause any event |

---

## oDFs implementation notes

Full match. ✓ All constants verified: damage formula (0.0001f), thresholds (7/40/60), ducking multiplier (×2), water reduction (0.25/0.5), TIMER_LAND=130ms.
