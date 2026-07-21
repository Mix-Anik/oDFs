# PM_Accelerate — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000067f2`  
**Called from:** PM_WaterMove, PM_FlyMove, PM_AirMove, PM_WalkMove, PM_NoclipMove

---

## Structure

3 args: `wishdir` (vec3_t), `wishspeed` (float), `accel` (float)

```
currentSpeed = DotProduct(velocity, wishdir)
addSpeed = wishspeed - currentSpeed
if addSpeed <= 0: return
accelSpeed = accel * frametime * wishspeed
if accelSpeed > addSpeed: accelSpeed = addSpeed
for i in 0..2:
    velocity[i] += accelSpeed * wishdir[i]
```

Standard Q3 PM_Accelerate. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x67f2` | Function entry |
| `0x67fc–0x681e` | DotProduct(velocity, wishdir) → local14 = currentSpeed |
| `0x681f–0x6825` | addSpeed = wishspeed − currentSpeed; if ≤ 0: return |
| `0x682c–0x6835` | accelSpeed = accel × frametime × wishspeed |
| `0x6836–0x683e` | Cap: if accelSpeed > addSpeed → accelSpeed = addSpeed |
| `0x683f–0x686b` | Loop i=0..2: velocity[i] += accelSpeed × wishdir[i] |
| `0x686c` | Function return |

---

## oDFs implementation notes

Full match. ✓
