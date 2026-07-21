# PM_StepSlideMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00008af9`  
**Called from:** `0x6e71` (PM_AirMove), `0x726e` (PM_WalkMove)

---

## Structure (matches stock Q3 mostly)

1. Save `start_o` (origin) and `start_v` (velocity)
2. Call PM_SlideMove — if no bumps, return
3. Trace DOWN from `start_o` to check for floor
4. **Step-up check (key CPM modification — see below)**
5. Trace UP by `STEPSIZE (18)` from `start_o`
6. If `allsolid` (ceiling too close): **in promode, zero velocity** then return; else just return
7. Copy step-up endpos as new origin, restore `start_v`, call PM_SlideMove again
8. Trace DOWN by `stepSize` to land on surface
9. If surface hit: `ClipVelocity(velocity, surface_normal, velocity, OVERCLIP)`
   - **In promode:** use internal copy of ClipVelocity (`0x1a0b`) and return immediately (no step sounds)
   - **Non-promode:** use `PM_ClipVelocity (0x6667)`, then play step sounds (EV_STEP_4 etc.)

---

## Key CPM modification: step-up early-return condition

### Stock Q3 behavior

```c
// never step up when you still have up velocity
if ( pm->ps->velocity[2] > 0 && (trace.fraction == 1.0 ||
                                    DotProduct(trace.plane.normal, up) < 0.7)) {
    return;
}
```

When the player has upward velocity **and** there is no floor below (or the floor is too steep),
Q3 refuses to step. This causes high-speed mid-air ramp impacts to simply redirect velocity
without placing the player on the ramp surface.

### Defrag promode override

```
0x8b5b: velocity[2] <= 0  → skip (no issue, step normally)
0x8b5f: trace.fraction == 1.0 → promode check branch
0x8b72: DotProduct(normal, up) >= 0.7 → skip (good floor, step normally)

; velocity[2] > 0 AND (no floor OR steep floor):
0x8b73: if NOT promode  → return (stock Q3 behavior)

; promode:
0x8b87: const 0x43b90000 = 370.0
0x8b88: if ps->velocity[2] > 370.0  → return (too high, skip)   ← checks POST-SlideMove velocity
0x8b8d: load stats[10] = STAT_DOUBLEJUMP_WINDOW
0x8b8f: if stats[10] <= 0  → return (no window, skip)

; promode + velocity[2] in (0, 370] + doublejump window open → continue stepping
```

**Disassembly confirmed (0x8b7d–0x8b81):** Defrag caches `pm->ps` into a local variable immediately before the check and reads `*(pm->ps + 0x28)` = `ps->velocity[2]` — the **post**-PM_SlideMove value, not `start_v[2]`.

**In CPM promode, the step-up is allowed even with upward velocity when:**
1. `velocity[2] > 0` (upward, from ramp-face redirect) — post-SlideMove
2. No walkable floor directly below (player is airborne)
3. `ps->velocity[2] <= 370.0` (not launched too high) — QVM uses post-SlideMove value
4. `ps->stats[STAT_DOUBLEJUMP_WINDOW] > 0` (within 400ms of a promode jump)

### Why this matters (ramp jump mechanic)

The player sequence for a CPM ramp jump:
1. Press jump on flat ground → `stats[10]` set to 400 (doublejump window open)
2. Strafejump to gain speed, airborne
3. Trajectory hits a ramp **face** in mid-air → `PM_SlideMove` clips velocity along ramp → `velocity[2] > 0`
4. Down trace from `start_o` finds no floor (player was airborne)
5. **Stock Q3:** returns early — player bounces off with redirected velocity, no step up; horizontal speed lost
6. **Defrag CPM with window open:** steps onto the ramp surface
7. Next frame: `PM_GroundTrace` detects ramp → `PM_WalkMove` → `PM_CheckJump` fires
8. `velocity[2] > 0` → double-jump path: `velocity[2] += 270` (+100 if stats[10] > 0)

The 370 ceiling prevents the step when the player is in a high jump arc (well above ramp height).
The `stats[10] > 0` gate ensures this only happens within the doublejump window after a jump.

### Note on velocity[2] threshold

