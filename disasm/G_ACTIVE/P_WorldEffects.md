# P_WorldEffects — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00019f04`  
**Called from:** ClientThink_real (somewhere after Pmove)  
**Args:** `arg0` = ent (gentity_t*)

---

## Structure

Handles environmental damage: drowning, lava, slime. Equivalent to Q3/oDFs `P_WorldEffects`.

### 1. Noclip early exit (0x19f05–0x19f0a)

```
global 0x131820 = noclip flag (game-wide?)
if 0x131820 != 0: return   ; noclip players skip world effects
```

Note: oDFs checks `ent->client->noclip` per-client; DeFRaG checks a global flag.

### 2. Setup (0x19f0b–0x19f45)

```
if *(client + 0x2c0) != 0:          ; DeFRaG noclip flag in gclient_t at offset 0x2c0
    client->airOutTime = level.time + 12000ms (0x2ee0)   ; airOutTime at client+0x340
    return

waterlevel = ent->waterlevel (ent+0x314)
envirosuit = (ps->powerups[PW_BATTLESUIT] (ps+0x140)) > level.time
```

### 3. Drowning (waterlevel == 3) (0x19f46–0x1a022)

```
if waterlevel != 3: skip to step 4
if envirosuit:
    airOutTime = level.time + 10000ms (0x2710)
else:
    if airOutTime >= level.time: skip damage
    airOutTime += 1000ms (0x3e8)
    if ent->health (ent+0x2dc) <= 0: skip damage
    ent->damage (ent+0x2e4) += 2
    if ent->damage > 15 (0xf): ent->damage = 15
    if ent->health > ent->damage:
        play sound "*drown.wav" + G_Damage(ent, ..., ent->damage, DAMAGE_NO_ARMOR, MOD_WATER=14)
    else:
        play gurp1.wav or gurp2.wav (random) + G_Damage(ent, ..., ent->damage, DAMAGE_NO_ARMOR, MOD_WATER=14)
    pain_debounce_time (client+0x378) = level.time + 200ms (0xc8)
```

**DeFRaG difference**: Sound selection for drowning uses an extra check (health vs damage threshold) to choose between drown.wav and gurp sounds. Q3/oDFs doesn't do this — uses a random number for sound selection. Physics impact: none.

### 4. Non-drowning water reset (waterlevel != 3) (0x1a023–0x1a039)

```
airOutTime = level.time + 12000ms
ent->damage = 2 (reset)
```

### 5. Lava/slime damage (0x1a03a–0x1a0dc)

```
if waterlevel == 0: return
if !(watertype (ent+0x310) & 0x18): return   ; 0x18 = LAVA(8)|SLIME(16)
if ent->health <= 0: return
if pain_debounce_time > level.time: return

if envirosuit:
    G_AddEvent(EV_POWERUP_BATTLESUIT = 0x3e)
else:
    if watertype & 0x8 (CONTENTS_LAVA):
        G_Damage(ent, ..., 30*waterlevel, 0, MOD_LAVA=16)
    if watertype & 0x10 (CONTENTS_SLIME):
        G_Damage(ent, ..., 10*waterlevel, 0, MOD_SLIME=15)
    pain_debounce_time = level.time + 200ms
```

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x19f04` | Function entry |
| `0x19f0b` | client+0x2c0 = per-client noclip check |
| `0x19f23` | airOutTime = level.time + 0x2ee0 (12000ms) |
| `0x19f28` | waterlevel = ent+0x314 |
| `0x19f34` | envirosuit check: ps+0x140 = powerups[PW_BATTLESUIT] > level.time |
| `0x19f48` | waterlevel == 3 check |
| `0x19f7f` | airOutTime += 1000ms |
| `0x19f93` | ent->damage += 2 (ent+0x2e4) |
| `0x19f9a` | ent->damage cap at 15 |
| `0x1a01c` | G_Damage args: MOD_WATER=0xe, DAMAGE_NO_ARMOR=0x2 |
| `0x1a095` | G_Damage: lava = 0x1e (30) × waterlevel |
| `0x1a0bf` | G_Damage: slime = 0xa (10) × waterlevel |
| `0x1a0dc` | Function return |

---

## Confirmed gclient_t field offsets

| Offset | Field |
|--------|-------|
| client+0x2c0 | DeFRaG noclip flag |
| client+0x340 | airOutTime |
| client+0x378 | pain_debounce_time |
| ent+0x2dc | ent->health |
| ent+0x2e4 | ent->damage |
| ent+0x310 | ent->watertype |
| ent+0x314 | ent->waterlevel |

---

## oDFs implementation notes

**Full match on damage logic.** ✓  
Drowning: += 2 per second, max 15, MOD_WATER=14, DAMAGE_NO_ARMOR=2, pain_debounce=200ms ✓  
Lava: 30×waterlevel, MOD_LAVA=16 ✓  
Slime: 10×waterlevel, MOD_SLIME=15 ✓  
Minor difference: noclip check uses global flag vs per-client check; drowning sound selection logic differs. Neither affects physics.
