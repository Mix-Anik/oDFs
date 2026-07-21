# G_COMBAT — Defrag QVM disassembly analysis

Analysis of `g_combat.c`-equivalent functions (damage, knockback, radius
damage) from the Defrag qagame.qvm. Investigates the empirically-observed
"CPM self-splash rocket jumps are stronger than vanilla math predicts" claim.

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md).

---

## Method

`baseq3-game-functions.hmap` (a hash→name map built from unmodified vanilla
baseq3 `qagame`) was cross-referenced against the function hash table at the
top of `qagame.dis`. Vanilla function order is preserved in DeFRaG's compiled
module, so functions were located by walking both tables in lock-step,
allowing for small address drift caused by earlier size-changed functions.
A hash mismatch at the matching table position = confirmed modified function;
a hash match = confirmed byte-identical (opcode-shape-identical — see caveat
in [../G_MISSILE/fire_rocket.md](../G_MISSILE/fire_rocket.md)) to vanilla.

## Functions analyzed

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `G_Damage` | `0x2015b` | `0x18b18` | ❌ modified (+309 instr) | ✅ Analyzed — see [G_Damage.md](G_Damage.md) |
| `CanDamage` | `0x20648` | `0x18ed1` | ✅ identical | ✅ Confirmed unmodified — see [CanDamage.md](CanDamage.md) |
| `G_RadiusDamage` | `0x20773` | `0x18ffc` | ❌ modified (+2 instr, minor) | ✅ Analyzed — see [G_RadiusDamage.md](G_RadiusDamage.md) |

## Headline findings

### G_Damage — CONFIRMED promode self-splash knockback boost

Under `df_promode` (cvar cache `0x1317c4`) **and** self-inflicted rocket splash
damage (`targ == attacker && mod == MOD_ROCKET_SPLASH`), DeFRaG multiplies the
horizontal (`X`/`Y`) components of the knockback `dir` vector by **1.2** before
the standard vanilla knockback formula (`kvel = dir * g_knockback.value *
min(damage,200) / 200`, `mass = 200` hardcoded) runs. The vertical (`Z`)
component is untouched. Outside of this exact condition (non-promode, or
non-self, or non-rocket-splash damage), the formula is byte-for-byte identical
to vanilla. See [G_Damage.md](G_Damage.md) for the full breakdown.

`g_knockback.value` is confirmed still read live from the cvar (via a small
per-axis helper function, not hardcoded). `mass = 200` is confirmed hardcoded,
matching vanilla.

### G_RadiusDamage — CONFIRMED vanilla-identical core algorithm

Bounding-box (absmin/absmax) distance measurement, `dir[2] += 24` bias, and
`points = damage * (1 - dist/radius)` (truncated to int at the `G_Damage` call
site) are all confirmed unchanged from vanilla. The only difference is a
signature/plumbing change (separate `inflictor` parameter, non-NULL `point`)
that does not affect the damage/knockback magnitude. See
[G_RadiusDamage.md](G_RadiusDamage.md).

### CanDamage — CONFIRMED unmodified

Hash-identical to vanilla; DeFRaG did not touch the line-of-sight check used
by radius damage. See [CanDamage.md](CanDamage.md).

## Follow-up pass: chasing an ~11.7% knockback-speed gap (see G_Damage.md)

A live-server measurement showed DeFRaG's actual self-splash knockback speed
running ~11.7% higher than the confirmed 1.2x-horizontal formula predicts.
This pass re-examined `G_Damage`, `G_RadiusDamage`, `fire_rocket`, and (newly)
`G_ExplodeMissile`/`G_MissileImpact` looking for a second, missed multiplier.
**None was found** — see the "Re-examination pass" section in
[G_Damage.md](G_Damage.md) for the full list of what was checked and ruled
out. One genuine correction came out of it (see below); it does not change
the numeric result for the specific test case that prompted the investigation.

### Resolved this pass