oDFs now uses `ps->velocity[2]` (post-SlideMove), matching Defrag exactly. A previous oDFs version used `start_v[2]` (pre-SlideMove) to address ramp amplification at high horizontal speeds, but this was reverted to match Defrag. If ramp-jump issues reappear at high speed, switching back to `start_v[2]` is a documented workaround. Reason for the deviation:

At high horizontal speed (800+ u/s) hitting a shallow ramp (30–45°), PM_SlideMove's ClipVelocity redistributes horizontal momentum into the vertical axis. For example, at 800 u/s horizontal hitting a 30° ramp, velocity[2] after clip becomes ~414 — past the 370 threshold even though the player only had velocity[2] = 270 from a normal jump. Using the post-SlideMove value would incorrectly block the step-up at high speeds.

`start_v[2]` is the actual jump velocity (270 or 370 from a normal doublejump; much higher from a jump pad). Checking it isolates the intent: "was the player launched too high *before* hitting the ramp?" Jump pads still correctly block the step (start_v[2] >> 370). Normal jumps always pass (start_v[2] ≤ 370).

Defrag maps are typically designed with near-vertical ramp faces where the horizontal-to-vertical redirect is minimal, keeping post-SlideMove velocity[2] below 370. In oDFs with shallower geometry, the QVM behavior would reject valid ramp hits.

---

## allsolid path in promode

When the step-up trace is `allsolid` (ceiling too close, can't step):

- **Stock Q3:** just return
- **Defrag promode:** zero `ps->velocity` (all 3 components to 0.0) then return

Relevant addresses: `0x8bd1–0x8c01`. Implemented in oDFs.

---

## Step sound suppression in promode

After the push-down clip in promode (`0x8c94–0x8c98`), Defrag jumps directly to the
function epilogue — skipping the `EV_STEP_4/8/12/16` events and the `"stepped"` debug print.
Non-promode falls through to the step-sound block. Functionally identical in terms of
velocity, but the player hears no step sounds during CPM ramp steps. Implemented in oDFs.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x8af9` | Function entry |
| `0x8b08–0x8b0e` | First PM_SlideMove call |
| `0x8b0f–0x8b14` | Early return if no bumps |
| `0x8b18–0x8b4e` | Down-trace setup (step-down check) |
| `0x8b5b–0x8b92` | Step-up early-return check (CPM modification) |
| `0x8b73–0x8b91` | Promode override: velocity[2] and stats[10] guards |
| `0x8b92–0x8b9f` | Save origin/velocity before step-up |
| `0x8ba3–0x8ba8` | Add STEPSIZE to up-trace end (local64 += 18.0) |
| `0x8bca–0x8bcf` | Trace UP call |
| `0x8bd1–0x8c01` | allsolid path (promode: zero velocity + return) |
| `0x8c1a–0x8c1c` | Second PM_SlideMove (from stepped position) |
| `0x8c2b–0x8c59` | Push-down trace |
| `0x8c65–0x8c98` | Push-down clip: promode → `0x1a0b`, non-promode → `0x6667` |
| `0x8c97–0x8c98` | Promode: jump to return (no step sounds) |
| `0x8cad–0x8ce1` | Non-promode: step sound events |
| `0x8cf1` | Function return |

---

## oDFs implementation

In `code/game/bg_slidemove.c`, `PM_StepSlideMove`:

```c
VectorSet(up, 0, 0, 1);
// never step up when you still have up velocity
if ( pm->ps->velocity[2] > 0 && (trace.fraction == 1.0 ||
                                    DotProduct(trace.plane.normal, up) < 0.7)) {
    // CPM: allow step-up when doublejump window is open and not launched too high.
    // Use start_v[2] (pre-SlideMove) — high horizontal speed hitting a shallow ramp
    // would otherwise amplify post-SlideMove velocity[2] past 370 and falsely block the step.
    if ( !pm->promode ||
         start_v[2] > 370.0f ||
         pm->ps->stats[STAT_DOUBLEJUMP_WINDOW] <= 0 ) {
        return;
    }
}
```

`STAT_DOUBLEJUMP_WINDOW = 10` — defined in `bg_public.h`.

Note: QVM uses `ps->velocity[2]` (post-SlideMove); oDFs uses `start_v[2]` (pre-SlideMove). See "oDFs deviation" section above.
