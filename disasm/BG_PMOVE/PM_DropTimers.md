# PM_DropTimers — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00008203`  
**Called from:** PmoveSingle (every frame, unconditionally)

**Note:** The doublejump window countdown is NOT inside this function in DeFRaG — it is inline in PmoveSingle at `0x85c3–0x85ef`. In oDFs it was placed inside PM_DropTimers; the net behavior is identical.

**Misidentification warning:** `0x74cb` (dead player velocity decay) and `0x7544` (noclip movement) were previously labeled as PM_DropTimers in early analysis. They are different functions.

---

## Structure

### 1. pm_time countdown (0x8203–0x823c)

```
if ps->pm_time == 0: skip
if pml.msec >= ps->pm_time:
    ps->pm_flags &= ~PMF_ALL_TIMES   ; ~0x160 = clear TIME_LAND|TIME_KNOCKBACK|TIME_WATERJUMP
    ps->pm_time = 0
else:
    ps->pm_time -= pml.msec
```

### 2. legsTimer countdown (0x823d–0x8263)

```
if ps->legsTimer <= 0: skip
ps->legsTimer -= pml.msec
if ps->legsTimer < 0: ps->legsTimer = 0
```

### 3. torsoTimer countdown (0x8264–0x828a)

```
if ps->torsoTimer <= 0: skip
ps->torsoTimer -= pml.msec
if ps->torsoTimer < 0: ps->torsoTimer = 0
```

### Doublejump countdown (PmoveSingle 0x85c3–0x85ef — NOT inside this function)

```
if NOT promode (pm_flags & 0x8000 == 0): skip
if ps->stats[10] <= 0:
    if ps->stats[11] != 0: ps->stats[11] = 0   ; clear bonus-used marker
    skip
ps->stats[10] -= pml.msec   ; countdown the 400ms window
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x8203` | Function entry |
| `0x8207` | Load ps->pm_time (ps+0x10) |
| `0x820b` | If pm_time == 0: skip to 0x823d |
| `0x820c` | Load pml.msec (0x13289c) |
| `0x8214` | `lti`: if msec < pm_time → subtract path (0x822d) |
| `0x8215–0x822a` | msec ≥ pm_time: clear PMF_ALL_TIMES, pm_time = 0 |
| `0x822d–0x823c` | msec < pm_time: pm_time -= msec |
| `0x823d` | legsTimer section start; ps->legsTimer at ps+0x48 |
| `0x8264` | torsoTimer section start; ps->torsoTimer at ps+0x50 |
| `0x828b` | Function return |
| `0x85c3` | Doublejump countdown in PmoveSingle (promode check) |
| `0x85c7–0x85de` | stats[10] (ps+0xe0) countdown path |
| `0x85e1–0x85ef` | stats[11] (ps+0xe4) clear when window expired |

---

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `0x132898` | pml.frametime | (not used here) |
| `0x13289c` | pml.msec | subtracted from timers |
| `~0x160` = `-0x161` | PMF_ALL_TIMES mask | clears TIME_LAND(32)+TIME_KNOCKBACK(64)+TIME_WATERJUMP(256) |

---

## oDFs implementation notes

Full match. ✓  
Structural difference: oDFs puts the doublejump countdown inside PM_DropTimers; DeFRaG runs it inline in PmoveSingle. Net behavior is identical — it executes once per frame.
