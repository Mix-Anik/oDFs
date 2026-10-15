/* generated from the DeFRaG ui.qvm by pagegen.py (page chs: init 0x6804) */
static const char *uiDfChsList_3288[] = { "Off", "On", 0 };
static const char *uiDfChsList_3832[] = { "Off", "On", "On For Demos", "Off For Demos", "Always On", 0 };
static const char *uiDfChsList_3856[] = { "Black", "Red", "Green", "Yellow", "Blue", "Cyan", "Magenta", "White", 0 };
static const char *uiDfChsList_3892[] = { "Arrows 1", "Arrows 2", "Arrows 1 Tree", "Letters Center", "Letters Tree", "User Center", "User Tree", 0 };
static const char *uiDfChsList_3924[] = { "Crosshair", "Left", "Center", "Right", "Left/Right", 0 };
static const char *uiDfChsList_3948[] = { "Off", "Left", "Center 1", "Center 2", "Right", 0 };
static const char *uiDfChsList_3972[] = { "Graphic", "Text Mode 1 (+/0)", "Text Mode 2 (+/+)", 0 };
static const int uiDfChsShow[3] = { 6, 1, 8 };
static const uiDfModItem_t uiDfChsItems[] = {
	{ 0, 2, 101, 350, 50, "Draw:", "df_chs0_Draw", 3, 0, 4, 0, 0, 0, uiDfChsList_3832, "User Input CHS0 on/off ^1(default = Off)", "df_chs0_Draw" },
	{ 0, 2, 102, 350, 70, "Display Type:", "df_chs0_DisplayType", 3, 0, 6, 0, 0, 0, uiDfChsList_3892, "Display Type ^1(default = Arrows 1)", "df_chs0_DisplayType" },
	{ 0, 1, 103, 350, 110, "Font Size:", "df_chs0_FontSize", 1, 4, 50, 0, 2, 2, NULL, "Font Size ^1(4 to 50 default = 8)", "df_chs0_FontSize" },
	{ 0, 2, 104, 350, 130, "Font Shadow:", "df_chs0_FontShadow", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "Font Shadow on/off ^1(default = Off)", "df_chs0_FontShadow" },
	{ 0, 1, 105, 350, 150, "Font Transparency:", "df_chs0_FontTransparency", 2, 0, 1.0, 0, 4, 4, NULL, "Font Transparency ^1(0.00 to 1.00 default = 1.00)", "df_chs0_FontTransparency" },
	{ 0, 2, 106, 350, 170, "Font Color:", "df_chs0_FontColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Font Color ^1(default = White)", "df_chs0_FontColor" },
	{ 0, 2, 107, 350, 190, "Label Color:", "df_chs0_LabelColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Label Color - 2 Tone Display ^1(default = White)", "df_chs0_LabelColor" },
	{ 0, 1, 108, 350, 230, "Offset X:", "df_chs0_OffsetX", 1, -320, 320, 0, 4, 4, NULL, "Horizontal position relative to screen center ^1(-320 to 320 default = 0)", "df_chs0_OffsetX" },
	{ 0, 1, 109, 350, 250, "Offset Y:", "df_chs0_OffsetY", 1, -240, 240, 0, 4, 4, NULL, "Vertical position relative to screen center ^1(-240 to 240 default = 0)", "df_chs0_OffsetY" },
	{ 1, 0, 0, 225, 30, "CHS 1", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 1, 2, 201, 275, 50, "Draw:", "df_chs1_Draw", 3, 0, 4, 0, 0, 0, uiDfChsList_3832, "CHS1 on/off ^1(default = Off)", "df_chs1_Draw" },
	{ 1, 2, 202, 275, 70, "Display Type:", "df_chs1_DisplayType", 3, 0, 4, 0, 0, 0, uiDfChsList_3924, "Display Type ^1(default = Crosshair)", "df_chs1_DisplayType" },
	{ 1, 1, 203, 275, 90, "Font Size:", "df_chs1_FontSize", 1, 4, 50, 0, 2, 2, NULL, "Font Size ^1(4 to 50 default = 10)", "df_chs1_FontSize" },
	{ 1, 2, 204, 275, 110, "Font Shadow:", "df_chs1_FontShadow", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "Font Shadow on/off ^1(default = On)", "df_chs1_FontShadow" },
	{ 1, 1, 205, 275, 130, "Font Transp.:", "df_chs1_FontTransparency", 2, 0, 1.0, 0, 4, 4, NULL, "Font Transparency ^1(0.00 to 1.00 default = 1.00)", "df_chs1_FontTransparency" },
	{ 1, 2, 206, 275, 150, "Font Color:", "df_chs1_FontColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Font Color ^1(default = White)", "df_chs1_FontColor" },
	{ 1, 2, 207, 275, 170, "Label Color:", "df_chs1_LabelColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Label Color ^1(default = White)", "df_chs1_LabelColor" },
	{ 1, 2, 208, 275, 190, "Label Type:", "df_chs1_LabelType", 3, 0, 4, 0, 0, 0, uiDfChsList_3948, "Label Type ^1(default = Off)", "df_chs1_LabelType" },
	{ 1, 1, 209, 275, 210, "Offset X:", "df_chs1_OffsetX", 1, -320, 320, 0, 4, 4, NULL, "Horizontal position relative to screen center ^1(-320 to 320 default = 0)", "df_chs1_OffsetX" },
	{ 1, 1, 210, 275, 230, "Offset Y:", "df_chs1_OffsetY", 1, -240, 240, 0, 4, 4, NULL, "Vertical position relative to screen center ^1(-240 to 240 default = 0)", "df_chs1_OffsetY" },
	{ 1, 2, 211, 275, 250, "Enable Combos:", "df_chs1_EnableCombos", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "User Defined Combos on/off ^1(default = Off)", "df_chs1_EnableCombos" },
	{ 1, 1, 212, 275, 270, "Combo Sep.:", "df_chs1_ComboSeparator", 4, 0, 0, 0, 5, 5, NULL, "User Defined Combo Separator String ^1(default = /)", "df_chs1_ComboSeparator" },
	{ 1, 1, 213, 275, 290, "Info Item 1:", "df_chs1_Info1", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info1" },
	{ 1, 1, 214, 275, 310, "Info Item 2:", "df_chs1_Info2", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info2" },
	{ 1, 1, 215, 275, 330, "Info Item 3:", "df_chs1_Info3", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info3" },
	{ 1, 1, 216, 275, 350, "Info Item 4:", "df_chs1_Info4", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info4" },
	{ 1, 1, 217, 275, 370, "Info Item 5:", "df_chs1_Info5", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info5" },
	{ 1, 1, 218, 275, 390, "Info Item 6:", "df_chs1_Info6", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info6" },
	{ 1, 1, 219, 275, 410, "Info Item 7:", "df_chs1_Info7", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info7" },
	{ 1, 1, 220, 275, 430, "Info Item 8:", "df_chs1_Info8", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs1_Info8" },
	{ 2, 0, 0, 450, 30, "CHS 2", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 2, 2, 301, 500, 50, "Draw:", "df_chs2_Draw", 3, 0, 4, 0, 0, 0, uiDfChsList_3832, "CHS2 on/off ^1(default = Off)", "df_chs2_Draw" },
	{ 2, 2, 302, 500, 70, "Display Type:", "df_chs2_DisplayType", 3, 0, 4, 0, 0, 0, uiDfChsList_3924, "Display Type ^1(default = Left)", "df_chs2_DisplayType" },
	{ 2, 1, 303, 500, 90, "Font Size:", "df_chs2_FontSize", 1, 4, 50, 0, 2, 2, NULL, "Font Size ^1(4 to 50 default = 8)", "df_chs2_FontSize" },
	{ 2, 2, 304, 500, 110, "Font Shadow:", "df_chs2_FontShadow", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "Font Shadow on/off ^1(default = On)", "df_chs2_FontShadow" },
	{ 2, 1, 305, 500, 130, "Font Transp.:", "df_chs2_FontTransparency", 2, 0, 1.0, 0, 4, 4, NULL, "Font Transparency ^1(0.00 to 1.00 default = 1.00)", "df_chs2_FontTransparency" },
	{ 2, 2, 306, 500, 150, "Font Color:", "df_chs2_FontColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Font Color ^1(default = White)", "df_chs2_FontColor" },
	{ 2, 2, 307, 500, 170, "Label Color:", "df_chs2_LabelColor", 3, 0, 7, 0, 0, 0, uiDfChsList_3856, "Label Color ^1(default = White)", "df_chs2_LabelColor" },
	{ 2, 2, 308, 500, 190, "Label Type:", "df_chs2_LabelType", 3, 0, 4, 0, 0, 0, uiDfChsList_3948, "Label Type ^1(default = Left)", "df_chs2_LabelType" },
	{ 2, 1, 309, 500, 210, "Offset X:", "df_chs2_OffsetX", 1, -320, 320, 0, 4, 4, NULL, "Horizontal position relative to screen center ^1(-320 to 320 default = -300)", "df_chs2_OffsetX" },
	{ 2, 1, 310, 500, 230, "Offset Y:", "df_chs2_OffsetY", 1, -240, 240, 0, 4, 4, NULL, "Vertical position relative to screen center ^1(-240 to 240 default = 100)", "df_chs2_OffsetY" },
	{ 2, 2, 311, 500, 250, "Enable Combos:", "df_chs2_EnableCombos", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "User Defined Combos on/off ^1(default = Off)", "df_chs2_EnableCombos" },
	{ 2, 1, 312, 500, 270, "Combo Sep.:", "df_chs2_ComboSeparator", 4, 0, 0, 0, 5, 5, NULL, "User Defined Combo Separator String ^1(default = /)", "df_chs2_ComboSeparator" },
	{ 2, 1, 313, 500, 290, "Info Item 1:", "df_chs2_Info1", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info1" },
	{ 2, 1, 314, 500, 310, "Info Item 2:", "df_chs2_Info2", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info2" },
	{ 2, 1, 315, 500, 330, "Info Item 3:", "df_chs2_Info3", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info3" },
	{ 2, 1, 316, 500, 350, "Info Item 4:", "df_chs2_Info4", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info4" },
	{ 2, 1, 317, 500, 370, "Info Item 5:", "df_chs2_Info5", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info5" },
	{ 2, 1, 318, 500, 390, "Info Item 6:", "df_chs2_Info6", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info6" },
	{ 2, 1, 319, 500, 410, "Info Item 7:", "df_chs2_Info7", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info7" },
	{ 2, 1, 320, 500, 430, "Info Item 8:", "df_chs2_Info8", 4, 0, 0, 0, 10, 30, NULL, "Type ^2info^7 in console ^3(while ingame)^7 for complete list ^1(default = 0)", "df_chs2_Info8" },
	{ 3, 2, 400, 350, 50, "Draw:", "df_accel_Draw", 3, 0, 4, 0, 0, 0, uiDfChsList_3832, "Accelmeter on/off ^1(default = Off)", "df_accel_Draw" },
	{ 3, 2, 401, 350, 70, "Display Type:", "df_accel_Mode", 3, 0, 2, 0, 0, 0, uiDfChsList_3972, "Display type ^1(default = Graphic)", "df_accel_Mode" },
	{ 3, 1, 402, 350, 110, "Font Size:", "df_accel_FontSize", 1, 4, 50, 0, 2, 2, NULL, "Font Size (for Text display type) ^1(4 to 50 default = 7)", "df_accel_FontSize" },
	{ 3, 1, 403, 350, 150, "Offset X:", "df_accel_OffsetX", 1, -320, 320, 0, 4, 4, NULL, "Horizontal position relative to screen center ^1(-320 to 320 default = 0)", "df_accel_OffsetX" },
	{ 3, 1, 404, 350, 170, "Offset Y:", "df_accel_OffsetY", 1, -240, 240, 0, 4, 4, NULL, "Vertical position relative to screen center ^1(-240 to 240 default = 0)", "df_accel_OffsetY" },
	{ 3, 1, 405, 350, 210, "Scale:", "df_accel_Scale", 2, -1000.0, 1000.0, 0, 8, 8, NULL, "Sensitivity (negative=velocity scaling) ^1(-1000.000 to 1000.000 def. = -1.2)", "df_accel_Scale" },
	{ 3, 2, 406, 350, 230, "Adjust Maximum Speed:", "df_accel_AdjustMaxSpeed", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "Adjust Maximum Speed ^2(off for TrueMaxSpeed) ^1(default = On)", "df_accel_AdjustMaxSpeed" },
	{ 3, 2, 407, 350, 270, "Jump Stats Data Output:", "df_accel_Output", 3, 0, 1, 0, 0, 0, uiDfChsList_3288, "Accelmeter jump statistics on landing on/off ^1(default = Off)", "df_accel_Output" },
	{ 3, 1, 408, 350, 310, "Jump History Reset Speed:", "df_accel_HistoryResetSpeed", 1, 0, 10000, 0, 8, 8, NULL, "Resets jump histories if ^2ground speed < value ^1(0 to 10000 default = 100)", "df_accel_HistoryResetSpeed" },
};
static const char *uiDfChsPageNames[] = { "CHS1/CHS2", "User Input CHS0", "Accelmeter", 0 };
#define UIDF_CHS_SPINLABEL "> Crosshairs:"
#define UIDF_CHS_SPINHELP "^1Click for ^7Crosshair^1 choices"
