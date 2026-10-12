# DeFRaG `target_init` (spawn `0x2bf2`, use `0x2aa8`)

Spawn: `ent->use (+0x2c4) = 0x2aa8`. Use (activator needs a client, else return); spawnflags (`+0x210`) *keep* things; the mode globals are cached cvars from `G_InitRaceMode`'s cvar reads (`0x131820` = `df_ndm_damage`, `0x13182c` bit `0x6` = `df_ndm_weaponsPickup`, bit `0x60` = `df_ndm_powerups`, bit `0x18` = damage-related; see G_TRIGGER/func_0x1e60.md — the cvar names for `0x13182c` bits confirmed this pass from the strings next to the `|= 0x18/0x60/0x6` writes at `0x2078..0x20da`):

| step | condition | effect |
|---|---|---|
| 1 | `!(f&1) && df_ndm_damage` | `ps.stats[3]` (armor, `client+0xc4`) = 0 |
| 2 | `!(f&2) && df_ndm_damage` | health = `client+0x224 + 25` (maxHealth+25) in `ent` and `ps.stats[0]` (`client+0xb8`) |
| 3 | `!(f&4) && (0x13182c&6)` | `ammo[0..15] = 0` (`client+0x178`), `stats[2]` (weapons, `+0xc0`) = 2, `ammo[1] = -1`; unless `f&0x20`: weapons \|= 4, `ammo[2] = 100` |
| 4 | `!(f&8) && (0x13182c&0x60)` | if `powerups[7]` (`client+0x154`, PW_REDFLAG) -> `0x26d23(1)`; elif `[8]` -> `0x26d23(2)`; elif `[9]` -> `0x26d23(0)` (`0x26d23` = `Team_ReturnFlag(team)`, strings "The flag has returned!"/"The %s flag has returned!"); then `memset(client+0x138, 0, 0x40)` (all 16 powerups) |
| 5 | `!(f&0x10) && (0x13182c&0x60)` | `ps.stats[1]` (holdable, `client+0xbc`) = 0 |
| 6 | always | `G_UseTargets(ent, activator)` (`0x28151`), then `ps.stats[12] ^= 1` (`client+0xe8`) |

The toggle of `stats[12]` bit 0 (cgame consumer unknown; bit 1 is the race-timer "initialized" flag) is reproduced as-is.

## oDFs — PORTED
`g_target.c` `Use_Target_Init`/`SP_target_init` (+ spawn table); new cvar `df_ndm_powerups` (default 0, SERVERINFO|LATCH) in `g_main.c` (DeFRaG default `0`, LATCH). Differences: oDFs reads the cvars directly instead of DeFRaG's cached mode globals (note: DeFRaG's mode 1 forces those globals to "all on" — `G_InitRaceMode` sets `0x13182c = 126`, `0x131820 = 1` — oDFs does not replicate that);
max-health source is `ps.stats[STAT_MAX_HEALTH]` instead of the raw `client+0x224` (pers field). Syntax-checked, untested.
**Behaviour change:** none for existing maps (new entity class + a new cvar defaulting to 0). Revert = remove the spawn entry.
