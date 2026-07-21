# CPM_PM_Aircontrol — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x1a84`  
**Called from:** `0x6e71` (PM_AirMove, promode path only)

---

## Structure

Implements CPM horizontal air-direction control. Signature: `CPM_PM_Aircontrol(pm, wishdir, wishspeed)`.

| Local | Variable |
|-------|----------|
| `local14` | `zspeed` |
| `localc` | `speed` |
| `local10` | `dot` |
| `local28` | `k` |

Arguments:
- `arg0` (0x40) = `pm`
- `arg1` (0x44) = `wishdir`
- `arg2` (0x48) = `wishspeed`

---

## Step-by-step

### 1. Guard (0x1a85–0x1a9a)

```
local18 = ps->movementDir  (ps + 0x58)
if movementDir != 0 AND movementDir != 4 → return   ; only allow straight or back
if wishspeed == 0.0 → return
```

Equivalent C: `if ( (pm->ps->movementDir && pm->ps->movementDir != 4) || wishspeed == 0.0 ) return;`

Matches the reference source. ✓

### 2. Save and zero velocity[2] (0x1a9b–0x1aa9)

```
local14 = ps->velocity[2]   (ps + 0x28)
ps->velocity[2] = 0
```

✓

### 3. speed = VectorNormalize(velocity) (0x1aaa–0x1ab7)

Calls `0x9d12` with `&ps->velocity[0]` (ps + 0x20). Return value stored as `localc = speed`. ✓

### 4. dot = DotProduct(velocity, wishdir) (0x1ab8–0x1ae3)

Full 3-component dot product (velocity[2] is 0 at this point). Stored as `local10 = dot`. ✓

### 5. ⚠ Extra guard: if dot ≤ 0, skip aircontrol (0x1ae7)

```
0x1ae7  lef  dot, 0.0 → jump to 0x1b2d   ; skip to second loop if dot ≤ 0
```

**This guard is NOT present in the reference source.** When the normalised horizontal velocity is not aligned toward `wishdir` (dot ≤ 0), the disassembly skips the `k` calculation, the first velocity loop, and the second `VectorNormalize` call entirely — jumping straight to `velocity[i] *= speed`.

Effect: aircontrol has **no effect** when the player's velocity direction is perpendicular or away from the desired direction. The reference source would still compute `k = 4800 * dot² * frametime` (positive even for negative dot) and add `wishdir * k` to velocity.

### 6. k = 4800 × dot² × frametime (0x1ae8–0x1af3) *(dot > 0 path)*

```
0x1ae9  const 0x45960000   ; 4800.0f
k = 4800.0 * dot * dot * pml.frametime   (pml global: 0x132898)
```

`0x45960000` = 4800.0f (verified). ✓

### 7. First loop — unrolled i = 0, 1 (0x1af4–0x1b23)

```
velocity[0] = velocity[0]*speed + wishdir[0]*k
velocity[1] = velocity[1]*speed + wishdir[1]*k
```

✓

### 8. VectorNormalize(velocity) (0x1b24–0x1b2c)

Second call to `0x9d12`. Return value discarded (`pop`). ✓

### 9. Second loop + restore zspeed (0x1b2d–0x1b54) *(both paths merge here)*

```
velocity[0] *= speed
velocity[1] *= speed
velocity[2] = zspeed   (local14)
```

✓

---

## Corrected C implementation

```c
static void CPM_PM_Aircontrol( pmove_t *pm, vec3_t wishdir, float wishspeed ) {
    float zspeed, speed, dot, k;
    int   i;

    if ( (pm->ps->movementDir && pm->ps->movementDir != 4) || wishspeed == 0.0 )
        return;

    zspeed = pm->ps->velocity[2];
    pm->ps->velocity[2] = 0;
    speed = VectorNormalize( pm->ps->velocity );

    dot = DotProduct( pm->ps->velocity, wishdir );
    if ( dot > 0 ) {                                   // DeFRaG: skip when not aligned
        k = 4800 * dot * dot * pml.frametime;
        for ( i = 0; i < 2; i++ )
            pm->ps->velocity[i] = pm->ps->velocity[i] * speed + wishdir[i] * k;
        VectorNormalize( pm->ps->velocity );
    }

    for ( i = 0; i < 2; i++ )
        pm->ps->velocity[i] *= speed;

    pm->ps->velocity[2] = zspeed;
}
```

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x1a84` | Function entry |
| `0x1a85–0x1a9a` | movementDir / wishspeed guard |
| `0x1a9b–0x1aa9` | Save zspeed, zero velocity[2] |
| `0x1aaa–0x1ab7` | VectorNormalize (first call) |
| `0x1ab8–0x1ae3` | DotProduct |
| `0x1ae7` | `lef dot, 0.0` — dot ≤ 0 early-skip guard (DeFRaG addition) |
| `0x1ae8–0x1af3` | k = 4800·dot²·frametime |
| `0x1af4–0x1b23` | First velocity loop (unrolled i=0,1) |
| `0x1b24–0x1b2c` | VectorNormalize (second call) |
| `0x1b2d–0x1b54` | Second velocity loop + restore velocity[2] |
| `0x1b55–0x1b56` | return |
