// ui_defrag.c -- DeFRaG additions to the classic Q3 menu UI (q3_ui): cvars of the DeFRaG settings menus.
// Sources: ui.dis cvar table (disasm/UI/ui_cvar_table.md) and the settings-defaults registration 0x3936.

#include "ui_local.h"

vmCvar_t	df_demos_UICompatibility;
vmCvar_t	df_demos_CurrentPath;
vmCvar_t	df_demos_CurrentListVars;
vmCvar_t	df_demos_Search;
vmCvar_t	df_demos_ReturnToBrowser;
vmCvar_t	df_demos_indexMaxAge;
vmCvar_t	df_ui_CustomMenu;
vmCvar_t	df_ui_DisplayCVarName;
vmCvar_t	df_ui_TextColorLabel;
vmCvar_t	df_ui_TextColorValue;
vmCvar_t	df_ui_TextColorMedium;
vmCvar_t	df_ui_TextColorBig;
vmCvar_t	df_ui_TextColorDisabled;
vmCvar_t	df_ui_TextColorHighlight;
vmCvar_t	df_ui_PulseColor;
vmCvar_t	df_ui_ListbarColor;
vmCvar_t	df_ui_StatusColor;
vmCvar_t	df_ui_CursorColor;

typedef struct {
	vmCvar_t	*vmCvar;
	const char	*cvarName;
	const char	*defaultString;
	int			cvarFlags;
} uiDfCvar_t;

static uiDfCvar_t uiDfCvarTable[] = {
	{ &df_demos_UICompatibility, "df_demos_UICompatibility", "1", CVAR_ARCHIVE },
	{ &df_demos_CurrentPath, "df_demos_CurrentPath", "", CVAR_TEMP },
	{ &df_demos_CurrentListVars, "df_demos_CurrentListVars", "0 0", CVAR_TEMP },
	{ &df_demos_Search, "df_demos_Search", "", CVAR_TEMP },		// oDFs: demo browser search text, kept while a demo plays
	{ &df_demos_ReturnToBrowser, "df_demos_ReturnToBrowser", "", CVAR_TEMP },	// oDFs: name of the demo the browser started (UI_DemosReturnAfterPlayback)
	{ &df_demos_indexMaxAge, "df_demos_indexMaxAge", "24", CVAR_ARCHIVE },	// oDFs: hours a demo index file is trusted before a rescan (0 = always scan)
	{ &df_ui_CustomMenu, "df_ui_CustomMenu", "2", CVAR_ARCHIVE },
	{ &df_ui_DisplayCVarName, "df_ui_DisplayCVarName", "1", CVAR_ARCHIVE },
	{ &df_ui_TextColorLabel, "df_ui_TextColorLabel", "1.00 1.00 0.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_TextColorValue, "df_ui_TextColorValue", "1.00 0.00 0.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_TextColorMedium, "df_ui_TextColorMedium", "0.00 0.00 1.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_TextColorBig, "df_ui_TextColorBig", "1.00 0.00 0.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_TextColorDisabled, "df_ui_TextColorDisabled", "0.50 0.50 0.50 1.00", CVAR_ARCHIVE },
	{ &df_ui_TextColorHighlight, "df_ui_TextColorHighlight", "1.00 1.00 0.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_PulseColor, "df_ui_PulseColor", "1.00 1.00 1.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_ListbarColor, "df_ui_ListbarColor", "1.00 0.00 0.00 0.30", CVAR_ARCHIVE },
	{ &df_ui_StatusColor, "df_ui_StatusColor", "1.00 1.00 1.00 1.00", CVAR_ARCHIVE },
	{ &df_ui_CursorColor, "df_ui_CursorColor", "0.50 0.80 1.00 1.00", CVAR_ARCHIVE },
};

static int uiDfCvarTableSize = ARRAY_LEN( uiDfCvarTable );

/*
=================
DeFRaG settings defaults (ui 0x3936, called at the end of the cvar registration 0xcdae)

The DeFRaG ui registers ~180 cgame / game cvars with flags 0 so that the settings menus always have a value to show,
even before the cgame or the game has registered them.  Entries marked server (third field 1) belong to the server
(g_synchronousClients 1, pmove_fixed 1, pmove_msec 8, df_mp_*, df_lg_Knockback, df_hook_Type): they are only seeded when
df_ui_seedServerCvars is set, so that a listen server keeps the operator's own defaults.
=================
*/
typedef struct { const char *name, *def; int server; } uiDfDefault_t;

