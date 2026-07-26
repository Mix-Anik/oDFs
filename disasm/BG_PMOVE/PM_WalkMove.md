# PM_WalkMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0000726e`  
**Called from:** `0x8331` (PmoveSingle, walking path)

---

## Structure

Closely matches stock Q3 PM_WalkMove. Order of operations:

1. **Water check** — if waterlevel > 2 and moving into ground plane, call `PM_WaterMove (0x6c68)` and return
2. **`PM_CheckJump (0x6a5e)`** — if jumped, call `PM_WaterMove` or `PM_AirMove (0x6e71)` and return early
3. **`PM_Friction (0x66e0)`**
4. Read `cmd.forwardmove`, `cmd.rightmove`
5. **`PM_CmdScale (0x686e)`** → `wishspeed`
6. **`PM_SetMovementDir (0x6912)`**
7. Zero `pml.forward[2]`, `pml.right[2]`
8. `PM_ClipVelocity (0x6667)` on `pml.forward` and `pml.right` against ground normal (for slope projection)
9. `VectorNormalize (0x9d12)` on both
10. Build `wishvel` loop (3 components)
11. Normalize wishvel → wishdir + wishspeed; multiply by scale
12. Duck speed clamp
13. **Acceleration (CPM modification — see below)**
14. Gravity for SURF_SLICK / PMF_TIME_KNOCKBACK only
15. `vel = VectorLength(velocity)`
16. `PM_ClipVelocity(velocity, groundTrace.plane.normal, velocity, OVERCLIP)` — clip to ground plane
17. `VectorNormalize(velocity)` + `VectorScale(velocity, vel, velocity)` — preserve speed on slopes
18. If `velocity[0] == 0 && velocity[1] == 0`: return (standing still)
19. **`PM_StepSlideMove (0x8af9)`** with `gravity = 0`

---

## CPM water speed clamp in PM_WalkMove (0x735a–0x73e5)

When `pm->waterlevel > 0`, wishspeed is capped before acceleration. The cap formula is:

```c
waterScale = waterlevel / 3.0;
waterScale = 1.0 - (1.0 - swimScale) * waterScale;
// cap wishspeed at ps->speed * waterScale
```

The `swimScale` base value differs by promode:

| Mode | waterlevel == 1 (wading) | waterlevel > 1 (swimming) |
|------|--------------------------|---------------------------|
| Non-promode | `pm_swimScale` (0.5) | `pm_swimScale` (0.5) |
| CPM promode | `cpm_pm_swimscale_shallow` (0.585f) | `cpm_pm_swimscale_deep` (0.54f) |

Confirmed at `0x736b–0x73e5`: promode reads waterlevel, branches on `!= 1`, selects 0x3f15c28f (0.585) or 0x3f0a3d71 (0.540); non-promode uses `pm_swimScale` from global `0x14c0`.

---

## CPM ground acceleration modification (0x73e6–0x7413)

Acceleration selection:

| Condition | Promode | Non-promode |
|-----------|---------|-------------|
| Normal ground | 15.0 (`cpm_pm_accelerate`) | `pm_accelerate` (10.0, global `0x14c8`) |
| SURF_SLICK or PMF_TIME_KNOCKBACK | 15.0 | `pm_airaccelerate` (1.0, global `0x14cc`) |

Promode always uses 15.0 regardless of surface. Non-promode distinguishes slick/knockback (uses air accel) from normal ground (uses ground accel).

---

## Velocity clip + speed preservation (matches stock Q3)

At `0x7464–0x74a9`:

```c
PM_ClipVelocity(velocity, groundTrace.plane.normal, velocity, OVERCLIP);
VectorNormalize(velocity);
velocity *= vel;   // scale back to original magnitude (per-component)
```

This is identical to stock Q3 and oDFs — preserves speed when going up or down slopes.

---

## Early return on PM_CheckJump

```
0x728e: call PM_CheckJump (0x6a5e)
0x7295: if return == 0 (no jump): continue to friction, acceleration, etc.
0x7296–0x7299: check waterlevel
0x729c: if waterlevel > 1: call PM_WaterMove (0x6c68), jump to return
0x72a2: else: call PM_AirMove (0x6e71), jump to return
```

PM_WalkMove exits immediately after calling PM_AirMove on jump. The friction, ClipVelocity, and step-slide code **only runs when NOT jumping**.

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x726e` | Function entry |
| `0x726f–0x728d` | Water check → PM_WaterMove |
| `0x728e–0x72a6` | PM_CheckJump → PM_AirMove/PM_WaterMove on jump |
| `0x72a7–0x72a9` | PM_Friction |
| `0x72aa–0x72b1` | Read forwardmove, rightmove |
| `0x72bc–0x72c7` | PM_CmdScale |
| `0x72cc–0x72ce` | PM_SetMovementDir |
| `0x72d1–0x72d4` | Zero pml.forward[2], pml.right[2] |
| `0x72d8–0x72f9` | ClipVelocity on pml.forward, pml.right; VectorNormalize both |
| `0x72fa–0x7323` | Build wishvel loop |
| `0x7323–0x7336` | Normalize wishvel, multiply by scale |
| `0x7337–0x7359` | Duck speed clamp |
| `0x735a–0x73e5` | CPM water speed clamp (waterScale/swimScale, see section above) — **not** acceleration selection, corrected from earlier mislabel |
| `0x73e6–0x7413` | Acceleration selection (promode: 15.0 unconditional; non-promode: pm_airaccelerate if slick/knockback else pm_accelerate) |
| `0x7414–0x741e` | The (single) PM_Accelerate call — earlier "second call" note was wrong; there is only one call here, verified against raw disassembly |
| `0x7422–0x744e` | Gravity for slick/knockback; velocity[2] subtraction |
| `0x7464–0x7470` | ClipVelocity velocity against ground normal |
| `0x7477–0x7479` | VectorNormalize velocity |
| `0x747a–0x74a9` | VectorScale velocity × vel (speed preservation, 3 components) |
| `0x74b2–0x74c3` | Check velocity[0]==0 && velocity[1]==0 → early return |
| `0x74c4–0x74c8` | PM_StepSlideMove(gravity=0) call |
| `0x74c9–0x74ca` | Function return |

---

## Function addresses identified within PM_WalkMove call graph

| Address | Likely function |
|---------|----------------|
| `0x6c68` | PM_WaterMove |
| `0x6a5e` | PM_CheckJump |
| `0x6e71` | PM_AirMove |
| `0x66e0` | PM_Friction |
| `0x686e` | PM_CmdScale |
| `0x6912` | PM_SetMovementDir |
| `0x6667` | PM_ClipVelocity |
| `0x9d12` | VectorNormalize |
| `0x67f2` | PM_Accelerate |
| `0x8d63` | VectorLength |
| `0x8af9` | PM_StepSlideMove |
