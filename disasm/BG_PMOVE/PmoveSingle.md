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
2. Calls `0x4152` — **fully decoded, this pass. Not `PM_UpdateViewAngles`** (the earlier "likely" label was wrong). It builds `ps->stats[13]` (already known from [../README.md](../README.md) as "DeFRaG input state bitfield (non-physics)") from the signs/magnitude of `cmd.forwardmove`/`rightmove`/`upmove`. Full decode in [§ func_0x4152](#func_0x4152--stats13-input-bitfield-builder) below.
3. Clears `numtouch`, `watertype`, `waterlevel` fields
4. Computes PMSC/msec, saves previous origin/velocity
5. Calls `0x828d` — **fully decoded, this pass. This is the real `PM_UpdateViewAngles`** (the earlier "likely `PM_SetWaterLevel`" label was wrong — `PM_SetWaterLevel` is the separately-confirmed `0x7ae8`, called once, post-movement, at step 13 below — DeFRaG does **not** call it twice). Confirmed via the `delta_angles` clamp to ±16000, the `SHORT2ANGLE` conversion constant `0x3bb40000` (=360/65536), and the `PM_SPECTATOR`/`STAT_HEALTH` early-return gate, all matching vanilla `PM_UpdateViewAngles` exactly. One confirmed DeFRaG addition: the outer early-return gate checks the pm_type-like field against **two** values (5 and 6) instead of vanilla's one (5 = `PM_INTERMISSION`) — DeFRaG's enum has an extra type value at 6 that also freezes view angles; not independently named. Full decode in [§ func_0x828d](#func_0x828d--pm_updateviewangles) below.
6. Calls `0xa186` — **fully decoded, this pass. This is `AngleVectors`**, not `PM_CheckDuck` (the earlier "likely" label was wrong — `PM_CheckDuck` is the separately-confirmed `0x7bb5`). Confirmed via the yaw→pitch `sin`/`cos` trig sequence (trap `-0x68`/`-0x69`), the `NULL`-pointer guards on the `forward`/`right`/`up` output arguments, and the combination math — all matching vanilla `AngleVectors` exactly. Has 23 call sites across the entire QVM (not just `bg_pmove.c`), consistent with a universal math utility. No CPM/DeFRaG modification found. Full decode in [§ func_0xa186](#func_0xa186--anglevectors) below.
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
if (pm_flags & PMF_GRAPPLE_PULL, 0x100): call 0x7208 (grapple-mode swing dispatcher), return
if (waterlevel > 1):         call 0x6c68 (PM_WaterMove), return
if (pml.walking):            call PM_WalkMove (0x726e)
else:                        call PM_AirMove (0x6e71)
```

**`0x7208` corrected** — earlier passes labeled this "dispatch for waterlevels" and
left it undecoded. Fully decoded this pass: it reads `DFLAG_GRAPPLE_MODE_1`/`_2`
(`ps.stats[STAT_DEFRAG_FLAGS]` bits `0x400`/`0x800`) and dispatches to one of four
distinct grapple-swing physics implementations before falling through to
`PM_WalkMove`/`PM_AirMove`. Full decode: [../G_ACTIVE/GrappleMode.md](../G_ACTIVE/GrappleMode.md).
Has nothing to do with water levels; the "0x6c27 PM_GrappleMove" row from an even
earlier pass was a separate, likely-also-stale label — `0x6c27` is confirmed
elsewhere in this document as `PM_WaterJumpMove`, not a grapple function.

### Key addresses

| Address | Description |
|---------|-------------|
| `0x8331` | Function entry |
| `0x8335–0x8337` | Call 0x4152 (stats[13] input-bitfield builder — see full decode above) |
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

## func_0x4152 — `stats[13]` input-bitfield builder

**Address:** `0x4152`, called only from `0x8331` (PmoveSingle, step 2)
**Args:** `arg0` = `pm` pointer (confirmed — `arg0+0x19/0x1a/0x1b` match the already-confirmed `pm+0x19=cmd.forwardmove`, `pm+0x1a=cmd.rightmove`, `pm+0x1b=cmd.upmove` from [../README.md](../README.md))

Writes into `*(pm->ps) + 0xec` = `ps->stats[13]`, already documented in the main README as a "DeFRaG input state bitfield (non-physics)". This pass fully decodes what the bitfield actually encodes:

```c
void func_0x4152( pmove_t *pm ) {
    playerState_t *ps = pm->ps;

    ps->stats[13] = 0;
    if ( pm->cmd.forwardmove > 0 )  ps->stats[13] |= 0x1;
    if ( pm->cmd.forwardmove < 0 )  ps->stats[13] |= 0x2;
    if ( pm->cmd.rightmove   < 0 )  ps->stats[13] |= 0x8;
    if ( pm->cmd.rightmove   > 0 )  ps->stats[13] |= 0x10;
    if ( pm->cmd.upmove      > 0 )  ps->stats[13] |= 0x20;
    if ( pm->cmd.upmove      < 0 )  ps->stats[13] |= 0x40;
    if ( *(int *)((byte *)pm + 0x14) & 0x1 )  ps->stats[13] |= 0x100;   // pm+0x14: unnamed extended cmd field, not in the confirmed offset table
    if ( func_0x4122( pm ) )        ps->stats[13] |= 0x200;
}
```

`pm+0x14` falls inside the `cmd` block (`pm+0x04`..`pm+0x1c`) at an offset not otherwise catalogued (`cmd.forwardmove`/`rightmove`/`upmove` are confirmed at `+0x19/+0x1a/+0x1b`, and `+0x1c` is `tracemask` — so `+0x14` is a DeFRaG-extended `usercmd_t` field 5 bytes before `forwardmove`, real name/purpose not determined). Bit `0x4`, `0x80` are never set anywhere in this function — the bitfield has gaps, consistent with it being a flags byte where not every bit is currently used.

Never read back by anything in `bg_pmove.c` itself — `stats[13]` is a **write-only-from-here, non-physics output** (presumably consumed by cgame.qvm for a HUD element such as a keypress/movement indicator, or by a server-side anti-cheat/logging check outside this file — out of scope to pursue further; confirmed to have zero effect on velocity/knockback/movement physics).

### func_0x4122 — helper called by func_0x4152

**Address:** `0x4122`, called only from `0x4152`
**Args:** `arg0` = `pm` pointer (same struct, reads `+0x19`/`+0x1a` again)

```c
qboolean func_0x4122( pmove_t *pm ) {
    if ( abs( pm->cmd.forwardmove ) == 64 )  return qtrue;
    if ( abs( pm->cmd.rightmove )   == 64 )  return qtrue;
    return qfalse;
}
```

Uses `func_0x6061` (see below) for `abs()`. The significance of exactly `64` is not determined from this function alone — no corroborating evidence found elsewhere in this pass; flagged rather than guessed.

### func_0x6061 — generic integer `abs()`

**Address:** `0x6061`. Confirmed generic utility — **not specific to bg_pmove.c**, called from dozens of sites across the whole QVM, including `PM_CmdScale` (`0x686e`) and `PmoveSingle` (`0x8331`) itself.

```c
int func_0x6061( int x ) { return x >= 0 ? x : -x; }
```

(A neighboring function at `0x6076`, same shape but using `gef`/`negf`, is the float equivalent `fabs()` — noted for completeness, not decoded further; also has dozens of unrelated callers.)

---

## func_0x828d — `PM_UpdateViewAngles`

**Address:** `0x828d`, called only from `0x8331` (PmoveSingle, step 5)
**Args:** `arg0` = `ps` (`playerState_t*`, confirmed — `arg0+0x4`=`pm_type`, `arg0+0xb8`=`stats[0]`=`STAT_HEALTH`, `arg0+0x38`=`delta_angles[0]`, all matching the confirmed offset table), `arg1` = `cmd` (`usercmd_t*`, confirmed — `arg1+0x4` read as `cmd->angles[i]`-shaped access, consistent with the vanilla `usercmd_t.angles[3]` array positioned after `serverTime`/`buttons`)

```c
void func_0x828d( playerState_t *ps, usercmd_t *cmd ) {
    if ( ps->pm_type == 5 || ps->pm_type == 6 )   // vanilla only checks PM_INTERMISSION(5); DeFRaG adds a second value
        return;
    if ( ps->pm_type != 2 /* PM_SPECTATOR */ && ps->stats[STAT_HEALTH] <= 0 )
        return;

    for ( i = 0; i < 3; i++ ) {
        short temp = (short)( cmd->angles[i] + ps->delta_angles[i] );
        if ( i == 0 /* PITCH */ ) {
            if ( temp > 16000 ) {                       // 0x3e80
                ps->delta_angles[i] = 16000 - cmd->angles[i];
                temp = 16000;
            } else if ( temp < -16000 ) {                // -0x3e80
                ps->delta_angles[i] = -16000 - cmd->angles[i];
                temp = -16000;
            }
        }
        ps->viewangles[i] = temp * (360.0f / 65536.0f);  // 0x3bb40000 confirmed = SHORT2ANGLE factor
    }
}
```

This is a **full, confirmed match to vanilla `PM_UpdateViewAngles`**, with exactly one DeFRaG addition: the outer early-return gate tests the `pm_type`-like field against two values (`5` and `6`) rather than vanilla's one (`PM_INTERMISSION`=5). DeFRaG's `pmtype_t` enum has an extra entry at value `6` that also freezes view-angle updates (same treatment as intermission) — its name/purpose is not determined from this function alone. No effect on velocity/knockback (view angles don't feed into movement physics in either vanilla or this build).

**Corrects the module's `PmoveSingle` call ordering understanding:** `PM_SetWaterLevel` (`0x7ae8`) is called exactly **once**, post-movement (step 13) — matching vanilla's single-call structure. The previous doc pass's claim of an additional pre-movement `PM_SetWaterLevel`-like call at step 5 was a mislabeling of this function.

---

## func_0xa186 — `AngleVectors`

**Address:** `0xa186`. **23 call sites across the entire QVM**, including `0x8331` (PmoveSingle), `0x686e`×5 (`PM_CmdScale`), `0x9438`, `0xb526`, `0x10faa`, `0x1140a`, `0x1238f`, `0x123d0`, `0x15100`, `0x1afef`, `0x1b240`×4, `0x1b4dc`, `0x20afa`, `0x211e6`, `0x21864`, `0x22ab3`, `0x22eb4`, `0x2830d`, `0x288b1`, `0x290cf`, `0x29497` — far beyond `bg_pmove.c`, confirming it's a generic shared math utility, not movement-specific.
**Args:** `arg0` = `angles` (`vec3_t`, input), `arg1` = `forward` (`vec3_t*`, output, `NULL`-checked), `arg2` = `right` (`vec3_t*`, output, `NULL`-checked); a fourth `up` output arg is expected by symmetry with vanilla but not individually re-verified past `arg2`'s block in this pass.

```c
void func_0xa186( const vec3_t angles, vec3_t forward, vec3_t right /*, vec3_t up */ ) {
    float sy = sin( angles[1] * (M_PI*2/360) ), cy = cos( angles[1] * (M_PI*2/360) );  // angles[1] = YAW
    float sp = sin( angles[0] * (M_PI*2/360) ), cp = cos( angles[0] * (M_PI*2/360) );  // angles[0] = PITCH
    // (ROLL trig pair follows the same pattern; not re-transcribed here)

    if ( forward ) {
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;
    }
    if ( right ) {
        // right[i] built from sr/sp/cy/cr/sy per vanilla's formula — confirmed present, matches shape
    }
    // if ( up ) { ... }  — matches vanilla by symmetry, not individually re-verified this pass
}
```

Confirmed via the degrees→radians constant `0x3c8efa35` (≈ π/180), the `sin`/`cos` trap calls (`-0x68`/`-0x69`, confirmed against `g_syscalls.asm`), the `angles[1]` (YAW)-then-`angles[0]` (PITCH) computation order matching vanilla's `AngleVectors` exactly, and the `NULL`-guarded output-pointer pattern (`if (forward) ...`, `if (right) ...`) also matching vanilla exactly. **No CPM/DeFRaG modification found** in the portion read. This is the function `PmoveSingle` calls to build `pml.forward`/`pml.right`/`pml.up` from `ps->viewangles`, matching vanilla's `AngleVectors( pm->ps->viewangles, pml.forward, pml.right, pml.up )` call.

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