static const uiDfDefault_t uiDfDefaults[] = {
	{ "cg_fov", "90", 0 },
	{ "cg_zoomfov", "22.5", 0 },
	{ "cg_crosshairSize", "24", 0 },
	{ "cg_crosshairColor", "7", 0 },
	{ "cg_crosshairHealth", "1", 0 },
	{ "cg_crosshairX", "0", 0 },
	{ "cg_crosshairY", "0", 0 },
	{ "cg_trueLightning", "0.0", 0 },
	{ "cg_railTrailTime", "400", 0 },
	{ "cg_oldRail", "1", 0 },
	{ "cg_oldPlasma", "1", 0 },
	{ "cg_oldRocket", "1", 0 },
	{ "cg_noProjectileTrail", "0", 0 },
	{ "cg_shadows", "1", 0 },
	{ "cg_gibs", "1", 0 },
	{ "cg_footsteps", "1", 0 },
	{ "cg_draw2D", "1", 0 },
	{ "cg_drawStatus", "1", 0 },
	{ "cg_drawTimer", "0", 0 },
	{ "cg_drawFPS", "0", 0 },
	{ "cg_lagometer", "1", 0 },
	{ "cg_drawGun", "1", 0 },
	{ "cg_draw3dIcons", "1", 0 },
	{ "cg_drawIcons", "1", 0 },
	{ "cg_drawAmmoWarning", "1", 0 },
	{ "cg_drawFriend", "1", 0 },
	{ "cg_drawAttacker", "1", 0 },
	{ "cg_drawRewards", "1", 0 },
	{ "cg_scorePlums", "1", 0 },
	{ "cg_centertime", "3", 0 },
	{ "cg_noVoiceChats", "0", 0 },
	{ "cg_noVoiceText", "0", 0 },
	{ "cg_teamChatsOnly", "0", 0 },
	{ "cg_teamChatHeight", "0", 0 },
	{ "cg_thirdPerson", "0", 0 },
	{ "cg_thirdPersonRange", "40", 0 },
	{ "cg_thirdPersonAngle", "0", 0 },
	{ "cg_cameraOrbit", "0", 0 },
	{ "cg_cameraOrbitDelay", "50", 0 },
	{ "cg_nochatbeep", "0", 0 },
	{ "pmove_msec", "8", 1 },
	{ "g_synchronousClients", "1", 1 },
	{ "pmove_fixed", "1", 1 },
	{ "cg_drawDamage", "1", 0 },
	{ "df_chs_UsePredicted", "0", 0 },
	{ "df_chs0_Draw", "0", 0 },
	{ "df_chs0_FontSize", "8", 0 },
	{ "df_chs0_FontShadow", "0", 0 },
	{ "df_chs0_FontColor", "7", 0 },
	{ "df_chs0_FontTransparency", "1.00", 0 },
	{ "df_chs0_LabelColor", "7", 0 },
	{ "df_chs0_DisplayType", "0", 0 },
	{ "df_chs0_OffsetX", "0", 0 },
	{ "df_chs0_OffsetY", "0", 0 },
	{ "df_chs1_Draw", "0", 0 },
	{ "df_chs1_FontSize", "10", 0 },
	{ "df_chs1_FontShadow", "1", 0 },
	{ "df_chs1_FontColor", "7", 0 },
	{ "df_chs1_FontTransparency", "1.00", 0 },
	{ "df_chs1_LabelColor", "7", 0 },
	{ "df_chs1_LabelType", "0", 0 },
	{ "df_chs1_DisplayType", "0", 0 },
	{ "df_chs1_OffsetX", "0", 0 },
	{ "df_chs1_OffsetY", "0", 0 },
	{ "df_chs1_EnableCombos", "0", 0 },
	{ "df_chs1_ComboSeparator", "/", 0 },
	{ "df_chs1_Info1", "0", 0 },
	{ "df_chs1_Info2", "0", 0 },
	{ "df_chs1_Info3", "0", 0 },
	{ "df_chs1_Info4", "0", 0 },
	{ "df_chs1_Info5", "0", 0 },
	{ "df_chs1_Info6", "0", 0 },
	{ "df_chs1_Info7", "0", 0 },
	{ "df_chs1_Info8", "0", 0 },
	{ "df_chs2_Draw", "0", 0 },
	{ "df_chs2_FontSize", "8", 0 },
	{ "df_chs2_FontShadow", "1", 0 },
	{ "df_chs2_FontColor", "7", 0 },
	{ "df_chs2_FontTransparency", "1.00", 0 },
	{ "df_chs2_LabelColor", "7", 0 },
	{ "df_chs2_LabelType", "1", 0 },
	{ "df_chs2_DisplayType", "1", 0 },
	{ "df_chs2_OffsetX", "-300", 0 },
	{ "df_chs2_OffsetY", "100", 0 },
	{ "df_chs2_EnableCombos", "0", 0 },
	{ "df_chs2_ComboSeparator", "/", 0 },
	{ "df_chs2_Info1", "0", 0 },
	{ "df_chs2_Info2", "0", 0 },
	{ "df_chs2_Info3", "0", 0 },
	{ "df_chs2_Info4", "0", 0 },
	{ "df_chs2_Info5", "0", 0 },
	{ "df_chs2_Info6", "0", 0 },
	{ "df_chs2_Info7", "0", 0 },
	{ "df_chs2_Info8", "0", 0 },
	{ "df_accel_Draw", "0", 0 },
	{ "df_accel_FontSize", "7", 0 },
	{ "df_accel_Mode", "0", 0 },
	{ "df_accel_Output", "0", 0 },
	{ "df_accel_OffsetX", "0", 0 },
	{ "df_accel_OffsetY", "0", 0 },
	{ "df_accel_Scale", "-1.2", 0 },
	{ "df_accel_AdjustMaxSpeed", "1", 0 },
	{ "df_accel_HistoryResetSpeed", "100", 0 },
	{ "df_ghosts_Command", "", 0 },
	{ "df_ghosts_FileExtension", "", 0 },
	{ "df_ghosts_DrawOutput", "0", 0 },
	{ "df_ghosts_AutoModeMP", "0", 0 },
	{ "df_ghosts_AutoMode", "4", 0 },
	{ "df_ghosts_NoDrawRadius", "200", 0 },
	{ "df_ghosts_LiveAmmo", "0", 0 },
	{ "df_ghosts_CheckPVS", "1", 0 },
	{ "df_ghosts_SelfMissilesOnly", "1", 0 },
	{ "df_ghosts_SpriteInfo", "", 0 },
	{ "df_ghosts_SpriteSize", "8", 0 },
	{ "df_ghosts_MiniviewDraw", "3", 0 },
	{ "df_ghosts_MiniviewDrawGun", "1", 0 },
	{ "df_ghosts_MiniviewCameraMode", "1", 0 },
	{ "df_ghosts_MiniviewSize", "25", 0 },
	{ "df_ghosts_MiniviewOffsetX", "235", 0 },
	{ "df_ghosts_MiniviewOffsetY", "-155", 0 },
	{ "df_ghosts_MiniviewBorderSize", "4", 0 },
	{ "df_ghosts_MiniviewFOV", "90", 0 },
	{ "df_ghost1_RecordFileFormat", "2", 0 },
	{ "df_ghost1_RecordInterval", "50", 0 },
	{ "df_ghost1_RecordView", "0", 0 },
	{ "df_ghost1_PlayTimescale", "1.0", 0 },
	{ "df_ghost1_PlayDrawPlayer", "3", 0 },
	{ "df_ghost1_PlayDrawMissiles", "2", 0 },
	{ "df_ghost1_PlayCameraMode", "0", 0 },
	{ "df_ghost1_PlayLoop", "0", 0 },
	{ "df_ghost2_RecordFileFormat", "2", 0 },
	{ "df_ghost2_RecordInterval", "50", 0 },
	{ "df_ghost2_RecordView", "0", 0 },
	{ "df_ghost2_PlayTimescale", "1.0", 0 },
	{ "df_ghost2_PlayDrawPlayer", "3", 0 },
	{ "df_ghost2_PlayDrawMissiles", "2", 0 },
	{ "df_ghost2_PlayCameraMode", "0", 0 },
	{ "df_ghost2_PlayLoop", "0", 0 },
	{ "df_demos_CameraMode", "0", 0 },
	{ "df_demos_CameraTransitionSpeed", "50", 0 },
	{ "df_demos_HidePacketEntities", "0", 0 },
	{ "df_demos_HideLocalEntities", "0", 0 },
	{ "df_demos_ReplayMode", "1", 0 },
	{ "df_demos_ReplayDrawMask", "32767", 0 },
	{ "df_demos_ReplayFilename", "$demoname", 0 },
	{ "df_demos_UnlockThirdPersonPitch", "0", 0 },
	{ "df_demos_ModSupport", "0", 0 },
	{ "df_demos_Seek", "0.00", 0 },
	{ "df_demos_CamSeqPosition", "", 0 },
	{ "df_demos_CamSeqSpeed", "", 0 },
	{ "df_demos_CamSeqMode", "", 0 },
	{ "df_demos_CamOffsetPositionX", "0", 0 },
	{ "df_demos_CamOffsetPositionY", "0", 0 },
	{ "df_demos_CamOffsetPositionZ", "0", 0 },
	{ "df_demos_CamOffsetAngleX", "0", 0 },
	{ "df_demos_CamOffsetAngleY", "0", 0 },
	{ "df_demos_CamOffsetAngleZ", "0", 0 },
	{ "df_demos_CamOffsetRadiusF", "0", 0 },
	{ "df_demos_CamOffsetRadiusR", "0", 0 },
	{ "df_demos_CamOffsetRadiusU", "0", 0 },
	{ "df_keymaps", "0", 0 },
	{ "df_keymaps_forward", "", 0 },
	{ "df_keymaps_back", "", 0 },
	{ "df_keymaps_moveleft", "", 0 },
	{ "df_keymaps_moveright", "", 0 },
	{ "df_keymaps_moveup", "", 0 },
	{ "df_keymaps_movedown", "", 0 },
	{ "df_transitions", "", 0 },
	{ "df_mp_NoDrawRadius", "200", 1 },
	{ "df_mp_TrackPlayerNum", "-1", 0 },
	{ "df_varHelpers_DrawOutput", "0", 0 },
	{ "df_ob_Fast", "1", 0 },
	{ "df_ob_AllSlopes", "0", 0 },
	{ "df_ob_OffsetStartPosZ", "0.00", 0 },
	{ "df_ob_OffsetStopPosZ", "0.00", 0 },
	{ "df_player_SmokeTrails", "0.00", 0 },
	{ "df_player_Sprites", "1", 0 },
	{ "df_mp_InterferenceOff", "3", 1 },
	{ "df_ob_KillOBs", "0", 0 },
	{ "df_hook_Type", "0", 1 },
	{ "df_lg_Knockback", "0", 1 },
	// ui 0x8e0 (called before 0x3936 from the cvar registration): the settings the DeFRaG menu pages edit
	{ "df_autorecord", "3", 0 },
	{ "df_ar_stopdelay", "3000", 0 },
	{ "df_ar_format", "$map.$-route[$gt.$phys.$-mode]$m.$-s.$-ms($pl.$plc)", 0 },
	{ "df_ar_speedcapture_format", "0", 0 },
	{ "df_checkpoints", "2", 0 },
	{ "df_country", "CountryHere", 0 },
	{ "df_demos_validation", "0", 0 },
	{ "df_demos_exitAfterPlayback", "0", 0 },
	{ "df_drawBesttime", "1", 0 },
	{ "df_drawJumpmeters", "0", 0 },
	{ "df_drawSpeed", "1", 0 },
	{ "df_drawCheckpointStats", "1", 0 },
	{ "df_drawCheckpointEvents", "1", 0 },
	{ "df_hud", "1", 0 },
	{ "df_hud_3DIcons", "1", 0 },
	{ "df_hud_color", "4", 0 },
	{ "df_hud_colorRGB", "", 0 },
	{ "df_hud_colorwarning", "1", 0 },
	{ "df_hud_drawStatus", "1", 0 },
	{ "df_hud_drawSpectFollow", "2", 0 },
	{ "df_hud_forceteamcolors", "1", 0 },
	{ "df_hud_fontcolor", "7", 0 },
	{ "df_hud_fontcolorRGB", "", 0 },
	{ "df_hud_fontshadow", "1", 0 },
	{ "df_hud_transparency", "0.25", 0 },
	{ "df_hud_videocapture_enable", "0", 0 },
	{ "df_hud_videocapture_x", "320", 0 },
	{ "df_hud_videocapture_y", "240", 0 },
	{ "df_hud_videocapture_fontsize", "16", 0 },
	{ "df_hud_videocapture_fontcolor", "7", 0 },
	{ "df_hud_videocapture_fontshadow", "1", 0 },
	{ "df_hud_videocapture_fontjustify", "0", 0 },
	{ "df_recfiles_keepLatest", "0", 0 },
	{ "df_recfiles_session", "", 0 },
	{ "df_name", "", 0 },
	{ "df_script_onMapLoad", "0", 0 },
	{ "df_script_onRespawn", "0", 0 },
	{ "df_sound_flagGrabbing", "0", 0 },
	{ "df_unit", "0", 0 },
	{ "df_hud_cgaz", "0", 0 },
	{ "df_gametype", "0", 1 },
	{ "df_promode", "0", 1 },
	{ "df_competition", "", 1 },
	{ "df_ndm_timer_start", "", 1 },
	{ "df_ndm_timer_checkpoint", "", 1 },
	{ "df_ndm_timer_stop", "", 1 },
	{ "df_mode_tricksmode", "2", 1 },
	{ "df_mode_fastcaps", "2", 1 },
	{ "df_ndm_damage", "0", 1 },
	{ "df_ndm_powerups", "0", 1 },
	{ "df_ndm_weaponsSet", "1022", 1 },
	{ "df_ndm_weaponsPickup", "0", 1 },
	{ "df_ndm_itemsSpawn", "1", 1 },
	{ "df_ndm_nopad", "0", 1 },
	{ "df_ndm_noteleport", "0", 1 },
	{ "df_ndm_nomover", "0", 1 },
	{ "df_ar_verbose", "0", 0 },
	{ "s_ambient", "1", 0 },
};

