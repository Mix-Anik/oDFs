# PM_FlyMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006de6`  
**Called from:** PmoveSingle (when `ps->field_0x150 != 0`, i.e., flight powerup active)

---

## Structure

No args. Handles movement while the flight powerup is active.

```
PM_Friction()
scale = PM_CmdScale(&cmd)
if scale == 0:
    wishvel = (0, 0, 0)
else:
    for i in 0..2:
        wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove
    wishvel[2] += upmove * scale
VectorNormalize(wishdir from wishvel)
wishspeed = VectorLength(wishvel) * scale
PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate)   ; pm_flyaccelerate = 8.0f at 0x14d4
VectorMA(origin, frametime, velocity, origin)
```

Standard Q3 PM_FlyMove with 3D wishvel (includes z). No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6de6` | Function entry |
| `0x6de7` | Call PM_Friction (0x66e0) |
| `0x6ded–0x6df2` | Call PM_CmdScale (0x686e) → scale |
| `0x6dfa` | If scale == 0: clear wishvel, jump to move |
| `0x6e06–0x6e3f` | Build wishvel from forward×fmove + right×smove; z += upmove |
| `0x6e3f–0x6e58` | VectorNormalize wishdir, compute wishspeed |
| `0x6e64` | PM_Accelerate with pm_flyaccelerate (0x14d4 = 8.0f) |
| `0x6e6a–0x6dea` | VectorMA: origin += velocity × frametime |
| `0x6de5` | Function return |

---

## oDFs implementation notes

Full match. ✓ No promode checks. pm_flyaccelerate = 8.0f confirmed from global at 0x14d4.
