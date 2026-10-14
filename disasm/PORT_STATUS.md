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
| Finish handler format/messages | G_TRIGGER/finish_handler.md | yes | yes (iter 30: ClientTimerStop log line too; cheat suppression, `stats`, bestTime/rank done earlier) | no |
| Scoreboard protocol | G_CLIENT/scoreboard.md | yes | yes | no |
| Spectator kill, ExitLevel d1/d2 | G_CLIENT/Cmd_Kill.md | yes | yes | no |
| Callvote system (df_vote_*, CheckVote) | G_CLIENT/callvote.md | yes | yes (iter 6; all 11 cmds (gametype iter 28/29, kill_OBs iter 29); `df_vote_system 0` = vanilla) | no |
| `df_ob_KillOBs` consumer (overbounce disable) | cvar table / callvote.md | yes | yes (pre-existing `pm.obs_disabled`) | no |
| Auth handshake | G_CLIENT/auth_handshake.md | yes, fully resolved (iter 16) | yes, server + cgame reply (iter 16), opt-in `df_auth 1` (default 0); precise incl. DeFRaG's shared-record quirk | no |
| Forced cvars on map load | G_UTILS/G_InitGame_forced_cvars.md | yes | yes (iter 5, opt-in `df_forceCvars 1`; defrag_vers already set, defrag_bspcrc not) | no |
| Per-frame rules `0x23f2` | G_UTILS/func_0x23f2.md | yes | partial (iter 4): sv_fps clamp (df_enforceSvFps, default 0) + idle script (df_sv_script_idleCfg/Delay) ported; promode lock (`df_lockPromode`, default 0) + defrag_svfps/clfps mirrors ported iter 25 | no |
| Clock flags `0x3ccd` (stats[12] 0x20/0x40) | G_UTILS/func_0x3ccd.md | yes | yes (iter 3, cvar df_clockCheck) | no |
| ClientEvents teleporter via `0x1c6be` (arena tag) | G_ACTIVE/ClientEvents.md | yes | yes (iter 1; oDFs SelectSpawnPoint already = furthest picker, only arena tag was missing) | no |
| Finish-time sv_cheats suppression / `stats` cmd | G_TRIGGER/finish_handler.md | yes | yes (iter 2; ClientTimerStop log line still no) | no |
| `defrag_svfps/clfps` writers, `df_competition`, `o3j_version` info keys | G_UTILS/cvar_table.md | partly | yes (iter 27: `df_competition`, `o3j_version`, `defrag_obs` registered with decoded flags; svfps/clfps writers iter 26; `defrag_servername` not in table) | no |

## cgame (code/cgame) — all rely on disasm/CGAME docs

