# PM_DeadMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000074cb`  
**Called from:** PmoveSingle (only when `pm_type == PM_DEAD (3)`, after PM_GroundTrace)

**Identification note:** This was previously misidentified as "PM_DropTimers (speed/timer variant)". It is the dead-player velocity decay function.

---

## Structure

No args. Decays dead-player velocity while airborne.

```
if pml.walking != 0: return   ; only apply when airborne (DeFRaG)
                               ; oDFs had !pml.walking (inverted — was applying when on ground)

speed = VectorLength(ps->velocity)
newspeed = speed − 20.0f
if newspeed <= 0:
    ps->velocity = (0, 0, 0)
    return
VectorNormalize(ps->velocity)
ps->velocity *= newspeed
```

The subtraction of 20 units per frame is a fixed amount (not frametime-scaled). Since this only runs when `pm_type == PM_DEAD` and when airborne, it decays dead-body momentum.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x74cb` | Function entry |
| `0x74cc` | Load pml.walking (0x1328a0) |
| `0x74cf` | If walking != 0 (`ne`): return (only runs when airborne) |
| `0x74d2–0x74db` | Call VectorLength (0x8d63) on ps->velocity (ps+0x20) |
| `0x74dc` | localc = speed |
| `0x74e3` | Subtract 20.0f (0x41a00000) |
| `0x74e8` | If (speed − 20) > 0: jump to normalize path |
| `0x74ea–0x7507` | speed ≤ 20: zero out velocity (block_copy from 0x1524 = vec3_origin) |
| `0x7509–0x7511` | VectorNormalize (0x9d12) |
| `0x7512–0x7541` | velocity[0..1] *= newspeed |
| `0x7542` | Function return |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x41a00000` | 20.0f | Speed subtracted per frame |
| `0x1524` | | Address of vec3_origin global |

---

## oDFs implementation notes

**Fixed.** oDFs originally had `if (!pml.walking) return` (skip when airborne), which is the opposite of DeFRaG. Corrected to `if (pml.walking) return` so dead-player speed decays only while airborne, matching DeFRaG. ✓
