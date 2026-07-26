# Defrag QVM Disassembly — Project Overview

Reverse engineering of `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
(Defrag qagame.qvm, disassembled with q3vm tools).

Goal: verify that oDFs (`d:\Other\Programming\C\oDFs`) matches Defrag's movement implementation exactly.

---

## Analyzed modules

| Module | Folder | Status |
|--------|--------|--------|
| **Server-side** (qagame.qvm) | | |
| `bg_pmove.c` — player movement | [BG_PMOVE/](BG_PMOVE/) | ✅ Complete — all functions analyzed |
| `bg_slidemove.c` — slide/step movement | [BG_SLIDEMOVE/](BG_SLIDEMOVE/) | ✅ Complete — PM_SlideMove, PM_StepSlideMove |
| `g_active.c` — per-frame client update | [G_ACTIVE/](G_ACTIVE/) | ✅ Substantially complete — Pmove outer, pm struct, P_WorldEffects, ClientThink_real, race timer tick |
| `g_trigger.c` — trigger entities | [G_TRIGGER/](G_TRIGGER/) | ✅ Substantially complete — race timer entities (start/stop/checkpoint); G_InitRaceMode |
| `g_combat.c` — damage, knockback, radius damage | [G_COMBAT/](G_COMBAT/) | ✅ G_Damage, G_RadiusDamage, CanDamage analyzed — confirmed df_promode self-splash knockback boost. Searched the entire `0x208f2`-`0x22066` gap plus per-frame movement code for an unexplained ~+10 additive knockback term implied by live-server data — confirmed absent everywhere checked, see [G_COMBAT/RocketJump_ExtraVelocity_Search.md](G_COMBAT/RocketJump_ExtraVelocity_Search.md). Follow-up pass exhaustively swept **every `addf` in the whole QVM** (848 instructions) for any unexplained `ps.velocity` write, checked every other `G_RadiusDamage`/`trap_EntitiesInBox` caller, and traced `previous_velocity` fully — confirmed negative again, see [G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md) |
| `g_missile.c` — projectile spawning/impact | [G_MISSILE/](G_MISSILE/) | ✅ Complete — fire_rocket analyzed, confirmed df_promode speed (1000 vs 900), re-verified no promode damage/radius term. **G_ExplodeMissile and G_MissileImpact now fully decoded end-to-end** (previously only their damage/knockback-relevant portions) — both confirmed 100% vanilla-identical; corrects two prior errors (final trap call is `trap_LinkEntity` not `trap_UnlinkEntity`; a classname literal misread as `"grapple"` is actually `"hook"`, and its gated `G_RadiusDamage` call is reachable by rockets, not unreachable). **G_BounceItem and G_BounceMissile also now fully decoded** (previously hash-match-only) — `G_BounceItem` confirmed 100% vanilla-identical; `G_BounceMissile` has one confirmed real difference (missing `ent->s.time = level.time/4` in its `EF_BOUNCE_HALF` stop-path). See [G_MISSILE/README.md](G_MISSILE/README.md). G_RunMissile fully decoded — no promode/knockback involvement, closes the last gap in the rocket fire→explode→damage chain; see [G_MISSILE/G_RunMissile.md](G_MISSILE/G_RunMissile.md). **fire_plasma/fire_grenade/fire_bfg now fully decoded** — all vanilla-identical for damage/speed/radius/MOD, no df_promode/cvar involvement; each's +9-instruction delta is one common unconditional `bolt->s.clientNum = self->s.number` write (also present in fire_rocket, explaining its own +49 as +9 common +~40 promode-branch-specific) — see [G_MISSILE/README.md](G_MISSILE/README.md) |
| `g_weapon.c` — weapon-fire server logic | [G_WEAPON/](G_WEAPON/) | ✅ Substantially complete — `FireWeapon`'s real address corrected to `0x29497` (was misattributed to `0x21b5e`), full switch(weapon) decoded; `CheckGauntletAttack`, `BFG_Fire`, `Weapon_RocketLauncher_Fire`, `weapon_grenadelauncher_fire`, `Weapon_GrapplingHook_Fire` all found modified (hardcoded quadFactor, new "LAG CHEAT DETECTED" fire-rate gate, `noclip` check apparently removed from gauntlet); `Bullet_Fire`/`ShotgunPattern`/`weapon_supershotgun_fire`/`weapon_railgun_fire`/`Weapon_Plasmagun_Fire`/`Weapon_HookFree`/`Weapon_HookThink`/`CalcMuzzlePoint`/`CalcMuzzlePointOrigin`/`LogAccuracyHit` confirmed unmodified; **`ShotgunPellet` found MODIFIED — calls `G_Damage` before `LogAccuracyHit` (vanilla reverse order), so a killing pellet isn't counted toward `accuracy_hits`** (see [G_WEAPON/ShotgunPellet.md](G_WEAPON/ShotgunPellet.md)); Kamikaze/prox-launcher family confirmed absent (Team Arena only, not compiled). See [G_WEAPON/README.md](G_WEAPON/README.md) |
| **Client-side** (cgame.qvm) | | |
| `cg_draw.c` — timer HUD rendering | [CGAME/](CGAME/) | ✅ Race timer HUD architecture analyzed — state machine, cvar communication, ps field requirements |

---

## QVM disassembly notes

Function names are **not preserved** in QVM. All identifications are inferred from constants, call patterns, and comparison against Q3 source.

### Key QVM instructions

| Instruction | Meaning |
|-------------|---------|
| `enter N` | Function entry; allocates N bytes of stack frame |
| `leave N` | Function return |
| `const 1 X` | Push integer constant X |
| `load4` | Pop address, push 4-byte value at that address |
| `store4 -2` | Pop value and address, write value to address |
| `load1` | Byte load |
| `sex8` | Sign-extend byte to int |
| `arg -1 0x8` | Set next call argument slot |
| `call -1` | Call function whose address is on stack |
| `band`, `bor`, `bxor` | Bitwise AND, OR, XOR |
| `lsh`, `rsh` | Left/right shift |
| `addf`, `subf`, `mulf`, `divf` | Float arithmetic |
| `addi`, `subi`, `muli`, `divi` | Int arithmetic |
| `lef`, `gef`, `ltf`, `gtf` | Float comparisons (less/greater-equal/less-than/greater-than) |
| `lei`, `gei`, `lti`, `gti` | Integer comparisons |
| `eq`, `ne` | Equal / not equal; branch to operand address if true |
| `cvif` | Convert int to float |
| `cvfi` | Convert float to int (truncate) |
| `block_copy N` | Copy N bytes between two addresses |
| `negf`, `negi` | Negate float/int |

### Calling convention

- Arguments pushed via `arg -1 0x8`, `arg -1 0xc`, ... (slots 0x8, 0xc, 0x10, ...)
- Local variables at frame offsets: `local 1 0x8`, `local 1 0xc`, ...
- Caller arguments at `local 1 0x28`, `local 1 0x2c`, ... (above frame base)
- Return value left on stack before `leave`
- Negative call targets (e.g., `call -0x6b`) are **trap calls** to the engine

### Identifying functions without names

1. **Constants**: IEEE 754 floats decode to known physics values (270.0=jump, 800.0=grapple speed, etc.)
2. **Call patterns**: what functions it calls and what calls it narrows identity quickly
3. **Offsets**: known struct field offsets (ps+0x20=velocity, ps+0x0c=pm_flags, etc.) confirm identity
4. **Flags**: `pm_flags & 0x8000` = promode check; `surfaceFlags & 0x80000` = SURF_NOOB

---

## Known struct field offsets

### playerState_t (ps) — accessed via `*(*(0x132900))`

| Offset | Field | Notes |
|--------|-------|-------|
| +0x00 | commandTime | |
| +0x04 | pm_type | PM_NORMAL=0, PM_NOCLIP=1, PM_DEAD=3 |
| +0x08 | bobCycle | |
| +0x0c | pm_flags | bit 0x8000 = promode (DeFRaG); bit 0x1 = PMF_DUCKED |
| +0x10 | pm_time | |
| +0x14 | origin[0] | |
| +0x18 | origin[1] | |
| +0x1c | origin[2] | |
| +0x20 | velocity[0] | |
| +0x24 | velocity[1] | |
| +0x28 | velocity[2] | |
| +0x2c | weaponTime | |
| +0x30 | gravity | (int, cast to float when used) |
| +0x34 | speed | |
| +0x38 | delta_angles[0] | |
| +0x3c | delta_angles[1] | |
| +0x40 | delta_angles[2] | |
| +0x44 | groundEntityNum | ENTITYNUM_NONE = 0x3ff |
| +0x48 | legsTimer | |
| +0x4c | legsAnim | ANIM_TOGGLEBIT = 0x80 |
| +0x50 | torsoTimer | |
| +0x54 | torsoAnim | |
| +0x58 | movementDir | 0-7 directional encoding |
| +0x5c | grapplePoint[0] | |
| +0x60 | grapplePoint[1] | |
| +0x64 | grapplePoint[2] | |
| +0x90 | weapon | WP_GAUNTLET=1, WP_GRAPPLING_HOOK=10 |
| +0x94 | weaponstate | WEAPON_READY=0, RAISING=1, DROPPING=2, FIRING=3 |
| +0xa4 | viewheight | DEAD=−16, CROUCH=12, DEFAULT=26 |
| +0xb8 | stats[0] | STAT_HEALTH |
| +0xbc | stats[1] | |
| +0xc8 | stats[3] | powerups[PW_HASTE] equivalent? |
| +0xd4 | stats[7] | waterlevel (via pm+0xd4) |
| +0xe0 | stats[10] | doublejump window countdown (400ms) |
| +0xe4 | stats[11] | bonus-used marker |
| +0xe8 | stats[12] | bit 0x200 = OBS disable (`df_ob_KillOBs`, 0x132550); bits 0x400/0x800 = grapple mode (0x13183c); bit 0x1000 = LG knockback (`df_lg_Knockback`, 0x131840) |
| +0xec | stats[13] | DeFRaG input state bitfield (non-physics) |
| +0x170 | powerups[PW_INVULNERABILITY] | |

### pmove_t (pm) — stored at global `0x132900`

Key fields confirmed from disassembly. DeFRaG's pmove_t is heavily extended beyond Q3's standard layout.

| Offset | Field | Notes |
|--------|-------|-------|
| +0x00 | *ps | pointer to playerState_t |
| +0x04 | cmd | usercmd_t (24 bytes) |
| +0x19 | cmd.forwardmove | (byte within cmd) |
| +0x1a | cmd.rightmove | |
| +0x1b | cmd.upmove | |
| +0x1c | tracemask | |
| +0x28 | noFootsteps | or gauntletHit — uncertain due to layout extension |
| +0xb8 | mins[0] | player bounding box |
| +0xbc | mins[1] | |
| +0xc0 | mins[2] | |
| +0xc4 | maxs[0] | |
| +0xc8 | maxs[1] | |
| +0xcc | maxs[2] | |
| +0xd0 | watertype | |
| +0xd4 | waterlevel | |
| +0xd8 | xyspeed | computed in PM_Footsteps |
| +0xe4 | trace() callback | |
| +0xe8 | pointcontents() callback | |

### pml_t (pml) — base address `0x132874`

| Address | Offset | Field |
|---------|--------|-------|
| `0x132874` | +0x00 | forward[0] |
| `0x132878` | +0x04 | forward[1] |
| `0x13287c` | +0x08 | forward[2] |
| `0x132880` | +0x0c | right[0] |
| `0x132884` | +0x10 | right[1] |
| `0x132888` | +0x14 | right[2] |
| `0x13288c` | +0x18 | up[0] |
| `0x132890` | +0x1c | up[1] |
| `0x132894` | +0x20 | up[2] |
| `0x132898` | +0x24 | frametime |
| `0x13289c` | +0x28 | msec |
| `0x1328a0` | +0x2c | walking |
| `0x1328a4` | +0x30 | groundPlane |
| `0x1328a8` | +0x34 | groundTrace (trace_t, 64 bytes) |
| `0x1328d4` | +0x60 | groundTrace.surfaceFlags |
| `0x1328d8` | +0x64 | groundTrace.contents |
| `0x1328ec` | +0x78 | previous_origin[0] |
| `0x1328f0` | +0x7c | previous_origin[1] |
| `0x1328f4` | +0x80 | previous_origin[2] |
| `0x1328f8` | +0x84 | previous_velocity[0] — **traced, read-only** (see below) |
| `0x1328fc` | +0x88 | previous_velocity[1] / previous_waterlevel — **traced, read-only + one write** (see below) |
| `0x132900` | — | pm global pointer (the pm variable itself) |

**`previous_velocity`/`previous_waterlevel` usage (traced in full,
[G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md)):**
all 8 reference sites in the QVM read these fields for **comparison purposes
only** — `PM_CrashLand`'s fall-impact-speed quadratic (`0x76db`), a
landing/hard-impact `pm_flags` bit in `PM_GroundTrace` (`0x7a9e`), and a
4-site water-enter/leave splash-sound check in `PM_WaterEvents`
(`0x7e7c`-`0x7ebb`). The one write site found (`0x85a2`, in `PmoveSingle`,
before `PM_CheckDuck`/`PM_GroundTrace`/`PM_DropTimers` run) stores the
*current* waterlevel into `previous_waterlevel` as a per-frame "before"
snapshot — exactly matching vanilla Q3's `pml.previous_origin`/
`previous_velocity` design. **Nothing ever reads these fields and adds the
result back into `ps.velocity`** — confirmed not a hidden velocity-injection
mechanism.

---

## Physics global variables (data segment)

| Address | Value | Variable |
|---------|-------|----------|
| `0x14b8` | 100.0f | pm_stopspeed |
| `0x14bc` | 0.25f | pm_duckScale |
| `0x14c0` | 0.50f | pm_swimScale |
| `0x14c8` | 10.0f | pm_accelerate |
| `0x14cc` | 1.0f | pm_airaccelerate |
| `0x14d0` | 4.0f | pm_wateraccelerate |
| `0x14d4` | 8.0f | pm_flyaccelerate |
| `0x14d8` | 6.0f | pm_friction |
| `0x14dc` | 1.0f | pm_waterfriction |
| `0x14e0` | 3.0f | pm_flightfriction |
| `0x14e4` | 5.0f | pm_spectatorfriction |
| `0x14e8` | 0 | c_pmove (frame counter) |
| `0x1317c4` | — | df_promode cvar cache (set at game init) |
| `0x1524` | — | vec3_origin (0,0,0) |

---

## Other identified addresses

| Address | Description |
|---------|-------------|
| `0x503d` | BG_AddPredictableEventToPlayerstate (called by PM_AddEvent) |
| `0x4152` | DeFRaG input state tracker — builds stats[13] from cmd each frame |
| `0x828d` | PM_UpdateViewAngles — (ps, cmd) args, angle clamping |
| `0x7208` | DeFRaG grapple-mode dispatcher — uses stats[12] bits 0x400/0x800 |
| `0x8653` | PmoveSingle subdivision loop (called from Pmove outer 0x1a866) |
| `0x9d12` | VectorNormalize |
| `0x8d63` | VectorLength |
| `0x6061` | abs() |
| `−0x6b` | sqrt() trap call |
| `−0x2b` | trap_SnapVector |
| `0x937c` | Trig function containing 270.0° — false positive for jump velocity search |
| `0x2015b` | `G_Damage` (DeFRaG) — modified vs vanilla, see [G_COMBAT/G_Damage.md](G_COMBAT/G_Damage.md) |
| `0x20648` | `CanDamage` (DeFRaG) — confirmed byte-identical to vanilla |
| `0x20773` | `G_RadiusDamage` (DeFRaG) — confirmed vanilla-identical core algorithm, plumbing signature change only |
| `0x2276a` | `fire_rocket` (DeFRaG) — modified vs vanilla, see [G_MISSILE/fire_rocket.md](G_MISSILE/fire_rocket.md) |
| `0x22066` | `G_ExplodeMissile` (DeFRaG) — calls `G_RadiusDamage` with vanilla-identical args (no promode variant); see [G_MISSILE/G_ExplodeMissile.md](G_MISSILE/G_ExplodeMissile.md) |
| `0x2210f` | `G_MissileImpact` (DeFRaG) — direct-hit path, calls `G_Damage` with `mod=MOD_ROCKET` (not promode-boostable); see [G_MISSILE/G_MissileImpact.md](G_MISSILE/G_MissileImpact.md) |
| `0x29383` | `CalcMuzzlePoint` (DeFRaG) — confirmed byte-identical to vanilla |
| `0x2940d` | `CalcMuzzlePointOrigin` (DeFRaG) — confirmed byte-identical to vanilla |
| `0x3ec7` | `g_knockback` cvar getter — returns hardcoded 1000.0 unless `sv_cheats` (`0x132100`) is non-zero, in which case returns the live cvar value (corrected; previously thought to be a simple init-cache check) |
| `0x1b675` | Generic "cvar as float" helper: `atof(Cvar_VariableStringBuffer(name))`, no scaling — confirmed |
| `0x132100` | Cached `sv_cheats` integer (confirmed via cvar-name string bytes at its sole writer, `0x3d7e`) |
| `0xa2ea` | Generic `clamp(min, max, value)` helper — fully decoded, confirmed (used in G_Damage's promode pm_time clamp) |
| `0x132660` | Debug/developer flags bitmask — fully traced across 8 reference sites; gates entity validation / an alternate missile trace path / debug-print logging; no effect on damage or knockback |
| `-33` (trap_EntitiesInBox) | Confirmed fingerprint for `G_RadiusDamage` |
| `-25` (trap_Trace) | Used by `CanDamage`'s line-of-sight check |
| `0x20d2d` | `Pickup_Weapon` (DeFRaG) — weapon-item pickup handler; zero float instructions, unrelated to knockback; see [G_ACTIVE/func_0x20d2d.md](G_ACTIVE/func_0x20d2d.md) |
| `0x20f11` | `Pickup_Item` (DeFRaG, identity strongly evidenced not independently confirmed) — item-pickup dispatcher, switches on itemType 1-8, dispatches to `Pickup_Weapon` (`0x20d2d`) for IT_WEAPON |
| `0x21561`, `0x21f4f` | `G_BounceItem`/`G_BounceMissile` — missile/item bounce/reflection physics (`v' = v - 2(v·n)n` off a `trace_t.plane.normal`), operates on the entity's own `entityState_t.pos.trDelta`, not player `ps.velocity`. Both **fully decoded** entry-to-leave (see [G_MISSILE/G_BounceItem.md](G_MISSILE/G_BounceItem.md), [G_MISSILE/G_BounceMissile.md](G_MISSILE/G_BounceMissile.md)): `G_BounceItem` confirmed 100% vanilla-identical; `G_BounceMissile` confirmed **missing** `ent->s.time = level.time/4` in its `EF_BOUNCE_HALF` stop-path vs vanilla (one real, confirmed behavioral delta) |
| `0x4f43` | `BG_EvaluateTrajectoryDelta(trajectory_t*, int atTime, vec3_t result)` — confirmed via `G_BounceItem`/`G_BounceMissile`/`G_MissileImpact`'s shared usage pattern (matches vanilla's velocity-at-time evaluator) |
| `0x286ad` | `G_SetOrigin(gentity_t*, vec3_t)` — confirmed via consistent 2-arg (entity, vec3) usage across `G_BounceItem`, `G_BounceMissile`, `G_ExplodeMissile`, `G_MissileImpact` |
| `0x8f7f` | `DirToByte(vec3_t)` — confirmed by reading its own disassembly (null check + loop over a 12-byte-stride `bytedirs`-style table computing `DotProduct`/best-match, the standard vanilla shape) |
| `0x2861b` | `G_AddEvent(gentity_t*, int newEvent, int eventParm)` — confirmed via immediate values matching `bg_public.h`'s `entity_event_t` ordinals exactly at multiple call sites (`0x33`=`EV_MISSILE_MISS`, `0x2c`=`EV_GRENADE_BOUNCE`, `0x32`=`EV_MISSILE_HIT`, `0x34`=`EV_MISSILE_MISS_METAL`) |
| `0x289a6` | `SnapVectorTowards(vec3_t, vec3_t towards)` — confirmed via `G_MissileImpact`'s `"hook"`-branch and final-tail usage, matching vanilla's exact call sites/argument order |
| `0x2916e` | `Weapon_HookThink` — confirmed via `G_MissileImpact`'s `"hook"`-branch `ent->think` assignment; consistent with `G_WEAPON/README.md`'s existing "confirmed unmodified" note, now pinned to an address |
| `0x5c8f` | `strcmp`-shaped string-compare helper — confirmed via `G_MissileImpact`'s `if (!strcmp(ent->classname, "hook"))` gate (literal string at `0x8fcb` = `"hook"`, **not** `"grapple"` as a prior pass mistakenly reported) |
| `0x12a314` | `level.previousTime` — confirmed via `G_BounceItem`/`G_BounceMissile`'s `hitTime = level.previousTime + (level.time - level.previousTime) * trace->fraction` computation |
| `0x21b5e` | **CORRECTED — this is NOT `FireWeapon`** (superseded, see [G_WEAPON/FireWeapon.md](G_WEAPON/FireWeapon.md)). It takes 3 args (a second `gentity_t*` target/enemy pointer), computes a lead-corrected aim direction using a per-weapon travel-speed table (700/900/2000 matching grenade/rocket/plasma) toward that target, and its `switch` only covers cases 4–9 (no gauntlet/machinegun/shotgun/grapple) calling `fire_grenade`/`fire_rocket`/etc. directly. Its own address is stored as a function-pointer field on an entity from `0x21e94` (called from 7 sites), not called directly — structurally matches a generic multi-weapon `g_misc.c`-style "shooter" trigger entity, not `g_weapon.c`'s player-invoked `FireWeapon`. Out of scope, not further identified. |
| `0x29497` | `FireWeapon` (DeFRaG) — **actual address**, corrects the `0x21b5e` guess above. Confirmed via full switch(weapon) decode (all 10 cases, `case WP_LIGHTNING` calling the already-documented `0x291f4`, `case WP_ROCKET_LAUNCHER` calling `Weapon_RocketLauncher_Fire`=`0x28e2b` which itself calls `fire_rocket`=`0x2276a`). Also the caller previously attributed only to a Pmove-embedded LG helper in [G_ACTIVE/LG_Knockback.md](G_ACTIVE/LG_Knockback.md) — see [G_WEAPON/FireWeapon.md](G_WEAPON/FireWeapon.md) for full detail. `s_quadFactor` hardcoded to 3.0/1.0 (not a cvar read). Does not touch shooter's `ps.velocity`. |
| `0x2111a`, `0x211e6` | `TossClientItems`-class (weapon-drop-on-death) — spawns pickup entity via `G_Spawn` (`0x283de`), sets its own toss-velocity trajectory; unrelated to player `ps.velocity` |
| `0x4d73` | `BG_EvaluateTrajectory` — generic shared trajectory evaluator (`switch` on `trType`, 6 cases matching vanilla `TR_STATIONARY`..`TR_GRAVITY`); writes only into the caller-supplied output vector, never a fixed struct offset; confirmed unrelated to `ps.velocity` — see [G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md) |
| `0x22880` | DeFRaG Lightning-Gun "phantom missile" spawner — `fire_*`-shaped, `damage=0`, `splashDamage=45`, `splashRadius=46`, `mod=11`(MOD_LIGHTNING), `think` = `G_FreeEntity`-only wrapper (`0x22877`→`0x284b7`); never calls `G_Damage`/`G_RadiusDamage`; see [G_MISSILE/README.md](G_MISSILE/README.md) |
| `0x284b7` | `G_FreeEntity` (DeFRaG) — confirmed via `memset`-style entity clear (`trap -0x65`), classname reset, `freetime`/`inuse` writes |
| `0x2295f` | `fire_grapple` — grapple hook launch, confirmed via `trDelta = dir * 800.0` (`0x44c80000`) |
| `0x22b16` | Generic 3x3 matrix-vector multiply helper (`out = M * v`), used by the grapple-swing subsystem; no fixed struct identity, purely a math utility |
| `0x22b82`, `0x22eb4` | DeFRaG grapple-swing physics — writes the grapple hook entity's own `s.pos.trBase` (not `ps.velocity`); when `arg0->client != NULL`, also directly nudges `client->ps.origin` and `ps.delta_angles[1]` (`ANGLE2SHORT`-style). Reached only via `0x23429`→`0x232f6`→`0x22fe6`, gated on entity flags bit `0x400` (hook actively attached) — unreachable without an active grapple hook; see [G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md) |
| `0x20998`–`0x20afa` | ET_MISSILE-gated proximity scan — builds an `EntitiesInBox` query around a missile's `currentOrigin` and dispatches through a per-entity function-pointer field; grepped for `df_promode`/`G_Damage`/`G_RadiusDamage`/the `pm` global — zero matches; calls neither damage function; purpose (plausibly homing-target or proximity-trigger scan) not further identified but confirmed unrelated to knockback |
| `0x1cb3b` | `TeamCount` (DeFRaG) — confirmed structurally identical to vanilla Q3 `g_utils.c`; see [G_ACTIVE/PickTeam.md](G_ACTIVE/PickTeam.md) |
| `0x1cbbd` | `PickTeam` (DeFRaG) — confirmed; **corrects a prior wrong guess that this address was `G_RunClient`**; see [G_ACTIVE/PickTeam.md](G_ACTIVE/PickTeam.md) |
| `0x1cb0b` | `ClientRespawn` (DeFRaG) — confirmed; **corrects a prior wrong guess that this address was "ClientThink"**; called from inside Pmove outer, gated on `health<=0`; see [G_ACTIVE/ClientRespawn.md](G_ACTIVE/ClientRespawn.md) |
| `func_0x1c937` | `CopyToBodyQue` (DeFRaG) — strong structural match (8-slot body queue, matches vanilla `BODY_QUEUE_SIZE=8`); **corrects a prior wrong guess that this was a "cmd fetch" helper**; see [G_ACTIVE/ClientRespawn.md](G_ACTIVE/ClientRespawn.md) |
| `0x1d18b` | `ClientBegin` (DeFRaG) — confirmed via decoded debug strings (`"ClientBegin: %i"`, `"entered the game."`); see [G_ACTIVE/ClientRespawn.md](G_ACTIVE/ClientRespawn.md) |
| `0x1d27b` | Identity **uncertain** — previously documented outright as `ClientThink_real`, but its only 2 callers (`ClientBegin`, `ClientRespawn`) are both spawn-related, not a per-command dispatcher; may be `ClientSpawn`, or a DeFRaG-specific `ClientSpawn`/`ClientThink_real` merge; see [G_ACTIVE/ClientRespawn.md#open-question](G_ACTIVE/ClientRespawn.md#open-question-what-is-0x1d27b) |

### Hashing scheme caveat (important for future work)

The function hash table at the top of `qagame.dis`, and the hashes in
`baseq3-game-functions.hmap`, appear to be computed over **instruction
opcodes only, not operand/immediate values**. Confirmed by observation:
vanilla's `fire_plasma`, `fire_bfg`, and `fire_rocket` all share one identical
hash (`7100620e`) despite embedding different damage/speed/model constants.
**A hash match proves identical instruction shape/control-flow, not identical
constants.** Always read float/int immediates directly from the bytecode
before concluding two hash-matched functions are truly behaviorally
identical — this was confirmed to matter in practice: DeFRaG's `fire_rocket`
diverges from vanilla via a `df_promode`-conditional branch that a
naive hash-only check would not necessarily flag if the branch's two arms
happened to opcode-match vanilla's single arm (they didn't, in this case —
`fire_rocket`'s hash did change — but this can't be assumed in general).
