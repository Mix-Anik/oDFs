# PM_Friction — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000066e0`  
**Called from:** `0x6c68` (PM_WaterMove), `0x6de6`, `0x6e71` (PM_AirMove), `0x726e` (PM_WalkMove)

---

## Structure

Closely matches stock Q3 PM_Friction. Order of operations:

1. Copy velocity to local vec; if walking, zero z component (ignore slope)
2. `VectorLength(vec)` → speed
3. If speed < 1.0: zero vel[0], vel[1], return
4. drop = 0
5. **Ground friction** (if waterlevel <= 1, walking, not slick, not knockback): `drop += control * pm_friction * frametime`
6. **Water friction** (if waterlevel > 0): see CPM modification below
7. Flying friction (PW_FLIGHT): `drop += speed * pm_flightfriction * frametime`
8. Spectator friction: `drop += speed * pm_spectatorfriction * frametime`
9. `newspeed = max(speed - drop, 0) / speed`; scale all velocity components

---

## CPM modification: water friction

At addresses 0x6750–0x6787:

```c
// stock Q3:
drop += speed * pm_waterfriction * pm->waterlevel * frametime;  // pm_waterfriction = 1.0

// Defrag CPM promode:
if (promode)
    drop += speed * 0.5f * pm->waterlevel * frametime;
else
    drop += speed * pm_waterfriction * pm->waterlevel * frametime;
```

**Promode water friction = 0.5** (half of stock 1.0). Swimming in promode has less drag, so players maintain speed longer underwater.

The constant 0.5 (0x3f000000) is hardcoded in the promode path; it is not a configurable global.

---

## Ground friction (stock Q3, unchanged in Defrag)

```c
if (waterlevel <= 1 && walking && !SURF_SLICK && !PMF_TIME_KNOCKBACK) {
    control = max(speed, pm_stopspeed);
    drop += control * pm_friction * frametime;
}
```

No promode modification to ground friction.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x66e0` | Function entry |
| `0x66e1–0x66f2` | Copy velocity, zero z if walking |
| `0x66f3–0x670c` | VectorLength; early return if speed < 1 |
| `0x670d–0x674f` | Ground friction (walking, not slick, not knockback) |
| `0x6750–0x6756` | Check waterlevel != 0 |
| `0x6757–0x675e` | Check promode (pm_flags & 0x8000) |
| `0x6761–0x6775` | Promode path: water friction = 0.5 |
| `0x6776–0x6787` | Non-promode path: water friction = pm_waterfriction (1.0) |
| `0x678a–0x679x` | Flying/spectator friction, velocity scale |
