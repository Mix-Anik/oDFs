# PM_FootstepForSurface — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007698`  
**Called from:** PM_Footsteps (when a footstep event should fire on dry ground), PM_CrashLand

---

## Structure

No args. Returns an event number based on the current ground surface flags.

```
surfaceFlags = pml.groundTrace.surfaceFlags   ; at 0x1328d4

if surfaceFlags & 0x2000 (SURF_NOSTEPS):  return 0 (EV_NONE)
if surfaceFlags & 0x1000 (SURF_METALSTEPS): return 2 (EV_FOOTSTEP_METAL)
return 1 (EV_FOOTSTEP)
```

Standard Q3 function. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7698` | Function entry |
| `0x7699` | Load pml.groundTrace.surfaceFlags (0x1328d4) |
| `0x769b` | `band 0x2000` = SURF_NOSTEPS check |
| `0x769e` | If set: return 0 (no footstep) |
| `0x76a3` | Load surfaceFlags again |
| `0x76a5` | `band 0x1000` = SURF_METALSTEPS check |
| `0x76a8` | If set: return 2 (EV_FOOTSTEP_METAL) |
| `0x76ad` | return 1 (EV_FOOTSTEP) |
| `0x76af` | Function return (default) |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x2000` | | SURF_NOSTEPS — no footstep sounds |
| `0x1000` | | SURF_METALSTEPS — metal footstep sound |
| `0` | EV_NONE | No event |
| `1` | EV_FOOTSTEP | Normal footstep |
| `2` | EV_FOOTSTEP_METAL | Metal footstep |

---

## oDFs implementation notes

Full match. ✓ Event enum: EV_NONE=0, EV_FOOTSTEP=1, EV_FOOTSTEP_METAL=2 confirmed.
