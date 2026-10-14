# DeFRaG → oDFs port status

Feature matrix. "Tested" = verified at runtime (headless/screenshots); most ports are only syntax-checked.
Source of truth for the claims is the linked doc; update every loop iteration.

## qagame (code/game)

| Feature | Doc | Decoded | Ported | Tested |
|---|---|---|---|---|
| Race timer publish (ps.stats[7/8/12]) | G_TRIGGER/func_0x38ba.md | yes | yes | yes (HUD) |
| Respawn sweep / timer reset / per-client trigger wait | G_CLIENT/func_0x2830.md, G_TRIGGER/multi_trigger_perclient.md | yes | yes | yes |
| Mode presets (spawn loadout) | G_TRIGGER/func_0x1e60.md | yes | yes | partly |
| 1ms sub-frame timer | G_TRIGGER/Touch_Multi_1mstimer.md | yes | yes | no |
| Interference (df_mp_InterferenceOff) | G_COMBAT/interference.md | yes | yes | no |
| killWallbug | BG_PMOVE/killWallbug.md | yes | yes | no |
| respawnpoint / arena / ignore / firemissile cmds | G_CLIENT/*.md | yes | yes | no |
| Session persistence, lastTeam | G_CLIENT/session_data.md | yes | yes | no |
| Entity-mode exclusion, FL_MOVER | G_SPAWN/entity_mode_exclusion.md | yes | yes | no |
| Mode auto-detect | G_UTILS/func_0x21fc.md | yes | yes (g_gametype=4 forcing no) | no |
| target_* entities (multimanager, speed, smallprint, init, fragsFilter, give, score, teleporter, print) | G_TRIGGER/*.md | yes | yes | no |
| Shooters | G_SPAWN/shooters.md | yes | yes | no |
| Finish handler format/messages | G_TRIGGER/finish_handler.md | yes | partial (cheat suppression, ClientTimerStop log, `stats` cmd, rank bookkeeping missing) | no |
| Scoreboard protocol | G_CLIENT/scoreboard.md | yes | yes | no |
| Spectator kill, ExitLevel d1/d2 | G_CLIENT/Cmd_Kill.md | yes | yes | no |
| Callvote system (df_vote_*, CheckVote) | G_CLIENT/callvote.md | yes | yes (iter 6; 9 of 11 cmds, `gametype` + `kill_OBs` not; `df_vote_system 0` = vanilla) | no |
| `df_ob_KillOBs` consumer (overbounce disable) | cvar table / callvote.md | no (consumer undecoded) | no | - |
| Auth handshake | G_CLIENT/auth_handshake.md | yes, fully resolved (iter 16) | yes, server + cgame reply (iter 16), opt-in `df_auth 1` (default 0); precise incl. DeFRaG's shared-record quirk | no |
| Forced cvars on map load | G_UTILS/G_InitGame_forced_cvars.md | yes | yes (iter 5, opt-in `df_forceCvars 1`; defrag_vers already set, defrag_bspcrc not) | no |
| Per-frame rules `0x23f2` | G_UTILS/func_0x23f2.md | yes | partial (iter 4): sv_fps clamp (df_enforceSvFps, default 0) + idle script (df_sv_script_idleCfg/Delay) ported; promode lock, defrag_svfps/clfps mirrors not | no |
| Clock flags `0x3ccd` (stats[12] 0x20/0x40) | G_UTILS/func_0x3ccd.md | yes | yes (iter 3, cvar df_clockCheck) | no |
| ClientEvents teleporter via `0x1c6be` (arena tag) | G_ACTIVE/ClientEvents.md | yes | yes (iter 1; oDFs SelectSpawnPoint already = furthest picker, only arena tag was missing) | no |
| Finish-time sv_cheats suppression / `stats` cmd | G_TRIGGER/finish_handler.md | yes | yes (iter 2; ClientTimerStop log line still no) | no |
| `defrag_svfps/clfps` writers, `df_competition`, `o3j_version` info keys | G_UTILS/cvar_table.md | partly | **no** | - |

## cgame (code/cgame) — all rely on disasm/CGAME docs

| Feature | Doc | Decoded | Ported |
|---|---|---|---|
| aswitch / cpsm server cmds | CGAME/server_commands.md | yes | yes |
| chkpt / df_options / stats(swallow) / legacy rec,newTime,exec,bestTime,ckpt swallow | CGAME/server_commands.md | yes | yes (iter 9); legacy TimerStarted/TimerStopped/NewTime/route printing not |
| Checkpoint popup `0x50b6` + HUD list `0x5153` (df_checkpoints) | CGAME/cgame_cvar_consumers.md | yes | yes (iter 8, `code/cgame/cg_defrag.c`; plain-time servers only, no reference run for mode 2, list position is ours) |
| Scoreboard parse/draw (DeFRaG `scores` format) | G_CLIENT/scoreboard.md | yes (parser fully, draw columns partly) | yes (iter 10: parser + best-time column + speed-award footer; column layout is ours) |
| Timer HUD + HUD widget system `0x2a12`/`0x259a` | CGAME/hud_widget_system.md | IN PROGRESS (iter 15: widget struct partly decoded; user chose exact decode) | no |
| Cheat/validity report `0x59b4` + checker `0x6237` | CGAME/cheat_report.md | yes | yes (iter 11, `df_validityChecks`; codes 0,1,4,10,11,12-14,16,17; not 2,3,5-9,15, no repeat log) |
| End-of-run stats | CGAME/cgame_stats_dump.md | trackers 0x3a4a/0x4677 + dump decoded (iter 12) | yes (iter 12, `df_noEndOfRunStats`); Highest Jump (0x458e) not |
| svinfo_report `0x4795` | CGAME/svinfo_report.md | yes (iter 14, structure + field list) | yes (iter 14; file-name filter and `time` line not; `svinfo_summary` `0x4b67` not) |
| Accel meter | CGAME/accel_meter.md | structure only | no |
| OB predictor | CGAME/ob_predictor.md | mostly | no |
| CGAZ `0x1cd86`/`0x2a12` | - | no | no |
| Replay/recam `0x176e7`/`0x18873`/`0x18287` | - | no | no |
| df_* cvar table (209 absent) | CGAME/cgame_cvar_table_full.md | yes | no |

## ui (code/ui)

| Feature | Doc | Decoded | Ported |
|---|---|---|---|
| exit after demo | UI/demo_exit.md | yes | **BLOCKED (iter 13)**: `0x1a92` is only called from the demo/.dmq loader `0x1ab2` (cvar `defrag_dmq_playback`, FS_FOpen/Read of a demo-queue file, then an EXEC_APPEND of a formatted command); oDFs ui (Team Arena menus) has no equivalent loader/hook and the engine has no dmq. A port would be dead code. Needs the loader decoded (or a user decision on where to hook) |
| df_ui_* theme + settings menu | UI/ui_theme_menu.md | mapped | no |
| df_demos_* demo browser | UI/ui_cvar_table.md | no | no |

## Tooling note
cgame syntax check: `cc.exe -fsyntax-only -DCGAME -DQ3_VM -I. -I../game -I../qcommon X.c` from `code/cgame` (without `-DQ3_VM -I../qcommon` q_shared.h fails on `Q_PRINTF_FUNC`).
