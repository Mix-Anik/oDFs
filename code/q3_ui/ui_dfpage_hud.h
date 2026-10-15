/* generated from the DeFRaG ui.qvm by pagegen.py (page hud: init 0x521f) */
static const char *uiDfHudList_3288[] = { "Off", "On", 0 };
static const char *uiDfHudList_3536[] = { "Q3", "Defrag", 0 };
static const char *uiDfHudList_3548[] = { "Black", "Red", "Green", "Yellow", "Blue", "Cyan", "Magenta", "White", "Gray", "Orange", "Purple", "Dark Green", 0 };
static const char *uiDfHudList_3600[] = { "Center", "Left", "Right", 0 };
static const char *uiDfHudList_3616[] = { "Quake Units (units/s)", "Meters (Km/h)", "Miles (miles/h)", 0 };
static const char *uiDfHudList_3632[] = { "Off", "On", "On - old style Q3 units", 0 };
static const char *uiDfHudList_3648[] = { "Off", "Relative time", "Offset time", "Effective time", 0 };
static const int uiDfHudShow[3] = { 1, 4, 2 };
static const uiDfModItem_t uiDfHudItems[] = {
	{ 0, 2, 101, 350, 50, "Hud Style:", "df_hud", 3, 0, 1, 0, 0, 0, uiDfHudList_3536, "HUD style ^1(default = Defrag)", "df_hud" },
	{ 0, 1, 102, 350, 70, "Hud Transparency:", "df_hud_transparency", 2, 0, 1.0, 0, 4, 4, NULL, "HUD Transparency ^1(0.00 to 1.00 default = 0.25)", "df_hud_transparency" },
	{ 0, 2, 103, 350, 110, "Color:", "df_hud_color", 3, 0, 11, 0, 0, 0, uiDfHudList_3548, "Color ^1(default = Blue)", "df_hud_color" },
	{ 0, 1, 104, 350, 130, "Color RGB:", "df_hud_colorRGB", 4, 0, 0, 0, 11, 11, NULL, "Color using ^2R,G,B ^1(default = empty)", "df_hud_colorRGB" },
	{ 0, 2, 105, 350, 150, "Color Warning:", "df_hud_colorwarning", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Low values red on/off ^1(default = On)", "df_hud_colorwarning" },
	{ 0, 2, 106, 350, 190, "Font Color:", "df_hud_fontcolor", 3, 0, 11, 0, 0, 0, uiDfHudList_3548, "Font color ^1(default = White)", "df_hud_fontcolor" },
	{ 0, 1, 107, 350, 210, "Font Color RGB:", "df_hud_fontcolorRGB", 4, 0, 0, 0, 11, 11, NULL, "Font color using ^2R,G,B ^1(default = empty)", "df_hud_fontcolorRGB" },
	{ 0, 2, 108, 350, 230, "Font Shadow:", "df_hud_fontshadow", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Font shadow on/off ^1(default = On)", "df_hud_fontshadow" },
	{ 0, 2, 109, 350, 270, "Force Team Colors:", "df_hud_forceteamcolors", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Sets HUD color to match player's team (ctf only) ^1(default = On)", "df_hud_forceteamcolors" },
	{ 0, 2, 110, 350, 310, "3D Icons:", "df_hud_3DIcons", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "3D icons on/off ^1(default = On)", "df_hud_3DIcons" },
	{ 0, 1, 111, 350, 350, "Camping Gaz Hud:", "df_hud_cgaz", 1, 0, 31, 0, 4, 4, NULL, "0 = Off, 1 = Accelmeter, 2 = Speed, 3 = All ^1(default = 0)", "df_hud_cgaz" },
	{ 1, 2, 201, 350, 50, "Enable:", "df_hud_videocapture_enable", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Video Capture HUD Mode on/off ^1(default = Off)", "df_hud_videocapture_enable" },
	{ 1, 1, 202, 350, 90, "X:", "df_hud_videocapture_x", 1, 0, 640, 0, 4, 4, NULL, "Timer horizontal pos. relative to screen resolution 640x480 ^1(default = 320)", "df_hud_videocapture_x" },
	{ 1, 1, 203, 350, 110, "Y:", "df_hud_videocapture_y", 1, 0, 480, 0, 4, 4, NULL, "Timer vertical pos. relative to screen resolution 640x480 ^1(default = 240)", "df_hud_videocapture_y" },
	{ 1, 1, 204, 350, 150, "Font Size:", "df_hud_videocapture_fontsize", 1, 4, 32, 0, 2, 2, NULL, "Fontsize ^1(4 to 32 default = 16)", "df_hud_videocapture_fontsize" },
	{ 1, 2, 205, 350, 170, "Font Color:", "df_hud_videocapture_fontcolor", 3, 0, 11, 0, 0, 0, uiDfHudList_3548, "Font color ^1(default = White)", "df_hud_videocapture_fontcolor" },
	{ 1, 2, 206, 350, 190, "Font Shadow:", "df_hud_videocapture_fontshadow", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Font shadow on/off ^1(default = On)", "df_hud_videocapture_fontshadow" },
	{ 1, 2, 207, 350, 210, "Font Justify:", "df_hud_videocapture_fontjustify", 3, 0, 2, 0, 0, 0, uiDfHudList_3600, "Font justification ^1(default = Center)", "df_hud_videocapture_fontjustify" },
	{ 2, 2, 301, 350, 50, "Units:", "df_unit", 3, 0, 2, 0, 0, 0, uiDfHudList_3616, "Units type for distance/velocity outputs ^1(default = Quake Units (units/s))", "df_unit" },
	{ 2, 2, 303, 350, 90, "Best Time:", "df_drawBesttime", 3, 0, 1, 0, 0, 0, uiDfHudList_3288, "Best Time display on/off ^1(default = On)", "df_drawBesttime" },
	{ 2, 2, 302, 350, 130, "Speed Meter:", "df_drawSpeed", 3, 0, 2, 0, 0, 0, uiDfHudList_3632, "Speedmeter display on/off ^1(default = On)", "df_drawSpeed" },
	{ 2, 2, 304, 350, 170, "Jump Height Meter:", "df_drawJumpmeters", 5, 1, 0, 0, 0, 0, uiDfHudList_3288, "Jump Height Meter display on/off ^1(default = On)", "df_drawJumpmeters" },
	{ 2, 2, 305, 350, 190, "Jump Distance Meter:", "df_drawJumpmeters", 5, 2, 0, 0, 0, 0, uiDfHudList_3288, "Jump Distance Meter display on/off ^1(default = On)", "df_drawJumpmeters" },
	{ 2, 2, 306, 350, 210, "Jump Meter Highs Only:", "df_drawJumpmeters", 5, 4, 0, 0, 0, 0, uiDfHudList_3288, "Height/Distance Jump Meter only high values displayed ^1(default = Off)", "df_drawJumpmeters" },
	{ 2, 2, 307, 350, 250, "Checkpoints:", "df_checkpoints", 3, 0, 3, 0, 0, 0, uiDfHudList_3648, "Checkpoints display ^1(default = 2)", "df_checkpoints" },
};
static const char *uiDfHudPageNames[] = { "Hud", "Misc", "Video Capture", 0 };
#define UIDF_HUD_SPINLABEL "> HUD:"
#define UIDF_HUD_SPINHELP "^1Click for ^7HUD^1 choices"