static vmCvar_t	df_ui_seedServerCvars;

static void UI_DF_RegisterDefaults( void ) {
	int i;

	trap_Cvar_Register( &df_ui_seedServerCvars, "df_ui_seedServerCvars", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "df_ui_browserDefragOnly", "1", CVAR_ARCHIVE );		// not a DeFRaG cvar: switch for the "game defrag" server list filter
	if ( df_ui_seedServerCvars.integer ) {
		trap_Cvar_Set( "sv_pure", "0" );		// ui 0x8e0 sets it on every start (it overrides the local server's setting)
	}
	for ( i = 0; i < ARRAY_LEN( uiDfDefaults ); i++ ) {
		if ( uiDfDefaults[i].server && !df_ui_seedServerCvars.integer ) {
			continue;
		}
		trap_Cvar_Register( NULL, uiDfDefaults[i].name, uiDfDefaults[i].def, 0 );
	}
}

/*
=================
DeFRaG menu theme (ui 0x4265, run when a df_ui_* cvar changes)

The nine standard menu colours are reset to their defaults and, with df_ui_CustomMenu set, replaced by the
df_ui_* cvars ("r g b a"; an empty or short string leaves the rest of the default).  DeFRaG keeps three more
colours than Q3: Value (10788), Medium (10804) and Big (10820).  Mapping (cvar -> colour):
 Pulse -> pulse_color, TextColorDisabled -> text_color_disabled, TextColorLabel -> text_color_normal,
 TextColorValue -> uiDfColorValue, TextColorMedium -> uiDfColorMedium (headings), TextColorBig -> uiDfColorBig (buttons),
 TextColorHighlight -> text_color_highlight, ListbarColor -> listbar_color, StatusColor -> text_color_status.
df_ui_CursorColor tints the mouse cursor (see UI_DF_DrawCursor).
=================
*/
extern vec4_t pulse_color;
extern vec4_t text_color_status;

