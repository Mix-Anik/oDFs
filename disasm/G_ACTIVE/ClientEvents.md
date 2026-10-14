# `func_0x1a63a` — `ClientEvents`

**Address:** `0x1a63a` (`enter 0x88`), args `(gentity_t *ent, int oldEventSequence)`.
Sole call site: `0x1aba5` inside `ClientThink_real` (`0x1a866`), with
`arg1 = local110` (the saved pre-`Pmove` `eventSequence`), immediately before
the post-Pmove bookkeeping. This is the vanilla `ClientEvents` call position.

**Correction:** earlier docs ([LG_Knockback.md](LG_Knockback.md),
[../G_WEAPON/FireWeapon.md](../G_WEAPON/FireWeapon.md)) called this a
"per-frame continuous-weapon dispatcher" keyed on `MOD_*` values and treated
the `0xb`/`0xc` cases as Lightning/other weapon cases. That was wrong: the
switch value is `ps.events[i & 1]` (an `EV_*` code), not a means-of-death.

## Evidence it is `ClientEvents`
- `client = ent+0x204`; `client+0x6c` = `ps.eventSequence`, `client+0x70+(i&1)*4` = `ps.events[]`
  (loop `for (i=old; i<ps.eventSequence; i++)`, with the vanilla clamp
  `old < eventSequence-2 → eventSequence-2`).
- `0xb`/`0xc` = `EV_FALL_MEDIUM`/`EV_FALL_FAR`: gate `ent->s.eType == ET_PLAYER`
  (`ent+4 == 1`), damage `5` (medium) / `10` (far), `ent+0x2d0`
  (`pain_debounce_time`) `= level.time + 200`, then `0x2015b` (`G_Damage`) with
  `(ent,0,0,0,0,damage,0,0x13=MOD_FALLING)`. (The earlier "10 if LG, 5 if 0xc"
  reading had the two inverted.)
- `0x17` = `EV_FIRE_WEAPON` → `0x29497` (`FireWeapon(ent)`).
- `0x19` = `EV_USE_ITEM1` (teleporter): checks `ps.powerups[7/8/9]`
  (`client+0x154/0x158/0x15c`; `client+0x138` = `powerups[0]`), `0x4aed` =
  `BG_FindItemForPowerup`, `0x211e6` = `Drop_Item(ent,item,0)`, sets
  `drop->count` (`+0x2f8`) `= (powerup - level.time)/1000`, clamped to ≥1,
  zeroes the powerup — verbatim vanilla flag-drop. Then
  `0x1c6be(client+0x14 /*ps.origin*/, &origin, &angles, client->+0x374)`
  followed by `0x21864` (`TeleportPlayer`).
- `0x1a` = `EV_USE_ITEM2` (medkit): `ent->health` (`+0x2dc`) `= ps.stats[6]`
  (`client+0xd0`, `STAT_MAX_HEALTH`) `+ 25`.
- No MISSIONPACK cases (kamikaze/portal/invulnerability) exist.

## Differences from vanilla
1. **No `DF_NO_FALLING` (`g_dmflags & 8`) check** in the fall case — only the
   `eType` gate. oDFs had the vanilla check; removed in `g_active.c`.
2. Teleporter uses `func_0x1c6be` (DeFRaG's furthest-spawn picker, see
   [../G_CLIENT/func_0x1c6be.md](../G_CLIENT/func_0x1c6be.md)) instead of
   `SelectSpawnPoint`, passing `client+0x374` as the tag filter. **PORTED**:
   oDFs `SelectSpawnPoint` already is the furthest picker and `+0x374` = arena
   number (arena_command.md); the teleporter now sets `s_spawnArena = pers.arena`
   first. Revert: delete that line in g_active.c.

## Still open
- (Resolved) the old "ClientEvents = `0x273f`" label was wrong; `0x273f` clears a
  race-timer record, see [../G_CLIENT/func_0x273f.md](../G_CLIENT/func_0x273f.md).
