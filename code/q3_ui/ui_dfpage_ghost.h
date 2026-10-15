/* generated from the DeFRaG ui.qvm by pagegen.py (page ghost: init 0x78d3) */
static const char *uiDfGhostList_3288[] = { "Off", "On", 0 };
static const char *uiDfGhostList_4324[] = { "0: Off", "1: On", "2: On - Quaded Missiles", "3: On - Self Style Missiles", "4: Quaded Self Style Missiles", 0 };
static const char *uiDfGhostList_4348[] = { "Off", "Normal", "Use CHS0 Colors", "Use CHS1 Colors", "Use CHS2 Colors", 0 };
static const char *uiDfGhostList_4372[] = { "0: Off", "1: Record From Demo", "2: Play During Demo", "3: Defrag Record/Play", "4: Defrag Record/Play No Cheats", "5: Defrag Play/Play", "6: Record From Defrag Demo", "7: Create Defrag Best From Demo", "8: Overwrite Defrag Best From Demo ", 0 };
static const char *uiDfGhostList_4412[] = { "Camera Only", "Player Only", "All", "All Including Cvars", 0 };
static const char *uiDfGhostList_4432[] = { "Off", "Normal", "Invisible", "Invisible - Use Radius", "Icon", 0 };
static const char *uiDfGhostList_4456[] = { "Off", "Missiles Only", "Missiles And Explosions", 0 };
static const char *uiDfGhostList_4472[] = { "Off", "On", "Pause At End", 0 };
static const char *uiDfGhostList_4488[] = { "0: Off", "1: 1st Person Locked", "2: 1st Person Free", "3: 3rd Person Locked", "4: 3rd Person Free", "5: 1st Person 1st Missile Locked", "6: 1st Person 1st Missile Free", "7: 3rd Person 1st Missile Locked", "8: 3rd Person 1st Missile Free", "9: 1st Person Last Missile Locked", "10: 1st Person Last Missile Free", "11: 3rd Person Last Missile Locked", "12: 3rd Person Last Missile Free", "13: 1st Person CamView", "14: 1st Person CamView PlayerTrack", "15: 1st Person View With Cvars", "16: Cvars Changes Only", "17: Replay Debug", 0 };
static const char *uiDfGhostList_4564[] = { "0: Off", "1: On For Ghosts", "2: On For Ghosts And Demos", "3: On For Demos", "4: Always On - 1P/3P Switching", "5: Always On - 1st Person", "6: Always On - 3rd Person", "7: Other Player Tracking", "8: Other Player Tracking (PVS)", 0 };
static const int uiDfGhostShow[3] = { 1, 6, 8 };
static const uiDfModItem_t uiDfGhostItems[] = {
	{ 0, 2, 101, 350, 50, "Auto Mode:", "df_ghosts_AutoMode", 3, 0, 8, 0, 0, 0, uiDfGhostList_4372, "Auto record/play mode ^1(default = 4: Defrag Record/Play No Cheats)", "df_ghosts_AutoMode" },
	{ 0, 2, 102, 350, 70, "Multiplayer Auto Mode:", "df_ghosts_AutoModeMP", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "Auto record/play mode on/off in multiplayer ^1(default = Off)", "df_ghosts_AutoModeMP" },
	{ 0, 1, 103, 350, 90, "No Draw Radius:", "df_ghosts_NoDrawRadius", 1, 0, 10000, 0, 5, 5, NULL, "Radius to hide ghosts/missiles ^1(0 to 10000 default = 200)", "df_ghosts_NoDrawRadius" },
	{ 0, 2, 104, 350, 130, "Draw Output:", "df_ghosts_DrawOutput", 3, 0, 4, 0, 0, 0, uiDfGhostList_4348, "Ghost console message color ^1(default = Use CHS2 Colors)", "df_ghosts_DrawOutput" },
	{ 0, 1, 105, 350, 170, "Sprite Info:", "df_ghosts_SpriteInfo", 4, 0, 0, 0, 15, 64, NULL, "Sprite info ^2(example: $ghostnum) ^1(default = empty)", "df_ghosts_SpriteInfo" },
	{ 0, 1, 106, 350, 190, "Sprite Size:", "df_ghosts_SpriteSize", 1, 4, 20, 0, 2, 2, NULL, "Sprite info size ^1(default = 8)", "df_ghosts_SpriteSize" },
	{ 0, 2, 107, 350, 230, "Check PVS:", "df_ghosts_CheckPVS", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "Potentially Visible Set check on/off ^1(default = On)", "df_ghosts_CheckPVS" },
	{ 0, 1, 108, 350, 250, "File Extension:", "df_ghosts_FileExtension", 4, 0, 0, 0, 5, 10, NULL, "Ghosts file extension ^1(default = empty)", "df_ghosts_FileExtension" },
	{ 0, 2, 109, 350, 290, "Self Missiles Only:", "df_ghosts_SelfMissilesOnly", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "Record only missiles/explosions from self ^1(default = On)", "df_ghosts_SelfMissilesOnly" },
	{ 0, 2, 110, 350, 330, "Live Ammo:", "df_ghosts_LiveAmmo", 3, 0, 4, 0, 0, 0, uiDfGhostList_4324, "Fire real missiles (during playback, if cheats are enabled) ^1(default = Off)", "df_ghosts_LiveAmmo" },
	{ 1, 0, 0, 280, 50, "Ghost 1", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 1, 2, 201, 350, 70, "File Format:", "df_ghost1_RecordFileFormat", 3, 0, 3, 0, 0, 0, uiDfGhostList_4412, "^1Record option:^7 Ghost file format ^1(default = All)", "df_ghost1_RecordFileFormat" },
	{ 1, 1, 202, 350, 90, "Interval:", "df_ghost1_RecordInterval", 1, 0, 1000, 0, 4, 4, NULL, "^1Record option:^7 Recording frequency ^1(0 to 1000 default = 50)", "df_ghost1_RecordInterval" },
	{ 1, 2, 203, 350, 110, "Record View:", "df_ghost1_RecordView", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "^1Record option:^7 Record the view positon (not player pos.) ^1(default = Off)", "df_ghost1_RecordView" },
	{ 1, 2, 207, 350, 130, "Camera Mode:", "df_ghost1_PlayCameraMode", 3, 0, 17, 0, 0, 0, uiDfGhostList_4488, "^2Play option:^7 Mainview ghost camera mode ^1(default = 0: Off)", "df_ghost1_PlayCameraMode" },
	{ 1, 2, 205, 350, 150, "Draw Player:", "df_ghost1_PlayDrawPlayer", 3, 0, 4, 0, 0, 0, uiDfGhostList_4432, "^2Play option:^7 Draw ghost player ^1(default = Invisible - Use Radius)", "df_ghost1_PlayDrawPlayer" },
	{ 1, 2, 206, 350, 170, "Draw Missiles:", "df_ghost1_PlayDrawMissiles", 3, 0, 2, 0, 0, 0, uiDfGhostList_4456, "^2Play option:^7 Draw missiles/explosions ^1(default = Missiles And Explosions)", "df_ghost1_PlayDrawMissiles" },
	{ 1, 2, 208, 350, 190, "Loop:", "df_ghost1_PlayLoop", 3, 0, 2, 0, 0, 0, uiDfGhostList_4472, "^2Play option:^7 Loop or pause at end of file ^1(default = Off)", "df_ghost1_PlayLoop" },
	{ 1, 1, 204, 350, 210, "Timescale:", "df_ghost1_PlayTimescale", 2, -10.0, 10.0, 0, 7, 7, NULL, "^2Play option:^7 Playback speed ^1(-10.000 to 10.000 default = 1.000)", "df_ghost1_PlayTimescale" },
	{ 2, 0, 0, 280, 250, "Ghost 2", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 2, 2, 301, 350, 270, "File Format:", "df_ghost2_RecordFileFormat", 3, 0, 3, 0, 0, 0, uiDfGhostList_4412, "^1Record option:^7 Ghost file format ^1(default = All)", "df_ghost2_RecordFileFormat" },
	{ 2, 1, 302, 350, 290, "Interval:", "df_ghost2_RecordInterval", 1, 0, 999, 0, 4, 4, NULL, "^1Record option:^7 Recording frequency ^1(0 to 1000 default = 50)", "df_ghost2_RecordInterval" },
	{ 2, 2, 303, 350, 310, "Record View:", "df_ghost2_RecordView", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "^1Record option:^7 Record the view positon (not player pos.) ^1(default = Off)", "df_ghost2_RecordView" },
	{ 2, 2, 307, 350, 330, "Camera Mode:", "df_ghost2_PlayCameraMode", 3, 0, 17, 0, 0, 0, uiDfGhostList_4488, "^2Play option:^7 Mainview ghost camera mode ^1(default = 0: Off)", "df_ghost2_PlayCameraMode" },
	{ 2, 2, 305, 350, 350, "Draw Player:", "df_ghost2_PlayDrawPlayer", 3, 0, 4, 0, 0, 0, uiDfGhostList_4432, "^2Play option:^7 Draw ghost player ^1(default = Invisible - Use Radius)", "df_ghost2_PlayDrawPlayer" },
	{ 2, 2, 306, 350, 370, "Draw Missiles:", "df_ghost2_PlayDrawMissiles", 3, 0, 2, 0, 0, 0, uiDfGhostList_4456, "^2Play option:^7 Draw missiles/explosions ^1(default = Missiles And Explosions)", "df_ghost2_PlayDrawMissiles" },
	{ 2, 2, 308, 350, 390, "Loop:", "df_ghost2_PlayLoop", 3, 0, 2, 0, 0, 0, uiDfGhostList_4472, "^2Play option:^7 Loop or pause at end of file ^1(default = Off)", "df_ghost2_PlayLoop" },
	{ 2, 1, 304, 350, 410, "Timescale:", "df_ghost2_PlayTimescale", 2, -10.0, 10.0, 0, 7, 7, NULL, "^2Play option:^7 Playback speed ^1(-10.000 to 10.000 default = 1.000)", "df_ghost2_PlayTimescale" },
	{ 3, 2, 401, 350, 50, "Draw:", "df_ghosts_MiniviewDraw", 3, 0, 8, 0, 0, 0, uiDfGhostList_4564, "Miniview on/off ^1(default = 3: On For Demos)", "df_ghosts_MiniviewDraw" },
	{ 3, 2, 403, 350, 70, "Ghost Camera Mode:", "df_ghosts_MiniviewCameraMode", 3, 0, 17, 0, 0, 0, uiDfGhostList_4488, "Miniview ghost camera mode ^1(default = 1: 1st Person Locked)", "df_ghosts_MiniviewCameraMode" },
	{ 3, 2, 402, 350, 90, "Draw Gun:", "df_ghosts_MiniviewDrawGun", 3, 0, 1, 0, 0, 0, uiDfGhostList_3288, "Draw gun in miniview on/off ^1(default = On)", "df_ghosts_MiniviewDrawGun" },
	{ 3, 1, 404, 350, 130, "Size:", "df_ghosts_MiniviewSize", 1, 5, 75, 0, 4, 4, NULL, "Size ^1(5 to 75 default = 25)", "df_ghosts_MiniviewSize" },
	{ 3, 1, 405, 350, 150, "Offset X:", "df_ghosts_MiniviewOffsetX", 1, -500, 500, 0, 4, 4, NULL, "Horizontal pos. relative to screen center ^1(-500 to 500 default = 235)", "df_ghosts_MiniviewOffsetX" },
	{ 3, 1, 406, 350, 170, "Offset Y:", "df_ghosts_MiniviewOffsetY", 1, -400, 400, 0, 4, 4, NULL, "Vertical pos. relative to screen center ^1(-400 to 400 default = -155)", "df_ghosts_MiniviewOffsetY" },
	{ 3, 1, 407, 350, 190, "Border Size:", "df_ghosts_MiniviewBorderSize", 1, 0, 25, 0, 4, 4, NULL, "Bordersize ^1(0 to 25 default = 4)", "df_ghosts_MiniviewBorderSize" },
	{ 3, 1, 408, 350, 230, "Field Of View:", "df_ghosts_MiniviewFOV", 1, 5, 250, 0, 3, 3, NULL, "Field-of-view ^1(5 to 250 default = 90)", "df_ghosts_MiniviewFOV" },
	{ 3, 1, 500, 350, 270, "Player To Track:", "df_mp_TrackPlayerNum", 1, -1, 63, 0, 6, 6, NULL, NULL, NULL },
};
static const char *uiDfGhostPageNames[] = { "Global", "Ghost 1/Ghost 2", "Miniview (PIP)", 0 };
#define UIDF_GHOST_SPINLABEL "> Ghosts:"
#define UIDF_GHOST_SPINHELP "^1Click for ^7Ghost^1 choices"