| Feature | Doc | Decoded | Ported |
|---|---|---|---|
| aswitch / cpsm server cmds | CGAME/server_commands.md | yes | yes |
| chkpt / df_options / stats(swallow) / legacy rec,newTime,exec,bestTime,ckpt swallow | CGAME/server_commands.md | yes | yes (iter 9); legacy TimerStarted/TimerStopped/NewTime/route printing not |
| Checkpoint popup `0x50b6` + HUD list `0x5153` (df_checkpoints) | CGAME/cgame_cvar_consumers.md | yes | yes (iter 8, `code/cgame/cg_defrag.c`; plain-time servers only, no reference run for mode 2, list position is ours) |
| Scoreboard parse/draw (DeFRaG `scores` format) | G_CLIENT/scoreboard.md | yes (parser fully, draw columns partly) | yes (iter 10: parser + best-time column + speed-award footer; column layout is ours) |
| Timer HUD + HUD widget system `0x2a12`/`0x259a` | CGAME/hud_widget_system.md | IN PROGRESS (iter 15: widget struct partly decoded; user chose exact decode) | no |
| Cheat/validity report `0x59b4` + checker `0x6237` | CGAME/cheat_report.md | yes | yes (iter 11, `df_validityChecks`; codes 0,1,4,10,11,12-14,16,17; not 2,3,5-9,15, no repeat log) |
| End-of-run stats | CGAME/cgame_stats_dump.md | trackers 0x3a4a/0x4677 + dump decoded (iter 12) | yes (iter 12, `df_noEndOfRunStats`); Highest Jump (0x458e) iter 31 |
| svinfo_report `0x4795` | CGAME/svinfo_report.md | yes (iter 14, structure + field list) | yes (iter 14; file-name filter and `time` line not; `svinfo_summary` ported iter 59) |
| Accel meter (df_accel_*) | CGAME/accel_meter.md (+ accel_meter_qdec.txt) | yes (iter 38/39, re-verified) | yes (iter 39, `cg_dfaccel.c`; percent/log, ghost players and jump-history logger not) |
| OB predictor `0xb21d` | CGAME/ob_predictor.md | mostly; only consumer = CHS panels (CGAME/chs.md) | no (needs CHS first) |
| CHS panels (`df_chs0/1/2_*`, ~200 info items) | CGAME/chs.md (+ chs_items.txt, chs_*_qdec.txt) | architecture + layout + draw decoded (iter 40/41), item formulas for ~25 of ~200 | partly (iter 41/42, `cg_dfchs.c`: panels 1/2, items 1-6, 20-25, 30-47, 100-109, 111, 114-118, 120-139, 150, 171/172, 210-214, 217, 219-239 (subset); panel 0 key display iter 57; the rest not) |
| CGAZ `0x1a57a` (df_hud_cgaz*) | CGAME/cgaz.md | yes (iter 37) | yes (iter 37, `cg_dfhud.c`, df_hud 1; visually untested) |
| Replay/recam `0x176e7`/`0x18873`/`0x18287` | - | no | no |
| Keymaps (`df_keymaps*`) | CGAME/keymaps.md | yes | yes (iter 45) |
| `$variable` expansion (chsInfo/chsLabel, named CHS vars, space/empty) | CGAME/variables.md | yes | yes (iter 47); var* commands (varMath/varCommand/varCycle/varCycleOnce/varJoin/varFormat/varSave) ported iter 48/49; pstat/player-num vars and change-log lines not |
| Command `info` (CHS lookup) | CGAME/variables.md (iter 53) | yes | yes (iter 53) |
| Demo seek (`df_demos_Seek`) | CGAME/demo_seek.md | yes | yes (iter 52; untested, needs a demo) |
| Commands help/commands/?, read, df_ndm_timer_aliases | CGAME/variables.md (iter 51) | yes | yes (iter 51) |
| Commands savepos / displaymessage / resetjumpmeter | CGAME/variables.md (iter 50) | yes | yes (iter 50; resetjumpmeter clears only oDFs' tracker) |
| Cvar transitions (`df_transitions`) | CGAME/transitions.md | yes | yes (iter 46; direct cvar set instead of `varMath`) |
| df_* cvar table (209 absent) | CGAME/cgame_cvar_table_full.md | yes | no |

| Local run records (`system/records/*.rec`, R1/R2/B, new-record logic, delta mode) | CGAME/hud_widget_system.md (iter 22/23) | yes | yes (iter 23, `cg_dfrecords.c`) |
| HUD widget system (`0x259a` draw, init `0x2fdd`, status bar, power-up list, speed/jump meters) | CGAME/hud_widget_system.md | mostly decoded (iters 15-21) | yes, first pass (iter 24, `cg_dfhud.c`, `df_hud 1`): widgets best/last/speed/powerups/health/armor/ammo/frags/checkpoint line; follow name iter 32, jump meters iter 33, timer/FPS stacking iter 34; vote line (0x1c9f7, deadline timing) iter 35, 0x1c966 is vanilla; helpers 0x2029/0x1c966/0x1c4eb are vanilla (iter 36), 0x235d ported; NOT flag box, holdable box, CGAZ 0x1a57a |

## ui (code/ui)

| Feature | Doc | Decoded | Ported |
|---|---|---|---|
| exit after demo | UI/demo_exit.md | yes | **BLOCKED (iter 13)**: `0x1a92` is only called from the demo/.dmq loader `0x1ab2` (cvar `defrag_dmq_playback`, FS_FOpen/Read of a demo-queue file, then an EXEC_APPEND of a formatted command); oDFs ui (Team Arena menus) has no equivalent loader/hook and the engine has no dmq. A port would be dead code. Needs the loader decoded (or a user decision on where to hook) |
| df_ui_* theme + settings menu | UI/ui_theme_menu.md | mapped | no |
| df_demos_* demo browser | UI/ui_cvar_table.md | no | no |

## Tooling note
cgame syntax check: `cc.exe -fsyntax-only -DCGAME -DQ3_VM -I. -I../game -I../qcommon X.c` from `code/cgame` (without `-DQ3_VM -I../qcommon` q_shared.h fails on `Q_PRINTF_FUNC`).

**Full QVM build (works, iter 58):** from `build-qvm`: `export TEMP=<scratch dir> TMP=<scratch dir>; export PATH=/c/Users/mixanik/Programs/msys64/mingw64/bin:$PATH; cmake --build .` — the "Cannot create temporary file in C:\WINDOWS" failure was just TEMP. Produces vm/qagame.qvm, cgame.qvm, ui.qvm; lcc warnings (e.g. char overflow) show up here but not in the host `cc -fsyntax-only` checks. All ports up to iter 57 link cleanly.

- **Iter 60**: OB predictor (cgame 0xb21d..0xb72e) ported as code/cgame/cg_dfob.c + df_ob_* cvars + CHS items 50-77, 79, 300-308, 310. QVM build clean. Untested at runtime; item-list composition for 50-60 follows the earlier decode (verify against chs_items.txt if output looks off). Revert: drop cg_dfob.c from CMakeLists and the CG_DF_ObFrame call in CG_Draw2D.

- **Iter 61**: CHS items 151/152/153 ("input" arrows / "input (FLBR)" letters / "input (binds)" bound keys; cgame 0xe4ff) ported in cg_dfchs.c `CG_DF_ChsInputLine`. QVM build clean, runtime untested. Revert: remove the `case 151..153` lines and the function.
- **Iter 62**: CHS items 190-193 (demoname/fulldemoname from cvar defrag_servername while cg.demoPlayback; tracking player name / no color) ported in cg_dfchs.c. Build clean, untested. ASSUMPTION: global 0x120e44 (tracked client) treated as cg.snap->ps.clientNum (writer not decoded). Revert: remove case 190-193.
- **Iter 63**: CHS 215 (h/a spent), 216 (shots fired), 225 (multiplayer), 227 (overbounces), 233 (gamepath = fs_game) ported; counters 0x11d238/3c/40 added to the snapshot tracker in cg_defrag.c (health lost, armor = always 0 as in DeFRaG - its own bug, shots = ammo dropped by exactly 1 with same weapon). chs_items.txt labels for 225-233 are off by one (225 multiplayer, 226 physics, 227 overbounces, 228 mapname). Build clean, untested. Revert: remove those cases + tracker lines.
- **Iter 64**: `$PreviousPlayerNum/$NextPlayerNum/$PreviousTeamPlayerNum/$NextTeamPlayerNum` vars (cgame 0x89ac..8a80 + helper 0x7fd5) ported in CG_DF_ExpandVar (`CG_DF_NextPlayerNum`). Start client 0x120e44 assumed = viewed client. `$pstat/$bpstat` deliberately NOT ported: gated by the debug password df_debug_OutputPassword (0x7eaa). Build clean, untested. Revert: remove the pn[] block + function.
- **Iter 65 (ui)**: 15 DeFRaG ui cvars (df_demos_UICompatibility/CurrentPath/CurrentListVars, df_ui_CustomMenu/DisplayCVarName and the 10 df_ui_* colour cvars) registered in code/ui/ui_main.c cvarTable with DeFRaG defaults/flags. Registration only: the colour apply function 0xa847 / register 0xa7e2 just Cvar_Register/Set the 12 theme entries; the consumers (menu drawing using these colours, settings menu 0xb59f, demo browser) are NOT ported. Build clean. Revert: remove the "DeFRaG additions" rows. Behaviour change: none visible beyond extra archived cvars.
- **Iter 66**: replay/recam system surveyed (NEW doc CGAME/replay_system.md: command set, data types, activator types, script writer 0x176e7 and edit overlay 0x18873 located, memory tables). Decoded: structure only; nothing ported. Next: decode the replay dispatcher + runtime 0x18287/0x12a10.
- **Iter 67**: replay dispatcher 0x18006 + slot/table model decoded (CGAME/replay_system.md); still nothing ported (parsers/runtime undecoded).
- **Iter 68**: replay SET parsers + name/limit tables decoded (replay_system.md Iter 68). Still undecoded: GET printer 0x18287, validate 0x16934, loader 0x17e8c, activate 0x17f21, runtime. No port yet.
- **Iter 69**: replay ACTIVATE/VALIDATE/GET-printer decoded (replay_system.md Iter 69). Port of the command layer is now possible: next iteration writes cg_dfreplay.c.
- **Iter 70**: replay command layer PORTED: code/cgame/cg_dfreplay.c (tables VAR/CAM/TRAN/ACT/LINK, `replay SET|GET|CLEAR|DELETE|ACTIVATE|VALIDATE|HELP`). Not ported: LOAD/SAVE (print a notice), GET formats 2/3 and CHAIN tracer, runtime/overlay. Non-DeFRaG cvar df_demos_ReplayVerbose (default 1) stands in for the unresolved global 0x2b150c. Build clean, untested. Revert: drop cg_dfreplay.c from CMakeLists, the `replay` entries in cg_consolecmds.c and the cvar row in cg_main.c.
- **Iter 71**: replay LOAD ported (cg_dfreplay.c: script = list of SET records; default name df_demos_ReplayFilename/$demoname/mapname; cvars df_demos_ReplayMode/Filename/DrawMask registered). Build clean, untested (needs a script file in system/replayscripts). Revert: remove DR_Load* and the LOAD branch. SAVE still pending.
- **Iter 72**: replay SAVE ported (cg_dfreplay.c DR_WriteFile + row/header writers; output is loadable by LOAD). Build clean, untested. Revert: remove the SAVE block/branch. Remaining replay work: GET table formats, playback runtime, edit overlay 0x18873.
