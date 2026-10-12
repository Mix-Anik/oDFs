# cgame QVM cvar table, full decode (`cgame.dis` data `0x30`..`0x12d0`, 288 entries, stride 16)

Decoded by script with the row format `{storage*, name*, default*, flags}` from [cgame_cvar_table.md](cgame_cvar_table.md). Flag names are the standard Q3 values. `in oDFs` = the name already appears in `code/cgame/cg_main.c`'s table.

Total 288 cvars; **199 are not registered by oDFs's cgame** (the whole DeFRaG client feature set: HUD, ghosts, demos, accel meter, CHS readouts, keymaps, auto-record, scripts). Default values of the ones oDFs does register match DeFRaG exactly.


## cg_ (vanilla/extended) (87)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `cg_ignore` | `0` | `0x0` 0 | yes |
| `cg_autoswitch` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawGun` | `1` | `0x1` ARCHIVE | yes |
| `cg_zoomfov` | `22.5` | `0x1` ARCHIVE | yes |
| `cg_fov` | `90` | `0x1` ARCHIVE | yes |
| `cg_viewsize` | `100` | `0x1` ARCHIVE | yes |
| `cg_stereoSeparation` | `0.4` | `0x1` ARCHIVE |  |
| `cg_shadows` | `1` | `0x1` ARCHIVE | yes |
| `cg_gibs` | `1` | `0x1` ARCHIVE | yes |
| `cg_draw2D` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawStatus` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawTimer` | `0` | `0x1` ARCHIVE | yes |
| `cg_drawFPS` | `0` | `0x1` ARCHIVE | yes |
| `cg_drawSnapshot` | `0` | `0x1` ARCHIVE | yes |
| `cg_draw3dIcons` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawIcons` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawAmmoWarning` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawAttacker` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawCrosshair` | `4` | `0x1` ARCHIVE | yes |
| `cg_drawCrosshairNames` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawRewards` | `1` | `0x1` ARCHIVE | yes |
| `cg_crosshairSize` | `24` | `0x1` ARCHIVE | yes |
| `cg_crosshairHealth` | `1` | `0x1` ARCHIVE | yes |
| `cg_crosshairX` | `0` | `0x1` ARCHIVE | yes |
| `cg_crosshairY` | `0` | `0x1` ARCHIVE | yes |
| `cg_brassTime` | `2500` | `0x1` ARCHIVE | yes |
| `cg_simpleItems` | `0` | `0x1` ARCHIVE | yes |
| `cg_marks` | `1` | `0x1` ARCHIVE | yes |
| `cg_lagometer` | `1` | `0x1` ARCHIVE | yes |
| `cg_lagometer_x` | `1` | `0x1` ARCHIVE |  |
| `cg_lagometer_y` | `1` | `0x1` ARCHIVE |  |
| `cg_railTrailTime` | `400` | `0x1` ARCHIVE | yes |
| `cg_gunX` | `0` | `0x0` 0 | yes |
| `cg_gunY` | `0` | `0x0` 0 | yes |
| `cg_gunZ` | `0` | `0x0` 0 | yes |
| `cg_centertime` | `3` | `0x0` 0 | yes |
| `cg_runpitch` | `0.002` | `0x1` ARCHIVE | yes |
| `cg_runroll` | `0.005` | `0x1` ARCHIVE | yes |
| `cg_bobup` | `0.005` | `0x1` ARCHIVE | yes |
| `cg_bobpitch` | `0.002` | `0x1` ARCHIVE | yes |
| `cg_bobroll` | `0.002` | `0x1` ARCHIVE | yes |
| `cg_swingSpeed` | `0.3` | `0x200` CHEAT | yes |
| `cg_animspeed` | `1` | `0x200` CHEAT | yes |
| `cg_debuganim` | `0` | `0x200` CHEAT | yes |
| `cg_debugposition` | `0` | `0x200` CHEAT | yes |
| `cg_debugevents` | `0` | `0x200` CHEAT | yes |
| `cg_errordecay` | `100` | `0x0` 0 | yes |
| `cg_nopredict` | `0` | `0x0` 0 | yes |
| `cg_noplayeranims` | `0` | `0x200` CHEAT | yes |
| `cg_showmiss` | `0` | `0x0` 0 | yes |
| `cg_footsteps` | `1` | `0x1` ARCHIVE | yes |
| `cg_tracerchance` | `0.4` | `0x200` CHEAT | yes |
| `cg_tracerwidth` | `1` | `0x200` CHEAT | yes |
| `cg_tracerlength` | `100` | `0x200` CHEAT | yes |
| `cg_thirdPersonRange` | `40` | `0x200` CHEAT | yes |
| `cg_thirdPersonAngle` | `0` | `0x200` CHEAT | yes |
| `cg_thirdPerson` | `0` | `0x0` 0 | yes |
| `cg_teamChatTime` | `3000` | `0x1` ARCHIVE | yes |
| `cg_teamChatHeight` | `0` | `0x1` ARCHIVE | yes |
| `cg_forceModel` | `0` | `0x1` ARCHIVE | yes |
| `cg_predictItems` | `1` | `0x1` ARCHIVE | yes |
| `cg_deferPlayers` | `1` | `0x1` ARCHIVE | yes |
| `cg_drawTeamOverlay` | `0` | `0x1` ARCHIVE | yes |
| `cg_stats` | `0` | `0x0` 0 | yes |
| `cg_drawFriend` | `1` | `0x1` ARCHIVE | yes |
| `cg_teamChatsOnly` | `0` | `0x1` ARCHIVE | yes |
| `cg_noVoiceChats` | `0` | `0x1` ARCHIVE | yes |
| `cg_noVoiceText` | `0` | `0x1` ARCHIVE | yes |
| `cg_cameraOrbit` | `0` | `0x200` CHEAT | yes |
| `cg_cameraOrbitDelay` | `50` | `0x1` ARCHIVE | yes |
| `cg_timescaleFadeEnd` | `1` | `0x0` 0 | yes |
| `cg_timescaleFadeSpeed` | `0` | `0x0` 0 | yes |
| `cg_scorePlums` | `1` | `0x1` ARCHIVE | yes |
| `cg_smoothClients` | `0` | `0x3` ARCHIVE|USERINFO | yes |
| `cg_noTaunt` | `0` | `0x1` ARCHIVE | yes |
| `cg_noProjectileTrail` | `0` | `0x1` ARCHIVE | yes |
| `cg_oldRail` | `1` | `0x1` ARCHIVE | yes |
| `cg_oldRocket` | `1` | `0x1` ARCHIVE | yes |
| `cg_oldPlasma` | `1` | `0x1` ARCHIVE | yes |
| `cg_trueLightning` | `0.0` | `0x1` ARCHIVE | yes |
| `cg_noDamageKick` | `0` | `0x1` ARCHIVE |  |
| `cg_noFallKick` | `0` | `0x1` ARCHIVE |  |
| `cg_crosshairColor` | `7` | `0x1` ARCHIVE |  |
| `cg_drawDamage` | `1` | `0x1` ARCHIVE |  |
| `cg_nochatbeep` | `0` | `0x1` ARCHIVE |  |
| `cg_drawBBox` | `0` | `0x100` TEMP |  |
| `cg_muzzleFlash` | `0` | `0x1` ARCHIVE |  |

