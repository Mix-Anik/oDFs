# G_MISSILE — Defrag QVM disassembly analysis

Analysis of `g_missile.c`-equivalent functions (projectile spawning, impact,
splash damage triggering) from the Defrag qagame.qvm.

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md). For the damage/knockback formula itself (called
from `G_MissileImpact` via `G_RadiusDamage`), see [../G_COMBAT/](../G_COMBAT/).

---

## Functions located (via hash/order cross-reference against `baseq3-game-functions.hmap`)

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `fire_rocket` | `0x2276a` | `0x1abb8` | ❌ modified (+49 instr vs vanilla; also differs from DeFRaG's own fire_plasma/fire_bfg) | ✅ Analyzed — see [fire_rocket.md](fire_rocket.md) |
| `fire_plasma` | `0x224b5` | `0x1a91b` | ❌ modified (+9 instr) | 🔍 Identified only, not decoded |
| `fire_grenade` | `0x2259a` | `0x1a9f8` | ❌ modified (+9 instr) | 🔍 Identified only, not decoded |
| `fire_bfg` | `0x22685` | `0x1aadb` | ❌ modified (+9 instr) | 🔍 Identified only, not decoded |
| `G_BounceItem` | `0x21561` | `0x19c3d` | ✅ identical | Confirmed unmodified (hash match only) |
| `G_BounceMissile` | `0x21f4f` | `0x1a3b8` | ✅ identical | Confirmed unmodified (hash match only) |
| `G_ExplodeMissile` | `0x22066` | `0x1a4cf` | ❌ modified (+4 instr, minor) | ✅ Damage/knockback-relevant portion decoded — see [G_ExplodeMissile.md](G_ExplodeMissile.md) |
| `G_MissileImpact` | `0x2210f` | `0x1a575` | ❌ modified (+4 instr, minor) | ✅ Direct-hit damage portion decoded — see [G_MissileImpact.md](G_MissileImpact.md) |
| `G_RunMissile` | `0x223cb` | `0x1a82e` | ❌ modified (−2 instr, minor) | 🔍 Identified only, not decoded |

Original task scope only required decoding `fire_rocket` (projectile
speed/damage constants). A follow-up pass (chasing an unexplained ~11.7%
knockback-speed gap — see
[G_COMBAT/G_Damage.md](../G_COMBAT/G_Damage.md)'s re-examination section)
decoded the damage/knockback-relevant portions of `G_ExplodeMissile` and
`G_MissileImpact`. Finding: **`G_RadiusDamage` is called from
`G_ExplodeMissile`, not `G_MissileImpact`** (correcting the earlier
order-position guess — `G_RadiusDamage.md` has been updated). `G_MissileImpact`
instead calls `G_Damage` directly for the *direct-hit* case
(`mod = MOD_ROCKET`, not `MOD_ROCKET_SPLASH`), which structurally cannot
trigger the promode self-splash knockback boost. Neither function contains
any `df_promode`-conditional logic (confirmed by grep across their full
instruction ranges). `G_RunMissile` was also grepped for the `df_promode`
cache address with zero matches (its non-decoded portions were not otherwise
read). The event-broadcast/entity-bookkeeping portions of both functions
remain undecoded (not damage/knockback-relevant, out of scope).

`fire_plasma`, `fire_grenade`, `fire_bfg` are also confirmed modified (each
+9 instructions vs vanilla) but were out of scope for this task and were not
decoded. Their modification is presumably related to whatever common change
was applied across all `fire_*` functions (note: DeFRaG's `fire_plasma` and
`fire_bfg` share an identical hash with each other, `7ab22a35`, mirroring the
vanilla pattern where these functions are structurally identical to one
another) — but `fire_rocket` clearly received substantial rocket-specific
extra code (the promode speed branch) beyond whatever that common change is.

## Two more `fire_*`-shaped functions found immediately after `fire_rocket`

A whole-QVM `ps.velocity` write sweep (see
[G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md))
identified and fully decoded two more functions sitting right after
`fire_rocket` (`0x22869` end) that were not in the original hash-table survey
because they aren't part of vanilla's `fire_*` family at all:

| Function | DeFRaG address | Identity | Notes |
|----------|-----------------|----------|-------|
| DeFRaG LG "phantom missile" spawner | `0x22880` | Not vanilla — DeFRaG-added | `s.weapon=6`(WP_LIGHTNING), `methodOfDeath=11`(MOD_LIGHTNING), `damage=0`, `splashDamage=45`, `splashRadius=46`, 10ms fuse, `think` = a 2-instruction wrapper that calls **only** `G_FreeEntity` (`0x284b7`, newly identified) — never `G_Damage`/`G_RadiusDamage`. Spawns a harmless, self-cleaning marker entity; not a damage/knockback mechanism of any kind. |
| `fire_grapple` | `0x2295f` | Grapple hook launch | `s.weapon=0xa`(WP_GRAPPLING_HOOK), `trDelta = dir * 800.0` (confirmed via the `0x44c80000` literal, matching `disasm/README.md`'s pre-existing "800.0=grapple speed" identification), `think=0x29145` (not decoded further). |

`0x284b7` (**`G_FreeEntity`**, newly identified) — confirmed via its
`trap -0x65(0, 0x43c=sizeof(gentity_t), ent)` memset-style clear, a
classname reset to a "freed"-style string constant, and `freetime`/`inuse`
field writes, matching vanilla `G_FreeEntity`'s shape exactly.

Neither of these two functions is reachable from, or relevant to, the rocket
self-splash investigation.

## Headline finding

### fire_rocket — CONFIRMED promode-conditional speed

```c
if ( df_promode )
    trDelta = forward * 1000.0;  // confirmed literal 0x447a0000
else
    trDelta = forward * 900.0;   // confirmed literal 0x44610000
```

`damage = 100`, `splashDamage = 100`, `splashRadius = 120` all confirmed
unconditional (not promode-gated), matching vanilla exactly. See
[fire_rocket.md](fire_rocket.md) for full detail, including a caveat about the
hashing scheme (opcode-shape-based, not a strict content checksum — a hash
match does not by itself prove immediate/constant values are unchanged, only
that instruction shape is).
