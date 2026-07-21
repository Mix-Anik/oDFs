# PM_SlideMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00008709`  
**Called from:** PM_StepSlideMove, PM_WaterJumpMove, and the DeFRaG grapple-mode functions

---

## Structure

1 arg: `gravity` (qboolean — whether to apply gravity during the slide)

Large function spanning `0x8709–0x8af8` (multiple return points). Implements the core iterative plane-clipping movement used by all major movement paths.

### Algorithm

```
Save velocity to primal_velocity
If gravity:
    velocity[2] -= gravity * frametime * 0.5   ; pre-apply half-frame gravity

Loop up to 4 bumps:
    Move end = origin + velocity * frametime
    trace(origin, mins, maxs, end, ...)
    if trace.fraction == 1.0:
        origin = end
        break   ; moved freely, no clip needed
    Move to impact point (trace.fraction of the way)
    Add trace.plane to plane list
    Clip velocity against impact plane (PM_ClipVelocity)
    Handle corner-case plane interactions (velocity reversal, etc.)

If gravity:
    velocity[2] -= gravity * frametime * 0.5   ; post-apply remaining half-frame gravity

Return qtrue if blocked, qfalse if unobstructed
```

No CPM modifications. Standard Q3 PM_SlideMove.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x8709` | Function entry; `enter 0x158` (large stack frame) |
| `0x870a` | numplanes = 4 (max bump planes) |
| `0x870d–0x8713` | Save velocity to primal_velocity |
| `0x8716` | If gravity == 0: skip gravity pre-application |
| `0x8717–0x875b` | Gravity pre-apply: velocity[2] -= gravity × frametime × 0.5 |
| `0x875c–0x875f` | Trace call setup begins |
| `0x8808` | Return point (unobstructed) |
| `0x884e` | Return point (bumped/clipped) |
| `0x8a93` | Return point |
| `0x8af6`, `0x8af8` | Final return points |

---

## oDFs implementation notes

Full match — no `pm_flags & 0x8000` (promode) checks anywhere in the function range. ✓
