# DeFRaG qagame cvar table (`gameCvarTable`, data segment `0x4`..`0x88c`, 78 entries, stride 28)

Decoded by script from the data segment (entry = `{vmCvar*, name*, default*, flags, modificationCount, trackChange, teamShader}`; strings are pool
literals). This replaces the earlier *inferred* defaults for the `df_ndm_*` cvars with the real ones (all medium-high guesses were correct:
`damage 0`, `powerups 0`, `weaponsSet 1022`, `weaponsPickup 0`, `itemsSpawn 1`, `nopad 0`, `noteleport 0`, `nomover 0`, all `CVAR_ARCHIVE`).
Flag names use the standard Q3 values (`1` ARCHIVE, `2` USERINFO, `4` SERVERINFO, `8` SYSTEMINFO, `0x10` INIT, `0x20` LATCH, `0x40` ROM, `0x200` CHEAT, `0x400` NORESTART);
**correction:** the first version of this doc used a wrong name map (it labelled `1` as SERVERINFO etc.); cross-checked here against vanilla cvars (`g_gametype 0x26` = USERINFO|SERVERINFO|LATCH,
`fraglimit 0x405` = NORESTART|SERVERINFO|ARCHIVE, `sv_maxclients 0x25`).

| cvar | default | flags | trackChange |
|---|---|---|---|
| `sv_cheats` | `` | `0x0` 0 |  |
| `gamename` | `defrag` | `0x44` SERVERINFO|ROM |  |
| `gamedate` | `Feb 29 2024` | `0x40` ROM |  |
| `g_restarted` | `0` | `0x40` ROM |  |
| `sv_mapname` | `` | `0x44` SERVERINFO|ROM |  |
| `g_gametype` | `0` | `0x26` USERINFO|SERVERINFO|LATCH |  |
| `sv_maxclients` | `8` | `0x25` ARCHIVE|SERVERINFO|LATCH |  |
| `g_maxGameClients` | `0` | `0x25` ARCHIVE|SERVERINFO|LATCH |  |
| `fraglimit` | `20` | `0x405` ARCHIVE|SERVERINFO|NORESTART | yes |
| `timelimit` | `0` | `0x405` ARCHIVE|SERVERINFO|NORESTART | yes |
| `capturelimit` | `8` | `0x405` ARCHIVE|SERVERINFO|NORESTART | yes |
| `g_synchronousClients` | `1` | `0x8` SYSTEMINFO |  |
| `g_friendlyFire` | `0` | `0x1` ARCHIVE | yes |
| `g_teamAutoJoin` | `0` | `0x1` ARCHIVE |  |
| `g_teamForceBalance` | `0` | `0x1` ARCHIVE |  |
| `g_warmup` | `20` | `0x1` ARCHIVE | yes |
| `g_doWarmup` | `0` | `0x40` ROM | yes |
| `g_log` | `games.log` | `0x1` ARCHIVE |  |
| `g_logSync` | `0` | `0x1` ARCHIVE |  |
| `g_password` | `` | `0x2` USERINFO |  |
| `g_banIPs` | `` | `0x1` ARCHIVE |  |
| `g_filterBan` | `1` | `0x1` ARCHIVE |  |
| `g_needpass` | `0` | `0x44` SERVERINFO|ROM |  |
| `dedicated` | `0` | `0x0` 0 |  |
| `defrag_vers` | `` | `0x64` SERVERINFO|LATCH|ROM |  |
| `defrag_gametype` | `` | `0x64` SERVERINFO|LATCH|ROM |  |
| `defrag_mode` | `` | `0x64` SERVERINFO|LATCH|ROM |  |
| `defrag_route` | `` | `0x68` SYSTEMINFO|LATCH|ROM |  |
| `defrag_obs` | `` | `0x68` SYSTEMINFO|LATCH|ROM |  |
| `defrag_flags` | `0` | `0x64` SERVERINFO|LATCH|ROM |  |
| `defrag_bspcrc` | `` | `0x68` SYSTEMINFO|LATCH|ROM |  |
| `df_gametype` | `0` | `0x20` LATCH |  |
| `df_promode` | `0` | `0x24` SERVERINFO|LATCH |  |
| `df_mode_tricksmode` | `2` | `0x20` LATCH |  |
| `df_mode_fastcaps` | `2` | `0x20` LATCH |  |
| `df_competition` | `` | `0x24` SERVERINFO|LATCH |  |
| `df_ndm_timer_start` | `` | `0x20` LATCH |  |
| `df_ndm_timer_checkpoint` | `` | `0x20` LATCH |  |
| `df_ndm_timer_stop` | `` | `0x20` LATCH |  |
| `df_ndm_damage` | `0` | `0x20` LATCH |  |
| `df_ndm_powerups` | `0` | `0x20` LATCH |  |
| `df_ndm_weaponsSet` | `1022` | `0x20` LATCH |  |
| `df_ndm_weaponsPickup` | `0` | `0x20` LATCH |  |
| `df_ndm_itemsSpawn` | `1` | `0x20` LATCH |  |
| `df_ndm_nopad` | `0` | `0x20` LATCH |  |
| `df_ndm_noteleport` | `0` | `0x20` LATCH |  |
| `df_ndm_nomover` | `0` | `0x20` LATCH |  |
| `df_sv_script_idleCfg` | `` | `0x1` ARCHIVE |  |
| `df_sv_script_idleDelay` | `50` | `0x1` ARCHIVE |  |
| `df_dbg_1mstimer` | `0` | `0x0` 0 |  |
| `g_speed` | `320` | `0x208` SYSTEMINFO|CHEAT | yes |
| `g_gravity` | `800` | `0x208` SYSTEMINFO|CHEAT | yes |
| `g_knockback` | `1000` | `0x208` SYSTEMINFO|CHEAT | yes |
| `dmflags` | `0` | `0x204` SERVERINFO|CHEAT |  |
| `g_quadfactor` | `3` | `0x200` CHEAT |  |
| `g_weaponrespawn` | `5` | `0x200` CHEAT |  |
| `g_weaponTeamRespawn` | `5` | `0x200` CHEAT |  |
| `o3j_version` | `1016` | `0x264` SERVERINFO|LATCH|ROM|CHEAT |  |
| `df_mp_InterferenceOff` | `3` | `0x28` SYSTEMINFO|LATCH |  |
| `df_ob_KillOBs` | `0` | `0x20` LATCH |  |
| `df_hook_Type` | `0` | `0x20` LATCH |  |
| `df_lg_Knockback` | `0` | `0x20` LATCH |  |
| `g_forcerespawn` | `20` | `0x0` 0 | yes |
| `g_inactivity` | `0` | `0x0` 0 | yes |
| `g_debugMove` | `0` | `0x0` 0 |  |
| `g_debugDamage` | `0` | `0x0` 0 |  |
| `g_debugAlloc` | `0` | `0x0` 0 |  |
| `g_motd` | `` | `0x0` 0 |  |
| `com_blood` | `1` | `0x0` 0 |  |
| `g_podiumDist` | `80` | `0x0` 0 |  |
| `g_podiumDrop` | `70` | `0x0` 0 |  |
| `g_allowVote` | `1` | `0x1` ARCHIVE |  |
| `g_listEntity` | `0` | `0x0` 0 |  |
| `g_smoothClients` | `1` | `0x0` 0 |  |
| `pmove_fixed` | `1` | `0x8` SYSTEMINFO |  |
| `pmove_msec` | `8` | `0x8` SYSTEMINFO |  |
| `g_rankings` | `0` | `0x0` 0 |  |
| `g_killWallbug` | `1` | `0x21` ARCHIVE|LATCH |  |

