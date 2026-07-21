# PM_SetMovementDir — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006912`  
**Called from:** PM_WalkMove, PM_AirMove

---

## Structure

No args. Reads `pm->cmd.forwardmove` (pm+0x19) and `pm->cmd.rightmove` (pm+0x1a).  
Sets `ps->movementDir` (ps+0x58).

8 directional cases:

| movementDir | Condition |
|-------------|-----------|
| 0 | rightmove == 0, forwardmove > 0 (forward) |
| 1 | rightmove < 0, forwardmove > 0 (forward-right) |
| 2 | rightmove < 0, forwardmove == 0 (right) |
| 3 | rightmove < 0, forwardmove < 0 (back-right) |
| 4 | rightmove == 0, forwardmove < 0 (back) |
| 5 | rightmove > 0, forwardmove < 0 (back-left) |
| 6 | rightmove > 0, forwardmove == 0 (left) |
| 7 | rightmove > 0, forwardmove > 0 (forward-left) |

No-movement fallback (fwd=0, right=0):
- If current movementDir == 2 → set to 1
- If current movementDir == 6 → set to 7

Standard Q3 PM_SetMovementDir. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6912` | Function entry |
| `0x691a–0x692b` | Initial: if fwd==0 AND right==0 → jump to no-movement fallback (0x6a3c) |
| `0x692c–0x694d` | Case 0: right==0 AND fwd>0 → movementDir=0 |
| `0x694e–0x696f` | Case 1: right<0 AND fwd>0 → movementDir=1 |
| `0x6970–0x6991` | Case 2: right<0 AND fwd==0 → movementDir=2 |
| `0x6992–0x69b3` | Case 3: right<0 AND fwd<0 → movementDir=3 |
| `0x69b4–0x69d5` | Case 4: right==0 AND fwd<0 → movementDir=4 |
| `0x69d6–0x69f7` | Case 5: right>0 AND fwd<0 → movementDir=5 |
| `0x69f8–0x6a19` | Case 6: right>0 AND fwd==0 → movementDir=6 |
| `0x6a1a–0x6a3b` | Case 7: right>0 AND fwd>0 → movementDir=7 |
| `0x6a3c–0x6a5b` | No-movement fallback: dir==2→1, dir==6→7 |
| `0x6a5c` | Function return |

---

## oDFs implementation notes

Full match. ✓ All 8 direction cases and no-movement fallback identical to Q3/oDFs.