## other (13)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `teamoverlay` | `0` | `0x42` USERINFO|ROM | yes |
| `com_buildScript` | `0` | `0x0` 0 | yes |
| `cl_paused` | `0` | `0x40` ROM | yes |
| `com_blood` | `1` | `0x1` ARCHIVE | yes |
| `g_synchronousClients` | `0` | `0x0` 0 | yes |
| `timescale` | `1` | `0x0` 0 | yes |
| `com_cameraMode` | `0` | `0x200` CHEAT | yes |
| `pmove_fixed` | `0` | `0x0` 0 | yes |
| `pmove_msec` | `8` | `0x0` 0 | yes |
| `ui_smallFont` | `0.25` | `0x1` ARCHIVE | yes |
| `ui_bigFont` | `0.4` | `0x1` ARCHIVE | yes |
| `ch_recordMessage` | `1` | `0x1` ARCHIVE |  |
| `s_ambient` | `1` | `0x21` ARCHIVE|LATCH |  |

## defrag_* (2)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `defrag_version_server` | `` | `0x40` ROM |  |
| `defrag_version_client` | `` | `0x40` ROM |  |

## df_ misc (23)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_autorecord` | `3` | `0x1` ARCHIVE |  |
| `df_checkpoints` | `2` | `0x1` ARCHIVE |  |
| `df_country` | `CountryHere` | `0x1` ARCHIVE |  |
| `df_drawBesttime` | `1` | `0x1` ARCHIVE |  |
| `df_drawJumpmeters` | `0` | `0x1` ARCHIVE |  |
| `df_drawSpeed` | `1` | `0x1` ARCHIVE |  |
| `df_drawCheckpointStats` | `1` | `0x1` ARCHIVE |  |
| `df_drawCheckpointEvents` | `1` | `0x1` ARCHIVE |  |
| `df_noEndOfRunStats` | `0` | `0x1` ARCHIVE |  |
| `df_noTargetPrint` | `0` | `0x1` ARCHIVE |  |
| `df_cl_respawnMsec` | `250` | `0x3` ARCHIVE|USERINFO |  |
| `df_cl_alwaysDrawItems` | `1` | `0x1` ARCHIVE |  |
| `df_name` | `` | `0x3` ARCHIVE|USERINFO |  |
| `df_recfiles_keepLatest` | `0` | `0x1` ARCHIVE |  |
| `df_recfiles_session` | `` | `0x0` 0 |  |
| `df_tieOverwrites` | `1` | `0x1` ARCHIVE |  |
| `df_unit` | `0` | `0x1` ARCHIVE | yes |
| `df_transitions` | `` | `0x100` TEMP |  |
| `df_mp_NoDrawRadius` | `200` | `0x1` ARCHIVE |  |
| `df_mp_TrackPlayerNum` | `-1` | `0x100` TEMP |  |
| `df_varHelpers_DrawOutput` | `0` | `0x1` ARCHIVE |  |
| `df_player_SmokeTrails` | `0.00` | `0x1` ARCHIVE |  |
| `df_player_Sprites` | `1` | `0x1` ARCHIVE |  |

