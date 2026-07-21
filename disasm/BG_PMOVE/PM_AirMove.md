# PM_AirMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006e71`  
**Called from:** `0x726e` (PM_WalkMove, after a jump), `0x1a866` (PmoveSingle, airborne path)

---

## Structure

Matches stock Q3 PM_AirMove closely. Order of operations:

1. `PM_Friction (0x66e0)` — called first (same as stock Q3)
2. Read `cmd.forwardmove` and `cmd.rightmove`
3. `PM_CmdScale (0x686e)` — compute input scale
4. `PM_SetMovementDir (0x6912)` — set animation direction
5. Clear `pml.forward[2]` and `pml.right[2]` to 0 (flatten to horizontal)
6. `VectorNormalize (0x9d12)` on forward and right
7. Build `wishvel` from forward/right × move inputs (loop over 3 components)
8. Copy wishvel → wishdir, normalize, multiply by scale → `wishspeed`
9. **Acceleration logic (CPM modification — see below)**
10. **`pml.groundPlane` clip** — if `pml.groundPlane != 0`, call `PM_ClipVelocity (0x6667)` on velocity against `pml.groundTrace.plane.normal` with OVERCLIP
11. `PM_StepSlideMove (0x8af9)` with `gravity = 1`

---

## CPM acceleration modification

At `0x6ef0`: checks `pm_flags & 0x8000` (promode).

### Non-promode path (`0x6f52–0x6f5c`)
```c
PM_Accelerate(wishdir, wishspeed, pm_airaccelerate);
// pm_airaccelerate loaded from global constant at 0x14cc
```

### Promode path (`0x6ef4–0x6f51`)
```c
float wishspeed2 = wishspeed;
float accel;

// dot of velocity with wishdir:
float dot = dot(ps->velocity, wishdir);
if (dot < 0) accel = 3.0;      // going against wish: faster correction
else         accel = pm_airaccelerate;  // forward: low accel

// movementDir == 2 (pure right) or == 6 (pure left): CPM strafejump accel
if (movementDir == 2 || movementDir == 6) {
    if (wishspeed > 30.0)      // 0x41f00000 = 30.0
        wishspeed = 30.0;
    accel = 70.0;              // 0x428c0000 ≈ 70.0 = cpm_pm_airaccelerate
}

PM_Accelerate(wishdir, wishspeed, accel);
CPM_PM_Aircontrol(0x1a84, wishdir, wishspeed2);
```

Constants:
| Hex constant | Float value | Purpose |
|---|---|---|
| `0x41f00000` | 30.0 | Max wishspeed when strafing (CPM max air wishspeed) |
| `0x428c0000` | 70.0 | Strafe acceleration (cpm_pm_airaccelerate) |
| `0x40200000` | 2.5 | Acceleration when moving against velocity (dot < 0 path at `0x6f1f`) |

Confirmed: `0x40200000 = 2.5` — oDFs was incorrectly using 3.0; fixed to 2.5f.

---

## groundPlane velocity clip

Addresses `0x6f5d–0x6f74`:

```c
if (pml.groundPlane) {  // 0x1328a4
    PM_ClipVelocity(ps->velocity, pml.groundTrace.plane.normal, ps->velocity, OVERCLIP);
}
```

This is **identical** to the stock Q3 and current oDFs code at `bg_pmove.c:707–710`.
No Defrag modification here.

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x6e71` | Function entry |
| `0x6e72–0x6e74` | PM_Friction call |
| `0x6e87–0x6e96` | PM_CmdScale call |
| `0x6e97–0x6e99` | PM_SetMovementDir call |
| `0x6e9a–0x6e9f` | Zero pml.forward[2], pml.right[2] |
| `0x6ea0–0x6ea9` | VectorNormalize forward, right |
| `0x6eaa–0x6ed2` | Build wishvel loop (3 components) |
| `0x6ed3–0x6edf` | Normalize wishvel → wishspeed |
| `0x6eea–0x6ef3` | Check pm_flags & 0x8000 (promode branch) |
| `0x6ef4–0x6f51` | Promode acceleration (CPM aircontrol + strafe accel) |
| `0x6f52–0x6f5c` | Non-promode acceleration (standard airaccelerate) |
| `0x6f5d–0x6f74` | pml.groundPlane ClipVelocity |
| `0x6f75–0x6f79` | PM_StepSlideMove(gravity=1) call |
| `0x6f7b` | Function return |

---

## Function addresses identified within PM_AirMove call graph

| Address | Likely function |
|---------|----------------|
| `0x66e0` | PM_Friction |
| `0x686e` | PM_CmdScale |
| `0x6912` | PM_SetMovementDir |
| `0x9d12` | VectorNormalize |
| `0x67f2` | PM_Accelerate |
| `0x1a84` | CPM_PM_Aircontrol |
| `0x6667` | PM_ClipVelocity |
| `0x8af9` | PM_StepSlideMove |
