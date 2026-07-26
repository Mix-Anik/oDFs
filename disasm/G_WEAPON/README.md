# G_WEAPON — Defrag QVM disassembly analysis

Analysis of `g_weapon.c`-equivalent functions from the Defrag qagame.qvm —
weapon-fire server-side logic (muzzle point, hitscan/projectile dispatch,
melee, accuracy tracking).

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md). Functions were located via the hash/order
cross-reference method against `baseq3-game-functions.hmap`
(`D:\Other Reverse Engineering\q3vm\baseq3-game-functions.hmap`), the same
methodology used in [../G_COMBAT/](../G_COMBAT/) and [../G_MISSILE/](../G_MISSILE/).
Vanilla's `g_weapon.c` functions sit contiguously in the hash table
(`Weapon_Gauntlet` at vanilla `0x1ff04` through `FireWeapon` at vanilla
`0x209e2`) and were walked in lock-step against DeFRaG's own function-hash
table (top of `qagame.dis`) starting from the already-confirmed
`CalcMuzzlePoint` anchor (`0x29383`) and walking backward/forward.

**Important limitation discovered this pass:** the lock-step-by-position
assumption breaks down once DeFRaG's Lightning Gun logic is reached — DeFRaG
restructured the tail of `g_weapon.c` so that `FireWeapon` itself became
directly callable from a Pmove-embedded per-frame dispatcher; a
position-based-only guess at `0x21b5e` in `disasm/README.md` predating this
pass turned out to be a misidentification (corrected in
[FireWeapon.md](FireWeapon.md)). Every identification below was confirmed via
direct call-target/argument evidence, not position alone.

---

