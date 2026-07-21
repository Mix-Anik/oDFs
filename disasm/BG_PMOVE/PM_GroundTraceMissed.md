# PM_GroundTraceMissed — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007895`  
**Called from:** PM_GroundTrace (when trace.fraction == 1.0 — no ground found)

---

## Structure

No args. Called when the downward ground trace finds nothing — player is airborne.

1. If `ps->groundEntityNum == ENTITYNUM_NONE (0x3ff)`: already airborne, skip to step 3
2. **Leaving-ground transition** (was on ground last frame):
   - Print debug "lift" message if debugLevel
   - Trace 64 units down from origin (point = origin, point[2] -= 64)
   - If trace hits nothing (`fraction == 1.0f`): set jump animation
     - `forwardmove >= 0`: force LEGS_JUMP (0x12), clear `pm_flags & ~0x9` (clears PMF_DUCKED|PMF_BACKWARDS_JUMP)
     - `forwardmove < 0`: force LEGS_JUMPB (0x14), set `pm_flags |= 0x8` (PMF_BACKWARDS_JUMP)
3. Set `ps->groundEntityNum = ENTITYNUM_NONE`
4. Set `pml.groundPlane = 0` (0x1328a4)
5. Set `pml.walking = 0` (0x1328a0)

**Minor DeFRaG difference:** In the forward-jump path, DeFRaG clears `pm_flags & ~0x9` (= clears PMF_DUCKED(1) AND PMF_BACKWARDS_JUMP(8)). Q3/oDFs only clears PMF_BACKWARDS_JUMP. This forces an unduck on forward jump. Since PM_CheckDuck re-evaluates every frame, the practical impact is zero.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7895` | Function entry |
| `0x7899` | Load ps->groundEntityNum (ps+0x44); check != ENTITYNUM_NONE (0x3ff) |
| `0x789d` | If ENTITYNUM_NONE: skip to 0x791f |
| `0x78a0–0x78ac` | Debug print "lift" |
| `0x78ad–0x78b3` | Copy ps->origin to local point |
| `0x78b4–0x78b9` | point[2] -= 64 (0x42800000) |
| `0x78e7` | Trace call via pm->trace (pm+0xe4) |
| `0x78ec` | `nef`: if trace.fraction != 1.0 → skip animation |
| `0x78ef–0x78f4` | Load cmd.forwardmove; if < 0 → jump to backward path |
| `0x78f5–0x790a` | Forward path: LEGS_JUMP(0x12), clear pm_flags & ~0x9 |
| `0x790b–0x791e` | Backward path: LEGS_JUMPB(0x14), pm_flags |= 0x8 |
| `0x791f–0x792b` | Set groundEntityNum=ENTITYNUM_NONE, groundPlane=0, walking=0 |
| `0x792c` | Function return |

---

## oDFs implementation notes

Full match with minor note: DeFRaG also clears PMF_DUCKED on forward jump (`~0x9` vs oDFs's `~0x8`). PM_CheckDuck overrides this every frame, so no physics impact. ✓
