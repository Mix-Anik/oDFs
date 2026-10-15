/* generated from the DeFRaG ui.qvm by pagegen.py (page mp: init 0x9c2e) */
static const char *uiDfMpList_3288[] = { "Off", "On", 0 };
static const char *uiDfMpList_3300[] = { "On", "Off", 0 };
static const int uiDfMpShow[1] = { 1 };
static const uiDfModItem_t uiDfMpItems[] = {
	{ 0, 0, 0, 300, 50, "Client", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 0, 1, 101, 350, 70, "No Draw Radius:", "df_mp_NoDrawRadius", 1, -10000, 10000, 0, 6, 6, NULL, "Hides other players/missiles inside radius ^1(-10000 to 10000 default = 200)", "df_mp_NoDrawRadius" },
	{ 0, 0, 0, 300, 110, "Server", NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL },
	{ 0, 2, 102, 350, 130, "Players Interference:", "df_mp_InterferenceOff", 5, 1, 0, 0, 0, 0, uiDfMpList_3300, "Player to player interference ^1(default = On)", "df_mp_InterferenceOff +1" },
	{ 0, 2, 103, 350, 150, "Missiles Interference:", "df_mp_InterferenceOff", 5, 2, 0, 0, 0, 0, uiDfMpList_3300, "Missiles interference ^1(default = On)", "df_mp_InterferenceOff +2" },
	{ 0, 2, 104, 350, 170, "Team Based Interference:", "df_mp_InterferenceOff", 5, 4, 0, 0, 0, 0, uiDfMpList_3288, "Missiles interference off for enemies (on for teammates only) ^1(default = Off)", "df_mp_InterferenceOff +4" },
};
static const char *uiDfMpPageNames[] = { "Interference", 0 };
#define UIDF_MP_SPINLABEL "> Multiplayer:"
#define UIDF_MP_SPINHELP "^1Click for ^7Multiplayer^1 choices"