- **`func_0xa2ea`** — fully decoded: a generic 3-arg `clamp(min, max, value)`,
  no side effects, used only for the `pm_time` duration clamp (not velocity).
- **Global `0x132660`** — fully traced across all 8 reference sites in the
  module. It is a debug/developer flags bitmask (never written from QVM code,
  presumably engine-set); its bits gate an entity-validation loop, an
  alternate missile debug-trace path, and debug-print logging. No arithmetic
  on damage/knockback/velocity at any site — confirmed unrelated to the
  knockback formula.
- **`func_0x1b675`** (called from `func_0x3ec7`) — fully decoded: plain
  `atof(Cvar_VariableStringBuffer(name))`, no scaling or special-casing.
- **`func_0x3ec7`'s guard condition** — **corrected**. The global `0x132100`
  it checks is not a "first call" cache flag as previously assumed; it is the
  cached **`sv_cheats`** integer (confirmed via the cvar name string bytes at
  its writer function, `0x3d7e`). Actual behavior: `g_knockback`'s live cvar
  value is only honored when `sv_cheats` is non-zero; otherwise `G_Damage`
  uses a hardcoded `1000.0` regardless of the server's actual `g_knockback`
  setting. This doesn't affect the investigated test case (server's
  `g_knockback` was itself `1000`, identical to the fallback) but is a
  behaviorally real finding for servers running a non-default `g_knockback`
  with `sv_cheats 0` — **not yet implemented in oDFs**, flagged for a
  follow-up decision.
- **`G_RadiusDamage`'s actual caller** — corrected from an order-position
  guess (`G_MissileImpact`) to the confirmed caller, `G_ExplodeMissile`. Its
  `radius`/`damage` arguments were traced and confirmed to be the missile's
  `splashRadius`/`splashDamage` fields, read unconditionally with no
  `df_promode` branch anywhere in between — ruling out a promode-specific
  effective-radius hypothesis. See
  [G_MISSILE/G_ExplodeMissile.md](../G_MISSILE/G_ExplodeMissile.md).
- **`G_MissileImpact`'s direct-hit path** — decoded; passes
  `mod = MOD_ROCKET` (not `MOD_ROCKET_SPLASH`), so it cannot trigger the
  promode boost by construction; no `df_promode` reference anywhere in the
  function. See
  [G_MISSILE/G_MissileImpact.md](../G_MISSILE/G_MissileImpact.md).

### Still unresolved / not pursued

- `gclient_t + 0xd0` — an unidentified per-client field used to scale incoming
  damage by `field/100` for non-self damage. Purpose unconfirmed; does not
  affect self-splash knockback (explicitly skipped when `attacker == targ`).
- `G_RunMissile` — grepped for the `df_promode` cache address (zero matches);
  not otherwise instruction-decoded.

## Pass 2: whole-QVM `ps.velocity` write sweep (see RocketJump_ExtraVelocity_Search_Pass2.md)

A second follow-up pass dropped the assumption that the missing mechanism
references `df_promode` or any previously-examined function, and instead
enumerated **every `addf` instruction in the entire QVM** (848 total) looking
for any write to a `ps.velocity`-shaped destination (`base+0x20/0x24/0x28`),
plus every other `G_RadiusDamage`/`trap_EntitiesInBox` call site, plus a full
trace of the `previous_velocity` field. **Confirmed negative again** — see
[RocketJump_ExtraVelocity_Search_Pass2.md](RocketJump_ExtraVelocity_Search_Pass2.md)
for the full method and results. Two new, previously-undocumented functions
were discovered and fully decoded along the way (a DeFRaG Lightning-Gun
"phantom missile" spawner and a grapple-swing physics subsystem), both
confirmed unrelated to rocket splash damage. `G_MissileImpact`'s
previously-undecoded "grapple/hook special case" was also fully decoded this
pass — see the updated
[G_MISSILE/G_MissileImpact.md](../G_MISSILE/G_MissileImpact.md).