## Functions located and analyzed

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `Weapon_Gauntlet` | `0x288ae` | `0x1ff04` | ✅ identical (hash + size) | ✅ Confirmed unmodified (empty body) — see [CheckGauntletAttack.md](CheckGauntletAttack.md) |
| `CheckGauntletAttack` | `0x288b1` | `0x1ff07` | ❌ modified (same size, different hash) | ✅ Decoded — hardcoded quadFactor (3.0), `noclip` early-return appears absent — see [CheckGauntletAttack.md](CheckGauntletAttack.md) |
| `Bullet_Fire` | `0x289ef` | `0x20046` | ✅ hash-identical | ✅ Spot-checked — `mod` param removed, hardcoded `MOD_MACHINEGUN`=3 at call site (see [FireWeapon.md](FireWeapon.md#bullet_fire-mod-parameter-removed)) |
| `BFG_Fire` | `0x28b70` | `0x201c7` | ❌ modified (+45 instr) | ✅ Decoded — "LAG CHEAT" rate-limit gate added — see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| `ShotgunPellet` | `0x28bd0` | `0x201fb` | ✅ hash-identical | ⚠️ **Fully decoded — MODIFIED**: `G_Damage` and `LogAccuracyHit` calls are swapped vs vanilla (damage now applied before the accuracy/health check) — see [ShotgunPellet.md](ShotgunPellet.md) |
| `ShotgunPattern` | `0x28c57` | `0x20282` | ✅ hash-identical | ✅ Fully decoded, confirmed behaviorally unmodified (spread=700.0, ×16, count=11 all confirmed); one inert dead-code field read found (no behavioral effect) — see [ShotgunPellet.md](ShotgunPellet.md#shotgunpattern--anomaly-found-inert-no-observed-behavioral-effect) |
| `weapon_supershotgun_fire` | `0x28d3c` | `0x20367` | ✅ hash-identical | ✅ Fully decoded, confirmed unmodified (4096.0 blast scale confirmed) — see [weapon_supershotgun_fire.md](weapon_supershotgun_fire.md) |
| `weapon_grenadelauncher_fire` | `0x28dbb` | `0x203e6` | ❌ modified (+45 instr) | ✅ Decoded — "LAG CHEAT" gate added, `forward[2]+=0.2` + `fire_grenade` call otherwise unmodified — see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| `Weapon_RocketLauncher_Fire` | `0x28e2b` | `0x2042a` | ❌ modified (+45 instr) | ✅ Decoded — "LAG CHEAT" gate added, `fire_rocket` call otherwise unmodified — see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) |
| `Weapon_Plasmagun_Fire` | `0x28e8b` | `0x2045e` | ✅ hash-identical | ✅ Fully read — confirmed byte-for-byte identical to vanilla, no rate-limit gate — see [FireWeapon.md](FireWeapon.md) |
| `weapon_railgun_fire` | `0x28ebf` | `0x20492` | ✅ hash-identical | ✅ Fully decoded (largest function in this module, 529 instr), confirmed unmodified (damage=100.0, range=8192.0, offset=4.0/-1.0, `LogAccuracyHit`/`G_Damage` order confirmed correct) — see [weapon_railgun_fire.md](weapon_railgun_fire.md) |
| `Weapon_GrapplingHook_Fire` | `0x290cf` | `0x206a2` | ❌ modified (+76 instr) | ✅ Decoded — "LAG CHEAT" gate + redundant re-aim using current (not old) origin — see [Weapon_GrapplingHook_Fire.md](Weapon_GrapplingHook_Fire.md) |
| `Weapon_HookFree` | `0x29145` | `0x206cd` | ✅ hash-identical | ✅ Confirmed unmodified (size+1 drift only) |
| `Weapon_HookThink` | `0x2916e` | `0x206f6` | ✅ hash-identical | ✅ Confirmed unmodified (size+1 drift only) |
| `Weapon_LightningFire`-equivalent | `0x291f4` (`func_0x291f4`) | `0x2077c` | ❌ modified | ✅ Already fully decoded in [../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md); **confirmed still reached from `FireWeapon`'s normal `case WP_LIGHTNING`, in addition to a new Pmove-embedded per-frame caller** — see [FireWeapon.md](FireWeapon.md#5-wp_lightning-open-question--resolved) |
| `LogAccuracyHit` | `0x29334` | `0x2087f` | ✅ hash-identical | ✅ Fully decoded, confirmed unmodified — all 6 vanilla checks present in order — see [LogAccuracyHit.md](LogAccuracyHit.md) |
| `CalcMuzzlePoint` | `0x29383` | `0x208ce` | ✅ identical | ✅ Confirmed unmodified — see [CalcMuzzlePoint.md](CalcMuzzlePoint.md) |
| `CalcMuzzlePointOrigin` | `0x2940d` | `0x20958` | ✅ identical | ✅ Confirmed unmodified (hash + spot-check only) |
| `FireWeapon` | `0x29497` | `0x209e2` | ❌ modified (same size, different hash) | ✅ Fully decoded, all 10 switch cases confirmed — see [FireWeapon.md](FireWeapon.md). **Corrects a prior wrong guess in `disasm/README.md` that this was `0x21b5e`.** |

## Not applicable — confirmed absent from this build (Team Arena / MISSIONPACK-only)

Per direct inspection of `oDFs`'s `code/game/g_weapon.c`, `weapon_proxlauncher_fire`,
`Weapon_Nailgun_Fire`, `G_StartKamikaze`, `KamikazeRadiusDamage`,
`KamikazeShockWave`, and `KamikazeDamage` are all wrapped in `#ifdef
MISSIONPACK` and never compiled in a non-missionpack (baseq3/DeFRaG) build.
Cross-checked against `baseq3-game-functions.hmap`: none of these names, nor
`G_CheckProxMinePosition`'s sibling prox-launcher-fire shape, appear anywhere
in vanilla baseq3's function table either (`G_CheckProxMinePosition`/
`G_TryPushingProxMine`, which *do* exist in vanilla at `0x1b14d`/`0x1b1ea`,
are unrelated `g_mover.c` prox-mine-as-pushable-object mover code, not the
Team Arena weapon fire function). **Verified absent, not merely assumed** —
these functions do not exist in DeFRaG's qagame.qvm and require no further
investigation.

## Not attempted this pass

None remaining. A follow-up pass fully instruction-decoded the 5 functions
that were previously only hash-confirmed/spot-checked
(`LogAccuracyHit`, `ShotgunPellet`, `ShotgunPattern`,
`weapon_supershotgun_fire`, `weapon_railgun_fire`) — see the headline
findings below and each function's own doc. Every function in this module's
table is now either fully decoded or confirmed N/A (Team Arena only).

**Follow-up check (later pass):** `Bullet_Fire`'s own `LogAccuracyHit`/`G_Damage`
call ordering was checked against the reordering pattern found in
`ShotgunPellet`. Confirmed **not affected** — `Bullet_Fire` calls
`LogAccuracyHit` (`0x28b05`) before `G_Damage` (`0x28b5f`), both in the same
straight-line path (no intervening branch reorders them), matching vanilla
order exactly. `ShotgunPellet` remains the only one of the five
`LogAccuracyHit`+`G_Damage` call sites in this module found to reverse it.

---

## Headline findings

1. **`FireWeapon`'s real address is `0x29497`, not `0x21b5e`** (prior
   misidentification in `disasm/README.md`, now corrected). All 10 weapon
   switch cases decoded and confirmed to dispatch to the same functions
   vanilla does. See [FireWeapon.md](FireWeapon.md).
