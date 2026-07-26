# DFLAG_GRAPPLE_MODE_1/2 — grapple-swing physics mode selector (full decode)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Consumer:** `0x7208`, called from `PmoveSingle` (`0x8331`) when `pm_flags & PMF_GRAPPLE_PULL (0x100)`
is set — i.e. only while the player is actively attached to and being pulled by the
grapple hook. `0x7208` itself was previously flagged in
[PmoveSingle.md](../BG_PMOVE/PmoveSingle.md) as an undecoded "dispatch for waterlevels" —
that label was wrong; it has nothing to do with water.

---

## Why this exists

`func_0x42b5` (`SetGrappleMode`, see [func_0x42b5.md](func_0x42b5.md)) packs the
DeFRaG `df_hook_Type` cvar (0-3) into `ps.stats[STAT_DEFRAG_FLAGS]` bits `0x400`/`0x800`.
This pass found and fully decoded the consumer: **a 4-way dispatcher selecting between
four structurally distinct grapple-swing physics implementations**, active only while
`PMF_GRAPPLE_PULL` is set.

---

## `func_0x7208` — grapple-mode dispatcher

```c
void func_0x7208( void ) {
    int mode = 0;
    if ( ps->stats[STAT_DEFRAG_FLAGS] & DFLAG_GRAPPLE_MODE_1 ) mode += 1;
    if ( ps->stats[STAT_DEFRAG_FLAGS] & DFLAG_GRAPPLE_MODE_2 ) mode += 2;

    switch ( mode ) {
        case 0: func_0x6f7c(); PM_AirMove(); break;
        case 1: func_0x711b(); if (pml.walking) PM_WalkMove(); else PM_AirMove(); break;
        case 2: func_0x71a7(); if (pml.walking) PM_WalkMove(); else PM_AirMove(); break;
        case 3: func_0x704a(); PM_AirMove(); break;
    }
}
```

This exactly reconstructs the same 0-3 encoding `SetGrappleMode` writes — confirming
the two functions are a matched setter/consumer pair with no drift between them.
Note modes 1/2 allow falling through to `PM_WalkMove` if grounded (letting the swing
pull interact with ground friction/acceleration), while modes 0/3 always force
`PM_AirMove` regardless of ground state.

---

## `func_0x7033` — shared haste-aware grapple speed helper

Called by modes 1 and 3 (not 0 or 2):

```c
float func_0x7033( void ) {
    float speed = 800.0f;                    // matches the already-known "800.0 = grapple speed" constant
    if ( pm->ps-><field+0x144> != 0 )          // suspected ps.powerups[PW_HASTE] — not independently confirmed
        speed *= 1.3f;                         // matches the confirmed g_active.c Haste multiplier
    return speed;
}
```

The `1.3f` multiplier and its conditional gate are strongly consistent with
`PW_HASTE` (already confirmed elsewhere: `client->ps.speed *= 1.3` when
`powerups[PW_HASTE]`), but the exact field offset (`ps + 0x144`) was not
independently cross-checked against the confirmed offset table this pass — flagged,
not assumed.

---

## Mode 0 — `func_0x6f7c` (direct velocity snap, flat speed)

