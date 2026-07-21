# fire_rocket — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0002276a` (vanilla baseq3 equivalent at `0x1abb8`, hash `7100620e`)
**DeFRaG hash:** `2c347a52` — differs from vanilla, **and also differs from
DeFRaG's own `fire_plasma`/`fire_bfg`** (both `7ab22a35`), which in vanilla share
`fire_rocket`'s exact hash. This means DeFRaG gave `fire_rocket` extra,
rocket-specific code beyond whatever common change was applied to all three
`fire_*` functions.
**Size:** `0x10e` (270) instructions in DeFRaG vs `0xdd` (221) in vanilla —
**+49 instructions**, notably larger than `fire_plasma`/`fire_bfg` in DeFRaG
(`0xe6` = 230 instructions each, +9 over vanilla). The extra ~40 instructions in
`fire_rocket` beyond its siblings are exactly the promode speed branch described
below.

**Hashing caveat:** the hash values in `baseq3-game-functions.hmap` /
`qagame.dis`'s header appear to be computed over instruction **opcodes**, not
operand/immediate values — vanilla `fire_plasma`, `fire_bfg`, and `fire_rocket`
all share one hash (`7100620e`) despite obviously embedding different damage/
speed/model constants. **A hash match does NOT prove immediate/constant values
are identical — only that instruction shape/control-flow is identical.**
Constants must always be read directly from the bytecode, which is what this
analysis does below.

---

## Headline finding

DeFRaG's `fire_rocket` sets the rocket's `trDelta` (launch velocity) via an
explicit `df_promode`-conditional branch:

```c
if ( df_promode )
    trDelta = forward * 1000.0;   // confirmed literal 0x447a0000
else
    trDelta = forward * 900.0;    // confirmed literal 0x44610000
```

This is a **direct, binary-confirmed answer**: DeFRaG rockets travel at **1000
u/s under promode (CPM)** and **900 u/s otherwise (vanilla speed)**. This is not
an assumption or common community claim taken on faith — it was read directly
from the compiled bytecode's `const` immediates.

`damage`, `splashDamage`, and `splashRadius` are all set **unconditionally**
(not gated by promode) to vanilla's values:

```
bolt->damage        = 100   ; confirmed literal 0x64
bolt->splashDamage   = 100   ; confirmed literal 0x64
bolt->splashRadius   = 120   ; confirmed literal 0x78
bolt->methodOfDeath        = 6  ; MOD_ROCKET (confirmed matches oDFs bg_public.h enum)
bolt->splashMethodOfDeath  = 7  ; MOD_ROCKET_SPLASH (confirmed matches oDFs bg_public.h enum)
```

---

## Structure

### 1. Entity spawn + field setup (`0x2276a`–`0x227ee`)

```
VectorNormalize(dir)              ; call 0x9d12, confirmed, normalizes in-place on caller's arg2

bolt = G_Spawn()                  ; call 0x283de (identity inferred from context: zero-arg,
                                   ;   returns entity ptr subsequently populated as a gentity_t —
                                   ;   consistent with G_Spawn, not independently verified byte-by-byte)

bolt->classname          = "rocket"
bolt->nextthink          = level.time + 15000            ; (0x12a310 = level.time global, +0x3a98 offset ptr math)
bolt->think               = G_ExplodeMissile               ; address 0x22066 (confirmed = DeFRaG G_ExplodeMissile)
bolt->s.eType              = ET_MISSILE (3)
bolt->r.svFlags            = SVF_USE_CURRENT_ORIGIN (0x80)
bolt->s.weapon             = WP_ROCKET_LAUNCHER (5)
bolt->r.ownerNum           = self->s.number
bolt->parent               = self
bolt->damage               = 100
bolt->splashDamage         = 100
bolt->splashRadius         = 120
bolt->methodOfDeath        = 6   ; MOD_ROCKET
bolt->splashMethodOfDeath  = 7   ; MOD_ROCKET_SPLASH
bolt->clipmask             = 0x6000001                     ; MASK_SHOT-equivalent bitmask
bolt->target_ent           = NULL
bolt->s.pos.trType         = 2   ; TR_LINEAR
bolt->s.pos.trTime         = level.time - 50               ; MISSILE_PRESTEP_TIME(0x32=50)
VectorCopy(start, bolt->s.pos.trBase)                       ; block_copy, 0xc bytes
```

