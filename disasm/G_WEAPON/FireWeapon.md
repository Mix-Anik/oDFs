# FireWeapon — Defrag QVM disassembly analysis (full decode + identity correction)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00029497` (vanilla baseq3 equivalent at `0x209e2`, hash `307ab284`)
**DeFRaG hash:** `31f81ab5` — differs from vanilla.
**Size:** `0xd9` (217) instructions in DeFRaG vs `0xda` (218) in vanilla — effectively
unmodified in overall size (the usual "+1" instruction-count drift seen on every
other confirmed-unmodified function in this module is *absent* here — DeFRaG is
one instruction *smaller*, consistent with a small net content change, not a
large structural rewrite).

---

## IMPORTANT CORRECTION to `disasm/README.md`

`disasm/README.md` previously listed `0x21b5e` as `FireWeapon`, based on the
observation that it contains a `switch`-like dispatch that calls `fire_rocket`
(`0x2276a`) in one branch. **This identification is corrected: `0x21b5e` is NOT
FireWeapon.** The real `FireWeapon` is `0x29497` (this document).

Evidence `0x21b5e` is not `FireWeapon`:
- It takes **3 arguments** (`arg0`, an unused `arg1` slot, and `arg2` — a
  **second `gentity_t*`**, used as a target/enemy reference). Vanilla
  `FireWeapon(gentity_t *ent)` takes exactly one argument.
- Its preamble computes an aim direction either by (a) `VectorSubtract`+
  `VectorNormalize` from a stored origin toward `arg0+0x300`'s target position
  when that pointer field is non-NULL, combined with a **per-weapon travel-speed
  table (700.0/900.0/2000.0 — matching vanilla grenade/rocket/plasma projectile
  speeds)** used to compute a lead-time correction, or (b) a stored direction
  (`arg0+0x2a4`) plus a `crandom()`-scaled jitter using a per-weapon "spread"
  field (`arg0+0x320`) when the target pointer is NULL. Vanilla `FireWeapon`
  does no such lead-prediction or per-weapon-speed math at all — that logic
  lives inside the individual `fire_*` functions in `g_missile.c`.
- Its `switch` dispatches on `arg0+0xc0` (weapon field) for **cases 4–9 only**
  (`GRENADE_LAUNCHER`..`BFG`) calling `fire_grenade` (`0x2259a`) / `fire_rocket`
  (`0x2276a`) **directly** — bypassing `weapon_grenadelauncher_fire` /
  `Weapon_RocketLauncher_Fire` entirely (including their "LAG CHEAT DETECTED"
  rate-limit gates, see below). It has no cases for GAUNTLET/MACHINEGUN/SHOTGUN/
  GRAPPLE.