**Confirmed to be a byte-for-byte match of oDFs's existing (pre-this-pass)
`PM_GrappleMove`** ([bg_pmove.c:746-764](../../code/game/bg_pmove.c#L746)) — this
mode was already correctly implemented; it just wasn't previously known to be
"mode 0" of a 4-mode family, and there was no dispatcher/cvar selecting it.

```c
void func_0x6f7c( void ) {
    vec3_t probe = {
        ps->grapplePoint[0] + pml.forward[0] * -16.0f,   // constant is 0xC1800000 = -16.0, not -0.25 — verify raw bit pattern, not naive hex negation, when reading disasm negatives
        ps->grapplePoint[1] + pml.forward[1] * -16.0f,
        ps->grapplePoint[2] + pml.up[2] * -16.0f,        // note: uses pml.up[2], not forward[2]
    };
    vec3_t dir = { probe[0]-ps->origin[0], probe[1]-ps->origin[1], probe[2]-ps->origin[2] };
    float dist = VectorLength(dir);
    VectorNormalize(dir);

    float speed = (dist > 100.0f) ? 800.0f : (10.0f * dist);   // no haste-awareness
    VectorScale(dir, speed, ps->velocity);      // DIRECT OVERWRITE, not additive
    pml.groundPlane = qfalse;
}
```

No use of `PM_Accelerate` — every frame, `ps.velocity` is unconditionally
**replaced** (not accumulated) with a vector pointing toward a probe point offset
from the grapple attachment point, scaled to a flat 800 u/s when far (>100 units) or
a distance-proportional 10×dist when close. No Quad/Haste awareness. This is the
crudest of the four modes — likely a "VQ3-style" direct pull, and the one oDFs
already implements as its only grapple mode today.

---

## Mode 3 — `func_0x704a` (direct velocity snap, haste-aware, eye-height-adjusted)

```c
void func_0x704a( void ) {
    float speed = func_0x7033();   // 800 or 1040 (haste-aware)

    vec3_t probe = {
        ps->grapplePoint[0] + pml.forward[0] * -16.0f,
        ps->grapplePoint[1] + pml.forward[1] * -16.0f,
        ps->grapplePoint[2] + pml.up[2] * -16.0f,
    };
    vec3_t dir = {
        probe[0] - ps->origin[0],
        probe[1] - ps->origin[1],
        probe[2] - ps->origin[2] - (float)ps->viewheight - 64.0f,   // extra Z offset vs mode 0!
    };
    float dist = VectorLength(dir);
    VectorNormalize(dir);

    // NOTE: branch direction confirmed against mode 0's identical instruction shape —
    // dist > 100 uses the flat/full speed; dist <= 100 ramps up proportionally.
    float finalSpeed = (dist > 100.0f) ? speed : ((speed/100.0f) * dist);
    VectorScale(dir, finalSpeed, ps->velocity);   // DIRECT OVERWRITE
    pml.groundPlane = qfalse;
}
```

Same direct-overwrite structure as mode 0, but haste-aware, aims at a point
`viewheight + 64` units above the raw grapple point (i.e. accounts for the camera's
eye offset, making the pull feel centered on where the player is looking rather than
their feet/origin), and ramps the pull speed up smoothly from 0 as distance goes
0→100 instead of mode 0's `10×dist` (which has no ceiling relationship to the actual
target speed). Likely a refined variant of mode 0.

---

## Mode 1 — `func_0x711b` (acceleration-based swing, haste-aware)

```c
void func_0x711b( void ) {
    float wishspeed = func_0x7033();   // 800 or 1040 (haste-aware)

    vec3_t dir = { ps->grapplePoint[0]-ps->origin[0], ps->grapplePoint[1]-ps->origin[1], ps->grapplePoint[2]-ps->origin[2] };
    float dist = VectorLength(dir);
    VectorNormalize(dir);

    if ( dist/2.0f >= wishspeed )
        PM_Accelerate( dir, 2.0f*dist, dist*40.0f/wishspeed );   // far: faster, distance-scaled pull
    else
        PM_Accelerate( dir, wishspeed, 13.0f );                   // near: standard pull

    if ( dir[2] > 0.5f && pml.walking ) {
        pml.walking = qfalse;              // being yanked upward breaks ground contact
        PM_ForceLegsAnim( LEGS_JUMP /* 0x12 */ );
    }
    pml.groundPlane = qfalse;
}
```

Qualitatively different from modes 0/3: uses `PM_Accelerate` (`0x67f2`, confirmed) for
a gradual, physics-consistent pull rather than an instant velocity snap, and
explicitly breaks ground contact + forces a jump animation when the swing pulls the
player upward while grounded. This is the most "normal Q3 movement"-feeling of the
four — likely the "CPM-style" smooth swing.

---

## Mode 2 — `func_0x71a7` (elastic/spring pull, un-normalized direction)

```c
void func_0x71a7( void ) {
    vec3_t dir = { ps->grapplePoint[0]-ps->origin[0], ps->grapplePoint[1]-ps->origin[1], ps->grapplePoint[2]-ps->origin[2] };
    // NOTE: dir is NOT normalized here — its magnitude equals the raw distance to the hook

    PM_Accelerate( dir, 1.0f, 5.0f );   // wishdir is non-unit-length!

    if ( dir[2] > 0.5f && pml.walking ) {
        pml.walking = qfalse;
        PM_ForceLegsAnim( LEGS_JUMP /* 0x12 */ );
    }
    pml.groundPlane = qfalse;
}
```

Passing a **non-normalized** `dir` (magnitude = distance to hook) into
`PM_Accelerate` as `wishdir` means the pull's effective strength scales with distance
from the hook — a spring/elastic-like pull that gets stronger the farther the player
is from the attachment point, unlike the other three modes which normalize direction
first. Distinctive enough that this is almost certainly a deliberately different
"feel" (e.g. an "elastic"/"bungee" grapple mode), not an approximation of the others.

---

## Summary table

| Mode | Function | Method | Haste-aware | Ground-break + jump anim | Character |
|------|----------|--------|-------------|---------------------------|-----------|
| 0 | `func_0x6f7c` | Direct velocity snap | No | No | Flat-speed pull toward view-offset probe point (= oDFs's existing `PM_GrappleMove`) |
| 1 | `func_0x711b` | `PM_Accelerate` | Yes | Yes | Smooth physics-based swing (likely "CPM-style") |
| 2 | `func_0x71a7` | `PM_Accelerate`, un-normalized dir | No | Yes | Elastic/spring-like pull, strength scales with distance |
| 3 | `func_0x704a` | Direct velocity snap | Yes | No | Speed-ramped/eye-height-adjusted refinement of mode 0 |

---

## oDFs implementation notes

**Implemented.** oDFs's existing `PM_GrappleMove`
([bg_pmove.c:746-764](../../code/game/bg_pmove.c#L746)) was already a confirmed
byte-for-byte match for mode 0 — no changes were needed to that function's math.
Added this pass:
1. `df_hook_Type` cvar (0-3, registered in `g_main.c`) and `DFLAG_GRAPPLE_MODE_1`/`_2`
   set in `ps.stats[STAT_DEFRAG_FLAGS]` each frame in `g_active.c` (mirroring the
   existing `DFLAG_OBS_DISABLED` pattern).
2. `PM_GrappleModeMove` — the `func_0x7208`-equivalent dispatcher — now replaces the
   previous unconditional `PM_GrappleMove()` call at the `PMF_GRAPPLE_PULL` site in
   `bg_pmove.c`, selecting between mode 0 (existing `PM_GrappleMove`) and new
   `PM_GrappleMove1`/`PM_GrappleMove2`/`PM_GrappleMove3` via `PM_GrappleSpeed` (the
   shared haste-aware speed helper).
3. The haste-check field (`client+0x144` in the QVM) was implemented using
   `ps->powerups[PW_HASTE]` — a reasonable match for the confirmed `1.3×` shape but
   not independently confirmed against the QVM's exact offset; worth re-verifying
   against live behavior if the swing feel is ever reported as off specifically
   under Haste.
