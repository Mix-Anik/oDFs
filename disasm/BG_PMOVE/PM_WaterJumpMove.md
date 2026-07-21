# PM_WaterJumpMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006c27`  
**Called from:** PmoveSingle (when `pm_flags & PMF_TIME_WATERJUMP (0x100)`)

**Identification note:** Early analysis labeled this as "PM_GrappleMove" because it occupies the same dispatch slot as Q3's grapple function. It is actually PM_WaterJumpMove — called via PMF_TIME_WATERJUMP (0x100), not PMF_GRAPPLE_PULL (0x800).

---

## Structure

No args. Propels the player through a water jump (no player control, falls under gravity).

```
PM_StepSlideMove(gravity=true)   ; move with gravity, slide against walls
ps->velocity[2] -= float(ps->gravity) * pml.frametime
if ps->velocity[2] < 0:
    ps->pm_flags &= ~PMF_ALL_TIMES   ; cancel water jump once falling
    ps->pm_time = 0
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6c27` | Function entry |
| `0x6c28–0x6c2c` | Call PM_StepSlideMove (0x8af9) with arg=1 (gravity) |
| `0x6c2d–0x6c37` | localc = ps; local10 = &velocity[2] (ps+0x28) |
| `0x6c38–0x6c47` | velocity[2] -= float(ps->gravity) × pml.frametime |
| `0x6c4f` | If velocity[2] >= 0: return |
| `0x6c50–0x6c65` | velocity[2] < 0: pm_flags &= ~PMF_ALL_TIMES (~0x160) |
| `0x6c64` | ps->pm_time = 0 |
| `0x6c66` | Function return |

---

## oDFs implementation notes

Full match. ✓ Identical to oDFs PM_WaterJumpMove: StepSlide(true) → gravity → if falling: cancel timers.