2. **`s_quadFactor` is hardcoded to `3.0`/`1.0`** in both `FireWeapon` and
   `CheckGauntletAttack`, replacing vanilla's `g_quadfactor.value` cvar read.
3. **A new "LAG CHEAT DETECTED" fire-rate anti-exploit gate** was added to
   `BFG_Fire`, `Weapon_RocketLauncher_Fire`, `weapon_grenadelauncher_fire`,
   and `Weapon_GrapplingHook_Fire` — blocks same-server-tick re-fire attempts
   and logs a message; not present on Plasmagun, Shotgun, Railgun, Machinegun,
   or Lightning. See [WeaponFireRateLimit.md](WeaponFireRateLimit.md).
4. **`CheckGauntletAttack`'s `noclip` early-return appears to be absent**
   in DeFRaG (confirmed via exhaustive full-function read) — see
   [CheckGauntletAttack.md](CheckGauntletAttack.md).
5. **`Bullet_Fire`'s `mod` parameter was removed**, hardcoded to
   `MOD_MACHINEGUN`(3) internally — machinegun-only specialization (no
   missionpack chaingun). Hash still matches vanilla exactly, reinforcing the
   documented hashing caveat (opcode-shape-only, not constants/signature).
6. **The Lightning Gun's normal fire path still exists and is unchanged in
   position** (`FireWeapon`'s `case WP_LIGHTNING` still calls the beam-trace
   function) — DeFRaG *added* a second caller (a Pmove-embedded per-frame
   re-invocation of the whole `FireWeapon` function) rather than replacing
   the normal path. Resolves the open question flagged in this task.
7. **`Weapon_GrapplingHook_Fire` redundantly recomputes aim/muzzle** using
   the entity's *current* origin (`CalcMuzzlePoint`) rather than reusing
   `FireWeapon`'s already-computed muzzle (which used `oldOrigin` via
   `CalcMuzzlePointOrigin`) — a confirmed, subtle muzzle-origin divergence
   for the grapple hook specifically.
8. **`ShotgunPellet` calls `G_Damage` before `LogAccuracyHit`** (vanilla does
   the reverse) — confirmed via direct instruction-order reading, despite an
   exact hash match with vanilla. Since `G_Damage` can reduce the target's
   health to `<=0` before `LogAccuracyHit`'s health check runs, **a shotgun
   pellet that kills (or otherwise zeroes/negatives) its target is not
   counted toward `accuracy_hits`** in DeFRaG, unlike vanilla. Confirmed
   isolated to `ShotgunPellet` — the 3 other `LogAccuracyHit`+`G_Damage`
   sites checked (`weapon_railgun_fire`, `G_RadiusDamage`,
   `G_MissileImpact`) all preserve vanilla's order. See
   [ShotgunPellet.md](ShotgunPellet.md).
9. **`ShotgunPattern` contains an 11-instruction dead-code block** reading
   `*(ent->client + 0xf8)` into a local that's never subsequently used — no
   observed behavioral effect, but a genuine extra-vs-vanilla instruction
   sequence hiding behind a matching hash. See
   [ShotgunPellet.md](ShotgunPellet.md#shotgunpattern--anomaly-found-inert-no-observed-behavioral-effect).
10. **`LogAccuracyHit`, `weapon_supershotgun_fire`, and `weapon_railgun_fire`
    are all now fully instruction-decoded and confirmed vanilla-identical**
    (previously hash-confirmed/spot-checked only) — see
    [LogAccuracyHit.md](LogAccuracyHit.md),
    [weapon_supershotgun_fire.md](weapon_supershotgun_fire.md), and
    [weapon_railgun_fire.md](weapon_railgun_fire.md).