## Auto-record (5)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_ar_stopdelay` | `3000` | `0x1` ARCHIVE |  |
| `df_ar_format` | `$map.$-route[$gt.$phys.$-mode]$m.$-s.$-ms($pl.$plc)` | `0x1` ARCHIVE |  |
| `df_ar_speedcapture_format` | `0` | `0x1` ARCHIVE |  |
| `df_ar_verbose` | `0` | `0x0` 0 |  |
| `df_ar_targetfolder` | `./` | `0x0` 0 |  |

## Demos (23)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_demos_validation` | `0` | `0x1` ARCHIVE |  |
| `df_demos_CameraMode` | `0` | `0x1` ARCHIVE |  |
| `df_demos_CameraTransitionSpeed` | `50` | `0x1` ARCHIVE |  |
| `df_demos_HidePacketEntities` | `0` | `0x1` ARCHIVE |  |
| `df_demos_HideLocalEntities` | `0` | `0x1` ARCHIVE |  |
| `df_demos_ReplayMode` | `0` | `0x1` ARCHIVE |  |
| `df_demos_ReplayFilename` | `$demoname` | `0x1` ARCHIVE |  |
| `df_demos_ReplayDrawMask` | `32767` | `0x1` ARCHIVE |  |
| `df_demos_UnlockThirdPersonPitch` | `0` | `0x1` ARCHIVE |  |
| `df_demos_ModSupport` | `0` | `0x100` TEMP |  |
| `df_demos_Seek` | `0.00` | `0x100` TEMP |  |
| `df_demos_CamSeqPosition` | `` | `0x100` TEMP |  |
| `df_demos_CamSeqSpeed` | `` | `0x100` TEMP |  |
| `df_demos_CamSeqMode` | `` | `0x100` TEMP |  |
| `df_demos_CamOffsetPositionX` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetPositionY` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetPositionZ` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetAngleX` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetAngleY` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetAngleZ` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetRadiusF` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetRadiusR` | `0` | `0x100` TEMP |  |
| `df_demos_CamOffsetRadiusU` | `0` | `0x100` TEMP |  |

## HUD (25)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_hud` | `1` | `0x1` ARCHIVE |  |
| `df_hud_3DIcons` | `1` | `0x1` ARCHIVE |  |
| `df_hud_color` | `4` | `0x1` ARCHIVE |  |
| `df_hud_colorRGB` | `` | `0x1` ARCHIVE |  |
| `df_hud_colorwarning` | `1` | `0x1` ARCHIVE |  |
| `df_hud_drawStatus` | `1` | `0x0` 0 |  |
| `df_hud_drawSpectFollow` | `1` | `0x1` ARCHIVE |  |
| `df_hud_forceteamcolors` | `1` | `0x1` ARCHIVE |  |
| `df_hud_fontcolor` | `7` | `0x1` ARCHIVE |  |
| `df_hud_fontcolorRGB` | `` | `0x1` ARCHIVE |  |
| `df_hud_fontshadow` | `1` | `0x1` ARCHIVE |  |
| `df_hud_transparency` | `0.25` | `0x1` ARCHIVE |  |
| `df_hud_frameTransparency` | `1.0` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_enable` | `0` | `0x0` 0 |  |
| `df_hud_videocapture_x` | `320` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_y` | `240` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_fontsize` | `16` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_fontcolor` | `7` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_fontshadow` | `1` | `0x1` ARCHIVE |  |
| `df_hud_videocapture_fontjustify` | `0` | `0x1` ARCHIVE |  |
| `df_hud_scoreboard_size` | `0` | `0x1` ARCHIVE |  |
| `df_hud_scoreboard_showSpectators` | `1` | `0x1` ARCHIVE |  |
| `df_hud_cgaz` | `0` | `0x1` ARCHIVE |  |
| `df_hud_cgaz_opacity` | `0.5` | `0x1` ARCHIVE |  |
| `df_hud_cgaz_ypos` | `256` | `0x1` ARCHIVE |  |

## Scripts (3)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_script_onMapLoad` | `0` | `0x1` ARCHIVE |  |
| `df_script_onJoinTeam` | `0` | `0x1` ARCHIVE |  |
| `df_script_onRespawn` | `0` | `0x1` ARCHIVE |  |

