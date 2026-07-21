# PM_CheckJump — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006a5e`  
**Called from:** `0x726e` (PM_WalkMove)  
**Only called from ground movement** — not from PM_AirMove.

---

## Stock Q3 behavior (unchanged in Defrag)

- If `pm_flags & PMF_RESPAWNED (0x200)` → return false
- If `cmd.upmove < 10` → return false (not pressing jump)
- If `pm_flags & PMF_JUMP_HELD (0x2)` → zero `upmove`, return false
- Clear `pml.groundPlane` and `pml.walking`
- Set `pm_flags |= PMF_JUMP_HELD (0x2)`
- Set `ps->groundEntityNum = ENTITYNUM_NONE (0x3ff)`
- Call `PM_AddEvent(EV_JUMP)` — EV_JUMP = `0xe` (14) in this build

---

## Defrag modifications — double jump

### pm_flags bit `0x8000` — what it actually means

**Correction from initial analysis:** `0x8000` is NOT a double-jump-window flag and is NOT
set/cleared by PM_CheckJump itself. It is set at the start of every `PmoveSingle` call
by a small helper function (`0x2575`) based on the `df_promode` cvar value
(cached in global `0x1317c4` at game init). See [[func_0x2575]].

`pm_flags & 0x8000` simply means **"promode (CPM physics) is enabled"**.
It is set unconditionally every frame when `df_promode != 0`.

### velocity[2] assignment — full corrected logic

```
// set PMF_JUMP_HELD, groundEntityNum = NONE, clear pml first (same as stock Q3)

if (velocity[2] > 0.0  AND  pm_flags & 0x8000):
    velocity[2] += 270.0              // promode + going up → additive (double jump)
else:
    velocity[2] = 270.0               // not promode, or not going up → set (normal jump)

// stats bonus block — only runs when promode (0x8000) is set:
if (pm_flags & 0x8000):
    if (ps->stats[10] > 0):
        velocity[2] += 100.0          // bonus boost within window
        ps->stats[11] = 1             // mark bonus as used
    ps->stats[10] = 400               // always reset window after any promode jump
```

### Key insight: no one-time "used" guard in PM_CheckJump

`ps->stats[11]` is SET here but never CHECKED here. PM_CheckJump has no guard
preventing a second or third double jump. The only gate is `PMF_JUMP_HELD` —
the player must release and re-press jump each time. As long as they are going up
(`velocity[2] > 0`) and in promode, each fresh jump press adds `+270` (+100 if within
the 400ms bonus window of the previous jump).

`ps->stats[11]` is likely used by game/cgame code for display or scoring purposes,
not to restrict jumping.

### ps->stats fields

| Field | Offset from ps | Purpose |
|-------|---------------|---------|
| `ps->stats[10]` | `0xe0` | Bonus window countdown; set to 400 on every promode jump, checked > 0 for +100 boost |
| `ps->stats[11]` | `0xe4` | Set to 1 when +100 bonus fires; likely HUD/scoring use only, not a jump gate |

### Key constants (IEEE 754 float hex)

| Hex | Float value | Used for |
|-----|------------|---------|
| `0x43870000` | 270.0 | JUMP_VELOCITY — base value for all jumps |
| `0x42c80000` | 100.0 | Bonus velocity on double jump when `stats[10] > 0` |
| `0x190` | 400 (int) | Bonus window duration set in `stats[10]` after every promode jump |

**Note:** The other occurrences of `0x43870000` in the disassembly (lines 64144 and 64224,
function at `0x937c`) are a trig function using 270.0 as an angle in degrees — unrelated.

---

## Disassembly addresses of interest

| Address | Description |
|---------|-------------|
| `0x6a5e` | Function entry (`enter 0x1c`) |
| `0x6a65–0x6a6c` | PMF_RESPAWNED check → return false |
| `0x6a6f–0x6a78` | upmove < 10 check → return false |
| `0x6a7f–0x6a8c` | PMF_JUMP_HELD check → clear upmove, return false |
| `0x6a8d–0x6aa1` | Clear pml globals, set PMF_JUMP_HELD |
| `0x6aa7` | groundEntityNum = ENTITYNUM_NONE |
| `0x6ab0–0x6ab4` | Branch: velocity[2] <= 0.0 → jump to normal path |
| `0x6aba–0x6abd` | Branch: !(pm_flags & 0x8000) → jump to normal path |
| `0x6aca` | Double jump path: velocity[2] += 270.0 |
| `0x6acf–0x6ad5` | Normal jump path: velocity[2] = 270.0 |
| `0x6adc–0x6adf` | Second 0x8000 check (gate for stats bonus block) |
| `0x6ae3–0x6ae7` | Check stats[10] > 0 (bonus eligibility) |
| `0x6af4–0x6af6` | Bonus: velocity[2] += 100.0 |
| `0x6afc–0x6afd` | stats[11] = 1 |
| `0x6b03–0x6b04` | stats[10] = 400 (always, after any promode jump) |
| `0x6b07` | PM_AddEvent(EV_JUMP) call |

---

## oDFs implementation (current, matching Defrag)

```c
// in PM_CheckJump, after guards and clearing pml/flags:
if ( pm->promode && pm->ps->velocity[2] > 0.0f )
    pm->ps->velocity[2] += JUMP_VELOCITY;
else
    pm->ps->velocity[2] = JUMP_VELOCITY;

if ( pm->promode ) {
    if ( pm->ps->stats[STAT_DOUBLEJUMP_WINDOW] > 0 ) {
        pm->ps->velocity[2] += 100.0f;
        pm->ps->stats[STAT_DOUBLEJUMP_USED] = 1;
    }
    pm->ps->stats[STAT_DOUBLEJUMP_WINDOW] = 400;  // always reset
}
```

`STAT_DOUBLEJUMP_WINDOW = 10`, `STAT_DOUBLEJUMP_USED = 11` — defined in `bg_public.h`.
Window countdown (`-= pml.msec` per frame) handled in `PmoveSingle`.
