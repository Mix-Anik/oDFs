# fire_bfg — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00022685` (vanilla baseq3 equivalent at `0x1aadb`, hash `7100620e`)
**DeFRaG hash:** `7ab22a35` — differs from vanilla, **shared exactly with
DeFRaG's own `fire_plasma`** (mirroring the vanilla pattern where these two
functions are structurally identical to each other).
**Size:** `0xe6` (230) instructions in DeFRaG vs vanilla's declared size for
this shape (`0xdd`=221, same as vanilla `fire_rocket`/`fire_plasma`) —
**+9 instructions**, identical delta to `fire_plasma`/`fire_grenade`.

---

## Headline finding

**Fully decoded, instruction-by-instruction, zero unresolved bytes.** Exactly
the same shape as `fire_plasma`: **entirely linear code, no branches, no
`df_promode` (`0x1317c4`) or any other cvar/flag reference**. Every constant
matches vanilla `fire_bfg` (`code/game/g_missile.c` in oDFs, used as the
vanilla-baseline reference) exactly:

```c
bolt->damage               = 100    ; confirmed literal 0x64
bolt->splashDamage         = 100    ; confirmed literal 0x64
bolt->splashRadius         = 120    ; confirmed literal 0x78
bolt->methodOfDeath        = 12     ; MOD_BFG (0xc)
bolt->splashMethodOfDeath  = 13     ; MOD_BFG_SPLASH (0xd)
bolt->s.weapon             = 9      ; WP_BFG
bolt->s.pos.trType         = 2      ; TR_LINEAR
trDelta = dir * 2000.0               ; confirmed literal 0x44fa0000, unconditional
```

**The entire +9-instruction delta vs vanilla is, once again, exactly the same
extra unconditional field write** found in `fire_plasma`/`fire_grenade`/
`fire_rocket` (see [README.md](README.md)), appended right before `return
bolt`:

```c
bolt->s.clientNum = self->s.number;   // NEW vs vanilla — not present in oDFs/vanilla g_missile.c
```

8-instruction block at `0x2275f`–`0x22765` (`local`, `load4`, `const 0xa8`,
`add`, `local`(arg0), `load4`, `load4`, `store4`), writing offset `0xa8` =
`entityState_t.clientNum` (struct-offset math confirmed identically to
[fire_plasma.md](fire_plasma.md)'s "Struct offset confirmation" section — not
repeated here). **Unconditional, no gating.**

---

## Structure

### 1. Setup (`0x22685`–`0x2268e`)

```c
VectorNormalize(dir);       // call 0x9d12
bolt = G_Spawn();           // call 0x283de
```

### 2. Field setup, fully unconditional (`0x22690`–`0x22756`)

```c
bolt->classname            = "bfg"
bolt->nextthink            = level.time + 10000
bolt->think                = G_ExplodeMissile    ; 0x22066
bolt->s.eType              = ET_MISSILE (3)
bolt->r.svFlags            = SVF_USE_CURRENT_ORIGIN (0x80)
bolt->s.weapon             = WP_BFG (9)
bolt->r.ownerNum           = self->s.number
bolt->parent               = self
bolt->damage               = 100
bolt->splashDamage         = 100
bolt->splashRadius         = 120   ; 0x78
bolt->methodOfDeath        = 12    ; MOD_BFG (0xc)
bolt->splashMethodOfDeath  = 13    ; MOD_BFG_SPLASH (0xd)
bolt->clipmask             = 0x6000001   ; MASK_SHOT
bolt->target_ent           = NULL
bolt->s.pos.trType         = TR_LINEAR (2)
bolt->s.pos.trTime         = level.time - 50   ; MISSILE_PRESTEP_TIME
VectorCopy(start, bolt->s.pos.trBase)
bolt->s.pos.trDelta        = dir * 2000.0   ; 0x44fa0000, all 3 axes, unconditional
SnapVector(bolt->s.pos.trDelta)
VectorCopy(start, bolt->r.currentOrigin)
```

Field-for-field, value-for-value match to vanilla `fire_bfg`.

### 3. New: unconditional clientNum write (`0x2275f`–`0x22765`)

```c
bolt->s.clientNum = self->s.number;   // NOT present in vanilla
```

### 4. Return (`0x22766`–`0x2276a`)

```c
return bolt;
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x22685` | Function entry |
| `0x22689` | `VectorNormalize(dir)` call |
| `0x2268d` | `G_Spawn()`-equivalent call |
| `0x22696` | `classname = "bfg"` |
| `0x2269c` | `nextthink = level.time + 10000` |
| `0x226a5` | `think = G_ExplodeMissile` |
| `0x226ab` | `s.eType = 3` (ET_MISSILE) |
| `0x226b1` | `r.svFlags = 0x80` |
| `0x226b7` | `s.weapon = 9` (WP_BFG) |
| `0x226bd` | `r.ownerNum = self->s.number` |
| `0x226c5` | `parent = self` |
| `0x226cc` | `damage = 100` |
| `0x226d2` | `splashDamage = 100` |
| `0x226d8` | `splashRadius = 120` |
| `0x226de` | `methodOfDeath = 12` (MOD_BFG) |
| `0x226e4` | `splashMethodOfDeath = 13` (MOD_BFG_SPLASH) |
| `0x226ea` | `clipmask = 0x6000001` |
| `0x226f0` | `target_ent = NULL` |
| `0x226f6` | `s.pos.trType = 2` (TR_LINEAR) |
| `0x226fc` | `s.pos.trTime = level.time - 50` |
| `0x22705` | `VectorCopy(start, trBase)` |
| `0x2270c`/`0x2270e` | `trDelta.x = dir.x * 2000.0` (`0x44fa0000`) |
| `0x22716`/`0x22718` | `trDelta.y = dir.y * 2000.0` |
| `0x22722`/`0x22724` | `trDelta.z = dir.z * 2000.0` |
| `0x22758` | `VectorCopy(start, r.currentOrigin)` |
| `0x2275f` | **`s.clientNum = self->s.number`** — the +9-instruction addition |

---

## oDFs implementation notes

oDFs's `fire_bfg` (`code/game/g_missile.c`) matches vanilla/DeFRaG exactly for
all damage/speed/radius/MOD constants — no change needed there.
**Implemented**: `bolt->s.clientNum = self->s.number;` added right before
`return bolt;`, same as `fire_plasma`/`fire_grenade`. Downstream consumer
unconfirmed.
