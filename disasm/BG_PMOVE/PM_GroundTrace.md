# PM_GroundTrace — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0000792e`  
**Called from:** `0x8331` (PmoveSingle) — twice per frame: before and after the movement dispatch

---

## Structure

Closely matches stock Q3 PM_GroundTrace. Order of operations:

1. Copy ps->origin into local point; subtract 0.25 from z → down trace endpoint
2. Call trace function (via pm->trace callback at pm + 0xe4)
3. block_copy trace result → `pml.groundTrace` at global `0x1328a8`
4. If `trace.allsolid`: call `PM_CorrectAllSolid (0x77ae)`; if it returns non-zero → return
5. If `trace.fraction == 1.0`: call `PM_GroundTraceMissed (0x7895)`, set `pml.groundPlane = 0`, `pml.walking = 0`, return
6. **Kickoff check** (see below)
7. **Steep slope check**: if `trace.plane.normal[2] < 0.7 (MIN_WALK_NORMAL)`: set `groundEntityNum = ENTITYNUM_NONE`, `pml.groundPlane = 1`, `pml.walking = 0`, return
8. Set `pml.groundPlane = 1`, `pml.walking = 1`
9. **Landing detection** (see below)
10. **OBS kill** (see below)
11. Set `ps->groundEntityNum` from trace
12. Call `0x656c` (PM_AddTouchEnt or similar)

---

## Kickoff check (addresses 0x79a6–0x79c2)

```c
// velocity[2] (ps+0x28) stored in local74
if (local74 <= 0.0f) goto no_kickoff;

// dot product of velocity vs. groundTrace.plane.normal
float dot = velocity[0]*normal[0] + velocity[1]*normal[1] + velocity[2]*normal[2];
if (dot <= 10.0f) goto no_kickoff;  // 10.0 = 0x41200000

// kickoff: player is being thrown off ground
ps->groundEntityNum = ENTITYNUM_NONE;
pml.groundPlane = 0;
pml.walking = 0;
return;
```

Identical to stock Q3.

---

## Landing detection (addresses 0x7a62–0x7ab8)

When `ps->groundEntityNum == ENTITYNUM_NONE` (previous frame was airborne) and
the player is now on ground, Defrag runs landing logic:

1. Prints `"%i:Land"` if debugLevel set
2. Calls `0x76b1` (PM_CrashLand — fall damage calculation)
3. Check: if `(pml.groundTrace.surfaceFlags & 0x80000)` is clear AND `(ps->stats[12] & 0x200)` is clear: skip ClipVelocity
4. Otherwise: call `PM_ClipVelocity(ps->velocity, groundTrace.plane.normal, ps->velocity, OVERCLIP)` — clips landing velocity against ground normal
5. Check `pml.previous_velocity[2]` (global `0x1328f8`): if >= ~0.0 (not falling fast enough): skip pm_time landing stun
6. Otherwise: set `ps->pm_flags |= 0x20` and `ps->pm_time = 0xfa` (250ms landing stun)

The `surfaceFlags & 0x80000` and `stats[12] & 0x200` conditions gate both the landing ClipVelocity AND the OBS kill (see below). They appear to be two independent mechanisms to enable this behavior on a given surface/player state.

---

## OBS kill (addresses 0x7ac3–0x7adf)

```
; Condition A: ps->stats[12] & 0x200
if ((ps->stats[12] & 0x200) != 0):
    goto check_normal  // condition met, check flat surface

; Condition B: surfaceFlags & 0x80000 (SURF_NOOB — per-surface OBS disable)
if ((pml.groundTrace.surfaceFlags & 0x80000) == 0):
    goto end           // neither condition met, skip OBS kill

check_normal:
; Only kill OBS on exactly flat surface
if (trace.plane.normal[2] != 1.0f):
    goto end           // not flat, skip

; OBS kill
ps->velocity[2] = 0.0f

end:
```

**In plain C:**
```c
bool obs_condition = (ps->stats[12] & 0x200) || (pml.groundTrace.surfaceFlags & 0x80000);
if (obs_condition && trace.plane.normal[2] == 1.0f) {
    ps->velocity[2] = 0.0f;
}
```

---

## OBS kill — mechanism analysis

### Surface flag 0x80000 (SURF_NOOB)

A Defrag-specific custom surface flag (not defined in stock Q3 surfacetypes.h). When
applied to a brush face in a Defrag map, it disables overbounce on that specific surface
regardless of the `df_ob_KillOBs` cvar. Map authors can mark individual surfaces as
no-OB surfaces using this flag in their shader or surface properties.

This flag is also checked in the landing ClipVelocity path — surfaces with SURF_NOOB
get a landing velocity clip that regular surfaces do not.

### ps->stats[12] & 0x200 — global OBS disable flag

