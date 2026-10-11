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
| `fire_plasma` | `0x224b5` | `0x1a91b` | ❌ modified (+9 instr) | ✅ Fully decoded — see [fire_plasma.md](fire_plasma.md). All damage/speed/radius/MOD constants unconditionally vanilla-identical (dir*2000.0, damage 20/15/20, MOD_PLASMA); +9 delta = one added unconditional `bolt->s.clientNum = self->s.number` write, no df_promode/cvar involvement |
| `fire_grenade` | `0x2259a` | `0x1a9f8` | ❌ modified (+9 instr) | ✅ Fully decoded — see [fire_grenade.md](fire_grenade.md). All constants unconditionally vanilla-identical (dir*700.0, TR_GRAVITY, EF_BOUNCE_HALF, damage 100/100/150, MOD_GRENADE); +9 delta = same `s.clientNum` write as fire_plasma, no df_promode/cvar involvement |
| `fire_bfg` | `0x22685` | `0x1aadb` | ❌ modified (+9 instr) | ✅ Fully decoded — see [fire_bfg.md](fire_bfg.md). All constants unconditionally vanilla-identical (dir*2000.0, TR_LINEAR, damage 100/100/120, MOD_BFG); +9 delta = same `s.clientNum` write as fire_plasma, no df_promode/cvar involvement |
| `G_BounceItem` | `0x21561` | `0x19c3d` | ✅ identical | ✅ Fully decoded, entry-to-leave — see [G_BounceItem.md](G_BounceItem.md). **Confirmed 100% vanilla-identical**, no deltas found. |
| `G_BounceMissile` | `0x21f4f` | `0x1a3b8` | ✅ identical | ✅ Fully decoded, entry-to-leave — see [G_BounceMissile.md](G_BounceMissile.md). **One confirmed difference**: the `EF_BOUNCE_HALF` stop-path is missing `ent->s.time = level.time / 4;` (present in vanilla/oDFs, absent from DeFRaG's compiled bytecode). Everything else (0.65 scale, 0.2/40.0 thresholds, -2.0 reflection) is vanilla-identical. |
| `G_ExplodeMissile` | `0x22066` | `0x1a4cf` | ❌ modified (+4 instr, minor) | ✅ Fully decoded, entry-to-leave — see [G_ExplodeMissile.md](G_ExplodeMissile.md). **Confirmed 100% vanilla-identical** (the +4-instruction delta is compiler/plumbing noise from `G_RadiusDamage`'s already-documented signature change, not a behavior change); also corrects a prior guess (final trap call is `trap_LinkEntity`, not `trap_UnlinkEntity`). |
| `G_MissileImpact` | `0x2210f` | `0x1a575` | ❌ modified (+4 instr, minor) | ✅ Fully decoded, entry-to-leave — see [G_MissileImpact.md](G_MissileImpact.md). **Confirmed 100% vanilla-identical**, including the `"hook"` classname special case and the final event-broadcast/splash-damage tail. Corrects a prior mischaracterization: the second `G_RadiusDamage` call site (`0x223a2`) is *not* gated behind a `"grapple"` classname check (it was misread as `"grapple"`; the actual compared string is `"hook"`) — it sits in the unconditional default tail and **is reachable by rocket impacts**. |
| `G_RunMissile` | `0x223cb` | `0x1a82e` | ❌ modified (−2 instr, minor) | ✅ Fully decoded — see [G_RunMissile.md](G_RunMissile.md); no `df_promode` reference, no knockback/velocity involvement |
| `func_0x20998` (missile trigger-touch handler) | `0x20998` | — (new function, no vanilla counterpart) | — | ✅ Fully decoded — see [func_0x20998.md](func_0x20998.md). Found while searching for the generic `G_TouchTriggers` (see [`../G_UTILS/README.md`](../G_UTILS/README.md)); gated to `eType==ET_MISSILE` only, so it is **not** that generic function, but it is a genuine `G_TouchTriggers`-shaped dispatcher (`EntitiesInBox`→`EntityContact`→`hit->touch(hit,ent,trace)`) scoped to missiles. **Caller search exhausted** (checked every CODE call/tail-jump site and the full DATA+LIT segments for the literal address — zero hits); most likely genuinely dead/unwired code — **not implemented** |

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
read).

**Update — gap fully closed.** The event-broadcast/entity-bookkeeping portions
of `G_ExplodeMissile` and `G_MissileImpact` that were previously left
undecoded (and `G_BounceItem`/`G_BounceMissile`, previously only
hash-matched, never actually read) have now been fully decoded,
instruction-by-instruction, entry to `leave`, with zero gaps in all four
functions. Findings:

- **`G_BounceItem`**: confirmed 100% vanilla-identical — every constant
  (`-2.0` reflection, `ent->physicsBounce` scaling, `0.0`/`40.0`/`1.0`
  stop-check thresholds) matches. See [G_BounceItem.md](G_BounceItem.md).
- **`G_BounceMissile`**: confirmed **one real difference** — DeFRaG's compiled
  version omits `ent->s.time = level.time / 4;` from the `EF_BOUNCE_HALF`
  stop-path (present in vanilla and in oDFs's current code). Everything else
  matches exactly. See [G_BounceMissile.md](G_BounceMissile.md).
- **`G_ExplodeMissile`**: the remaining tail (`BG_EvaluateTrajectory` +
  `SnapVector` + `G_SetOrigin`, `dir=(0,0,1)`, `eType=ET_GENERAL`,
  `G_AddEvent(..., EV_MISSILE_MISS, ...)`, `freeAfterEvent`, the
  `accuracy_hits++` guard, and the final cleanup call) is confirmed 100%
  vanilla-identical. Also **corrects an error**: the final trap call
  (`-0x1f`) is `trap_LinkEntity`, not `trap_UnlinkEntity` as previously
  guessed. See [G_ExplodeMissile.md](G_ExplodeMissile.md).
- **`G_MissileImpact`**: the `"hook"` classname special case and the final
  event-broadcast/splash-damage tail are confirmed 100% vanilla-identical.
  Also **corrects a mischaracterization**: a prior pass misread the compared
  classname literal as `"grapple"` (it is actually `"hook"`, confirmed
  directly from the disassembly) and concluded the tail's `G_RadiusDamage`
  call (`0x223a2`) was gated behind it and thus unreachable by rockets — in
  fact that call sits in the unconditional default tail and **is reachable by
  any missile's direct-hit impact, including rockets**. This does not change
  the self-splash-knockback conclusion (still no `df_promode` gating anywhere
  in this call), but corrects the reachability claim. See
  [G_MissileImpact.md](G_MissileImpact.md).

Several previously-unidentified call targets were pinned down in this pass
(cross-confirmed across multiple call sites and, where practical, by reading
the target function's own disassembly): `0x4f43` =
`BG_EvaluateTrajectoryDelta`, `0x286ad` = `G_SetOrigin`, `0x8f7f` =
`DirToByte`, `0x2861b` = `G_AddEvent`, `0x289a6` = `SnapVectorTowards`,
`0x5c8f` = a `strcmp`-shaped string-compare helper, `0x2916e` =
`Weapon_HookThink`. New confirmed struct offsets: `0x12a314` =
`level.previousTime`; `gentity_t.physicsBounce` = `+0x238`;
`entityState_t.groundEntityNum` = `+0x94`; `gentity_t.client` = `+0x204`;
`gentity_t.freeAfterEvent` = `+0x22c`; `gentity_t.enemy` = `+0x300`;
`gentity_t.think` = `+0x2b4`; `gentity_t.nextthink` = `+0x2b0`;
`entityShared_t.mins`/`maxs` = `+0x1b4`/`+0x1c0`; `client_t.accuracy_hits` =
`+0x320` (relative to the `client` pointer). See
[../README.md](../README.md)'s "Other identified addresses" table for the
consolidated list.

`fire_plasma`, `fire_grenade`, `fire_bfg` (each +9 instructions vs vanilla)
have now been fully decoded — see [fire_plasma.md](fire_plasma.md),
[fire_grenade.md](fire_grenade.md), [fire_bfg.md](fire_bfg.md). All three are
**entirely linear code (no branches) with zero `df_promode`/cvar
involvement**, and every damage/speed/radius/MOD constant is unconditionally
identical to vanilla. The common +9-instruction delta across all three (and
also present as part of `fire_rocket`'s own +49, see below) is a single
added, unconditional field write appended right before `return bolt`:

```c
bolt->s.clientNum = self->s.number;
```

This sets the projectile's `entityState_t.clientNum` (offset `0xa8`,
confirmed via byte-for-byte match against vanilla `entityState_t`'s real
layout) to the shooter's entity number — a field vanilla Q3 never sets on
these missile types. Its downstream purpose (e.g. a cgame-side visual/hit
effect keyed on `clientNum`) is **not confirmed** and was not investigated
further; this is documented as a confirmed bytecode fact, not a guess about
intent. `fire_rocket`'s much larger +49 delta cleanly decomposes as this same
+9 `clientNum` write plus ~40 rocket-specific instructions for the
`df_promode` speed branch — `fire_rocket.md` did not previously call out the
`clientNum` write as a separate item (it predates this pass); it is the same
mechanism confirmed here.

**Implemented in oDFs**: `bolt->s.clientNum = self->s.number;` added to all
four functions (`fire_rocket`, `fire_plasma`, `fire_grenade`, `fire_bfg`) in
`code/game/g_missile.c`, right before each `return bolt;`.

## Two more `fire_*`-shaped functions found immediately after `fire_rocket`

A whole-QVM `ps.velocity` write sweep (see
[G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md))
identified and fully decoded two more functions sitting right after
`fire_rocket` (`0x22869` end) that were not in the original hash-table survey
because they aren't part of vanilla's `fire_*` family at all:

| Function | DeFRaG address | Identity | Notes |
|----------|-----------------|----------|-------|
| DeFRaG LG "phantom missile" spawner | `0x22880` | Not vanilla — DeFRaG-added | `s.weapon=6`(WP_LIGHTNING), `methodOfDeath=11`(MOD_LIGHTNING), `damage=0`, `splashDamage=45`, `splashRadius=46`, 10ms fuse, `think` = a 2-instruction wrapper that calls **only** `G_FreeEntity` (`0x284b7`, newly identified) — its own think never calls `G_Damage`/`G_RadiusDamage`. **Superseded — see correction below.** |
| `fire_grapple` | `0x2295f` | Grapple hook launch | `s.weapon=0xa`(WP_GRAPPLING_HOOK), `trDelta = dir * 800.0` (confirmed via the `0x44c80000` literal, matching `disasm/README.md`'s pre-existing "800.0=grapple speed" identification), `think=0x29145` (not decoded further). |

`0x284b7` (**`G_FreeEntity`**, newly identified) — confirmed via its
`trap -0x65(0, 0x43c=sizeof(gentity_t), ent)` memset-style clear, a
classname reset to a "freed"-style string constant, and `freetime`/`inuse`
field writes, matching vanilla `G_FreeEntity`'s shape exactly.

Neither of these two functions is reachable from, or relevant to, the rocket
self-splash investigation.

**Correction (superseding the table row above):** the LG "phantom missile"
spawner's own `think` callback being `G_FreeEntity`-only does **not** mean it's
harmless. A later pass ([../G_ACTIVE/LG_Knockback.md](../G_ACTIVE/LG_Knockback.md))
found `s.eType = ET_MISSILE` and `classname = "rocket"`, meaning the entity is
picked up by the generic `G_RunMissile` pipeline every frame regardless of its own
`think`, and detonates through the normal `G_MissileImpact` → `G_ExplodeMissile` →
`G_RadiusDamage` splash path exactly like a real rocket (`splashDamage=45`,
`splashRadius=46`, gated behind `DFLAG_LG_KNOCKBACK`/`df_lg_Knockback`). This is a
genuine, previously-missed damage/knockback mechanism — see `LG_Knockback.md` for
the full decode. Implemented in oDFs as `fire_lg_knockback` in `g_missile.c`.

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