vec4_t uiDfColorValue	= { 1.00f, 0.43f, 0.00f, 1.00f };
vec4_t uiDfColorMedium	= { 1.00f, 0.00f, 0.00f, 1.00f };
vec4_t uiDfColorBig		= { 1.00f, 0.00f, 0.00f, 1.00f };

static void UI_DF_ParseColor( const char *str, vec4_t color ) {
	const char	*p = str;
	const char	*token;
	int			i;

	for ( i = 0; i < 4; i++ ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		color[i] = atof( token );
	}
}

static void UI_DF_ApplyTheme( void ) {
	static const float defaults[9][4] = {
		{ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.5f, 0.5f, 1.0f }, { 1.0f, 0.43f, 0.0f, 1.0f }, { 1.0f, 0.43f, 0.0f, 1.0f },
		{ 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.43f, 0.0f, 0.3f },
		{ 1.0f, 1.0f, 1.0f, 1.0f }
	};
	vec4_t *const targets[9] = { &pulse_color, &text_color_disabled, &text_color_normal, &uiDfColorValue, &uiDfColorMedium,
		&uiDfColorBig, &text_color_highlight, &listbar_color, &text_color_status };
	const vmCvar_t *const cvars[9] = { &df_ui_PulseColor, &df_ui_TextColorDisabled, &df_ui_TextColorLabel, &df_ui_TextColorValue,
		&df_ui_TextColorMedium, &df_ui_TextColorBig, &df_ui_TextColorHighlight, &df_ui_ListbarColor, &df_ui_StatusColor };
	int i, j;

	for ( i = 0; i < 9; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			(*targets[i])[j] = defaults[i][j];
		}
		if ( df_ui_CustomMenu.integer ) {
			UI_DF_ParseColor( cvars[i]->string, *targets[i] );
		}
	}
}

