# PM_WaterMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006c68`  
**Called from:** `0x726e` (PM_WalkMove), `0x726e` (PM_WalkMove after jump), `0x8331` (PmoveSingle)

---

## Structure

Mostly matches stock Q3 PM_WaterMove. Order of operations:

1. Call `0x6b40` (PM_CheckWaterJump) — if non-zero, call PM_GrappleMove and return
2. `PM_Friction (0x66e0)`
3. `PM_CmdScale` → scale
4. Build `wishvel`: if scale == 0, set (0, 0, -60) to sink; else forward/right × cmd moves + upmove
5. `VectorNormalize(wishvel)` → wishdir, wishspeed
6. **Wishspeed cap + acceleration** (CPM modification — see below)
7. If `pml.groundPlane` and velocity going into ground: ClipVelocity + speed-preserve (same as stock Q3)
8. `PM_SlideMove(qfalse)`

---

## CPM modification: wishspeed cap and acceleration

At addresses 0x6cee–0x6d5e:

**Stock Q3:**
```c
// cap at pm_swimScale (0.5) * ps->speed
if (wishspeed > ps->speed * pm_swimScale)
    wishspeed = ps->speed * pm_swimScale;

PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);  // accel = 4.0
```

**Defrag CPM promode:**
```c
if (promode) {
    // Different cap depending on depth
    float cap = (waterlevel == 1) ? 0.53999f : 0.58501f;
    if (wishspeed > ps->speed * cap)
        wishspeed = ps->speed * cap;

    PM_Accelerate(wishdir, wishspeed, 5.0f);  // higher accel than stock
} else {
    if (wishspeed > ps->speed * pm_swimScale)  // 0.5
        wishspeed = ps->speed * pm_swimScale;

    PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);  // 4.0
}
```

### Constants

| Constant | Value | Context |
|----------|-------|---------|
| `cpm_pm_swimscale_shallow` | 0.585f | waterlevel == 1 (wading) — higher cap, less resistance |
| `cpm_pm_swimscale_deep`    | 0.54f  | waterlevel > 1 (swimming) — lower cap, more resistance |
| `cpm_pm_wateraccelerate`   | 5.0f     | CPM water acceleration (stock: 4.0) |

Promode allows faster wading (0.585 vs 0.5 cap), slightly restricted swimming (0.540 vs 0.5 cap), and higher responsiveness (accel 5.0 vs 4.0).

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6c68` | Function entry |
| `0x6c69–0x6c75` | PM_CheckWaterJump; if true → PM_GrappleMove, return |
| `0x6c76–0x6c78` | PM_Friction call |
| `0x6c79–0x6c85` | PM_CmdScale |
| `0x6c86–0x6ce0` | Build wishvel; add upmove; VectorNormalize → wishspeed |
| `0x6cee–0x6cf7` | Promode check (pm_flags & 0x8000) |
| `0x6cf8–0x6d17` | Promode: cap at 0.585 × speed if waterlevel != 1 |
| `0x6d18–0x6d2e` | Promode: cap at 0.540 × speed if waterlevel == 1 |
| `0x6d2f–0x6d3a` | Promode: PM_Accelerate(wishdir, wishspeed, 5.0) |
| `0x6d3b–0x6d5e` | Non-promode: cap at pm_swimScale (0.5), PM_Accelerate(4.0) |
| `0x6d5f–0x6ddf` | Ground plane clip + speed preserve |
| `0x6de0–0x6de4` | PM_SlideMove(qfalse) |
