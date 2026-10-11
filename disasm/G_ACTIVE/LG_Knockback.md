# DFLAG_LG_KNOCKBACK — Lightning Gun splash-knockback mechanic (full decode)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Consumer chain (corrected):** `PM_Weapon` `EV_FIRE_WEAPON` → `0x1a63a` (`ClientEvents`, called from `ClientThink_real` `0x1a866`) → `0x29497` (`FireWeapon`) → `0x291f4` (LG beam-trace/damage/
knockback-spawn loop) → `0x22880` (phantom "rocket" spawner, previously documented
as inert in [../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md))

---

## Why this exists

`func_0x4290` (`SetLGKnockback`, see [func_0x4290.md](func_0x4290.md)) packs the
`df_lg_Knockback` cvar into `ps.stats[STAT_DEFRAG_FLAGS]` bit `0x1000`
(`DFLAG_LG_KNOCKBACK`). Prior to this pass, nothing in the QVM was found to *read*
that bit — `oDFs`'s `g_active.c` explicitly left it unset with a
"not yet implemented" comment. This pass traced the actual consumer.

**Headline finding: `DFLAG_LG_KNOCKBACK` does not gate a simple velocity kick. It
gates whether Lightning Gun hits also spawn an invisible "fake rocket" at the
beam's impact point, which detonates through the exact same
`G_RunMissile`→`G_MissileImpact`→`G_ExplodeMissile`→`G_RadiusDamage` splash pipeline
already fully documented for real rockets.** The beam-trace/direct-hit damage loop
itself (`func_0x291f4`) runs on **every LG-holding frame regardless of the flag** —
the flag only controls whether the *bonus splash entity* gets spawned in addition.

---

## `func_0x1a63a` — **CORRECTED: this is `ClientEvents`**, see [ClientEvents.md](ClientEvents.md). The text below is the superseded earlier reading (MOD_* switch is really an EV_* switch; `0xb`/`0xc` are EV_FALL_MEDIUM/FAR).

**Address:** `0x1a63a`, called only from `0x1a866` (Pmove outer)

A large (~300+ instruction) function called every `Pmove()` invocation, gated on the
player's `s.eType == ET_PLAYER` and switching on a value derived from `arg1` (an
integer in the range checked against `0xb`(11)/`0xc`(12)/`0x17`(23)/`0x19`(25)/
`0x1a`(26) — consistent with `MOD_LIGHTNING`/`MOD_BFG`/`MOD_NAIL`/
`MOD_PROXIMITY_MINE`/`MOD_KAMIKAZE` in the standard `meansOfDeath_t` enum, though
the exact source of `arg1` and the other 4 branches were **not decoded this pass** —
out of scope beyond the Lightning Gun branch needed to resolve `DFLAG_LG_KNOCKBACK`).
The `0xb` (`MOD_LIGHTNING`) branch leads to `func_0x29497`.