void UI_DF_RegisterCvars( void ) {
	int i;

	for ( i = 0; i < uiDfCvarTableSize; i++ ) {
		trap_Cvar_Register( uiDfCvarTable[i].vmCvar, uiDfCvarTable[i].cvarName, uiDfCvarTable[i].defaultString, uiDfCvarTable[i].cvarFlags );
	}
	UI_DF_RegisterDefaults();
	UI_DF_ApplyTheme();
}

// ui 0x4106 (called by UI_Shutdown): a demo seek that is still running (cgame's df_demos_Seek with the fast-forward timescale of 100 / 1000)
// is aborted - timescale back to 1, df_demos_Seek cleared, the volumes saved by the seek restored
void UI_DF_Shutdown( void ) {
	char	buf[32];
	float	ts;

	if ( trap_Cvar_VariableValue( "df_demos_Seek" ) == 0 ) {
		return;
	}
	ts = trap_Cvar_VariableValue( "timescale" );
	if ( ts != 100 && ts != 1000 ) {
		return;
	}
	trap_Cvar_Set( "timescale", "1" );
	trap_Cvar_Set( "df_demos_Seek", "0.00" );
	trap_Cvar_VariableStringBuffer( "df_user_s_volume", buf, sizeof( buf ) );
	if ( atof( buf ) != 0 ) {
		trap_Cvar_Set( "s_volume", buf );
	}
	trap_Cvar_VariableStringBuffer( "df_user_s_musicvolume", buf, sizeof( buf ) );
	if ( atof( buf ) != 0 ) {
		trap_Cvar_Set( "s_musicvolume", buf );
	}
}

void UI_DF_UpdateCvars( void ) {
	int i, changed = 0;

	for ( i = 0; i < uiDfCvarTableSize; i++ ) {
		int count = uiDfCvarTable[i].vmCvar->modificationCount;

		trap_Cvar_Update( uiDfCvarTable[i].vmCvar );
		if ( uiDfCvarTable[i].vmCvar->modificationCount != count ) {
			changed = 1;
		}
	}
	if ( changed ) {
		UI_DF_ApplyTheme();
	}
}


