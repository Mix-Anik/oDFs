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
| Replay/recam `0x176e7`/`0x18873`/`0x18287` | CGAME/replay_system.md | commands + runtime decoded (iter 73); overlay 0x18873, free camera 0x15550 not | command layer + runtime (iter 70-73) |
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
- **Iter 73**: replay playback runtime decoded (CGAME/replay_system.md Iter 73) and PORTED: `CG_DF_ReplayFrame` (cg_dfreplay.c, hooked after `CG_CalcViewValues` in cg_view.c): ACT evaluation, TRAN stepping (VAR cvars / CAM interpolation), camera override (`df_demos_ReplayMode` 2/3, `df_demos_ReplayCameraMode` 0-8,12 + CamOffset*/TargetPosition* cvars). Host syntax check + full QVM build clean, runtime untested (needs a script in system/replayscripts and ReplayMode 2). BEHAVIOUR: only active when `df_demos_ReplayMode` >= 2 (default 0 = no change). Revert: remove the `CG_DF_ReplayFrame();` line in cg_view.c. Not ported: ReplayMode 1 key state machine, edit overlay 0x18873, camera modes 9-11 (0x15550), ghost-follow, cleanup messages.
- **Iter 74**: ghost subsystem surveyed (NEW CGAME/ghost_system.md): 5 slots x 313712 B, `ghost #N|#all none|stop|record|play|demo [file]` command layer decoded, engine (recorder/file format/playback/miniview) NOT decoded, nothing ported (a command layer without the engine would be dead code).
- **Iter 75**: ghost file format + frame layout + slot fields decoded (CGAME/ghost_system.md Iter 75); recorder sampling/playback/render still undecoded; no port yet.
- **Iter 76**: ghost recorder decoded (CGAME/ghost_system.md Iter 76: sampler, packet writers, frame fill); playback/render/miniview still undecoded; no port (some cg_t fields only identified by lerpFrame-size guess).
- **Iter 77**: ghost playback driver mapped (CGAME/ghost_system.md Iter 77); draw fns 0x13125/0x13317/0x133dc and miniview 0x12a10 undecoded; no port.
- **Iter 78**: ghost frame = predictedPlayerEntity snapshot identified (cg.predictedPlayerEntity = cgame 959248; legs/torso lerpFrames, weapon/anim states); player/missile/sprite draw fns decoded structurally (CGAME/ghost_system.md Iter 78). Still no port; next decode 0x268f2 (DeFRaG CG_Player variant).
- **Iter 79**: ghost helpers decoded (CG_Player is vanilla; label 0x12fc5; missile/sprite builders). Ghost port spec complete for record/play/draw; next iteration writes cg_dfghost.c.
- **Iter 80**: GHOSTS ported (cg_dfghost.c): `ghost` command, recorder, ghost file io (DeFRaG format), playback + CG_Player draw (DrawPlayer 1-3). Host syntax + full QVM build clean, runtime untested (record a ghost, `ghost #1 stop`, `ghost #1 play`). New cvars df_ghosts_NoDrawRadius/FileExtension, df_ghost1/2_* registered (archived); behaviour only with the command. Not ported: missiles/sprites/vars packets, label, marker mode 4, miniview, AutoMode, camera modes, reverse playback, demo cmd.
- **Iter 81**: CHS ghost items 90-96, 98, 99 ported (cg_dfchs.c + accessors in cg_dfghost.c: GhostDist/Nearest/Origin/Velocity/Header; 0xc320 cases 0xd870..0xdac9, helpers 0x14029/0x1406f/0x140b0/0x13eb1/0x13e18/0x13e44). Item 98 prints DeFRaG's constants (624 312000 1560000), item 99 our header magic/version. Build clean, untested (needs a playing ghost). Item 97 is blank in DeFRaG. Revert: remove the `case 90..99` block in cg_dfchs.c.
- **Iter 82**: ghost name label ported (cg_dfghost.c DG_Label; needs gfx/2d/characters/* from the DeFRaG pk3; off by default). Build clean, untested. Ghost remaining: missiles/sprites packets, marker mode 4, miniview, AutoMode, camera modes.
- **Iter 83**: ghost missiles ported (packet 5 record/read/play via CG_Missile with muted missileSound; cg_ents.c CG_DF_AddGhostMissile). Build clean, untested. Ghost remaining: sprites (packet 6, 0x11d58/0x133dc/0x12233), vars (packet 8), marker mode 4, miniview 0x12a10, AutoMode, camera modes, extra-effects 0x122ad.
- **Iter 84**: df_script_onMapLoad/onJoinTeam/onRespawn ported (CGAME/event_scripts.md; cg_defrag.c CG_DF_ScriptEvents + cg_snapshot.c hooks). Build clean, untested. Cgame cvars still absent from code/cgame: ~52 (df_ar_* autorecord 5, df_hud_videocapture_* 6, df_hud_scoreboard_*, df_demos_* misc 9, df_ghosts_* 9, df_cl_respawnMsec, df_player_Sprites/SmokeTrails, df_sound_*, df_mp_*, df_name...).
- **Iter 85**: df_sound_flagGrabbing/df_sound_leadChanges ported (CGAME/event_scripts.md; cg_playerstate.c CG_CheckLocalSounds gates). Build clean, untested. Consumers found (not yet ported): df_player_Sprites 0x26191, df_player_SmokeTrails 0x20dab/0x79c1, df_mp_NoDrawRadius 0x9c47/0x9cf3, df_mp_TrackPlayerNum (0x13488, 0x134dc, 0x14ed5, 0x7c81, 0x8887), df_hud_3DIcons 0x2a12, df_hud_scoreboard_size/showSpectators 0x668c.
- **Iter 86**: df_player_Sprites ported (CG_PlayerSprites gate + blinking connection icon + no friend icon on DeFRaG servers; CGAME/event_scripts.md). Build clean, untested. Next consumers: df_player_SmokeTrails 0x20dab/0x79c1, df_mp_NoDrawRadius 0x9c47/0x9cf3, df_mp_TrackPlayerNum, df_hud_3DIcons 0x2a12, df_hud_scoreboard_* 0x668c.
- **Iter 87**: df_mp_NoDrawRadius ported (hide other players/missiles within |radius| when stats[14] interference bits set; negative = translucent; CG_AddCEntity). Build clean, untested. BEHAVIOUR: with server df_mp_InterferenceOff 3 (default) players within 200u vanish client-side; `df_mp_NoDrawRadius 0` disables. Remaining: df_player_SmokeTrails (0x20a38/0x9082), df_mp_TrackPlayerNum (+ hide lists 0x14633/0x10912), df_hud_3DIcons 0x2a12, df_hud_scoreboard_* 0x668c, df_demos_Hide*Entities (8f77 masks), df_ar_*, videocapture HUD.
- **Iter 88**: df_player_SmokeTrails ported (smoke-puff LE_FRAGMENTs tagged 901-903; CGAME/event_scripts.md). Build clean, untested, off by default. Remaining absent cvars: df_mp_TrackPlayerNum (+0x14633/0x10912 hide lists), df_demos_Hide*Entities (mask 0x8f77), df_hud_3DIcons, df_hud_scoreboard_*, df_ar_* autorecord, df_hud_videocapture_*, misc df_demos_*, df_cl_respawnMsec (no consumer found).
- **Iter 89**: DeFRaG scoreboard draw 0x668c decoded structurally (CGAME/scoreboard_draw.md: size/showSpectators cvars, helpers, colours, strings, row layout); globals 967072..967088 and shader handles 1167984/88/92 still unmapped -> port next. No code change.
- **Iter 90**: DeFRaG scoreboard draw PORTED (CG_DF_DrawScoreboard, cvars df_scoreboard (new, default 1)/df_hud_scoreboard_size/showSpectators; CGAME/scoreboard_draw.md). Build clean, untested visually. BEHAVIOUR CHANGE: default scoreboard is now the race table; `df_scoreboard 0` = vanilla.
- **Iter 91**: AUTORECORD ported (cg_dfautorec.c; CGAME/autorecord.md): df_autorecord (default 3) records every run to demos/temp/current and saves finished runs as demos/<df_ar_format>.dm_<proto>. Build clean, untested at runtime. BEHAVIOUR CHANGE: demos are recorded/saved automatically; revert `df_autorecord 0`. Remaining absent cgame cvars: df_hud_3DIcons (flag box size: bit of unported CTF flag box), df_mp_TrackPlayerNum, df_demos_Hide*/CamSeq*/misc, df_hud_videocapture_*, df_name, df_varHelpers_DrawOutput, df_cl_respawnMsec.
- **Iter 92**: demo camera system (df_demos_CameraMode, df_mp_TrackPlayerNum, replay camera modes 9-11) decoded structurally (CGAME/demo_camera.md: camera object, driver 0x1549d, track record 0x7c81, hide list 0x14633); math of 0x14741 and per-mode details still open; no port yet.
- **Iter 93**: demo camera mode tables resolved (CGAME/demo_camera.md Iter 93: driver/hide/target/aim tables, third-person routine 0x14741). Port next (modes 8-11, 4-7).
- **Iter 94**: demo camera ported (cg_dfdemocam.c: df_demos_CameraMode 4-11, df_mp_TrackPlayerNum, transition, third-person, offsets; CGAME/demo_camera.md). Build clean, untested (needs a demo). Default off. Not ported: free cams 1-3, replay camera modes 9-11, CamSeq presets.
- **Iter 95**: replay camera modes 9-11 now run the demo-camera engine (CG_DF_ReplayFreeCamera; second camera object). Build clean, untested.
- **Iter 96**: df_demos_HidePacketEntities/HideLocalEntities/UnlockThirdPersonPitch ported (entity-type bitmask hide + unlocked third-person pitch; CGAME/demo_camera.md). Build clean, untested, default off. Remaining absent cvars: df_demos_ModSupport (0xa191..: mod-support hooks), df_demos_validation (0x4db6), df_demos_CamSeq* (0x14d30 presets), df_hud_videocapture_*, df_hud_3DIcons, df_varHelpers_DrawOutput, df_name, df_cl_respawnMsec.
- **Iter 97**: df_hud_videocapture_* ported (timer-only 2D mode; CGAME/event_scripts.md). Build clean, untested, off by default. Remaining absent cvars: df_demos_ModSupport (0xa191..), df_demos_validation (0x4db6), df_demos_CamSeq* (0x14d30), df_hud_3DIcons (flag box size, unported box), df_varHelpers_DrawOutput (0xb0ad), df_name, df_cl_respawnMsec.
- **Iter 98**: explosion owner tag (color[0]=clientNum for rocket/grenade/plasma/BFG) + explosion no-draw rule of df_mp_NoDrawRadius ported (CGAME/event_scripts.md). Build clean, untested. UNBLOCKS ghost sprites (packet 6): recorder 0x11d58 + draw 0x133dc/0x12233 next.
- **Iter 99**: ghost explosion sprites ported (packet 6). Ghost remaining: monitored vars (packet 8, 0x11046/0x110ff/0x11ee4), marker mode 4, miniview 0x12a10, AutoMode, play camera modes, reverse playback, extra effects 0x122ad.
- **Iter 100**: demo free cameras (modes 1-3) ported (DC_FreeUpdate: usercmd-driven look + movement). Build clean, untested (needs a demo). Demo camera now complete except CamSeq presets and the viewed-client HUD state (0x15280).
- **Iter 101**: `df_ghosts_Command` cvar pump ported (ghost commands via cvar; DG_Exec shared). AutoMode (0x143f0/0x14474/0x14270/0x14370) remains.
- **Iter 102 (loop paused)**: ghost AutoMode structure recorded (CGAME/ghost_system.md Iter 102), not ported. Loop stopped for a playtest checkpoint: ~60 UNTESTED ports are waiting (see memory file). Resume with /loop to continue (next: AutoMode 0x14110/0x14474, replay mode 1 / overlay 0x18873, CamSeq presets 0x14d30, df_demos_validation/ModSupport, df_name, flag/holdable HUD boxes, ui).
- **Post-pause fix**: live race timer (HUD 'last time' widget refreshed every snapshot) + RaceTime decoder for the obfuscated form (cg_defrag.c CG_DF_RaceTime); previously only the finish time was shown and only on gametype>=5 servers. Build clean, untested. Needs df_hud 1.