Exact match to vanilla `fire_rocket`'s entity setup, field-for-field, confirmed
via literal constants.

### 2. `df_promode`-conditional speed (`0x227ef`–`0x22839`)

```
if (df_promode_cvar_cache(0x1317c4) != 0):
    trDelta[0] = 1000.0 * dir[0]
    trDelta[1] = 1000.0 * dir[1]
    trDelta[2] = 1000.0 * dir[2]
else:
    trDelta[0] = 900.0 * dir[0]
    trDelta[1] = 900.0 * dir[1]
    trDelta[2] = 900.0 * dir[2]
```

Confirmed literal float constants: `0x447a0000` = **1000.0**, `0x44610000` =
**900.0**.

**RE-VERIFIED (follow-up pass):** the full `damage`/`splashDamage`/`splashRadius`
setup at `0x227b1`-`0x227cb` was confirmed to execute unconditionally, entirely
*before* the `df_promode` branch point at `0x227ef` — there is no code path in
which `df_promode` could have already influenced these fields. The function was
also re-read from the `df_promode` branch (`0x227ef`) through to `return bolt`
(`0x22869`) instruction-by-instruction, confirming **no subsequent write** to
`damage`, `splashDamage`, or `splashRadius` anywhere after the initial
unconditional set — only `trDelta` (the two speed-branch arms) and the
`SnapVector`/`VectorCopy` finish touch anything after that point. Also
confirmed: no reference to the `df_promode` cache address (`0x1317c4`) exists
anywhere else in the function outside the one documented branch point. This
rules out a promode-conditional damage/splash-radius multiplier that a
speed-only investigation could have missed.

### 3. SnapVector + finish (`0x22839`–`0x22869`)

```
trDelta[0] = (float)Q_ftol(trDelta[0])   ; per-axis SnapVector-equivalent rounding
trDelta[1] = (float)Q_ftol(trDelta[1])
trDelta[2] = (float)Q_ftol(trDelta[2])
VectorCopy(start, bolt->r.currentOrigin)   ; block_copy, 0xc bytes
return bolt
```

Matches vanilla's `SnapVector(bolt->s.pos.trDelta)` + `VectorCopy(start,
bolt->r.currentOrigin)`.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x2276a` | Function entry |
| `0x2276e` | `VectorNormalize(dir)` call (`0x9d12`) |
| `0x22772` | `G_Spawn()`-equivalent call (`0x283de`, not independently verified) |
| `0x227b1` | `bolt->damage = 100` |
| `0x227b7` | `bolt->splashDamage = 100` |
| `0x227bf` | `bolt->splashRadius = 120` |
| `0x227c5` | `bolt->methodOfDeath = 6` (MOD_ROCKET) |
| `0x227cb` | `bolt->splashMethodOfDeath = 7` (MOD_ROCKET_SPLASH) |
| `0x227ef` | **`df_promode` branch point** (`0x1317c4`) |
| `0x227f7` | promode path: `trDelta = dir * 1000.0` (`0x447a0000`) |
| `0x2281b` | non-promode path: `trDelta = dir * 900.0` (`0x44610000`) |
| `0x1317c4` | `df_promode` cvar cache (confirmed, matches README) |

---

## oDFs implementation notes

**Confirmed difference — FIXED.** oDFs's `fire_rocket`
(`code/game/g_missile.c`) unconditionally used `VectorScale( dir, 900,
bolt->s.pos.trDelta )`. Changed to:

```c
if ( df_promode.integer ) {
    VectorScale( dir, 1000, bolt->s.pos.trDelta );
} else {
    VectorScale( dir, 900, bolt->s.pos.trDelta );
}
```

`damage = 100`, `splashDamage = 100`, `splashRadius = 120` were already correct
in oDFs and required no change.