/*
=================
DeFRaG mouse cursor and hover events (ui 0x807 / 0x88a / 0xcb6c)

The cursor is two layers, df_gfx/menu/cursor_back (white while the pointer is over a usable widget, otherwise tinted)
and df_gfx/menu/cursor_front (always tinted), tinted with df_ui_CursorColor (default "0.50 0.80 1.00 1.00").  Those
shaders come with the DeFRaG pk3s: when either is missing the stock cursor is used.
The widget under the pointer also receives callback events 101 (mouse in) and 102 (mouse out).
=================
*/
qboolean		uiDfNoCursor;		// set by a menu that wants no cursor this frame (DeFRaG menu flag +544)
static qhandle_t	uiDfCursorBack, uiDfCursorFront;
static menucommon_s	*uiDfHover;

void UI_DF_RegisterCursor( void ) {
	uiDfCursorFront = trap_R_RegisterShaderNoMip( "df_gfx/menu/cursor_front" );
	uiDfCursorBack = trap_R_RegisterShaderNoMip( "df_gfx/menu/cursor_back" );
	uiDfHover = NULL;
}

void UI_DF_SetHover( menucommon_s *m ) {		// 0x88a
	if ( m == uiDfHover ) {
		return;
	}
	if ( uiDfHover && uiDfHover->callback ) {
		uiDfHover->callback( uiDfHover, 102 );
	}
	uiDfHover = m;
	if ( uiDfHover && uiDfHover->callback ) {
		uiDfHover->callback( uiDfHover, 101 );
	}
}

qboolean UI_DF_DrawCursor( void ) {			// 0x807
	vec4_t	color = { 0.50f, 0.80f, 1.00f, 1.00f };

	if ( uiDfNoCursor ) {
		return qtrue;
	}
	if ( !uiDfCursorBack || !uiDfCursorFront ) {
		return qfalse;
	}
	UI_DF_ParseColor( df_ui_CursorColor.string, color );
	if ( uiDfHover && !( uiDfHover->flags & ( QMF_HIDDEN|QMF_GRAYED|QMF_INACTIVE ) ) ) {
		UI_SetColor( color_white );
	} else {
		UI_SetColor( color );
	}
	UI_DrawHandlePic( uis.cursorx - 16, uis.cursory - 16, 32, 32, uiDfCursorBack );
	UI_SetColor( color );
	UI_DrawHandlePic( uis.cursorx - 16, uis.cursory - 16, 32, 32, uiDfCursorFront );
	UI_SetColor( NULL );
	return qtrue;
}

/*
=================
DeFRaG demo queue (ui 0x187d / 0x1895 / 0x18f2 / 0x1963 / 0x1a35 / 0x1a6e / 0x1a92 / 0x1ab2 / 0x1b67)

dmq_add <demo|folder/> appends demos (a folder adds every demo of the current protocol, sorted), dmq_play writes the queue to
system/temp/demoqueue.txt and starts the first demo, dmq_stop empties the file and disconnects, dmq_clear forgets the queue.
When the engine asks for the main menu (UI_SetActiveMenu UIMENU_MAIN, i.e. after a demo ended) the loader plays the next queued
demo instead; with no queue running, df_demos_exitAfterPlayback quits after a demo that was played back.
=================
*/
#define DMQ_FILE		"system/temp/demoqueue.txt"
#define DMQ_SIZE		32000

typedef struct {
	char	data[DMQ_SIZE];
	char	*wptr;
	int		full;
} uiDfDmq_t;

static uiDfDmq_t	uiDfDmq;

void UI_DF_DemoQueueClear( void ) {		// 0x187d
	memset( &uiDfDmq, 0, sizeof( uiDfDmq ) );
	uiDfDmq.wptr = uiDfDmq.data;
}

static void UI_DF_DmqAddFile( const char *name ) {		// 0x1895
	int len = strlen( name );

	if ( len + 2 > DMQ_SIZE - ( uiDfDmq.wptr - uiDfDmq.data ) ) {
		Com_Printf( "DMQ_add_file() : buffer is full.\n" );
		uiDfDmq.full = 1;
		return;
	}
	strcpy( uiDfDmq.wptr, name );
	uiDfDmq.wptr += len + 1;
	*uiDfDmq.wptr = 0;
	Com_Printf( "Adding [%s] to the queue.\n", name );
}

static void UI_DF_DmqAddDir( const char *dir ) {		// 0x18f2
	char	ext[8], buf[24576], *p, *list[512];
	const char	*t;
	int		i, j, n, count;

	Com_sprintf( ext, sizeof( ext ), "dm_%i", (int)trap_Cvar_VariableValue( "protocol" ) );
	count = trap_FS_GetFileList( va( "demos/%s", dir ), ext, buf, sizeof( buf ) );
	n = 0;
	p = buf;
	for ( i = 0; i < count && n < 512 - 1; i++ ) {
		// insertion sort by name (0x64a)
		for ( j = n; j > 0 && Q_stricmp( p, list[j - 1] ) < 0; j-- ) {
			list[j] = list[j - 1];
		}
		list[j] = p;
		n++;
		p += strlen( p ) + 1;
	}
	for ( i = 0; i < n && !uiDfDmq.full; i++ ) {
		t = va( "%s/%s", dir, list[i] );
		UI_DF_DmqAddFile( t );
	}
}

