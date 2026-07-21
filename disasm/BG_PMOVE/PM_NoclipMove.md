# PM_NoclipMove — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007544`  
**Called from:** PmoveSingle (only when `pm_type == PM_NOCLIP (1)`)

---

## Structure

No args. Full noclip fly-through-walls movement.

```
ps->viewheight = 26 (DEFAULT_VIEWHEIGHT = 0x1a)

; Friction
speed = VectorLength(ps->velocity)
if speed < 1.0:
    velocity = (0, 0, 0)
else:
    friction = pm_friction × 1.5   ; extra friction for noclip
    control = (speed < pm_stopspeed) ? pm_stopspeed : speed
    drop = control × friction × frametime
    newspeed = max(0, speed − drop)
    velocity *= newspeed / speed

; Acceleration
scale = PM_CmdScale(&cmd)
fmove = float(cmd.forwardmove)
smove = float(cmd.rightmove)
for i in 0..2:
    wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove
wishvel[2] += float(cmd.upmove)

VectorNormalize(wishdir)
wishspeed = VectorLength(wishvel) × scale
PM_Accelerate(wishdir, wishspeed, pm_accelerate)   ; 0x14c8 = 10.0f

; Move
for i in 0..2:
    origin[i] += velocity[i] × frametime
```

Standard Q3 PM_NoclipMove. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7544` | Function entry |
| `0x7548` | ps->viewheight (ps+0xa4) = 0x1a = 26 |
| `0x754c–0x7555` | VectorLength (0x8d63) of velocity |
| `0x755c` | If speed < 1.0f (0x3f800000): zero velocity |
| `0x7567–0x75a4` | Friction: pm_friction×1.5 from 0x14d8; control/drop/newspeed |
| `0x756b` | pm_friction loaded from 0x14d8 (= 6.0f) |
| `0x756c` | Multiplied by 0x3fc00000 = 1.5f |
| `0x7572` | pm_stopspeed from 0x14b8 (= 100.0f) for control |
| `0x75d5–0x75dc` | PM_CmdScale (0x686e) with cmd |
| `0x75e3–0x75f3` | Load fmove (pm+0x19), smove (pm+0x1a) |
| `0x75f7–0x761c` | Build wishvel loop (3 components, pml.forward=0x132874, right=0x132880) |
| `0x7620–0x7628` | wishvel[2] += upmove (pm+0x1b) |
| `0x762f` | VectorNormalize (0x9d12) wishdir |
| `0x7645` | PM_Accelerate (0x67f2) with pm_accelerate from 0x14c8 (= 10.0f) |
| `0x7648–0x7661` | origin[0..2] += velocity × frametime |
| `0x7542` | Function return |

---

## oDFs implementation notes

Full match. ✓ pm_friction×1.5 = 9.0f, pm_stopspeed = 100.0f, pm_accelerate = 10.0f — all confirmed from globals.
