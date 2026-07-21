# Pmove / PmoveSingle — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

---

## Corrected address mapping

Earlier analysis labeled `0x1a866` as PmoveSingle. This is incorrect. The correct layout is:

| Address | Role |
|---------|------|
| `0x1a866` | **Pmove** — outer function, called from G_RunClient. Reads arg0->field_0x204, clamps moves, calls func_0x2575 (promode flag), enforces pmove_msec. Calls `0x8653` in a subdivision loop. |
| `0x8653`  | **Pmove subdivision loop** — splits large frame times into sub-frames, calls PmoveSingle (0x8331) per iteration. After each PmoveSingle call, if `PMF_JUMP_HELD` is set, forces `cmd.upmove = 20` for the next sub-frame. |
| `0x8331`  | **PmoveSingle** — the per-sub-frame movement function. Sets `pm = arg0`, calls PM_GroundTrace, dispatches to PM_WalkMove/PM_AirMove/etc., calls PM_DropTimers, updates STAT_DOUBLEJUMP_WINDOW, runs post-movement PM_GroundTrace and snap. |

**Called from:**
- `0x1a866` (Pmove): called from `0x1a394` and indirectly from G_RunClient chain
- `0x8653` (subdivision loop): called from `0x1a394`, `0x1a866`
- `0x8331` (PmoveSingle): called from `0x86ed` inside `0x8653`

---

## 0x1a866 — Pmove (outer)

**Frame size:** `0x160` bytes of locals

Immediately after entry, Pmove:

1. Loads `arg0->field_0x204` — a pointer into the pmove/game state
2. Checks `pm_type == 2` (PM_DEAD) — if dead, jumps to `0x1ac8d` skipping most logic
3. Clamps `forwardmove`, `rightmove`, `upmove` to the range `[-127, 127]`
4. **Calls `func_0x2575`** (see [[func_0x2575]]) — sets/clears `pm_flags & 0x8000` based on `df_promode`
5. Clamps `serverTime` delta against a stored value at `global_0x12a310 + 0xc8`
6. Enforces `pmove_msec` limits (strings `"pmove_msec"`, `"8"`, `"33"` visible in disassembly)
7. Enters subdivision loop, calling `0x8653`

---

## 0x8653 — Pmove subdivision loop

**Called from:** `0x1a394`, `0x1a866`

Splits the frame time into sub-frames (max 66ms or pmove_msec per sub-frame). Per sub-frame:

1. Sets `cmd.serverTime = ps->commandTime + msec`
2. Calls PmoveSingle (`0x8331`)
3. After PmoveSingle: if `pm_flags & PMF_JUMP_HELD (0x2)`: sets `cmd.upmove = 20`

The `upmove = 20` override ensures that within the subdivision, JUMP_HELD state propagates correctly across sub-frames.

**Key addresses:**
| Address | Description |
|---------|-------------|
| `0x8653` | Function entry |
| `0x86ea–0x86ee` | Call PmoveSingle (`0x8331`) |
| `0x86f0–0x86ff` | Check PMF_JUMP_HELD → set upmove = 20 |
| `0x8700–0x8706` | Loop condition check |
| `0x8707–0x8708` | Function return |

---

## 0x8331 — PmoveSingle

**Frame size:** `0x58` bytes of locals

The per-sub-frame movement function. Key operations in order:

1. Stores `pm = arg0` into global `0x132900`
2. Calls `0x4152` (likely `PM_UpdateViewAngles`)
3. Clears `numtouch`, `watertype`, `waterlevel` fields
4. Computes PMSC/msec, saves previous origin/velocity
5. Calls `0x828d` (likely `PM_SetWaterLevel`)
6. Calls `0xa186` (likely `PM_CheckDuck`)
7. Calls **PM_GroundTrace (`0x792e`)** — first call, before movement
8. Calls **PM_DropTimers (`0x74cb`)** if pm_type != PM_FREEZE
9. **STAT_DOUBLEJUMP_WINDOW countdown** (at `0x85c7`): if promode and stats[10] > 0, decrement by pml.msec; reset stats[11] to 0 if set
10. Movement dispatch (see below)
11. Calls `0x81de` (likely `PM_Animate`)
12. Calls **PM_GroundTrace (`0x792e`)** — second call, post-movement
13. Calls `0x7ae8` (PM_SetWaterLevel)
14. Calls `0x7fb8`, `0x7f99`, `0x7d06`, `0x7e78` (PM_Weapon, PM_TorsoAnimation, PM_Footsteps, PM_WaterEvents — order uncertain)
15. Calls `trap_SnapVector` on ps->velocity

### Movement dispatch (0x85f0–0x8632)

```
if (ps->field_0x150 != 0):  call 0x6de6 (PM_FlyMove/PM_WaterJumpMove), return
if (pm_flags & 0x800):       call 0x7208 (dispatch for waterlevels), return
if (pm_flags & 0x100):       call 0x6c27 (PM_GrappleMove), return
if (waterlevel > 1):         call 0x6c68 (PM_WaterMove), return
if (pml.walking):            call PM_WalkMove (0x726e)
else:                        call PM_AirMove (0x6e71)
```

### Key addresses

| Address | Description |
|---------|-------------|
| `0x8331` | Function entry |
| `0x8335–0x8337` | Call 0x4152 (PM_UpdateViewAngles) |
| `0x8338–0x833b` | Store pm → global 0x132900 |
| `0x85c3–0x85ef` | Promode: STAT_DOUBLEJUMP_WINDOW countdown (-= pml.msec); reset STAT_DOUBLEJUMP_USED |
| `0x85af` | Call PM_GroundTrace (first, pre-movement) |
| `0x85b7` | Call PM_DropTimers (if pm_type != 3) |
| `0x85ba` | Call 0x8203 (PM_DropTimers or similar) |
| `0x8627–0x862f` | pml.walking check → PM_WalkMove (0x726e) |
| `0x8630–0x8632` | else → PM_AirMove (0x6e71) |
| `0x8636` | Call PM_GroundTrace (second, post-movement) |
| `0x864e` | Call trap_SnapVector on velocity |
| `0x8651` | Function return |

---

## Global memory layout (pml fields)

| Address | Field |
|---------|-------|
| `0x132900` | pm (pmove_t pointer, cached each PmoveSingle call) |
| `0x1328a0` | pml.walking |
| `0x1328a4` | pml.groundPlane |
| `0x1328a8` | pml.groundTrace (trace_t, 0x38 bytes) |
| `0x1328c0` | pml.groundTrace.plane.normal[0] |
| `0x1328c4` | pml.groundTrace.plane.normal[1] |
| `0x1328c8` | pml.groundTrace.plane.normal[2] |
| `0x1328d4` | pml.groundTrace.surfaceFlags |
| `0x1328e0` | pml.previous_origin (vec3, inferred) |
| `0x1328ec` | pml.previous_velocity (vec3, inferred) |
| `0x1328f8` | pml.previous_velocity[2] (inferred — used in PM_GroundTrace landing detection) |
| `0x132898` | pml.frametime |
| `0x13289c` | pml.msec |
| `0x132874` | pml.forward |
| `0x132880` | pml.right |
| `0x13287c` | pml.up |