When `df_ob_KillOBs` is set (globally disabling OBS), Defrag stores this state in
`ps->stats[12]` bit 0x200 rather than as a per-frame field in the pm struct (like
oDFs uses `pm->obs_disabled`).

**Where this bit is set:** Not located in this disassembly session. It is presumably
written by server-side code before calling Pmove — likely in the G_RunClient equivalent,
reading the `df_ob_KillOBs` cvar or a cached global derived from it.

**Hackability concern:** Because this bit lives in `playerState_t` (which is part of
the game state snapshot sent to clients), it could theoretically be inspected by
clients. However, since the SERVER writes it each frame, a client cannot override it —
only read it. oDFs's approach (storing it in `pm->obs_disabled`, a field in the pmove
struct that never enters the network snapshot) is cleaner from an architecture
standpoint: configuration is strictly server-side and has no representation in the
player state visible to clients.

### normal[2] == 1.0f (exact flat check)

Defrag checks `normal[2] == 1.0f` **exactly**, not approximately. Overbounce only
occurs on perfectly flat horizontal surfaces (where ClipVelocity with OVERCLIP > 1
produces a tiny upward bounce). On any sloped surface, normal[2] < 1.0 and the
overbounce condition cannot arise, so there is nothing to kill.

**oDFs deviation:** Our fix uses `normal[2] > 0.99f` (approximately flat) rather than
exactly 1.0. This is functionally equivalent for overbounce prevention since OBS only
occurs on truly flat ground. The `> 0.99f` threshold was a conservative choice; using
`== 1.0f` would also be safe but relies on float exact equality.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x792e` | Function entry |
| `0x792f–0x7948` | Set up trace endpoint: point = origin, point[2] -= 0.25 |
| `0x7949–0x7976` | Call trace; block_copy result to pml.groundTrace (0x1328a8) |
| `0x797b–0x797e` | allsolid check → PM_CorrectAllSolid (0x77ae) |
| `0x798b–0x7999` | fraction == 1.0 → PM_GroundTraceMissed (0x7895), set airborne |
| `0x799a–0x79c2` | Kickoff check: velocity[2] > 0 AND dot > 10.0 |
| `0x7a13–0x7a16` | normal[2] < MIN_WALK_NORMAL (0.3f = 0x3f333333): steep slope |
| `0x7a26–0x7a34` | Steep: set groundEntityNum = ENTITYNUM_NONE, groundPlane=1, walking=0 |
| `0x7a35–0x7a3a` | Normal ground: set pml.groundPlane = 1, pml.walking = 1 |
| `0x7a62–0x7ab8` | Landing detection (airborne→grounded transition) |
| `0x7a9e–0x7aa1` | previous_velocity[2] threshold for landing stun |
| `0x7ab8–0x7ac2` | Set ps->groundEntityNum; local78 = 0 |
| `0x7ac3–0x7ad4` | OBS kill conditions: stats[12] & 0x200, surfaceFlags & 0x80000 |
| `0x7ad5–0x7adf` | OBS kill: if normal[2] == 1.0f → ps->velocity[2] = 0.0f |
| `0x7ae0–0x7ae5` | Call 0x656c (PM_AddTouchEnt or equivalent) |
| `0x7ae6` | Function return |

---

## MIN_WALK_NORMAL discrepancy

At `0x7a13–0x7a16`: the steep slope check compares `trace.plane.normal[2]` against
`0x3f333333` which is **0.7** — matching stock Q3's `MIN_WALK_NORMAL = 0.7`.

However, the constant used for the kickoff check at `0x79a9` (lef threshold) is
`0x3f333333` as well. This is identical to stock Q3 behavior.

---

## Landing stun threshold

At `0x7aa0`: `const -0x3cb80000` = `0xc3480000` as IEEE 754 = **-200.0f**.
Check is `gef previous_velocity[2], -200.0 → skip`, i.e. stun only if `previous_velocity[2] < -200`. Matches oDFs exactly. ✓

---

## oDFs implementation notes

OBS kill is in PM_GroundTrace (same location as Defrag). SURF_NOOB is implemented.
Flat surface check uses `== 1.0f` exactly, matching Defrag. ✓

The only architectural difference: oDFs uses `pm->obs_disabled` (never networked) as the global kill
trigger instead of `ps->stats[12] & 0x200` (playerState_t field). Effect is equivalent.

**Landing ClipVelocity** (Defrag 0x7a8a–0x7a9d): when OBS is suppressed on landing, Defrag calls
`PM_ClipVelocity(velocity, groundNormal, velocity, OVERCLIP)` before the landing stun check.
This ensures landing velocity has no component going into the surface on SURF_NOOB/no-OBS surfaces.
Added to oDFs `bg_pmove.c` PM_GroundTrace, between `PM_CrashLand()` and the stun threshold check.

**Verified match (all):** ✓
