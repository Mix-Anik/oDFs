# fire_grenade — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0002259a` (vanilla baseq3 equivalent at `0x1a9f8`, hash `1cdf561a`)
**DeFRaG hash:** unique to this function (differs from `fire_plasma`/`fire_bfg`'s
shared `7ab22a35`, matching the vanilla pattern where grenade's extra
`eFlags`/`TR_GRAVITY` shape already makes it structurally distinct).
**Size:** `0xec` (236) instructions in DeFRaG vs vanilla's declared size for
this shape (`227`, i.e. `236 - 9`) — **+9 instructions**, same delta as
`fire_plasma`/`fire_bfg`.

---

## Headline finding

**Fully decoded, instruction-by-instruction, zero unresolved bytes.** Like
`fire_plasma`, the entire function is **linear code — no branch/jump
instructions anywhere**, and **no reference to `df_promode` (`0x1317c4`) or
any other cvar/flag** exists. Every constant matches vanilla `fire_grenade`
(`code/game/g_missile.c` in oDFs, used as the vanilla-baseline reference)
exactly:

```c
bolt->s.eFlags             = 0x20   ; EF_BOUNCE_HALF (confirmed matches oDFs bg_public.h define)
bolt->damage               = 100    ; confirmed literal 0x64
bolt->splashDamage         = 100    ; confirmed literal 0x64
bolt->splashRadius         = 150    ; confirmed literal 0x96
bolt->methodOfDeath        = 4      ; MOD_GRENADE
bolt->splashMethodOfDeath  = 5      ; MOD_GRENADE_SPLASH
bolt->s.weapon             = 4      ; WP_GRENADE_LAUNCHER
bolt->s.pos.trType         = 5      ; TR_GRAVITY (confirmed enum value)
trDelta = dir * 700.0                ; confirmed literal 0x442f0000, unconditional
```

**The entire +9-instruction delta vs vanilla is, again, exactly one extra
unconditional field write appended right before `return bolt`**, identical in
shape and position to the one found in `fire_plasma`/`fire_bfg`/`fire_rocket`
(see [README.md](README.md)):

```c
bolt->s.clientNum = self->s.number;   // NEW vs vanilla — not present in oDFs/vanilla g_missile.c
```

8-instruction block at `0x2267a`–`0x22680` (`local`, `load4`, `const 0xa8`,
`add`, `local`(arg0), `load4`, `load4`, `store4`), writing offset `0xa8` =
`entityState_t.clientNum` (struct-offset math confirmed identically to
`fire_plasma.md`'s "Struct offset confirmation" section — not repeated here).
**Unconditional, no gating.**

---

## Structure

### 1. Setup (`0x2259a`–`0x225a3`)

```c
VectorNormalize(dir);       // call 0x9d12
bolt = G_Spawn();           // call 0x283de
```

### 2. Field setup, fully unconditional (`0x225a5`–`0x22671`)

```c
bolt->classname            = "grenade"
bolt->nextthink            = level.time + 2500   ; 0x9c4
bolt->think                = G_ExplodeMissile    ; 0x22066
bolt->s.eType              = ET_MISSILE (3)
bolt->r.svFlags            = SVF_USE_CURRENT_ORIGIN (0x80)
bolt->s.weapon             = WP_GRENADE_LAUNCHER (4)
bolt->s.eFlags             = EF_BOUNCE_HALF (0x20)
bolt->r.ownerNum           = self->s.number
bolt->parent               = self
bolt->damage               = 100
bolt->splashDamage         = 100
bolt->splashRadius         = 150   ; 0x96
bolt->methodOfDeath        = 4     ; MOD_GRENADE
bolt->splashMethodOfDeath  = 5     ; MOD_GRENADE_SPLASH
bolt->clipmask             = 0x6000001   ; MASK_SHOT
bolt->target_ent           = NULL
bolt->s.pos.trType         = TR_GRAVITY (5)
bolt->s.pos.trTime         = level.time - 50   ; MISSILE_PRESTEP_TIME
VectorCopy(start, bolt->s.pos.trBase)
bolt->s.pos.trDelta        = dir * 700.0   ; 0x442f0000, all 3 axes, unconditional
SnapVector(bolt->s.pos.trDelta)
VectorCopy(start, bolt->r.currentOrigin)
```

Field-for-field, value-for-value match to vanilla `fire_grenade`, including
the `s.eFlags = EF_BOUNCE_HALF` write (present in vanilla too — this is not
part of the DeFRaG-specific delta, just a field `fire_plasma`/`fire_bfg`
don't have because vanilla doesn't set it for them either).

### 3. New: unconditional clientNum write (`0x2267a`–`0x22680`)

```c
bolt->s.clientNum = self->s.number;   // NOT present in vanilla
```

### 4. Return (`0x22681`–`0x22685`)

```c
return bolt;
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x2259a` | Function entry |
| `0x2259e` | `VectorNormalize(dir)` call |
| `0x225a2` | `G_Spawn()`-equivalent call |
| `0x225ab` | `classname = "grenade"` |
| `0x225b1` | `nextthink = level.time + 2500` |
| `0x225ba` | `think = G_ExplodeMissile` |
| `0x225c0` | `s.eType = 3` (ET_MISSILE) |
| `0x225c6` | `r.svFlags = 0x80` |
| `0x225cc` | `s.weapon = 4` (WP_GRENADE_LAUNCHER) |
| `0x225d2` | `s.eFlags = 0x20` (EF_BOUNCE_HALF) |
| `0x225d8` | `r.ownerNum = self->s.number` |
| `0x225e0` | `parent = self` |
| `0x225e7` | `damage = 100` |
| `0x225ed` | `splashDamage = 100` |
| `0x225f3` | `splashRadius = 150` |
| `0x225f9` | `methodOfDeath = 4` (MOD_GRENADE) |
| `0x225ff` | `splashMethodOfDeath = 5` (MOD_GRENADE_SPLASH) |
| `0x22605` | `clipmask = 0x6000001` |
| `0x2260b` | `target_ent = NULL` |
| `0x22611` | `s.pos.trType = 5` (TR_GRAVITY) |
| `0x22617` | `s.pos.trTime = level.time - 50` |
| `0x22620` | `VectorCopy(start, trBase)` |
| `0x22627`/`0x22629` | `trDelta.x = dir.x * 700.0` (`0x442f0000`) |
| `0x22631`/`0x22633` | `trDelta.y = dir.y * 700.0` |
| `0x2263d`/`0x2263f` | `trDelta.z = dir.z * 700.0` |
| `0x22673` | `VectorCopy(start, r.currentOrigin)` |
| `0x2267a` | **`s.clientNum = self->s.number`** — the +9-instruction addition |

---

## oDFs implementation notes

oDFs's `fire_grenade` (`code/game/g_missile.c`) matches vanilla/DeFRaG exactly
for all damage/speed/radius/MOD/eFlags constants — no change needed there.
**Implemented**: `bolt->s.clientNum = self->s.number;` added right before
`return bolt;`, same as `fire_plasma`. Downstream consumer unconfirmed.