- For case 4 (grenade), the spawned projectile gets a field written to
  `arg2` (the target pointer) directly, plus `(arg2 - g_entities_base) / sizeof(gentity_t)`
  (i.e. the target's entity index) stored in a second field — tagging the
  projectile with a target reference.
- `0x21b5e` itself is only referenced once in the whole QVM, at `0x21e99`,
  where its **address is stored as a function-pointer field** (`arg0+0x2c4`)
  on some entity — not called directly. The function doing that store
  (`0x21e94`) is itself called from 7 sites (`0x21f02`, `0x21f0d`, `0x21f18`,
  `0x21f23`, `0x21f2e`, `0x21f39`, `0x21f44`) and also zeroes that entity's
  `think`/`nextthink` fields (offsets `0x2b4`/`0x2b0`) in a sibling routine.

This is the structural signature of a **generic multi-weapon "shooter" trigger
entity** (vanilla Q3's `g_misc.c` has `shooter_rocket`/`shooter_grenade`/
`shooter_plasma` map entities that fire a projectile toward a fixed direction
or target when triggered) — **not** `g_weapon.c`'s player-invoked `FireWeapon`.
This is out of scope for this task (it belongs to `g_misc.c`, not `g_weapon.c`)
and was not investigated further. It is flagged here only to correct the prior
misidentification and to document why `fire_rocket`/`fire_grenade` have a
third call site beyond their normal wrapper functions.

---

## Confirmed structure of the real FireWeapon (`0x29497`)

### 1. Quad-damage scale factor (`0x29497`–`0x294aa`)

```c
if ( ent->client-><+0x13c> != 0 )   // PW_QUAD-style check, field not independently named
    s_quadFactor = 3.0f;            // 0x40400000 — HARDCODED, confirmed literal
else
    s_quadFactor = 1.0f;            // 0x3f800000
```

**Confirmed difference from vanilla:** vanilla sets `s_quadFactor =
g_quadfactor.value` (a tunable cvar) when quad is active. DeFRaG hardcodes
`3.0` directly — no cvar read anywhere in this function. (The exact same
hardcoded-3.0/1.0 pattern, using the same `ent->client+0x13c` field, is also
used in `CheckGauntletAttack`'s own quad check — see
[CheckGauntletAttack.md](CheckGauntletAttack.md) — so this is a deliberate,
QVM-wide DeFRaG simplification, not a one-off.) `0x1a205c` is the confirmed
`s_quadFactor` global (matches its use throughout `BFG_Fire`,
`Weapon_RocketLauncher_Fire`, `weapon_grenadelauncher_fire`,
`Weapon_Plasmagun_Fire`, and `func_0x291f4`/LG — see
[../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md)).

### 2. Accuracy-shots tracking (`0x294aa`–`0x294ca`)

```c
weapon = ent->s.weapon;   // arg0+0xc0
if ( weapon != 10 /*WP_GRAPPLING_HOOK*/ && weapon != 1 /*WP_GAUNTLET*/ )
    ent->client->accuracy_shots++;   // field at client+0x31c
```

Matches vanilla exactly (`if( ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET ) ent->client->accuracy_shots++;` — no `MISSIONPACK` nailgun special-case present, consistent with a non-missionpack build).

### 3. Aim setup (`0x294ca`–`0x294fc`)

```c
AngleVectors( ent->client->ps.viewangles, forward, right, up );   // call 0xa186, confirmed AngleVectors
CalcMuzzlePointOrigin( ent, ent->client->oldOrigin /* +0x2d4 */, forward, right, up, muzzle );  // call 0x2940d, confirmed
```

Both calls and field offsets match vanilla's `FireWeapon` exactly
(`AngleVectors(ent->client->ps.viewangles, forward, right, up);` then
`CalcMuzzlePointOrigin(ent, ent->client->oldOrigin, forward, right, up, muzzle);`).

### 4. Full `switch( ent->s.weapon )` — confirmed, all 10 cases decoded

Jump table at `0x294fc`, range **1–10** (matches vanilla's `WP_GAUNTLET`..
`WP_GRAPPLING_HOOK`, no case 0/`WP_NONE`, default falls straight to `leave`):

| Case | Weapon | Calls | Status |
|------|--------|-------|--------|
| 1 | GAUNTLET | `0x288ae` = `Weapon_Gauntlet` | ✅ hash+size identical to vanilla (empty function body) |
| 2 | MACHINEGUN | `0x289ef` = `Bullet_Fire(ent, 200.0, dmg)` — **only 3 args passed** (see [Bullet_Fire note](#bullet_fire-mod-parameter-removed)); `dmg` = 7 or 5 depending on `*(int*)0x1b1ec == 3` (`g_gametype==GT_TEAM`) | Dispatch site unmodified vs vanilla logic; callee simplified (see below) |
| 3 | SHOTGUN | `0x28d3c` = `weapon_supershotgun_fire` | ✅ hash+constants (4096.0 blast scale) confirmed identical to vanilla |
| 4 | GRENADE_LAUNCHER | `0x28dbb` = `weapon_grenadelauncher_fire` | ❌ modified — LAG CHEAT gate added, see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| 5 | ROCKET_LAUNCHER | `0x28e2b` = `Weapon_RocketLauncher_Fire` | ❌ modified — LAG CHEAT gate added, see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| 6 | LIGHTNING | `0x291f4` = `func_0x291f4` | ❌ modified — see [../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md); **resolves this task's open question, see below** |
| 7 | RAILGUN | `0x28ebf` = `weapon_railgun_fire` | ✅ hash+constants (100.0 damage, 8192.0 range, 4.0 offset) confirmed identical to vanilla |
| 8 | PLASMAGUN | `0x28e8b` = `Weapon_Plasmagun_Fire` | ✅ confirmed byte-for-byte identical to vanilla (calls `fire_plasma`=`0x224b5`, `damage`/`splashDamage` *= quadFactor, no lag-cheat gate — full read, see below) |
| 9 | BFG | `0x28b70` = `BFG_Fire` | ❌ modified — LAG CHEAT gate added, see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| 10 | GRAPPLING_HOOK | `0x290cf` = `Weapon_GrapplingHook_Fire` | ❌ modified — LAG CHEAT gate + redundant re-aim, see [Weapon_GrapplingHook_Fire.md](Weapon_GrapplingHook_Fire.md) |

All 10 call targets read directly from the bytecode (`const 1 <addr>` immediately
preceding each `call`), not inferred.

**`Weapon_Plasmagun_Fire` (case 8) full read — confirmed unmodified:**
```c
void Weapon_Plasmagun_Fire( gentity_t *ent ) {
    gentity_t *m = fire_plasma( ent, muzzle, forward );   // 0x224b5
    m->damage       = Q_ftol( (float)m->damage       * s_quadFactor );
    m->splashDamage = Q_ftol( (float)m->splashDamage * s_quadFactor );
}
```
No lag-cheat check, no extra branches — exact structural match to vanilla and
to `BFG_Fire`/`Weapon_RocketLauncher_Fire`'s *shared tail* (the part after
their added lag-cheat gate).

### `Bullet_Fire` mod parameter removed

DeFRaG's `Bullet_Fire` (`0x289ef`, hash `77685d53` — **hash-identical to
vanilla**) is called from `FireWeapon`'s case 2 with only **3 arguments**
(`ent`, `spread=200.0`, `damage`), not vanilla's 4 (`ent, spread, damage,
mod`). Directly confirmed by reading `Bullet_Fire`'s own body: **no
instruction anywhere in the function reads the 4th argument slot** (frame+0x14,
i.e. `0xcc` given its `enter 0xb8`) — the `G_Damage` call at the end passes a
**hardcoded literal `0x3` (`MOD_MACHINEGUN`, confirmed via oDFs
`bg_public.h`'s `meansOfDeath_t` enum)** as its `mod` argument (`Arg[0x24] =
0x3` at `0x28b5e`, immediately before the `call 0x2015b`/`G_Damage`).
`Bullet_Fire` was specialized to be machinegun-only (no `MISSIONPACK`
chaingun in this build), so this is functionally inert for oDFs (which also
never compiles chaingun/nailgun code) — noted for completeness, not a
behavioral bug. This is also a second confirmed instance of the documented
hashing caveat: identical hash despite a real signature/constant change.

### 5. `WP_LIGHTNING` open question — RESOLVED

The task's open question was whether DeFRaG's `Weapon_LightningFire`-equivalent
(reached via normal `FireWeapon`/`EV_FIRE_WEAPON`) still exists, given
[../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md) had already found
`func_0x291f4` (the beam-trace/damage loop) reachable from a **Pmove-embedded**
per-frame chain (`0x1a866`→`0x1a63a`→`0x29497`→`0x291f4`).

**Answer, directly confirmed by this decode: `0x29497` in that chain *is*
`FireWeapon` itself** (this document) — not a separate Pmove-only helper as
LG_Knockback.md's narrower trace had assumed. `func_0x291f4` is called from
`FireWeapon`'s own `case WP_LIGHTNING` (`0x29514`–`0x2951b`), **exactly
matching vanilla's `case WP_LIGHTNING: Weapon_LightningFire(ent); break;`**.

So DeFRaG did **not** remove or no-op the normal-path Lightning Gun fire
function — it is reached identically to vanilla through `FireWeapon`'s switch.
What DeFRaG *added* is a **second caller**: `func_0x1a63a` (a per-frame
continuous-weapon dispatcher called from inside `Pmove` outer, `0x1a866`) also
calls `FireWeapon` (`0x29497`) directly once per server frame while a player
holds the Lightning Gun — re-running the *entire* `FireWeapon` function
(quad-factor setup, accuracy tracking, aim/muzzle calc, and the
`WP_LIGHTNING` case) every tick, independent of the normal
`EV_FIRE_WEAPON`-triggered call. This is DeFRaG's mechanism for continuous
beam damage/knockback consistency; it is not a replacement for the normal
path, it is an addition to it.

**Correction to `LG_Knockback.md`:** that document's characterization of
`func_0x29497` as "quad-damage scale setup... called only from `0x1a63a`" is
superseded — `0x29497` is `FireWeapon`, called from both the Pmove per-frame
chain **and** the normal client fire-weapon-command path. Its full switch
table (this document) was not previously decoded.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x29497` | `FireWeapon` entry (corrected identity — was previously misattributed to `0x21b5e`) |
| `0x294a1` | Quad-factor branch (hardcoded 3.0/1.0, not `g_quadfactor.value`) |
| `0x294b4`/`0x294b8` | Accuracy-shots skip check (`weapon==10 \|\| weapon==1`) |
| `0x294d8` | `AngleVectors` call (`0xa186`) |
| `0x294f2` | `CalcMuzzlePointOrigin` call (`0x2940d`) |
| `0x294fc` | `switch(weapon)` dispatch start, range 1–10 |
| `0x21b5e` | **NOT FireWeapon** — unidentified `g_misc.c`-style multi-weapon "shooter" entity callback (out of scope, documented above only as a correction) |

---

## oDFs implementation notes

**Implemented.** `s_quadFactor = g_quadfactor.value;` replaced with a
hardcoded `s_quadFactor = 3.0f;` in `FireWeapon` (`code/game/g_weapon.c`),
matching DeFRaG exactly (the `g_quadfactor` cvar is no longer consulted for
the live multiplier, matching DeFRaG's behavior of ignoring it).

The Lightning Gun's per-frame re-invocation of `FireWeapon` from inside
Pmove is already implemented in oDFs per
[../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md)'s notes — no
further action needed from this document.
