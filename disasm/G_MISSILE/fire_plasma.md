# fire_plasma — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x000224b5` (vanilla baseq3 equivalent at `0x1a91b`, hash `7100620e`)
**DeFRaG hash:** `7ab22a35` — differs from vanilla. DeFRaG's `fire_bfg` shares this
exact hash, mirroring the vanilla pattern where `fire_plasma`/`fire_bfg` are
structurally identical to each other.
**Size:** `0xe6` (230) instructions in DeFRaG vs vanilla's declared size for
this shape (`0xdd`=221, same as vanilla `fire_rocket`/`fire_bfg`, all three
sharing hash `7100620e`) — **+9 instructions**.

**Hashing caveat:** as established in [fire_rocket.md](fire_rocket.md), the
hash is opcode-shape-based, not a content checksum. All constants below were
read directly from the bytecode `const` immediates, not inferred from the
hash.

---

## Headline finding

**Fully decoded, instruction-by-instruction, zero unresolved bytes.** The
entire function is **linear code — no branch/jump instructions exist anywhere
in it**, and there is **no reference to the `df_promode` cache (`0x1317c4`)
or any other cvar/flag**. Every damage/speed/radius/type constant is
**identical to vanilla** `fire_plasma` (`code/game/g_missile.c` in oDFs, used
here as the vanilla-baseline reference):

```c
bolt->damage               = 20     ; confirmed literal 0x14
bolt->splashDamage         = 15     ; confirmed literal 0xf
bolt->splashRadius         = 20     ; confirmed literal 0x14
bolt->methodOfDeath        = 8      ; MOD_PLASMA (confirmed matches oDFs bg_public.h enum)
bolt->splashMethodOfDeath  = 9      ; MOD_PLASMA_SPLASH
bolt->s.weapon             = 8      ; WP_PLASMAGUN
trDelta = dir * 2000.0              ; confirmed literal 0x44fa0000, unconditional (no promode branch)
```

**The entire +9-instruction delta vs vanilla is exactly one extra,
unconditional field write appended right before `return bolt`, after
`VectorCopy(start, bolt->r.currentOrigin)`:**

```c
bolt->s.clientNum = self->s.number;   // NEW vs vanilla — not present in oDFs/vanilla g_missile.c
```

This is an 8-instruction block (`local`, `load4`, `const 0xa8`, `add`,
`local`(arg0), `load4`, `load4`, `store4`) at `0x2258d`–`0x22594`. It sets the
plasma bolt's `entityState_t.clientNum` field (offset `0xa8` within
`entityState_t`, confirmed via byte-for-byte match against vanilla Q3's
`entityState_t` layout in `q_shared.h` — see "Struct offset confirmation"
below) to the shooter's own entity number. **No conditional gating of any
kind** — this write always executes. The same exact 8-instruction pattern,
at the same relative position, was also found in `fire_grenade`, `fire_bfg`,
and `fire_rocket` (see [README.md](README.md) for the cross-function
comparison) — this is a change common to all `fire_*` projectile spawners,
not specific to plasma.

The purpose of this `clientNum` write is **not confirmed** — it is not
damage/knockback-relevant (no `G_Damage`/`G_RadiusDamage` call reads it in
this function), and its downstream use (if any, e.g. a cgame-side visual/hit
effect) was not investigated as part of this task. Marked unknown rather than
guessed.

---

## Struct offset confirmation

Every field-write offset in this function was cross-checked against vanilla
Q3's real struct layouts (`entityState_t`, `entityShared_t`, `gentity_s` in
`ioq3/code/qcommon/q_shared.h`, `game/g_public.h`, `game/g_local.h`) by
computing byte offsets field-by-field and comparing to the disassembly's
literal `const` offsets. All matched exactly except a `+4`-byte shift on
`target_ent`/`nextthink`/`think` (see below):

| Offset | Field | Computed from vanilla struct | Confirmed by |
|--------|-------|-------------------------------|--------------|
| `0x4` | `s.eType` | `entityState_t.eType` | value `3` = ET_MISSILE |
| `0xc` | `s.pos.trType` | `entityState_t.pos.trType` | value `2` = TR_LINEAR |
| `0x10` | `s.pos.trTime` | `entityState_t.pos.trTime` | `level.time - 0x32(50)` |
| `0x18` | `s.pos.trBase` | `entityState_t.pos.trBase` | `VectorCopy(start, ...)` |
| `0x24` | `s.pos.trDelta` | `entityState_t.pos.trDelta` | `dir * 2000.0` |
| `0xa8` | `s.clientNum` | `entityState_t.clientNum` | `self->s.number` (**the new write**) |
| `0xc0` | `s.weapon` | `entityState_t.weapon` | value `8` = WP_PLASMAGUN |
| `0x1a8` | `r.svFlags` | `entityShared_t.svFlags` | value `0x80` = SVF_USE_CURRENT_ORIGIN |
| `0x1e8` | `r.currentOrigin` | `entityShared_t.currentOrigin` | `VectorCopy(start, ...)` |
| `0x200` | `r.ownerNum` | `entityShared_t.ownerNum` | `self->s.number` |
| `0x20c` | `classname` | `gentity_s.classname` | `"plasma"` string pointer |
| `0x23c` | `clipmask` | `gentity_s.clipmask` | `0x6000001` = MASK_SHOT |
| `0x258` | `parent` | `gentity_s.parent` | `self` pointer copy |
| `0x29c` | `target_ent` | *(computed vanilla offset would be `0x298`)* | value `0` — **shifted +4 vs a naive vanilla layout calc** |
| `0x2b0`/`0x2b4` | `nextthink`/`think` | *(computed vanilla offsets `0x2ac`/`0x2b0`)* | **also shifted +4** |
| `0x2e4`/`0x2e8`/`0x2ec` | `damage`/`splashDamage`/`splashRadius` | `gentity_s` | `20`/`15`/`20` |
| `0x2f0`/`0x2f4` | `methodOfDeath`/`splashMethodOfDeath` | `gentity_s` | `8`/`9` = MOD_PLASMA/MOD_PLASMA_SPLASH |