static void UI_DF_DmqStripExt( char *s, int n ) {		// 0x60a: zero the dots among the last n+1 characters
	char	*c;
	int		k = 0;

	if ( !*s ) {
		return;
	}
	for ( c = s + strlen( s ) - 1; c != s && k <= n; c--, k++ ) {
		if ( *c == '.' ) {
			*c = 0;
		}
	}
}

static qboolean UI_DF_DmqIsDir( const char *dirPath, const char *name ) {		// 0x5b3
	char	list[8192], *p;
	int		count, i;

	count = trap_FS_GetFileList( dirPath, "/", list, sizeof( list ) );
	p = list;
	for ( i = 0; i < count; i++ ) {
		if ( !Q_stricmp( name, p ) ) {
			return qtrue;
		}
		p += strlen( p ) + 1;
	}
	return qfalse;
}

static void UI_DF_DmqAdd( const char *arg ) {		// 0x1963
	char	path[512], dir[256], file[256], dirPath[300], fileName[300];
	const char *a;
	int		dirMode = 0, len, i;

	if ( uiDfDmq.full || !arg[0] ) {
		return;
	}
	a = ( arg[0] == '/' ) ? arg + 1 : arg;
	Q_strncpyz( path, a, 256 );
	len = strlen( path );
	if ( len && path[len - 1] == '/' ) {
		path[len - 1] = 0;
		dirMode = 1;
	} else {
		UI_DF_DmqStripExt( path, 5 );
	}

	// split into folder and file (0x4d5)
	len = strlen( path );
	for ( i = len; i > 0 && path[i] != '/' && path[i] != '\\'; i-- ) {
	}
	if ( path[i] == '/' || path[i] == '\\' ) {
		Q_strncpyz( dir, path, ( i + 1 < (int)sizeof( dir ) ) ? i + 1 : sizeof( dir ) );
		Q_strncpyz( file, path + i + 1, sizeof( file ) );
	} else {
		dir[0] = 0;
		Q_strncpyz( file, path, sizeof( file ) );
	}

	Com_sprintf( dirPath, sizeof( dirPath ), "demos/%s", dir );
	if ( UI_DF_DmqIsDir( dirPath, file ) ) {
		UI_DF_DmqAddDir( path );
		return;
	}
	if ( dirMode ) {
		Com_Printf( "DMQ_add() : no such directory.\n" );
		return;
	}
	Com_sprintf( fileName, sizeof( fileName ), "%s.dm_%i", file, (int)trap_Cvar_VariableValue( "protocol" ) );
	{
		fileHandle_t	f;
		int				flen = trap_FS_FOpenFile( va( "%s/%s", dirPath, fileName ), &f, FS_READ );	// 0x58a

		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		if ( flen > 0 ) {
			UI_DF_DmqAddFile( path );
		} else {
			Com_Printf( "DMQ_add() : no such file or directory.\n" );
		}
	}
}

static void UI_DF_DmqWrite( void ) {		// 0x1a35
	fileHandle_t f;

	trap_FS_FOpenFile( DMQ_FILE, &f, FS_WRITE );
	if ( !f ) {
		Com_Printf( "DMQ_output() error : failed to open %s for writing\n", DMQ_FILE );
		return;
	}
	trap_FS_Write( uiDfDmq.data, ( uiDfDmq.wptr - uiDfDmq.data ) + 1, f );
	trap_FS_FCloseFile( f );
}

static void UI_DF_DmqStop( void ) {			// 0x1a6e
	fileHandle_t f;

	trap_FS_FOpenFile( DMQ_FILE, &f, FS_WRITE );
	trap_FS_Write( "", 1, f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
}

static void UI_DF_ExitAfterDemo( void ) {	// 0x1a92
	if ( (int)trap_Cvar_VariableValue( "defrag_demoPlayback" ) && (int)trap_Cvar_VariableValue( "df_demos_exitAfterPlayback" ) ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "wait; wait; quit\n" );
	}
}

static struct {
	menuframework_s	menu;
	menutext_s		text;
} s_dmqLoading;

static void UI_DF_DemoLoadingMenu( void ) {		// 0x1b67
	memset( &s_dmqLoading, 0, sizeof( s_dmqLoading ) );
	s_dmqLoading.menu.fullscreen = qtrue;
	s_dmqLoading.text.generic.type = MTYPE_BTEXT;
	s_dmqLoading.text.generic.x = 320;
	s_dmqLoading.text.generic.y = 222;
	s_dmqLoading.text.string = "LOADING DEMO";
	s_dmqLoading.text.style = UI_CENTER;
	s_dmqLoading.text.color = color_white;
	Menu_AddItem( &s_dmqLoading.menu, &s_dmqLoading.text );
	UI_PushMenu( &s_dmqLoading.menu );
}

