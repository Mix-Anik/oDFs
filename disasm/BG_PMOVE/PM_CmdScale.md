# PM_CmdScale — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0000686e`  
**Called from:** PM_WaterMove, PM_FlyMove, PM_AirMove, PM_WalkMove, PM_NoclipMove

---

## Structure

1 arg: `cmd` (usercmd_t pointer)

```
max = max(abs(forwardmove), abs(rightmove), abs(upmove))
if max == 0: return 0.0
total = sqrt(forwardmove² + rightmove² + upmove²)
return (float)ps->speed * max / (127.0 * total)
```

Standard Q3 PM_CmdScale. Reads `ps->speed` from pm global. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x686e` | Function entry |
| `0x686f–0x6888` | Compute max = max(abs(fwd), abs(right)) |
| `0x688d–0x68bb` | Extend max to upmove |
| `0x68bc–0x68c1` | If max == 0: return 0.0 |
| `0x68c4–0x68f0` | Compute total = sqrt(fwd²+right²+up²) |
| `0x68f5` | Call sqrt (trap -0x6b) |
| `0x6900–0x690c` | Return ps->speed × max / (127.0 × total) |
| `0x690f` | Function return |

---

## oDFs implementation notes

Full match. ✓  
`0x42fe0000` = 127.0f constant confirmed.