This confirms DeFRaG runs Lightning Gun's continuous-beam damage **from inside
`Pmove`**, not from `FireWeapon`/`EV_FIRE_WEAPON` the way vanilla Q3 does — consistent
with the architecture already noted in [README.md](README.md) ("DeFRaG relocates
per-frame client logic into Pmove outer").

**Not fully decoded**: the other switch branches — out of scope for the LG-knockback
question, but partially probed in a later pass:
- **`0xc` (12) shares the exact same dispatch target as `0xb`/`MOD_LIGHTNING` (`0x1a688`)**
  — the shared block then re-tests `local2c != 0xc` to pick between two constants
  (`local48 = 10` if not `0xc`, i.e. for the LG case; `local48 = 5` if `0xc`) before
  falling into what was documented as `func_0x29497`. This means the "quad-scale
  setup" entry point is NOT LG-exclusive as originally implied — it's shared with
  whatever mod value `0xc` represents (possibly `MOD_RAILGUN`=12 in the standard
  enum, unconfirmed). Not independently verified which weapon `0xc` corresponds to,
  nor whether the shared downstream code (`func_0x291f4`'s beam-trace loop) behaves
  sensibly for that second case — flagged, not chased further.
- `0x17`(23) and `0x19`(25) branches (`0x1a6d5`/`0x1a6dd`) lead into code reading
  `client+0x158`/`client+0x15c` (unidentified fields, plausibly extended
  powerup/ammo-style slots) and calling an unidentified helper (`0x4aed`) to select
  between constants `7`/`8` — shape suggests a per-weapon-slot active-state check,
  not obviously velocity/knockback-relevant. Not decoded further.
- `0x1a`(26) branch (`0x1a797`) not examined at all this pass.

---

> **RETRACTION (read first):** this and the preceding `0x1a63a` section
> were built on a wrong identity. `0x1a63a` = `ClientEvents`; `0x29497` =
> `FireWeapon`, whose only caller is `ClientEvents`' `EV_FIRE_WEAPON` case
> (verified: `0x29497` is referenced only at `0x1a6d8`). Claims below that DeFRaG
> runs LG from inside Pmove / calls `FireWeapon` once per frame are FALSE. The
> `0x291f4`/`0x22880` flag-gated phantom-rocket findings are unaffected.

## `func_0x29497` — quad-damage scale setup

**CORRECTION (from a later `disasm/G_WEAPON/` pass):** `0x29497` is not a
Pmove-only helper — **it is `FireWeapon` itself**, confirmed via its full
`switch(ent->s.weapon)` decode (all 10 vanilla cases present, dispatching to
`Weapon_Gauntlet`/`Bullet_Fire`/`weapon_supershotgun_fire`/etc., with `case
WP_LIGHTNING` calling `func_0x291f4` below exactly as described here). It has
**two callers**: the Pmove-embedded per-frame chain documented in this file
(`0x1a866`→`0x1a63a`→`0x29497`), **and** the normal client
fire-weapon-command path (i.e. it is the same function vanilla calls from
`EV_FIRE_WEAPON`/`ClientThink`). The description below (quad-scale setup then
falling into `func_0x291f4`) is accurate for the Lightning Gun case
specifically, but incomplete as a description of `0x29497` as a whole — see
[../G_WEAPON/FireWeapon.md](../G_WEAPON/FireWeapon.md) for the full decode.
This also means the "reached via the Pmove-embedded path, not
`FireWeapon`/`EV_FIRE_WEAPON`" framing a few lines above is **not quite
right**: `func_0x291f4` (the beam-trace/damage function, i.e. DeFRaG's
`Weapon_LightningFire`-equivalent) is reached via **both** paths — it is
`FireWeapon`'s normal `case WP_LIGHTNING` handler, and DeFRaG *additionally*
invokes the whole `FireWeapon` function once per Pmove frame while the LG is
held, for continuous per-tick damage application. It was not removed or
replaced; a second caller was added.

**Address:** `0x29497`, called from `0x1a63a` **and from the normal client
weapon-fire path** (see correction above)
**Args:** `arg0` = shooter `gentity_t*`

```c
void func_0x29497( gentity_t *ent ) {
    if ( ent->client-><field+0x13c> != 0 )   // suspected ps.powerups[PW_QUAD] or similar — not independently confirmed
        *(float*)0x1a205c = 3.0f;   // 0x40400000 — QUAD_DAMAGE_SCALE
    else
        *(float*)0x1a205c = 1.0f;   // 0x3f800000
    // ... additional ammo/state bookkeeping not decoded (0x294aa onward) ...
    func_0x291f4( ent );
}
```

`client + 0x13c` is checked for non-zero to select the 3.0/1.0 scale later applied
inside `func_0x291f4` (both to the beam-trace direct-hit damage *and* to the phantom
missile's `damage`/`splashDamage`). The exact field has not been independently
confirmed against the known `gclient_t`/`playerState_t` offset table — flagged as
unresolved rather than assumed, but the **3.0/1.0 values and their role as a
quad-damage-style multiplier are confirmed** via the literal float constants.

---

## `func_0x291f4` — beam-trace damage loop + knockback-spawn gate

**Address:** `0x291f4`, called only from `0x29497`
**Args:** `arg0` = shooter `gentity_t*`

```c
void func_0x291f4( gentity_t *ent ) {
    if ( ent->client->ps.stats[STAT_DEFRAG_FLAGS] & DFLAG_LG_KNOCKBACK ) {
        gentity_t *bolt = func_0x22880( ent, /*origin-ish*/ 0x1a202c, /*dir-ish*/ 0x1a2050 );
        bolt->damage       = Q_ftol( (float)bolt->damage       * (*(float*)0x1a205c) );  // 0 * scale = 0 (no direct-hit damage)
        bolt->splashDamage = Q_ftol( (float)bolt->splashDamage * (*(float*)0x1a205c) );  // 45 * scale = 45 or 135 (quad)
        damage = /* uninitialized/stale local — see note below */;
    } else {
        damage = Q_ftol( 8.0f * (*(float*)0x1a205c) );  // 8 or 24 (quad) — baseline per-segment LG damage
    }

    vec3_t origin = { *(float*)0x1a202c, *(float*)0x1a2030, *(float*)0x1a2034 };  // beam start, presumably ps.origin+eye
    int passent = ent->s.number;
    int step = 0;

    do {
        // advance the trace endpoint another ~12 units along the beam direction
        // (0x1a2050/0x1a2054/0x1a2058 — presumably the fixed beam direction vector)
        vec3_t end = { *(float*)0x1a202c + 12.0f * (*(float*)0x1a2050),
                       *(float*)0x1a2030 + 12.0f * (*(float*)0x1a2054),
                       *(float*)0x1a2034 + 12.0f * (*(float*)0x1a2058) };
        trace_t tr;
        trap_Trace( &tr, origin, NULL, NULL, end, passent, CONTENTS_SOLID|... /* 0x6000001 */ );

        if ( tr.entityNum != ENTITYNUM_WORLD ) {
            gentity_t *hit = &g_entities[tr.entityNum];
            if ( hit->takedamage ) {
                G_Damage( hit, ent, ent, /*dir*/(vec3_t*)0x1a2050, /*point*/&tr_endpos, damage, 0, MOD_LIGHTNING );
            }
            if ( hit->takedamage && hit->client ) {
                // spawn blood-spray hit-effect temp entity (event 0x32)
            } else if ( /* surfaceFlags & SURF_NOIMPACT-ish check */ ) {
                // spawn miss-spark temp entity (event 0x33)
            }
        }
        step++;
    } while ( step < 10 );
}
```

**Confirmed facts** (from direct instruction reads, not inferred):
- The `stats[12] & 0x1000` gate (`0x291fd`-`0x291fe`) selects between spawning the
  phantom missile (flag set) or computing a plain `8.0 * scale` damage value (flag
  clear) — **both paths then fall through into the same 10-iteration beam-trace loop**.
- The loop runs **exactly 10 iterations** (`local74 < 10`, confirmed at `0x29330`-`0x29331`).
- Each iteration calls `trap_Trace` (`-0x19`, confirmed) and, on a hit against a
  `takedamage`-able entity, calls `G_Damage` (`0x2015b`, confirmed) with
  `mod = 0xb = MOD_LIGHTNING` (confirmed) — this is a genuine second/repeated damage
  application per LG frame, separate from whatever direct-hit damage vanilla-style
  `LightningFire` would normally apply (that code was not located/decoded this pass;
  out of scope).
- **Open question, flagged not guessed:** in the flag-set path, no explicit
  `damage = ...` assignment was found before entering the shared loop — the
  `local7c` slot used as the `G_Damage` damage argument may be reusing a stale
  stack value from the `func_0x22880` call sequence. This was not resolved further;
  worth re-checking against oDFs test behavior rather than guessing a formula.

---

## `func_0x22880` — phantom "rocket" spawner (re-examined, corrects prior conclusion)

**Address:** `0x22880`. Previously documented in
[../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md)
as "a harmless, self-cleaning marker entity... **the `damage`/`splashDamage`/
`splashRadius`/`methodOfDeath` fields are set but never consumed**". That
conclusion was correct as far as the entity's *own* `think` callback goes (still
confirmed to be a two-instruction `G_FreeEntity`-only wrapper) — but incomplete,
because it didn't check whether the entity is independently processed by the
**generic missile pipeline** regardless of its own think function.

**Re-confirmed this pass, field-by-field, directly from the disassembly:**

| Offset | Field | Value |
|--------|-------|-------|
| `+0x4` | `s.eType` | **`3` = `ET_MISSILE`** (confirmed at `0x228a4`-`0x228a9`) |
| `+0x20c` | `classname` | **`"rocket"`** (string literal `0x8fb1`, confirmed at `0x22891`-`0x22894`) |
| `+0x2b0` | `nextthink` | `level.time + 10` |
| `+0x2b4` | `think` | `0x22877` (the `G_FreeEntity`-only wrapper) |
| `+0x1a8` | `svFlags` | `0x80` |
| `+0xc0` | `s.weapon` | `6` = `WP_LIGHTNING` |
| `+0x200` | `r.ownerNum` | shooter's entity number |
| `+0x2f0` | `methodOfDeath` | `0xb` = `MOD_LIGHTNING` |
| `+0x23c` | clipmask | `0x6000001` |
| `+0x258` | `parent` | shooter entity |

**This closes the mystery.** Because `s.eType = ET_MISSILE` and `classname = "rocket"`
(not `"grapple"`), this entity is picked up and processed by `G_RunMissile`
(`0x223cb`, already fully decoded in
[../G_MISSILE/G_RunMissile.md](../G_MISSILE/G_RunMissile.md)) on the very next
server frame exactly like a real rocket — including the impact-trace detection that
calls `G_MissileImpact` → `G_ExplodeMissile` → `G_RadiusDamage` (also already fully
documented for rockets). Since `classname == "rocket"` ≠ `"grapple"`, the
grapple-special-case branch inside `G_MissileImpact` (documented in
[../G_MISSILE/README.md](../G_MISSILE/README.md)) does **not** apply — this entity
takes the **normal** rocket-splash impact path.

**Conclusion: `DFLAG_LG_KNOCKBACK` makes Lightning Gun hits spawn an invisible,
zero-direct-damage "fake rocket" at the beam impact point, with `splashDamage=45`
(or `135` under Quad Damage) and `splashRadius=46`, `methodOfDeath=MOD_LIGHTNING`
(so death messages still attribute the kill to lightning, not rockets). It detonates
through the exact same splash-damage/knockback pipeline already documented for real
rockets — giving the Lightning Gun genuine area-effect splash knockback.** The 10ms
`think`/`G_FreeEntity` is a safety-net cleanup in case the phantom missile somehow
never registers an impact (e.g., spawned pointing into open air) — normal operation
is expected to detonate via collision well before the 10ms fuse expires.

---

## `func_0x22880`'s `trDelta` — resolved (supersedes the "12.0 incremental march" guess above)

Continued reading past the field table above found the actual `trDelta` assignment:
`trDelta[i] = arg2[i] * 0x461c4000`, and `0x461c4000` decodes to **exactly `10000.0`**.
Combined with `trBase = arg1` (a `VM_BlockCopy` of the `start` argument) and the
generic `fire_*` signature `func_0x22880(self, start, dir)` matching `fire_rocket`
exactly, this confirms `func_0x22880` is fired from the **muzzle**, along the **aim
direction**, as a real (if extremely fast) projectile — not spawned already-stationary
at the trace's hit point. The `12.0`-per-step figures in `func_0x291f4`'s beam-trace
loop pseudocode above describe a *separate* mechanism (the direct hit-scan damage
loop) and should not be confused with the phantom missile's own travel, which is a
one-shot 10,000 u/s launch handled entirely by the normal missile pipeline afterward.

---

## oDFs implementation notes

**Implemented.** `g_missile.c`'s `fire_lg_knockback` (new) spawns the phantom
entity using the field values confirmed above, matching `fire_rocket`'s structure
(`classname="rocket"`, `eType=ET_MISSILE`, `trDelta = dir * 10000`, `damage=0`,
`splashDamage=45` scaled by `s_quadFactor` at the call site, `splashRadius=46`,
`methodOfDeath=MOD_LIGHTNING`, `think=G_FreeEntity` at a 10ms fuse). Called from
`Weapon_LightningFire` in `g_weapon.c`, gated on the new `df_lg_Knockback` cvar,
fired from the muzzle along `forward` **before** the existing hit-scan trace loop
runs — matching the QVM's actual order and independent of whether that trace connects.

**The always-on 10-iteration beam-trace loop was not otherwise touched**: oDFs's
pre-existing `Weapon_LightningFire` already implements a 10-iteration
`trap_Trace`/`G_Damage(..., MOD_LIGHTNING)` loop with `damage = 8 * s_quadFactor` —
a near-exact match for `func_0x291f4`'s non-knockback baseline path (the QVM's
version bounces through multiple entities per shot the same way vanilla's
MISSIONPACK invulnerability-sphere bounce does; oDFs's loop already has the same
shape via its `for`/`continue`/`break` structure). No changes were needed there —
confirming the earlier "cross-check before assuming a gap" caution above was
warranted: the loop wasn't a gap at all, only the phantom-missile spawn was.
