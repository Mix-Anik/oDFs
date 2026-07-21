# PM_SetWaterLevel — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007ae8`  
**Called from:** PmoveSingle (twice — before movement dispatch and after PM_GroundTrace)

---

## Structure

No args. Sets `pm->waterlevel` and `pm->watertype` by testing the player's position at three heights with `pointcontents` (pm+0xe8).

```
pm->waterlevel = 0
pm->watertype = 0

point = (ps->origin[0], ps->origin[1], ps->origin[2] + MINS_Z + 1)
   = (origin.x, origin.y, origin.z − 23)

cont = pointcontents(point, clientNum)
if cont & MASK_WATER (0x38):
    pm->watertype = cont
    pm->waterlevel = 1

    sample2 = ps->viewheight − MINS_Z    ; = viewheight + 24
    sample1 = sample2 / 2

    point[2] = origin.z + MINS_Z + sample1
    cont = pointcontents(point, clientNum)
    if cont & MASK_WATER:
        pm->waterlevel = 2

        point[2] = origin.z + MINS_Z + sample2
        cont = pointcontents(point, clientNum)
        if cont & MASK_WATER:
            pm->waterlevel = 3
```

`MASK_WATER = 0x38` = CONTENTS_WATER(0x20) | CONTENTS_LAVA(0x8) | CONTENTS_SLIME(0x10)

Standard Q3 algorithm. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7ae8` | Function entry |
| `0x7aeb` | pm->waterlevel (pm+0xd4) = 0 |
| `0x7af1` | pm->watertype (pm+0xd0) = 0 |
| `0x7af5–0x7b10` | Build test point: origin + (0, 0, −24+1.0) = origin.z − 23 |
| `0x7b24` | First pointcontents call (pm+0xe8) |
| `0x7b2c` | `band 0x38` — MASK_WATER check |
| `0x7b2f` | If not water: return (waterlevel stays 0) |
| `0x7b30–0x7b46` | Set waterlevel=1, watertype, compute samples |
| `0x7b4d` | pm->waterlevel = 1 |
| `0x7b5a` | Second trace: point.z = origin.z − 24 + sample1 |
| `0x7b6e` | Second pointcontents call |
| `0x7b76` | MASK_WATER check for waterlevel 2 |
| `0x7b7e` | pm->waterlevel = 2 |
| `0x7b8d` | Third trace: point.z = origin.z − 24 + sample2 |
| `0x7ba1` | Third pointcontents call |
| `0x7ba9` | MASK_WATER check for waterlevel 3 |
| `0x7bb2` | pm->waterlevel = 3 |
| `0x7bb3` | Function return |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x38` | | MASK_WATER (WATER\|LAVA\|SLIME) |
| `−0x3e400000` | −24.0f | MINS_Z offset |
| `0x3f800000` | 1.0f | +1 from bottom |

---

## oDFs implementation notes

Full match. ✓ Note: `−MINS_Z` appears in the disasm as `sub −0x3e400000` which is `subtract (−24.0f)` = add 24. The QVM `sub` instruction computes `a − b`, so subtracting a negative value adds the magnitude. This yields `viewheight − (−24) = viewheight + 24` = sample2, matching oDFs.
