# Animation Helper Functions — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

Covers: PM_AddEvent, PM_AddTouchEnt, PM_StartTorsoAnim, PM_StartLegsAnim, PM_ContinueLegsAnim, PM_ContinueTorsoAnim, PM_ForceLegsAnim, PM_FootstepForSurface (see dedicated file)

---

## PM_AddEvent — `0x0000655d`

**Called from:** many functions throughout pmove

1 arg: `newEvent` (int)

```
BG_AddPredictableEventToPlayerstate(newEvent, parm=0, ps)
```

Wrapper around the engine event function. No DeFRaG modifications.

**Full match.** ✓

---

## PM_AddTouchEnt — `0x0000656c`

**Called from:** PM_GroundTrace (when landing on entity), PM_SlideMove

1 arg: `entityNum` (int)

```
if entityNum == ENTITYNUM_WORLD (0x3fe = 1022): return
if pm->numtouch (pm+0x34) == MAXTOUCH (32 = 0x20): return
for i = 0 .. pm->numtouch:
    if pm->touchents[i] == entityNum: return   ; already registered
pm->touchents[pm->numtouch] = entityNum
pm->numtouch++
```

**Full match.** ✓

| Key address | Description |
|-------------|-------------|
| `0x656c` | Entry |
| `0x656f` | ENTITYNUM_WORLD check (0x3fe) |
| `0x6578` | numtouch < MAXTOUCH (0x20) check |
| `0x6581–0x659d` | Duplicate search loop |
| `0x65ae–0x65be` | Add entity + increment numtouch |

---

## PM_StartTorsoAnim — `0x000065c1`

**Called from:** PM_Weapon, PM_TorsoAnimation (via PM_ContinueTorsoAnim)

1 arg: `anim` (int)

```
if ps->pm_type >= PM_DEAD (3): return
ps->torsoAnim = (ps->torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT | anim
   ; ANIM_TOGGLEBIT = 0x80
```

Flips the toggle bit to force animation change. **Full match.** ✓

| Key address | Description |
|-------------|-------------|
| `0x65c1` | Entry |
| `0x65c7–0x65c9` | pm_type < 3 check (lti → return if ≥ PM_DEAD) |
| `0x65cc–0x65d2` | local8 = &ps->torsoAnim (ps+0x54) |
| `0x65d4` | ANIM_TOGGLEBIT = 0x80 |
| `0x65d6–0x65e4` | `(torsoAnim & 0x80) ^ 0x80 | anim` |

---

## PM_StartLegsAnim — `0x000065e7`

**Called from:** PM_ContinueLegsAnim, PM_ForceLegsAnim, PM_GroundTraceMissed

1 arg: `anim` (int)

```
if ps->pm_type >= PM_DEAD (3): return
if ps->legsTimer (ps+0x48) > 0: return    ; high-priority animation running
ps->legsAnim = (ps->legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT | anim
```

**Full match.** ✓

| Key address | Description |
|-------------|-------------|
| `0x65e7` | Entry |
| `0x65ed–0x65ef` | pm_type < 3 check |
| `0x65f5–0x65f9` | legsTimer (ps+0x48) > 0: return |
| `0x65fc–0x6614` | Apply anim to ps->legsAnim (ps+0x4c) |

---

## PM_ContinueLegsAnim — `0x00006617`

**Called from:** PM_Footsteps

1 arg: `anim` (int)

```
if (ps->legsAnim & ~ANIM_TOGGLEBIT) == anim: return   ; already playing this anim
if ps->legsTimer > 0: return                            ; high-priority running
PM_StartLegsAnim(anim)
```

**Full match.** ✓

| Key address | Description |
|-------------|-------------|
| `0x6617` | Entry |
| `0x661d` | `band ~0x80` mask on legsAnim |
| `0x6622` | Anim already playing: return |
| `0x6628–0x662c` | legsTimer > 0: return |
| `0x6632` | Call PM_StartLegsAnim (0x65e7) |

---

## PM_ContinueTorsoAnim — `0x00006637`

**Called from:** PM_TorsoAnimation, PM_Weapon

1 arg: `anim` (int)

```
if (ps->torsoAnim & ~ANIM_TOGGLEBIT) == anim: return
if ps->torsoTimer (ps+0x50) > 0: return
PM_StartTorsoAnim(anim)
```

**Full match.** ✓ Same structure as PM_ContinueLegsAnim but for torso.

---

## PM_ForceLegsAnim — `0x00006657`

**Called from:** PM_GroundTraceMissed (LEGS_JUMP/LEGS_JUMPB), PM_CrashLand (LEGS_LAND/LEGS_LANDB), PM_CheckJump (LEGS_JUMP)

1 arg: `anim` (int)

```
ps->legsTimer = 0    ; clear any running animation
PM_StartLegsAnim(anim)
```

**Full match.** ✓

| Key address | Description |
|-------------|-------------|
| `0x6657` | Entry |
| `0x665a–0x665e` | ps->legsTimer (ps+0x48) = 0 |
| `0x6662` | Call PM_StartLegsAnim (0x65e7) |

---

## oDFs implementation notes

All animation helpers are full matches. ✓  
`ANIM_TOGGLEBIT = 0x80` confirmed. `ps->legsAnim` at ps+0x4c, `ps->torsoAnim` at ps+0x54, `ps->legsTimer` at ps+0x48, `ps->torsoTimer` at ps+0x50.
