# PM_TorsoAnimation — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007f99`  
**Called from:** PmoveSingle (after PM_Weapon)

---

## Structure

No args. Sets the idle torso animation based on weapon state and current weapon.

```
if ps->weaponstate (ps+0x94) != WEAPON_READY (0): return

if ps->weapon (ps+0x90) == WP_GAUNTLET (1):
    PM_ContinueTorsoAnim(TORSO_STAND2 = 12 = 0xc)
else:
    PM_ContinueTorsoAnim(TORSO_STAND = 11 = 0xb)
```

Only active when weapon is READY (not firing/switching). Animation-only.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7f99` | Function entry |
| `0x7f9d` | ps->weaponstate (ps+0x94) != 0: return |
| `0x7fa6` | ps->weapon (ps+0x90) == 1 (WP_GAUNTLET)? |
| `0x7faa–0x7faf` | weapon == 1: PM_ContinueTorsoAnim(12 = TORSO_STAND2) |
| `0x7fb1–0x7fb5` | weapon != 1: PM_ContinueTorsoAnim(11 = TORSO_STAND) |
| `0x7fb6` | Function return |

---

## oDFs implementation notes

Full match. ✓ WEAPON_READY=0, WP_GAUNTLET=1, TORSO_STAND=11, TORSO_STAND2=12 all confirmed.