// 0x1ab2: returns qtrue when a queued demo was started
qboolean UI_DF_DemoQueueNext( void ) {
	char			buf[DMQ_SIZE + 1], *p;
	fileHandle_t	f;
	int				len;

	if ( !(int)trap_Cvar_VariableValue( "defrag_dmq_playback" ) ) {
		UI_DF_ExitAfterDemo();
		return qfalse;
	}
	memset( buf, 0, sizeof( buf ) );
	len = trap_FS_FOpenFile( DMQ_FILE, &f, FS_READ );
	if ( !f || len <= 0 ) {
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		trap_Cvar_Set( "defrag_dmq_playback", "0" );
		UI_DF_ExitAfterDemo();
		return qfalse;
	}
	if ( len > DMQ_SIZE ) {
		len = DMQ_SIZE;
	}
	trap_FS_Read( buf, len, f );
	trap_FS_FCloseFile( f );
	if ( !buf[0] ) {
		trap_Cvar_Set( "defrag_dmq_playback", "0" );
		UI_DF_ExitAfterDemo();
		return qfalse;
	}
	p = buf;
	while ( *p ) {
		p++;
	}
	p++;
	trap_FS_FOpenFile( DMQ_FILE, &f, FS_WRITE );
	trap_FS_Write( p, len - ( p - buf ), f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo \"%s\"\n", buf ) );
	UI_DF_DemoLoadingMenu();
	return qtrue;
}

/*
=================
DeFRaG ui console commands (ui 0xb84; map check 0x58a)

 silent <cmd ...>        run a console command with cl_noprint forced to 1 (restored to 0 afterwards when it was 0)
 vq3|cpm <map>           set df_promode 0|1 and load the map
 devvq3|devcpm <map>     same with devmap
The map must exist as maps/<map>.bsp, otherwise "Map not found." is printed.
dmq_add / dmq_play / dmq_stop / dmq_clear: see the demo queue above.
=================
*/
static qboolean UI_DF_MapExists( const char *name ) {
	fileHandle_t	f;
	int				len;

	len = trap_FS_FOpenFile( va( "maps/%s.bsp", name ), &f, FS_READ );
	if ( f ) {
		trap_FS_FCloseFile( f );
	}
	return len > 0;
}

qboolean UI_DF_ConsoleCommand( const char *cmd ) {
	char	line[1024], arg[256], buf[8];
	int		i, argc, n;

	if ( !Q_stricmp( cmd, "dmq_add" ) ) {
		trap_Argv( 1, arg, sizeof( arg ) );
		UI_DF_DmqAdd( arg );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "dmq_play" ) ) {
		trap_Cvar_Set( "defrag_dmq_playback", "1" );
		UI_DF_DmqWrite();
		UI_DF_DemoQueueNext();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "dmq_stop" ) ) {
		UI_DF_DmqStop();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "dmq_clear" ) ) {
		UI_DF_DemoQueueClear();
		Com_Printf( "Demo queue cleared.\n" );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "silent" ) ) {
		argc = trap_Argc();
		if ( argc == 1 ) {
			Com_Printf( "Usage: silent [cmd]\n" );
			return qtrue;
		}
		n = 0;
		line[0] = 0;
		for ( i = 1; i < argc; i++ ) {
			if ( i > 1 ) {
				if ( n + 4 > (int)sizeof( line ) ) {
					Com_Printf( "buffer overflow.\n" );
					return qtrue;
				}
				line[n++] = ' ';
				line[n++] = '"';
			}
			trap_Argv( i, line + n, sizeof( line ) - n );		// straight into the line buffer like 0xbee
			n += strlen( line + n );
			if ( n >= (int)sizeof( line ) - 1 ) {
				Com_Printf( "buffer overflow.\n" );
				return qtrue;
			}
			if ( i > 1 ) {
				line[n++] = '"';
				line[n] = 0;
			}
		}
		trap_Cvar_VariableStringBuffer( "cl_noprint", buf, sizeof( buf ) );
		if ( buf[0] == '0' ) {
			// EXEC_INSERT: runs as  set cl_noprint 1; <cmd>; set cl_noprint 0
			trap_Cmd_ExecuteText( EXEC_INSERT, "set cl_noprint 0" );
			trap_Cmd_ExecuteText( EXEC_INSERT, line );
			trap_Cmd_ExecuteText( EXEC_INSERT, "set cl_noprint 1" );
		} else {
			trap_Cmd_ExecuteText( EXEC_INSERT, line );
		}
		return qtrue;
	}

	{
		static const struct { const char *name, *fmt; } maps[] = {
			{ "vq3", "set df_promode 0;map %s\n" }, { "cpm", "set df_promode 1;map %s\n" },
			{ "devvq3", "set df_promode 0;devmap %s\n" }, { "devcpm", "set df_promode 1;devmap %s\n" }
		};

		for ( i = 0; i < 4; i++ ) {
			if ( !Q_stricmp( cmd, maps[i].name ) ) {
				trap_Argv( 1, arg, sizeof( arg ) );
				if ( !UI_DF_MapExists( arg ) ) {
					Com_Printf( "Map not found.\n" );
					return qtrue;
				}
				trap_Cmd_ExecuteText( EXEC_APPEND, va( maps[i].fmt, arg ) );
				return qtrue;
			}
		}
	}
	return qfalse;
}
