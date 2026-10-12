# cgame overbounce predictor (`df_ob_Fast`, `df_ob_AllSlopes`) — decode notes

Entry chain (see [cgame_cvar_consumers.md](cgame_cvar_consumers.md)): `0xb21d` (17-slot loop, draw pass) → `0xb72e` → `0xb6e9` (`df_ob_Fast` switch) → `0xb583` (fast) **or** `0xb66f` (full).
All addresses are `cgame.dis`. The signatures below are read from the bytecode; the *purpose* label ("overbounce prediction") is an inference from the cvar names and the maths.

## `0xb583` — fast closed-form solver (**numerics decoded**)
Args `(z0, vz, zPlane, dt, g, &err, ?)` where `dt` is a frame time in seconds, `g` the gravity (800), `z0`/`vz` the start height and vertical velocity, `zPlane` the surface height. It finds the
integer frame count `n` after which free-fall with **per-frame integer-rounded gravity** reaches the plane:
```c
gdt   = g * dt;                       gdtR = (int)(gdt + 0.5f);   frac = gdt - gdtR;       // Pmove truncates the per-frame velocity change
h     = z0 - zPlane;                  vdt  = vz * dt;
disc  = vdt*vdt + 2.0f * gdtR * h * dt;           // sqrt via trap
n     = floor( (vdt + sqrt(disc)) / (gdtR * dt) ) - 1.0f;   // initial estimate, then walk forward (<= 100 iterations, local3c = 100)
for ( k = 1; k <= 100; k++, n += 1.0f ) {
    zn  = z0 + vdt*n - 0.5f*frac*n*dt - 0.5f*gdtR*n*n*dt;   // position after n discrete frames  (constants 0.5f = 0x3f000000)
    err = zn - zPlane;                 *errOut = err;
    if ( !(err < THRESH /* global float at 0x16cc */) ) { result = 0xb55c( Q_ftol(n), vz, ... ); break; }   // hit: build the result for frame n
}
return 0xb2ea( result, k, z0, vz, zn, err );        // store/return the prediction record
```
(`0xb55c`/`0xb2ea`/threshold globals are decoded in the `0xb66f` section below.)

## `0xb66f` — the "full" variant (**DECODED**, `df_ob_Fast == 0`)
Same signature as `0xb583`: `(z0, vz, zPlane, dt, g, &err, &prevErr)` (the `a5`/`a6` outputs are written inside the loop; each arg is first self-assigned `arg = *arg` by the compiler's by-value marshalling, ignore). Instead of a closed form it **simulates frame by frame**:
```c
z = z0; v = vz; result = -1; prev = 0;
for ( i = 1; i <= 10000; i++ ) {                  // local30 = 0x2710
    *prevErr = prev;                              // *a6 = previous frame's distance (initially 0)
    prev = err = z - zPlane;   *errOut = err;     // *a5 = current distance above the plane
    if ( err < HI /* data 0x16cc = 0x3e800000 = 0.25f */ ) {
        result = 0xb55c( err, i, vz );            // classify, then stop
        break;
    }
    0xb50a( &z, &v, dt, g, 1 );                   // advance exactly one frame
}
0xb2ea( result, i, z0, vz, z, err );              // debug print (below)
return result;
```
- **`0xb55c(err, i, vz)` — classifier (decoded):** `LO = data 0x16c8 = 0x3727c5ac = 1e-5f`, `HI = 0.25f`. Returns `0` unless `LO < err < HI`; then `1` if `i == 1 && vz != 0.0f` ("now": the overbounce happens on the very next frame), else `2` ("true", at a later frame).
  Results: `0` = no OB, `1` = OB now, `2` = OB at frame `i`. (`0xb583` calls the same classifier.)
- **`0xb50a(pos*, vel*, dt, g, n)` — one-frame step (decoded), repeated `n` times:** `gdt = (float)g * dt; *pos += (0.5f * (2*v - gdt)) * dt` (trapezoid: `pos += dt*(v - gdt/2)`); then `*vel = floor( v - gdt + 0.5f )` via cgame trap `-0x6c` (= `CG_FLOOR`; in the **cgame** trap enum SQRT=106, FLOOR=107), i.e. the vertical velocity is rounded to an integer every frame like Pmove's snapped velocity.
  So the full variant keeps gravity un-rounded and rounds velocity per frame, whereas the fast variant `0xb583` uses an integer-rounded per-frame gravity in a closed form — they approximate the same thing.
- **`0xb2ea` — result recorder/debug print (decoded):** when global `0x12a1bc == 0x384` (900; the meaning of that global is **not identified**) it prints, per `result`: `0` -> `"^3OB ( %5i ) = false vz = %9.3f   oz = %8.3f"` + `"^3                  distZ = %9.3f   z1 = %8.3f"`;
  `1` -> `"^1OB ( %5i ) = now   vz = %9.3f   oz = %8.3f"` + the `^1` distZ line; `2` -> `"^2OB ( %5i ) = true  v0 = %9.3f   z0 = %8.3f"` + the `^2` distZ line. (`%5i` = frame count `i`, `vz`/`v0` = start velocity, `oz`/`z0` = start height, `distZ` = final `err`, `z1` = final height.) Returns nothing used.

## `0xb72e` — candidate classification wrapper (492 lines, **partially read**)
`dt = 0.001f * *(int*)0x12b94` (frame time ms → s). It copies `arg1`, `arg2`, `arg4` into locals, computes `fabs(arg1)`, `fabs(arg3)` and `fabs(|arg1| - |arg3|)` and the same for the second pair
(`fabs` = `0x2f6c7`), compares them (`eq`/`ne` on `arg0` and float compares against `0.0f`), and then calls `0xb6e9` (hence `0xb583`/`0xb66f`) once or twice with the seven-argument form
(`Arg[0x18] = 0x14` passed to an intermediate call) and picks / records the better candidate. Its result is stored by `0xb21d` into the per-slot arrays `0x129b88` (result), `0x129c10`, `0x129bcc`.
Exact branch conditions were not reconstructed.

## oDFs
Nothing ported: oDFs's cgame has neither the cvars nor this feature. For prediction parity of *actual* overbounces oDFs relies on the shared `Pmove` code and `pm.obs_disabled`
(see [../G_COMBAT/interference.md](../G_COMBAT/interference.md) for the other networked flag); this predictor is a separate visual/analysis tool.
