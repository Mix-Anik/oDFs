# DeFRaG cgame — configstring contract (server → client audit, part 1)

Method: (1) `CG_ConfigString` = cgame `0xd6c` (hash-matched to vanilla, `trap_GetGameState` based; `CG_ParseServerinfo` `0x281a3`, `CG_ParseWarmup` `0x2829a`, `CG_SetConfigValues` `0x282dd` also hash-match vanilla); the DeFRaG `CG_ConfigStringModified` is **`0x283fc`** (no vanilla hash match, called from `0x28d70`). (2) Every call site of `0xd6c` with a literal index was tabulated by script (arg-0 constant before `call`).

## Indices read by DeFRaG cgame
| index | name | readers |
|---|---|---|
| 0 | `CS_SERVERINFO` | `0x3c33` (version/info parse), `0x7f2a`, `0xc320`, `0x1185f`, `0x118f6`, `0x11958`, `0x18ff3`, `0x281a3` |
| 1 | `CS_SYSTEMINFO` | `0x3c33`, `0x7f2a` (see `cheat_report.md`) |
| 2 | `CS_MUSIC` | `0xd8d` |
| 3, 4 | `CS_MESSAGE`, `CS_MOTD` | `0x34f5`, `0x27ce9` |
| 5 | `CS_WARMUP` | `0x2829a`, `0x282dd` |
| 6, 7 | `CS_SCORES1/2` | `0x282dd` |
| 8–11 | `CS_VOTE_TIME/STRING/YES/NO` | `0xdc1` |
| 0x14 (20) | `CS_GAME_VERSION` | `0xdc1`, `0x7f2a` |
| 0x15 (21) | `CS_LEVEL_START_TIME` | `0xdc1`, `0x282dd` |
| 0x17 (23) | `CS_FLAGSTATUS` | `0x282dd` |
| 0x18 (24) | `CS_SHADERSTATE` | `0x28344` |
| 0x1b (27) | `CS_ITEMS` | `0x358`, `0x835`, `0x3909` |
| variable | models `0x20..0x11f`, sounds `0x120..0x21f` (names starting `*` skipped, `0x2a`), players `0x220..0x25f` | `0x283fc` (change handler), `0x1f476`… |
`0x283fc` handles exactly the vanilla set: 0 (serverinfo), 1 (systeminfo), 2 (music), 5 (warmup), 6/7, 8–11 (vote), 12–19 (team votes: ranges `0xc..0xd`, `0xe..0xf`, `0x10..0x11`, `0x12..0x13`), 0x15, 0x16 (intermission), 0x17, models/sounds/players. **No DeFRaG-specific configstring index** is read anywhere in cgame.

## Game-version handshake (verified both sides)
`cgame 0xdc1` (CG_Init) compares `CS_GAME_VERSION` with `va("%s-%i", "defrag", 5)` = **`"defrag-5"`** (string pool `0x4a24`/`0x4a2b`) and errors `"Client/Server game mismatch: %s/%s"`. oDFs `bg_public.h`: `#define GAME_VERSION "defrag-5"` — identical. 

## Conclusion
All DeFRaG-specific data reaches the cgame through serverinfo/systeminfo cvars (`defrag_vers`, `defrag_flags`, `defrag_gametype`, `df_ndm_timer_*`, `df_promode`, physics cvars: see `cheat_report.md`, `cgame_timer_hud.md`) and `ps.stats`/`ps.powerups` — never through extra configstrings. oDFs qagame needs no configstring additions. Remaining contract audit items: the server-command handlers (`df_options`, `exec`, `rec`, `bestTime`, `newTime`, `chkpt`, `stats`; see `server_commands.md`).

## Server-command payloads that matter (cross-reference)
The `scores` command is **DeFRaG-specific in format** (cgame `0x3fb7` `DF_SVCmd_ParseScores`): see `G_CLIENT/scoreboard.md`. oDFs now sends it.