## Differences from oDFs's `g_main.c` table (common cvars with a different default)
| cvar | DeFRaG | oDFs | note |
|---|---|---|---|
| `g_synchronousClients` | `1` (SYSTEMINFO) | `0` | DeFRaG's `func_0x21fc` also forces it to `0` on its first-run dedicated path |
| `pmove_fixed` | `1` (SYSTEMINFO) | `0` | |
| `df_gametype` | `0` (LATCH) | `1` (ARCHIVE\|LATCH, added in the entity-mode port) | `0` is *invalid* in DeFRaG → triggers mode auto-detect (`G_UTILS/func_0x21fc.md`); oDFs lacks auto-detect so `1` is kept |
| `defrag_gametype` / `defrag_mode` / `defrag_flags` | `""` / `""` / `0`, flags `0x64` (SERVERINFO\|LATCH\|ROM) | `1` / `-1` / `2`, SERVERINFO | DeFRaG's are ROM and set by code |
| `g_weaponTeamRespawn` | `5` (CHEAT) | `30` | vanilla default is 30; DeFRaG also *forces* it (and other physics cvars) to fixed values on every map load, see [G_InitGame_forced_cvars.md](G_InitGame_forced_cvars.md) |

## Cvars in DeFRaG's table that oDFs does not register
`gamename` (`defrag`, 0x44), `gamedate`, `sv_mapname`, `defrag_vers` (oDFs registers it differently), `defrag_obs`, `defrag_bspcrc`, `df_mode_tricksmode` (`2`),
`df_mode_fastcaps` (`2`), `df_competition`, `df_ndm_powerups`, `df_ndm_itemsSpawn`, `df_sv_script_idleCfg`, `df_sv_script_idleDelay` (`50`), `df_dbg_1mstimer`,
`o3j_version` (`1016`, 0x264), `df_mp_InterferenceOff` (`3`, 0x28), **`g_killWallbug`** (`1`, ARCHIVE|LATCH — not SERVERINFO; consumer decoded and ported: [../BG_PMOVE/killWallbug.md](../BG_PMOVE/killWallbug.md)).
Not in this table but used by code: the `df_vote_*` cvars (read on demand), `defrag_svfps`, `defrag_clfps`, `defrag_svfpsInitialized`, `defrag_idle`.

## oDFs
No code change this pass (flag/default differences are behavioural — `pmove_fixed`/`g_synchronousClients` in particular affect physics timing and were not
changed without runtime testing).
