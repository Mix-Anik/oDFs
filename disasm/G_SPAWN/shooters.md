# DeFRaG shooters: `Use_Shooter` (`0x21b5e`), `InitShooter` (`0x21e94`), `InitShooter_Finish` (`0x21e70`)

Supersedes the "not further identified" note for `0x21b5e` in `disasm/README.md` (it is vanilla `g_misc.c` shooter code, extended).

## Identification (strong evidence)
- Spawn table (cgame-style `{name ptr, fn ptr}` pairs at data `0x277c..0x27b0`) maps **seven** classnames to seven tiny spawn fns that each just call `InitShooter(ent, weapon)` (`0x21e94`):
  | classname | spawn fn | weapon |
  |---|---|---|
  | `shooter_rocket` | `0x21f02` | 5 (rocket) |
  | `shooter_plasma` | `0x21f0d` | 8 (plasma) |
  | `shooter_grenade` | `0x21f18` | 4 (grenade) |
  | `shooter_rocket_targetplayer` | `0x21f23` | 5 |
  | `shooter_plasma_targetplayer` | `0x21f2e` | 8 |
  | `shooter_grenade_targetplayer` | `0x21f39` | 4 |
  | `shooter_bfg` | `0x21f44` | 9 (BFG) |
  Vanilla/oDFs only have rocket/plasma/grenade. The three `_targetplayer` fns are byte-identical to the plain ones (they do **not** set `spawnflags`); they are aliases, the behaviour switch is `spawnflags & 1` from the map.
- `InitShooter` (`0x21e94`): `ent->use (+0x2c4) = 0x21b5e`, `ent->s.weapon (+0xc0) = weapon`, `RegisterItem(BG_FindItemForWeapon(weapon))` (`0x4b6a`, `0x2147c`), `G_SetMovedir(ent->s.angles(+0x74), ent->movedir(+0x2a4))` (`0x2830d`),
  `if (!ent->random) ent->random = 1.0f; ent->random = sin(M_PI * ent->random / 180)` (trap `-0x68` = sin, constants `0x40490fdb`, `0x43340000` = 180), `if (ent->target) { ent->think(+0x2b4) = InitShooter_Finish; ent->nextthink(+0x2b0) = level.time + 500 }`, `trap_LinkEntity` (`-0x1f`). Identical to vanilla.
- `InitShooter_Finish` (`0x21e70`): `ent->enemy (+0x300) = G_PickTarget(ent->target)` (`0x280eb`); think/nextthink cleared. Identical to vanilla.

## `Use_Shooter(ent, other, activator)` (`0x21b5e`) — decoded structure
1. Aim `dir`: if `ent->enemy` → `enemy->r.currentOrigin - ent->s.origin` (`+0x1e8`/`+0x5c`), else `ent->movedir`.
2. **DeFRaG addition, `ent->spawnflags (+0x210) & 1`: aim at the activator instead.** `dir = activator->r.currentOrigin - ent->s.origin`; `vel = g_entities[activator+0x178].client->ps.velocity` (`client+0x20`).
   - flag `2` and/or `4` enable target-leading: `len = VectorLength(dir)` (`0x8d63`); projectile speed by `ent->s.weapon`: 4 (grenade) → 700, 5 (rocket) → 900, 8 (plasma) → 2000, else 0 (consts `0x442f0000`, `0x44610000`, `0x44fa0000`);
     `t = len / speed`, `k = 0.25 * t * t`; flag 2: `pos.xy += vel.xy * t`; flag 4: `pos.z += vz - g * t * k` with `g = client->ps.gravity (client+0x30)` — **the dimensional form of this z term looks odd (vz not scaled by t); transcribed literally, not verified for intent**.
     then `dir = pos - ent->s.origin`.
   - then (all spawnflags&1 cases): `u = normalize(vel)` (`0x9d12`); `dir.xy += ent->speed(+0x2a0) * u.xy; dir.z += (float)ent->count(+0x2f8) * u.z` — an extra aim offset along the target's velocity direction.
3. Else-branch tail (common): `VectorNormalize`, `PerpendicularVector` (`0xa27e`), `CrossProduct` (`0x8ed8`), two `crandom() * ent->random` spread terms (`rand()=0x5dc2`, `& 0x7fff / 32767 - 0.5, *2`), `VectorNormalize`, then `switch (ent->s.weapon)`: 4 → `fire_grenade` (`0x2259a`), 5 → `fire_rocket` (`0x2276a`), 8 → `fire_plasma` (`0x224b5`), 9 → `fire_bfg` (`0x22685`), 6/7 → `G_AddEvent`-only default; ends with `G_AddEvent(ent, EV_FIRE_WEAPON, 0)` (`0x2861b`).

## oDFs — PORTED
`g_misc.c`: `Use_Shooter` now has the `spawnflags & 1` activator-aim branch (flag 2 xy lead, flag 4 z term transcribed **literally** incl. the odd `vz - g*t*k` form; the `ent->speed`/`ent->count` velocity-direction offset; no random spread on this path) and `WP_BFG`;
new `SP_shooter_bfg` and `SP_shooter_{rocket,plasma,grenade}_targetplayer` (registered in `g_spawn.c`). Syntax-checked, untested.
**Behaviour change:** only maps using the new classnames or `spawnflags 1` on shooters are affected (vanilla shooters unchanged). Revert = delete the new spawn table entries / the `aimAtActivator` block.