## Sounds (2)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_sound_flagGrabbing` | `0` | `0x1` ARCHIVE |  |
| `df_sound_leadChanges` | `1` | `0x1` ARCHIVE |  |

## CHS (checkpoint/speed readouts) (50)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_chs_UsePredicted` | `1` | `0x1` ARCHIVE |  |
| `df_chs0_Draw` | `0` | `0x1` ARCHIVE |  |
| `df_chs0_FontSize` | `8` | `0x1` ARCHIVE |  |
| `df_chs0_FontShadow` | `0` | `0x1` ARCHIVE |  |
| `df_chs0_FontColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs0_FontTransparency` | `1.00` | `0x1` ARCHIVE |  |
| `df_chs0_LabelColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs0_DisplayType` | `0` | `0x1` ARCHIVE |  |
| `df_chs0_OffsetX` | `0` | `0x1` ARCHIVE |  |
| `df_chs0_OffsetY` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Draw` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_FontSize` | `10` | `0x1` ARCHIVE |  |
| `df_chs1_FontShadow` | `1` | `0x1` ARCHIVE |  |
| `df_chs1_FontColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs1_FontTransparency` | `1.00` | `0x1` ARCHIVE |  |
| `df_chs1_LabelColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs1_LabelType` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_DisplayType` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_OffsetX` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_OffsetY` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_EnableCombos` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_ComboSeparator` | `/` | `0x1` ARCHIVE |  |
| `df_chs1_Info1` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info2` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info3` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info4` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info5` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info6` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info7` | `0` | `0x1` ARCHIVE |  |
| `df_chs1_Info8` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Draw` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_FontSize` | `8` | `0x1` ARCHIVE |  |
| `df_chs2_FontShadow` | `1` | `0x1` ARCHIVE |  |
| `df_chs2_FontColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs2_FontTransparency` | `1.00` | `0x1` ARCHIVE |  |
| `df_chs2_LabelColor` | `7` | `0x1` ARCHIVE |  |
| `df_chs2_LabelType` | `1` | `0x1` ARCHIVE |  |
| `df_chs2_DisplayType` | `1` | `0x1` ARCHIVE |  |
| `df_chs2_OffsetX` | `-300` | `0x1` ARCHIVE |  |
| `df_chs2_OffsetY` | `100` | `0x1` ARCHIVE |  |
| `df_chs2_EnableCombos` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_ComboSeparator` | `/` | `0x1` ARCHIVE |  |
| `df_chs2_Info1` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info2` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info3` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info4` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info5` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info6` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info7` | `0` | `0x1` ARCHIVE |  |
| `df_chs2_Info8` | `0` | `0x1` ARCHIVE |  |

