/* generated from the DeFRaG ui.qvm by pagegen.py (page misc: init 0x97a8) */
static const char *uiDfMiscList_3288[] = { "Off", "On", 0 };
static const char *uiDfMiscList_3300[] = { "On", "Off", 0 };
static const char *uiDfMiscList_5260[] = { "Off", "Style 1", "Style 1 - CHS0 Colors", "Style 1 - CHS1 Colors", "Style 1 - CHS2 Colors", "Style 2", "Style 2 - CHS0 Colors", "Style 2 - CHS1 Colors", "Style 2 - CHS2 Colors", 0 };
static const char *uiDfMiscList_5300[] = { "Off", "Selective", "On", 0 };
static const char *uiDfMiscList_5316[] = { "On", "Off", "Kill", 0 };
static const char *uiDfMiscList_5332[] = { "On", "Off", "Kill", 0 };
static const char *uiDfMiscList_5348[] = { "Off", "Defrag timers (df)", "Respawn (resp)", "Weapon - Shotgun (sg)", "Weapon - Grenade Launcher (gl)", "Weapon - Rocket Launcher (rl)", "Weapon - Lightning Gun (lg)", "Weapon - Rail Gun (rg)", "Weapon - Plasma Gun (pg)", "Weapon - BFG (bfg)", "Weapon - Hook (hook)", "Ammo - Machinegun (amg)", "Ammo - Shotgun (asg)", "Ammo - Grenade Launcher (agl)", "Ammo - Rocket Launcher (arl)", "Ammo - Lightning Gun (alg)", "Ammo - Rail Gun (arg)", "Ammo - Plasma Gun (apg)", "Ammo - BFG (abfg)", "Health - 5 (h5)", "Health - 25 (h25)", "Health - 50 (h50)", "Mega Health (mh)", "Armor - Shard (as)", "Armor - Yellow (ya)", "Armor - Red (ra)", "Powerup - Quad (quad)", "Powerup - Battlesuit (bs)", "Powerup - Haste (hast)", "Powerup - Invisibility (inv)", "Powerup - Regeneration (reg)", "Powerup - Flight (fli)", "Holdable - Teleport (tp)", "Holdable - Medkit (med)", 0 };
static const char *uiDfMiscList_5628[] = { "Quake3", "Swinging", "Pendulum", "Quake2", 0 };
static const char *uiDfMiscList_5648[] = { "0: Autoconfig", "1: Defrag", "2: Tricks Mode", "3: Fast Captures", "4: Reserved (currently not used)", "5: Multiplayer Defrag", "6: Multiplayer Tricks Mode", "7: Multiplayer Fast Captures", 0 };
static const char *uiDfMiscList_5684[] = { "0: custom settings", "1: no weapon / no map object", "2: weapons / map objects", "3: no weapon / map objects", "4: weapons / no map object", "5: swinging hook", "6: quake3 hook", "7: original quake 3", 0 };
static const char *uiDfMiscList_5720[] = { "VQ3", "CPM", 0 };
static const int uiDfMiscShow[3] = { 4, 2, 1 };
static const uiDfModItem_t uiDfMiscItems[] = {
	{ 0, 2, 201, 350, 50, "Map Load Script:", "df_script_onMapLoad", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Execute ^2<mapname>.cfg^7 script when map is loaded ^1(default = Off)", "df_script_onMapLoad" },
	{ 0, 2, 202, 350, 70, "Respawn Script:", "df_script_onRespawn", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Execute ^2respawn.cfg^7 script each time player respawns ^1(default = Off)", "df_script_onRespawn" },
	{ 0, 2, 101, 350, 110, "CHS Use Predicted Data:", "df_chs_UsePredicted", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Use predicted player data for input into CHS/AccelMeter items ^1(default = Off)", "df_chs_UsePredicted" },
	{ 0, 2, 102, 350, 150, "OB Fast Predict:", "df_ob_Fast", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Overbounce fast prediction algorithm ^1(default = On)", "df_ob_Fast" },
	{ 0, 2, 103, 350, 170, "OB Predict All Slopes:", "df_ob_AllSlopes", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Overbounce prediction for non-horizontal floors ^1(default = Off)", "df_ob_AllSlopes" },
	{ 0, 1, 104, 350, 190, "OB Starting Vertical Offset:", "df_ob_OffsetStartPosZ", 2, -128000.0, 128000.0, 0, 7, 7, NULL, "Overbounce Vertical Starting Offset ^1(default = 0.00)", "df_ob_OffsetStartPosZ" },
	{ 0, 1, 105, 350, 210, "OB Landing Vertical Offset:", "df_ob_OffsetStopPosZ", 2, -128000.0, 128000.0, 0, 7, 7, NULL, "Overbounce Vertical Landing Offset ^1(default = 0.00)", "df_ob_OffsetStopPosZ" },
	{ 0, 2, 106, 350, 250, "VarHelper Output:", "df_varHelpers_DrawOutput", 3, 0, 8, 0, 0, 0, uiDfMiscList_5260, "varCycle, varJoin, varMath text output ^1(default = Style 2 - CHS2 Colors)", "df_varHelpers_DrawOutput" },
	{ 0, 2, 107, 350, 290, "Player Sprites:", "df_player_Sprites", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Draw talk balloons, rewards, etc above player ^1(default = On)", "df_player_Sprites" },
	{ 0, 1, 108, 350, 310, "Player Smoke Trails:", "df_player_SmokeTrails", 2, 0, 100000.0, 0, 7, 7, NULL, "Time in seconds ^2players.otherplayers ^1(default = 0.00)", "df_player_SmokeTrails" },
	{ 0, 2, 109, 350, 350, "Flag Grab Sounds:", "df_sound_flagGrabbing", 3, 0, 1, 0, 0, 0, uiDfMiscList_3288, "Flag grab sounds ^1(default = Off)", "df_sound_flagGrabbing" },
	{ 1, 2, 401, 350, 50, "Damage:", "df_ndm_damage", 3, 0, 1, 24, 0, 0, uiDfMiscList_3288, "Damage ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_damage" },
	{ 1, 2, 402, 350, 70, "Weapons Pickup:", "df_ndm_weaponsPickup", 3, 0, 1, 24, 0, 0, uiDfMiscList_3288, "Weapons Pickup ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_weaponsPickup" },
	{ 1, 2, 403, 350, 90, "Enable Powerups:", "df_ndm_powerups", 3, 0, 1, 24, 0, 0, uiDfMiscList_3288, "Enable Powerups ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_powerups" },
	{ 1, 2, 404, 350, 110, "Spawn Items:", "df_ndm_itemsSpawn", 3, 0, 2, 24, 0, 0, uiDfMiscList_5300, "Spawn Items ^3(requires map restart)^7 ^1(default = Selective)", "df_ndm_itemsSpawn" },
	{ 1, 2, 405, 350, 130, "Jumppads:", "df_ndm_nopad", 3, 0, 2, 24, 0, 0, uiDfMiscList_5316, "No Jumppads ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_nopad" },
	{ 1, 2, 406, 350, 150, "Teleporters:", "df_ndm_noteleport", 3, 0, 2, 24, 0, 0, uiDfMiscList_5332, "No Teleporters ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_noteleport" },
	{ 1, 2, 407, 350, 170, "Movers:", "df_ndm_nomover", 3, 0, 1, 24, 0, 0, uiDfMiscList_3300, "No Movers ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_nomover" },
	{ 1, 1, 408, 350, 190, "Weapons Set:", "df_ndm_weaponsSet", 1, 0, 100000, 24, 7, 7, NULL, "Weapons Set ^3(requires map restart)^7 ^1(default = 510)", "df_ndm_weaponsSet" },
	{ 1, 2, 409, 350, 230, "Start Timer On Item:", "df_ndm_timer_start", 6, 0, 33, 24, 0, 0, uiDfMiscList_5348, "Start Defrag timer on item ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_timer_start" },
	{ 1, 2, 410, 350, 250, "Checkpoint Timer On Item:", "df_ndm_timer_checkpoint", 6, 0, 33, 24, 0, 0, uiDfMiscList_5348, "Checkpoint Defrag timer on item ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_timer_checkpoint" },
	{ 1, 2, 411, 350, 270, "Stop Timer On Item:", "df_ndm_timer_stop", 6, 0, 33, 24, 0, 0, uiDfMiscList_5348, "Stop Defrag timer on item ^3(requires map restart)^7 ^1(default = Off)", "df_ndm_timer_stop" },
	{ 2, 2, 601, 350, 50, "Game Type:", "df_gametype", 3, 0, 7, 24, 0, 0, uiDfMiscList_5648, "Game Type ^3(requires map restart)^7 ^1(default = 0: Autoconfig)", "df_gametype" },
	{ 2, 2, 602, 350, 70, "TricksMode Game Mode:", "df_mode_tricksmode", 3, 0, 7, 24, 0, 0, uiDfMiscList_5684, "Game Mode ^3(requires map restart)^7 ^1(default = 0)", "df_mode_tricksmode" },
	{ 2, 2, 603, 350, 90, "Fast Caps  Game Mode:", "df_mode_fastcaps", 3, 0, 7, 24, 0, 0, uiDfMiscList_5684, "Game Mode ^3(requires map restart)^7 ^1(default = 0)", "df_mode_fastcaps" },
	{ 2, 2, 604, 350, 110, "Physics:", "df_promode", 3, 0, 1, 24, 0, 0, uiDfMiscList_5720, "Physics movement type ^3(requires map restart)^7 ^1(default = VQ3)", "df_promode" },
	{ 2, 2, 501, 350, 150, "Overbounces:", "df_ob_KillOBs", 3, 0, 1, 24, 0, 0, uiDfMiscList_3300, "Overbounces on/off ^3(requires map restart)^7 ^1(default = On)", "df_ob_KillOBs" },
	{ 2, 2, 502, 350, 190, "Grappling Hook Type:", "df_hook_Type", 3, 0, 3, 24, 0, 0, uiDfMiscList_5628, "Grappling Hook Type ^3(requires map restart)^7 ^1(default = Quake3)", "df_hook_Type" },
	{ 2, 2, 503, 350, 230, "Lightning Gun Knockback:", "df_lg_Knockback", 3, 0, 1, 24, 0, 0, uiDfMiscList_3288, "Lightning Gun Knockback on/off ^3(requires map restart)^7 ^1(default = Off)", "df_lg_Knockback" },
};
static const char *uiDfMiscPageNames[] = { "Game Settings", "Non Defrag Maps", "Misc", 0 };
#define UIDF_MISC_SPINLABEL "> Misc:"
#define UIDF_MISC_SPINHELP "^1Click for ^7Misc^1 choices"
