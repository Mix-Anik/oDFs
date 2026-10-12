# DeFRaG `ui` QVM cvar table (`ui.dis`, data segment `0x30`..`0x410`, 62 entries, stride 16)

Decoded by script (entry = `{vmCvar*, name*, default*, flags}`; flag names use the standard Q3 values). The table is vanilla `ui_main.c`'s `cvarTable` plus DeFRaG's own block; the string pool
rows immediately after the last entry (`Auto-config mode`, `ac`, `res`, ...) are unrelated literals picked up by the same stride and are excluded.

| cvar | default | flags |
|---|---|---|
| `ui_ffa_fraglimit` | `20` | `0x1` ARCHIVE |
| `ui_ffa_timelimit` | `0` | `0x1` ARCHIVE |
| `ui_tourney_fraglimit` | `0` | `0x1` ARCHIVE |
| `ui_tourney_timelimit` | `15` | `0x1` ARCHIVE |
| `ui_team_fraglimit` | `0` | `0x1` ARCHIVE |
| `ui_team_timelimit` | `20` | `0x1` ARCHIVE |
| `ui_team_friendly` | `1` | `0x1` ARCHIVE |
| `ui_ctf_capturelimit` | `8` | `0x1` ARCHIVE |
| `ui_ctf_timelimit` | `30` | `0x1` ARCHIVE |
| `ui_ctf_friendly` | `0` | `0x1` ARCHIVE |
| `g_arenasFile` | `` | `0x50` INIT|ROM |
| `g_botsFile` | `` | `0x50` INIT|ROM |
| `g_spScores1` | `` | `0x41` ARCHIVE|ROM |
| `g_spScores2` | `` | `0x41` ARCHIVE|ROM |
| `g_spScores3` | `` | `0x41` ARCHIVE|ROM |
| `g_spScores4` | `` | `0x41` ARCHIVE|ROM |
| `g_spScores5` | `` | `0x41` ARCHIVE|ROM |
| `g_spAwards` | `` | `0x41` ARCHIVE|ROM |
| `g_spVideos` | `` | `0x41` ARCHIVE|ROM |
| `g_spSkill` | `2` | `0x21` ARCHIVE|LATCH |
| `ui_spSelection` | `` | `0x40` ROM |
| `ui_browserMaster` | `0` | `0x1` ARCHIVE |
| `ui_browserGameType` | `0` | `0x1` ARCHIVE |
| `ui_browserSortKey` | `4` | `0x1` ARCHIVE |
| `ui_browserShowFull` | `1` | `0x1` ARCHIVE |
| `ui_browserShowEmpty` | `1` | `0x1` ARCHIVE |
| `cg_brassTime` | `2500` | `0x1` ARCHIVE |
| `cg_drawCrosshair` | `4` | `0x1` ARCHIVE |
| `cg_drawCrosshairNames` | `1` | `0x1` ARCHIVE |
| `cg_marks` | `1` | `0x1` ARCHIVE |
| `server1` | `` | `0x1` ARCHIVE |
| `server2` | `` | `0x1` ARCHIVE |
| `server3` | `` | `0x1` ARCHIVE |
| `server4` | `` | `0x1` ARCHIVE |
| `server5` | `` | `0x1` ARCHIVE |
| `server6` | `` | `0x1` ARCHIVE |
| `server7` | `` | `0x1` ARCHIVE |
| `server8` | `` | `0x1` ARCHIVE |
| `server9` | `` | `0x1` ARCHIVE |
| `server10` | `` | `0x1` ARCHIVE |
| `server11` | `` | `0x1` ARCHIVE |
| `server12` | `` | `0x1` ARCHIVE |
| `server13` | `` | `0x1` ARCHIVE |
| `server14` | `` | `0x1` ARCHIVE |
| `server15` | `` | `0x1` ARCHIVE |
| `server16` | `` | `0x1` ARCHIVE |
| `ui_cdkeychecked` | `0` | `0x40` ROM |
| `df_demos_UICompatibility` | `1` | `0x1` ARCHIVE |
| `df_demos_CurrentPath` | `` | `0x100` TEMP |
| `df_demos_CurrentListVars` | `0 0` | `0x100` TEMP |
| `df_ui_CustomMenu` | `2` | `0x1` ARCHIVE |
| `df_ui_DisplayCVarName` | `1` | `0x1` ARCHIVE |
| `df_ui_TextColorLabel` | `1.00 1.00 0.00 1.00` | `0x1` ARCHIVE |
| `df_ui_TextColorValue` | `1.00 0.00 0.00 1.00` | `0x1` ARCHIVE |
| `df_ui_TextColorMedium` | `0.00 0.00 1.00 1.00` | `0x1` ARCHIVE |
| `df_ui_TextColorBig` | `1.00 0.00 0.00 1.00` | `0x1` ARCHIVE |
| `df_ui_TextColorDisabled` | `0.50 0.50 0.50 1.00` | `0x1` ARCHIVE |
| `df_ui_TextColorHighlight` | `1.00 1.00 0.00 1.00` | `0x1` ARCHIVE |
| `df_ui_PulseColor` | `1.00 1.00 1.00 1.00` | `0x1` ARCHIVE |
| `df_ui_ListbarColor` | `1.00 0.00 0.00 0.30` | `0x1` ARCHIVE |
| `df_ui_StatusColor` | `1.00 1.00 1.00 1.00` | `0x1` ARCHIVE |
| `df_ui_CursorColor` | `0.50 0.80 1.00 1.00` | `0x1` ARCHIVE |

## DeFRaG additions (not in vanilla `ui_main.c`)
- `df_demos_UICompatibility` (`1`, ARCHIVE), `df_demos_CurrentPath` (``, TEMP), `df_demos_CurrentListVars` (`"0 0"`, TEMP) — the in-game **demo browser** state.
- `df_ui_CustomMenu` (`2`), `df_ui_DisplayCVarName` (`1`) and the colour set `df_ui_TextColorLabel` (yellow `1 1 0 1`), `TextColorValue` (red `1 0 0 1`), `TextColorMedium` (blue `0 0 1 1`),
  `TextColorBig` (red), `TextColorDisabled` (grey `0.5`), `TextColorHighlight` (yellow), `PulseColor` (white), `ListbarColor` (`1 0 0 0.3`), `StatusColor` (white), `CursorColor` (`0.5 0.8 1 1`) — DeFRaG's
  custom options/settings menu theme.

## oDFs
`code/ui` has none of the `df_ui_*`/`df_demos_*` cvars (grep finds no use). Not ported: they only matter once the matching DeFRaG menu files/UI code exist in oDFs's `ui` module.
