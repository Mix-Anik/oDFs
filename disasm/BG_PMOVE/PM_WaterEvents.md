# PM_WaterEvents — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007e78`  
**Called from:** PmoveSingle (last call before trap_SnapVector)

---

## Structure

No args. Fires sound events when the player enters or leaves water volumes by comparing current vs previous waterlevel.

`pml.previous_waterlevel` is stored at `0x1328fc`.

```
; Entering water from dry
if pml.previous_waterlevel == 0 AND pm->waterlevel != 0:
    PM_AddEvent(EV_WATER_TOUCH = 15 = 0xf)

; Completely leaving water
if pml.previous_waterlevel != 0 AND pm->waterlevel == 0:
    PM_AddEvent(EV_WATER_LEAVE = 16 = 0x10)

; Head going under
if pml.previous_waterlevel != 3 AND pm->waterlevel == 3:
    PM_AddEvent(EV_WATER_UNDER = 17 = 0x11)

; Head coming out
if pml.previous_waterlevel == 3 AND pm->waterlevel != 3:
    PM_AddEvent(EV_WATER_CLEAR = 18 = 0x12)
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7e78` | Function entry |
| `0x7e7c` | Load 0x1328fc = pml.previous_waterlevel |
| `0x7e7d–0x7e8d` | prev==0 AND cur!=0: EV_WATER_TOUCH (0xf) |
| `0x7e8e–0x7ea2` | prev!=0 AND cur==0: EV_WATER_LEAVE (0x10) |
| `0x7ea3–0x7eb7` | prev==3 AND cur!=3: EV_WATER_CLEAR (0x11) |
| `0x7eb8–0x7ecc` | prev!=3 AND cur==3: EV_WATER_UNDER (0x12) |
| `0x7ecd` | Function return |

---

## Event enum values (confirmed)

| Event | Value | Meaning |
|-------|-------|---------|
| `EV_WATER_TOUCH` | 15 (0xf) | Foot enters water |
| `EV_WATER_LEAVE` | 16 (0x10) | Foot exits water |
| `EV_WATER_UNDER` | 17 (0x11) | Head goes under |
| `EV_WATER_CLEAR` | 18 (0x12) | Head comes out |

---

## oDFs implementation notes

Full match. ✓ All 4 transition conditions and event values identical.
