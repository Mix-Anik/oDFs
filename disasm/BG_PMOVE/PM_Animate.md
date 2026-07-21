# PM_Animate — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000081de`  
**Called from:** PmoveSingle (every frame, after movement dispatch)

---

## Structure

No args. Handles gesture button → taunt animation.

```
if !(cmd.buttons & BUTTON_GESTURE (0x8)): return
if ps->torsoAnim (ps+0x54) != 0: return   ; high-priority anim running
PM_StartTorsoAnim(TORSO_GESTURE = 6)
ps->torsoTimer = 0x8f6 = 2294ms
PM_AddEvent(EV_TAUNT = 0x4c = 76)
```

Animation-only function. No physics impact.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x81de` | Function entry |
| `0x81e1` | Load pm+0x14 = cmd.buttons |
| `0x81e4` | `band 0x8` = BUTTON_GESTURE check |
| `0x81e7` | If not pressed: return |
| `0x81eb–0x81ef` | ps->torsoAnim (ps+0x50) != 0: return |
| `0x81f1` | PM_StartTorsoAnim (0x65c1) with arg 6 (TORSO_GESTURE) |
| `0x81f8–0x81fb` | ps->torsoTimer = 0x8f6 |
| `0x81fd` | PM_AddEvent (0x655d) with 0x4c = EV_TAUNT |
| `0x8201` | Function return |

---

## oDFs implementation notes

Full match — animation-only, no physics. ✓