**Note on the `+4` shift:** `target_ent` and everything after it
(`nextthink`, `think`) sit exactly 4 bytes later than a naive offset
calculation from vanilla ioq3's `gentity_s` would predict, while everything
up through `parent` (`0x258`) matches vanilla exactly with no shift. This
implies DeFRaG's `gentity_t` has **one extra 4-byte field inserted somewhere
between `parent` and `target_ent`** (i.e. somewhere among `nextTrain`,
`prevTrain`, `pos1`, `pos2`, `message`, `timestamp`, `target`, `targetname`,
`team`, `targetShaderName`, `targetShaderNewName` — none of which are touched
by this function, so the exact insertion point and the new field's identity
are **not confirmed**, only that a 4-byte insertion exists somewhere in that
range). This does not add any instructions to `fire_plasma` itself (the
shifted fields are written with the same instruction shape regardless of the
literal offset value) — it's a struct-layout observation only.

---

## Structure

### 1. Setup (`0x224b5`–`0x224be`)

```c
VectorNormalize(dir);       // call 0x9d12
bolt = G_Spawn();           // call 0x283de
```

### 2. Field setup, fully unconditional (`0x224c0`–`0x22586`)

```c
bolt->classname            = "plasma"
bolt->nextthink            = level.time + 10000
bolt->think                = G_ExplodeMissile           ; 0x22066
bolt->s.eType              = ET_MISSILE (3)
bolt->r.svFlags            = SVF_USE_CURRENT_ORIGIN (0x80)
bolt->s.weapon             = WP_PLASMAGUN (8)
bolt->r.ownerNum           = self->s.number
bolt->parent               = self
bolt->damage               = 20
bolt->splashDamage         = 15
bolt->splashRadius         = 20
bolt->methodOfDeath        = 8   ; MOD_PLASMA
bolt->splashMethodOfDeath  = 9   ; MOD_PLASMA_SPLASH
bolt->clipmask             = 0x6000001   ; MASK_SHOT
bolt->target_ent           = NULL
bolt->s.pos.trType         = TR_LINEAR (2)
bolt->s.pos.trTime         = level.time - 50   ; MISSILE_PRESTEP_TIME
VectorCopy(start, bolt->s.pos.trBase)
bolt->s.pos.trDelta        = dir * 2000.0   ; 0x44fa0000, all 3 axes, unconditional
SnapVector(bolt->s.pos.trDelta)             ; per-axis Q_ftol round-trip
VectorCopy(start, bolt->r.currentOrigin)
```

All of this is a field-for-field, value-for-value match to vanilla
`fire_plasma`.

### 3. New: unconditional clientNum write (`0x2258d`–`0x22594`)

```c
bolt->s.clientNum = self->s.number;   // NOT present in vanilla
```

### 4. Return (`0x22595`–`0x22599`)

```c
return bolt;
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x224b5` | Function entry |
| `0x224b9` | `VectorNormalize(dir)` call (`0x9d12`) |
| `0x224bd` | `G_Spawn()`-equivalent call (`0x283de`) |
| `0x224c6` | `classname = "plasma"` |
| `0x224cd` | `nextthink = level.time + 10000` |
| `0x224d6` | `think = G_ExplodeMissile` |
| `0x224db` | `s.eType = 3` (ET_MISSILE) |
| `0x224e2` | `r.svFlags = 0x80` |
| `0x224e8` | `s.weapon = 8` (WP_PLASMAGUN) |
| `0x224ed` | `r.ownerNum = self->s.number` |
| `0x224f5` | `parent = self` |
| `0x224fc` | `damage = 20` |
| `0x22502` | `splashDamage = 15` |
| `0x22508` | `splashRadius = 20` |
| `0x2250e` | `methodOfDeath = 8` (MOD_PLASMA) |
| `0x22514` | `splashMethodOfDeath = 9` (MOD_PLASMA_SPLASH) |
| `0x2251a` | `clipmask = 0x6000001` |
| `0x22520` | `target_ent = NULL` |
| `0x22526` | `s.pos.trType = 2` (TR_LINEAR) |
| `0x2252c` | `s.pos.trTime = level.time - 50` |
| `0x22535` | `VectorCopy(start, trBase)` |
| `0x2253c`/`0x2253e` | `trDelta.x = dir.x * 2000.0` (`0x44fa0000`) |
| `0x22546`/`0x22548` | `trDelta.y = dir.y * 2000.0` |
| `0x22552`/`0x22554` | `trDelta.z = dir.z * 2000.0` |
| `0x22588` | `VectorCopy(start, r.currentOrigin)` |
| `0x2258f` | **`s.clientNum = self->s.number`** — the +9-instruction addition |

---

## oDFs implementation notes

oDFs's `fire_plasma` (`code/game/g_missile.c`) matches vanilla/DeFRaG exactly
for all damage/speed/radius/MOD constants — no change needed there.
**Implemented**: `bolt->s.clientNum = self->s.number;` added right before
`return bolt;`, matching the confirmed DeFRaG addition. Downstream consumer
(if any, e.g. a cgame-side effect) remains unconfirmed.