## Accel meter (9)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_accel_Draw` | `0` | `0x1` ARCHIVE |  |
| `df_accel_FontSize` | `7` | `0x1` ARCHIVE |  |
| `df_accel_Mode` | `0` | `0x1` ARCHIVE |  |
| `df_accel_Output` | `0` | `0x1` ARCHIVE |  |
| `df_accel_OffsetX` | `0` | `0x1` ARCHIVE |  |
| `df_accel_OffsetY` | `0` | `0x1` ARCHIVE |  |
| `df_accel_Scale` | `-1.2` | `0x1` ARCHIVE |  |
| `df_accel_AdjustMaxSpeed` | `1` | `0x1` ARCHIVE |  |
| `df_accel_HistoryResetSpeed` | `100` | `0x1` ARCHIVE |  |

## Ghosts (35)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_ghosts_Command` | `` | `0x100` TEMP |  |
| `df_ghosts_FileExtension` | `` | `0x1` ARCHIVE |  |
| `df_ghosts_DrawOutput` | `0` | `0x1` ARCHIVE |  |
| `df_ghosts_AutoMode` | `4` | `0x1` ARCHIVE |  |
| `df_ghosts_AutoModeMP` | `0` | `0x1` ARCHIVE |  |
| `df_ghosts_NoDrawRadius` | `200` | `0x1` ARCHIVE |  |
| `df_ghosts_LiveAmmo` | `0` | `0x1` ARCHIVE |  |
| `df_ghosts_CheckPVS` | `1` | `0x1` ARCHIVE |  |
| `df_ghosts_SelfMissilesOnly` | `1` | `0x1` ARCHIVE |  |
| `df_ghosts_SpriteInfo` | `` | `0x1` ARCHIVE |  |
| `df_ghosts_SpriteSize` | `8` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewDraw` | `3` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewDrawGun` | `1` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewCameraMode` | `1` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewSize` | `25` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewOffsetX` | `235` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewOffsetY` | `-155` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewBorderSize` | `4` | `0x1` ARCHIVE |  |
| `df_ghosts_MiniviewFOV` | `90` | `0x1` ARCHIVE |  |
| `df_ghost1_RecordFileFormat` | `2` | `0x1` ARCHIVE |  |
| `df_ghost1_RecordInterval` | `50` | `0x1` ARCHIVE |  |
| `df_ghost1_RecordView` | `0` | `0x1` ARCHIVE |  |
| `df_ghost1_PlayTimescale` | `1.0` | `0x1` ARCHIVE |  |
| `df_ghost1_PlayDrawPlayer` | `3` | `0x1` ARCHIVE |  |
| `df_ghost1_PlayDrawMissiles` | `2` | `0x1` ARCHIVE |  |
| `df_ghost1_PlayCameraMode` | `0` | `0x1` ARCHIVE |  |
| `df_ghost1_PlayLoop` | `0` | `0x1` ARCHIVE |  |
| `df_ghost2_RecordFileFormat` | `2` | `0x1` ARCHIVE |  |
| `df_ghost2_RecordInterval` | `50` | `0x1` ARCHIVE |  |
| `df_ghost2_RecordView` | `0` | `0x1` ARCHIVE |  |
| `df_ghost2_PlayTimescale` | `1.0` | `0x1` ARCHIVE |  |
| `df_ghost2_PlayDrawPlayer` | `3` | `0x1` ARCHIVE |  |
| `df_ghost2_PlayDrawMissiles` | `2` | `0x1` ARCHIVE |  |
| `df_ghost2_PlayCameraMode` | `0` | `0x1` ARCHIVE |  |
| `df_ghost2_PlayLoop` | `0` | `0x1` ARCHIVE |  |

## Keymaps (7)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_keymaps` | `0` | `0x100` TEMP |  |
| `df_keymaps_forward` | `` | `0x100` TEMP |  |
| `df_keymaps_back` | `` | `0x100` TEMP |  |
| `df_keymaps_moveleft` | `` | `0x100` TEMP |  |
| `df_keymaps_moveright` | `` | `0x100` TEMP |  |
| `df_keymaps_moveup` | `` | `0x100` TEMP |  |
| `df_keymaps_movedown` | `` | `0x100` TEMP |  |

## Overbounce (4)

| cvar | default | flags | in oDFs |
|---|---|---|---|
| `df_ob_Fast` | `1` | `0x1` ARCHIVE |  |
| `df_ob_AllSlopes` | `0` | `0x1` ARCHIVE |  |
| `df_ob_OffsetStartPosZ` | `0.00` | `0x100` TEMP |  |
| `df_ob_OffsetStopPosZ` | `0.00` | `0x100` TEMP |  |
