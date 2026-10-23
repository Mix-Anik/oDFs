/* generated from the DeFRaG ui.qvm by pagegen.py (page demos: init 0x8a2b) */
static const char *uiDfDemosList_3288[] = { "Off", "On", 0 };
static const char *uiDfDemosList_4856[] = { "Off", "On", "Debug Level 1 - Output", "Debug Level 2 - Show Objects", "Debug Level 3 - No Activation", 0 };
static const char *uiDfDemosList_4904[] = { "Off", "General 1", "General 2", "OSP", 0 };
static const char *uiDfDemosList_4924[] = { "Off", "On", "On (verbose)", 0 };
static const char *uiDfDemosList_4956[] = { "0: Off", "1: Free Movement and Angles", "2: 1st Person, Free Angles", "3: 3rd Person, Free Angles", "4: Transitioning", "5: Track Player", "6: Track Player - No Pitch", "7: Track Player - No Yaw", "8: Normal, With Offsets", "9: Auto Chase", "10: User Chase 1", "11: User Chase 2 (thru walls)", 0 };
static const char *uiDfDemosList_5008[] = { "Off", "On", "On (old)", "Save Best Only", 0 };
static const int uiDfDemosShow[3] = { 1, 4, 2 };
static const uiDfModItem_t uiDfDemosItems[] = {
	{ 0, 2, 101, 350, 50, "Replay Mode:", "df_demos_ReplayMode", 3, 0, 4, 0, 0, 0, uiDfDemosList_4856, "Replay Mode ^1(default = On)", "df_demos_ReplayMode" },
	{ 0, 1, 102, 350, 70, "Replay Draw Mask:", "df_demos_ReplayDrawMask", 1, 0, 100000, 0, 25, 63, NULL, "Shows/hides replay entities in debug modes ^1(default = 32767 (all) )", "df_demos_ReplayDrawMask" },
	{ 0, 1, 103, 350, 90, "Replay Filename:", "df_demos_ReplayFilename", 4, 0, 0, 0, 25, 63, NULL, "Replay Filename ^1(default = $demoname)", "df_demos_ReplayFilename" },
	{ 0, 2, 104, 350, 130, "Check PVS:", "df_ghosts_CheckPVS", 3, 0, 1, 0, 0, 0, uiDfDemosList_3288, "Potentially Visible Set check on/off ^1(default = On)", "df_ghosts_CheckPVS" },
	{ 0, 2, 105, 350, 150, "Mod Support:", "df_demos_ModSupport", 3, 0, 3, 0, 0, 0, uiDfDemosList_4904, "Support for other mod demos ^1(default = Off)", "df_demos_ModSupport" },
	{ 0, 1, 106, 350, 170, "Demos UI Compatibility:", "df_demos_UICompatibility", 1, 0, 100000, 0, 10, 10, NULL, "Demos UI: ^2MASK: 0=normal, 1=strict names, 2=no folders, 3=all ^1(default = 0)", "df_demos_UICompatibility" },
	{ 0, 1, 107, 350, 190, "Seek Time:", "df_demos_Seek", 2, -10000, 10000, 0, 10, 10, NULL, "Demo Seek ^2minutes.seconds ^1(default = 0.00)", "df_demos_Seek" },
	{ 0, 2, 201, 350, 230, "Demo Validation:", "df_demos_validation", 3, 0, 2, 0, 0, 0, uiDfDemosList_4924, "Demo Validation ^1(default = Off)", "df_demos_validation" },
	{ 0, 2, 202, 350, 270, "Autorecord (AR):", "df_autorecord", 3, 0, 3, 0, 0, 0, uiDfDemosList_5008, "Demo Autorecord (AR) Options ^1(default = Save Best Only)", "df_autorecord" },
	{ 0, 1, 203, 350, 290, "AR Stop Delay:", "df_ar_stopdelay", 1, 0, 10000, 0, 5, 5, NULL, "AR Stop Delay Time ^1(default = 1800 ms)", "df_ar_stopdelay" },
	{ 0, 1, 204, 350, 310, "AR Naming Format:", "df_ar_format", 4, 0, 0, 0, 30, 63, NULL, "AR Format ^1(default = $map.$-route[$gt.$phys.$-mode]$m.$-s.$-ms($pl.$plc))", "df_ar_format" },
	{ 0, 2, 205, 350, 330, "AR SpeedCapture Format:", "df_ar_speedcapture_format", 3, 0, 1, 0, 0, 0, uiDfDemosList_3288, "AR Speedcapture Format On/Off ^1(default = Off)", "df_ar_speedcapture_format" },
	{ 0, 1, 206, 350, 350, "AR Target Folder:", "df_ar_targetfolder", 4, 0, 0, 0, 30, 63, NULL, "AR Target Folder - destination for AR demos ^1(default = empty)", "df_ar_targetfolder" },
	{ 0, 1, 207, 350, 390, "Player Name:", "df_name", 4, 0, 0, 0, 30, 63, NULL, "Defrag Player's Name", "df_name" },
	{ 0, 1, 208, 350, 410, "Player Country:", "df_country", 4, 0, 0, 0, 30, 63, NULL, "Defrag Player's Country", "df_country" },
	{ 0, 1, 209, 350, 430, "Competition String:", "df_competition", 4, 0, 0, 0, 30, 63, NULL, "Competition string ^3(requires map restart)^7 ^1(default = empty)", "df_competition" },
	{ 1, 2, 301, 350, 50, "Unlock 3rd Person Pitch:", "df_demos_UnlockThirdPersonPitch", 3, 0, 1, 0, 0, 0, uiDfDemosList_3288, "Unlock 3rd Person Pitch Angle ^1(default = Off)", "df_demos_UnlockThirdPersonPitch" },
	{ 1, 1, 302, 350, 90, "Hide Packet Entities:", "df_demos_HidePacketEntities", 1, 0, 64000, 0, 6, 6, NULL, "Hides Packet Entities (mask) ^1(default = 0)", "df_demos_HidePacketEntities" },
	{ 1, 1, 303, 350, 110, "Hide Local Entities:", "df_demos_HideLocalEntities", 1, 0, 64000, 0, 6, 6, NULL, "Hides Local Entities (mask) ^1(default = 0)", "df_demos_HideLocalEntities" },
	{ 1, 2, 304, 350, 150, "Keymapping:", "df_keymaps", 3, 0, 1, 0, 0, 0, uiDfDemosList_3288, "Keymapping on/off ^1(default = Off)", "df_keymaps" },
	{ 1, 1, 305, 350, 170, "+Forward Keymap:", "df_keymaps_forward", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +forward bound key ^1(default = empty)", "df_keymaps_forward" },
	{ 1, 1, 306, 350, 190, "+Back Keymap:", "df_keymaps_back", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +back bound key ^1(default = empty)", "df_keymaps_back" },
	{ 1, 1, 307, 350, 210, "+Moveleft Keymap:", "df_keymaps_moveleft", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +moveleft bound key ^1(default = empty)", "df_keymaps_moveleft" },
	{ 1, 1, 308, 350, 230, "+Moveright Keymap:", "df_keymaps_moveright", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +moveright bound key ^1(default = empty)", "df_keymaps_moveright" },
	{ 1, 1, 309, 350, 250, "+Moveup Keymap:", "df_keymaps_moveup", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +moveup bound key ^1(default = empty)", "df_keymaps_moveup" },
	{ 1, 1, 310, 350, 270, "+Movedown Keymap:", "df_keymaps_movedown", 4, 0, 0, 0, 30, 63, NULL, "Binds a 2nd command to the +movedown bound key ^1(default = empty)", "df_keymaps_movedown" },
	{ 1, 1, 311, 350, 310, "VarTransitions List:", "df_transitions", 4, 0, 0, 0, 30, 63, NULL, "VarTransitions Vstr List ^2t1 t2 ... ^1(default = empty)", "df_transitions" },
	{ 2, 2, 401, 350, 50, "Camera Mode:", "df_demos_CameraMode", 3, 0, 11, 0, 0, 0, uiDfDemosList_4956, "Demos Camera Mode ^1(default = 0: Off)", "df_demos_CameraMode" },
	{ 2, 1, 500, 350, 70, "Player To Track:", "df_mp_TrackPlayerNum", 1, -1, 63, 0, 6, 6, NULL, "Player Number for DemoCam to track ^1(default = -1: Main Player)", "df_mp_TrackPlayerNum" },
	{ 2, 1, 402, 350, 90, "Transition Speed:", "df_demos_CameraTransitionSpeed", 1, -10000, 10000, 0, 6, 6, NULL, "BASE: Negative = Speed, Positive = Time ^1(-10000 to 10000 default = 50)", "df_demos_CameraTransitionSpeed" },
	{ 2, 1, 403, 350, 130, "Position Sequence:", "df_demos_CamSeqPosition", 4, 0, 0, 0, 30, 63, NULL, "SavePosition Sequence Vstr List ^2pos1 pos2 ... ^1(default = empty)", "df_demos_CamSeqPosition" },
	{ 2, 1, 404, 350, 150, "Speed Sequence:", "df_demos_CamSeqSpeed", 4, 0, 0, 0, 30, 63, NULL, "Transition Speed Sequence Vstr List ^2s1 s2 ... ^1(default = empty)", "df_demos_CamSeqSpeed" },
	{ 2, 1, 405, 350, 170, "Mode Sequence:", "df_demos_CamSeqMode", 4, 0, 0, 0, 30, 63, NULL, "Demos Camera Mode Sequence Vstr List ^2m1 m2 ... ^1(default = empty)", "df_demos_CamSeqMode" },
	{ 2, 1, 406, 350, 210, "Position Offset X:", "df_demos_CamOffsetPositionX", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Position Offset X ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetPositionX" },
	{ 2, 1, 407, 350, 230, "Position Offset Y:", "df_demos_CamOffsetPositionY", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Position Offset Y ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetPositionY" },
	{ 2, 1, 408, 350, 250, "Position Offset Z:", "df_demos_CamOffsetPositionZ", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Position Offset Z ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetPositionZ" },
	{ 2, 1, 409, 350, 290, "Angles Offset X:", "df_demos_CamOffsetAngleX", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Angles Offset X ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetAngleX" },
	{ 2, 1, 410, 350, 310, "Angles Offset Y:", "df_demos_CamOffsetAngleY", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Angles Offset Y ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetAngleY" },
	{ 2, 1, 411, 350, 330, "Angles Offset Z:", "df_demos_CamOffsetAngleZ", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Angles Offset Z ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetAngleZ" },
	{ 2, 1, 412, 350, 370, "Forward Radius Offset:", "df_demos_CamOffsetRadiusF", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Forward Radius Offset ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetRadiusF" },
	{ 2, 1, 413, 350, 390, "Right Radius Offset:", "df_demos_CamOffsetRadiusR", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Right Radius Offset ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetRadiusR" },
	{ 2, 1, 414, 350, 410, "Up Radius Offset:", "df_demos_CamOffsetRadiusU", 1, -10000, 10000, 0, 10, 10, NULL, "Demos Camera Up Radius Offset ^1(-10000 to 10000 default = 0)", "df_demos_CamOffsetRadiusU" },
};
static const char *uiDfDemosPageNames[] = { "Misc1", "DemoCams", "Misc2", 0 };
#define UIDF_DEMOS_SPINLABEL "> Demos:"
#define UIDF_DEMOS_SPINHELP "^1Click for ^7Demos^1 choices"
