/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "ui_local.h"

/*
oDFs: the Player page of the Settings menu: the name (the colour codes are shown in colour), the handicap as a slider,
the colour of the effects as a row of swatches, the model (a button that opens the model picker) and the 3D preview.
*/

#define ID_NAME			10
#define ID_HANDICAP		11
#define ID_MODEL		14
#define ID_LOGIN		30
#define ID_FX0			20		// 7 swatches

#define MAX_NAMELENGTH	20

#define PS_X			226		// where the controls start, the labels end 8 left of it
#define PS_ROW			28
#define PS_FIELD_W		( MAX_NAMELENGTH * SMALLCHAR_WIDTH + 8 )
#define PS_SWATCH		20
#define PS_SWATCH_GAP	4
#define PS_BUTTON_W		( PS_FIELD_W )
#define PS_BUTTON_H		22

// the records login of the engine (cl_login, cl_loginStatus)
#define PS_REC_Y		200
#define PS_REC_LOGIN_Y	( PS_REC_Y + 28 )
#define PS_REC_ACCOUNT_Y ( PS_REC_Y + 56 )
#define PS_REC_STATUS_Y	( PS_REC_Y + 76 )

#define PS_PREVIEW_X	410
#define PS_PREVIEW_Y	36
#define PS_PREVIEW_W	220
#define PS_PREVIEW_H	350


typedef struct {
	menuframework_s		menu;

	menufield_s			name;
	menuslider_s		handicap;
	menutext_s			fx[7];
	menutext_s			model;
	menuradiobutton_s	login;
	qboolean			records;		// the engine has the login

	playerInfo_t		playerinfo;
	uiPlayerView_t		view;
	int					effects;
	char				playerModel[MAX_QPATH];
} playersettings_t;

static playersettings_t	s_playersettings;

static int gamecodetoui[] = {4,2,3,0,5,1,6};
static int uitogamecode[] = {4,6,2,3,1,5,7};

// the swatches in the order of the effects colour (red, yellow, green, cyan, blue, magenta, white)
static vec4_t	psFx[7] = {
	{ 1.00f, 0.15f, 0.10f, 1.0f }, { 1.00f, 0.90f, 0.10f, 1.0f }, { 0.20f, 0.95f, 0.20f, 1.0f }, { 0.10f, 0.90f, 0.95f, 1.0f },
	{ 0.25f, 0.35f, 1.00f, 1.0f }, { 1.00f, 0.20f, 0.90f, 1.0f }, { 1.00f, 1.00f, 1.00f, 1.0f }
};
static vec4_t	psWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	psPlate = { 0.14f, 0.02f, 0.02f, 0.90f };
static vec4_t	psPlateFocus = { 0.26f, 0.04f, 0.03f, 0.95f };
static vec4_t	psBorderFocus = { 1.0f, 0.85f, 0.25f, 1.0f };
static vec4_t	psHint = { 0.62f, 0.38f, 0.34f, 1.0f };
static vec4_t	psLabel ={ 1.0f, 0.82f, 0.76f, 1.0f };
static vec4_t	psFill = { 0.70f, 0.08f, 0.05f, 0.14f };
static vec4_t	psFillLit = { 0.85f, 0.15f, 0.08f, 0.32f };
static vec4_t	psGood = { 0.55f, 0.85f, 0.55f, 1.0f };
static vec4_t	psWarn = { 1.0f, 0.85f, 0.25f, 1.0f };


static void PS_Outline( int x, int y, int w, int h, int t, const float *color ) {
	UI_FillRect( x, y, w, t, color );
	UI_FillRect( x, y + h - t, w, t, color );
	UI_FillRect( x, y, t, h, color );
	UI_FillRect( x + w - t, y, t, h, color );
}

/*
=================
PS_DrawColored

A name with its colour codes at work (^0 is shown white).  Without "colors" the text is plain.
Returns the number of characters drawn.
=================
*/
static int PS_DrawColored( int x, int y, const char *txt, qboolean colors, int style ) {
	float		*color = psWhite;
	int			n, count = 0;

	while ( *txt ) {
		if ( colors && Q_IsColorString( txt ) ) {
			n = UI_ColorIndex( *( txt + 1 ) );
			if ( n == 0 ) {
				n = 7;
			}
			color = (float *)g_color_table[n];
			txt += 2;
			continue;
		}
		UI_DrawChar( x, y, *txt, style, color );
		txt++;
		x += SMALLCHAR_WIDTH;
		count++;
	}
	return count;
}

static int PS_ColoredLength( const char *txt ) {
	int	count = 0;

	while ( *txt ) {
		if ( Q_IsColorString( txt ) ) {
			txt += 2;
			continue;
		}
		txt++;
		count++;
	}
	return count;
}


/*
=================
PlayerSettings_DrawName
=================
*/
static void PlayerSettings_DrawName( void *self ) {
	menufield_s		*f = (menufield_s *)self;
	qboolean		focus = ( f->generic.parent->cursor == f->generic.menuPosition );
	int				x = f->generic.x, y = f->generic.y;
	int				style = UI_LEFT | UI_SMALLFONT;

	UI_DrawString( x - SMALLCHAR_WIDTH, y, "Name:", UI_RIGHT | UI_SMALLFONT,
		UI_DF_NAMECOLOR( f->generic.parent, focus ? text_color_highlight : text_color_normal ) );

	UI_FillRect( x - 4, y - 2, PS_FIELD_W, SMALLCHAR_HEIGHT + 4, focus ? psPlateFocus : psPlate );
	if ( focus ) {
		PS_Outline( x - 4, y - 2, PS_FIELD_W, SMALLCHAR_HEIGHT + 4, 1, psBorderFocus );
		PS_DrawColored( x, y, f->field.buffer, qfalse, style );
		// the cursor
		UI_DrawChar( x + f->field.cursor * SMALLCHAR_WIDTH, y, trap_Key_GetOverstrikeMode() ? 11 : 10, UI_LEFT | UI_SMALLFONT | UI_BLINK, psWhite );
	} else {
		PS_DrawColored( x, y, f->field.buffer, qtrue, style );
	}
}


/*
=================
PlayerSettings_SwatchDraw
=================
*/
static void PlayerSettings_SwatchDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	int			fx = t->generic.id - ID_FX0;
	qboolean	lit = ( t->generic.flags & QMF_HASMOUSEFOCUS ) || Menu_ItemAtCursor( t->generic.parent ) == self;
	int			x = t->generic.left, y = t->generic.top;

	UI_FillRect( x, y, PS_SWATCH, PS_SWATCH, psFx[fx] );
	if ( fx == s_playersettings.effects ) {
		PS_Outline( x - 3, y - 3, PS_SWATCH + 6, PS_SWATCH + 6, 2, psWhite );
	} else if ( lit ) {
		PS_Outline( x - 2, y - 2, PS_SWATCH + 4, PS_SWATCH + 4, 1, psBorderFocus );
	}
}


/*
=================
PlayerSettings_ModelDraw

The button shows the model that is set.
=================
*/
static void PlayerSettings_ModelDraw( void *self ) {
	menutext_s	*t = (menutext_s *)self;
	qboolean	lit = ( t->generic.flags & QMF_HASMOUSEFOCUS ) || Menu_ItemAtCursor( t->generic.parent ) == self;
	int			x = t->generic.left, y = t->generic.top;
	char		buf[MAX_QPATH];

	UI_DrawString( x - SMALLCHAR_WIDTH, y + ( PS_BUTTON_H - SMALLCHAR_HEIGHT ) / 2, "Model:", UI_RIGHT | UI_SMALLFONT,
		UI_DF_NAMECOLOR( t->generic.parent, text_color_normal ) );

	UI_FillRect( x, y, PS_BUTTON_W, PS_BUTTON_H, lit ? psFillLit : psFill );
	if ( lit ) {
		PS_Outline( x, y, PS_BUTTON_W, PS_BUTTON_H, 1, psBorderFocus );
	}
	trap_Cvar_VariableStringBuffer( "model", buf, sizeof( buf ) );
	if ( strlen( buf ) > MAX_NAMELENGTH ) {
		buf[MAX_NAMELENGTH - 1] = '>';
		buf[MAX_NAMELENGTH] = '\0';
	}
	UI_DrawString( x + 8, y + ( PS_BUTTON_H - SMALLCHAR_HEIGHT ) / 2, buf, UI_LEFT | UI_SMALLFONT, lit ? psWhite : psLabel );
}


/*
=================
PlayerSettings_DrawRecords

The records account: whether the login is on, who the server says you are, and your public key.  The status is only
known while a server is joined (the engine clears it on leaving); the account of the last login is remembered.
=================
*/
static void PlayerSettings_DrawRecords( void ) {
	char			status[64], account[64], text[32];
	const char		*reason;
	float			*statusColor = psHint, *accountColor = psHint;
	uiClientState_t	cs;
	qboolean		loginOn, connected, loggedIn = qfalse;
	int				labelX = PS_X - SMALLCHAR_WIDTH;

	trap_Cvar_VariableStringBuffer( "cl_loginStatus", status, sizeof( status ) );
	account[0] = '\0';
	trap_GetClientState( &cs );
	connected = cs.connState == CA_ACTIVE;
	loginOn = trap_Cvar_VariableValue( "cl_login" ) != 0;

	// what the server said
	if ( !loginOn ) {
		Q_strncpyz( text, "Login is off", sizeof( text ) );
	} else if ( !connected ) {
		Q_strncpyz( text, "Not connected", sizeof( text ) );
	} else if ( !Q_stricmpn( status, "ok ", 3 ) ) {
		Q_strncpyz( text, "Logged in", sizeof( text ) );
		statusColor = psGood;
		loggedIn = qtrue;
		Q_strncpyz( account, status + 3, sizeof( account ) );
	} else if ( !Q_stricmp( status, "unknown_key" ) ) {
		Q_strncpyz( text, "Key not registered", sizeof( text ) );
		statusColor = psWarn;
	} else if ( !Q_stricmp( status, "unavailable" ) ) {
		Q_strncpyz( text, "Service unavailable", sizeof( text ) );
		statusColor = psWarn;
	} else if ( !Q_stricmpn( status, "failed", 6 ) ) {
		reason = status[6] == ' ' ? status + 7 : "";
		Com_sprintf( text, sizeof( text ), "Failed: %.12s", reason );
		statusColor = psWarn;
	} else if ( !Q_stricmp( status, "off" ) ) {
		Q_strncpyz( text, "Logged out", sizeof( text ) );
	} else {
		Q_strncpyz( text, "No records here", sizeof( text ) );
	}

	// the account the server logged this player in as
	Q_CleanStr( account );
	if ( loggedIn ) {
		accountColor = psGood;
	}
	if ( strlen( account ) > 14 ) {
		account[13] = '>';
		account[14] = '\0';
	}

	UI_DrawString( 130, PS_REC_Y, "RECORDS", UI_LEFT | UI_SMALLFONT, uiDfColorMedium );
	UI_FillRect( 130, PS_REC_Y + 17, PS_X + PS_FIELD_W - 4 - 130, 1, psBorderFocus );

	UI_DrawString( labelX, PS_REC_ACCOUNT_Y, "Account:", UI_RIGHT | UI_SMALLFONT, UI_DF_NAMECOLOR( &s_playersettings.menu, text_color_normal ) );
	if ( account[0] ) {
		UI_DrawString( PS_X, PS_REC_ACCOUNT_Y, account, UI_LEFT | UI_SMALLFONT, accountColor );
	} else {
		UI_DrawString( PS_X, PS_REC_ACCOUNT_Y, "-", UI_LEFT | UI_SMALLFONT, psHint );
	}

	UI_DrawString( labelX, PS_REC_STATUS_Y, "Status:", UI_RIGHT | UI_SMALLFONT, UI_DF_NAMECOLOR( &s_playersettings.menu, text_color_normal ) );
	UI_DrawString( PS_X, PS_REC_STATUS_Y, text, UI_LEFT | UI_SMALLFONT, statusColor );

}


/*
=================
PlayerSettings_MenuDraw
=================
*/
static void PlayerSettings_MenuDraw( void ) {
	vec3_t	viewangles;
	char	buf[MAX_QPATH];
	int		y, len;

	trap_Cvar_VariableStringBuffer( "model", buf, sizeof( buf ) );
	if ( strcmp( buf, s_playersettings.playerModel ) != 0 ) {
		UI_PlayerInfo_SetModel( &s_playersettings.playerinfo, buf );
		strcpy( s_playersettings.playerModel, buf );

		viewangles[YAW]   = 180 - 30;
		viewangles[PITCH] = 0;
		viewangles[ROLL]  = 0;
		UI_PlayerInfo_SetInfo( &s_playersettings.playerinfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse );
	}
	UI_PlayerView_Draw( &s_playersettings.view, PS_PREVIEW_X, PS_PREVIEW_Y, PS_PREVIEW_W, PS_PREVIEW_H, &s_playersettings.playerinfo, uis.realtime / 2 );

	// the name as the others will see it, under the model
	y = PS_PREVIEW_Y + PS_PREVIEW_H + 6;
	len = PS_ColoredLength( s_playersettings.name.field.buffer );
	PS_DrawColored( PS_PREVIEW_X + PS_PREVIEW_W / 2 - len * SMALLCHAR_WIDTH / 2, y, s_playersettings.name.field.buffer, qtrue, UI_LEFT | UI_SMALLFONT );
	UI_DrawString( PS_PREVIEW_X + PS_PREVIEW_W / 2, y + SMALLCHAR_HEIGHT + 4, "drag to turn, wheel to zoom", UI_CENTER | UI_SMALLFONT, psHint );

	// the label of the swatches
	UI_DrawString( PS_X - SMALLCHAR_WIDTH, s_playersettings.fx[0].generic.top + ( PS_SWATCH - SMALLCHAR_HEIGHT ) / 2, "Effects:", UI_RIGHT | UI_SMALLFONT,
		UI_DF_NAMECOLOR( &s_playersettings.menu, text_color_normal ) );

	if ( s_playersettings.records ) {
		PlayerSettings_DrawRecords();
	}

	Menu_Draw( &s_playersettings.menu );
}


/*
=================
PlayerSettings_SaveChanges
=================
*/
static void PlayerSettings_SaveChanges( void ) {
	// name
	trap_Cvar_Set( "name", s_playersettings.name.field.buffer );

	// handicap
	trap_Cvar_SetValue( "handicap", (int)( s_playersettings.handicap.curvalue + 0.5f ) );

	// effects color
	trap_Cvar_SetValue( "color1", uitogamecode[s_playersettings.effects] );
}


/*
=================
PlayerSettings_MenuKey
=================
*/
static sfxHandle_t PlayerSettings_MenuKey( int key ) {
	menucommon_s	*m;
	int				fx;

	if( key == K_MOUSE2 || key == K_ESCAPE ) {
		PlayerSettings_SaveChanges();
	}

	// drag in the preview turns the model, the wheel there zooms
	if ( ( key == K_MOUSE1 || key == K_MWHEELUP || key == K_MWHEELDOWN )
		&& UI_PlayerView_Key( &s_playersettings.view, key, PS_PREVIEW_X, PS_PREVIEW_Y, PS_PREVIEW_W, PS_PREVIEW_H ) ) {
		return menu_null_sound;
	}

	// the swatches are a row: left and right move along it, up and down leave it
	m = Menu_ItemAtCursor( &s_playersettings.menu );
	if ( m && m->id >= ID_FX0 && m->id < ID_FX0 + 7 ) {
		fx = m->id - ID_FX0;
		switch ( key ) {
		case K_LEFTARROW: case K_KP_LEFTARROW:
			if ( fx > 0 ) {
				Menu_SetCursorToItem( &s_playersettings.menu, &s_playersettings.fx[fx - 1] );
				return menu_move_sound;
			}
			return menu_buzz_sound;
		case K_RIGHTARROW: case K_KP_RIGHTARROW:
			if ( fx < 6 ) {
				Menu_SetCursorToItem( &s_playersettings.menu, &s_playersettings.fx[fx + 1] );
				return menu_move_sound;
			}
			return menu_buzz_sound;
		case K_UPARROW: case K_KP_UPARROW:
			Menu_SetCursorToItem( &s_playersettings.menu, &s_playersettings.handicap );
			return menu_move_sound;
		case K_DOWNARROW: case K_KP_DOWNARROW:
			Menu_SetCursorToItem( &s_playersettings.menu, &s_playersettings.model );
			return menu_move_sound;
		}
	}
	return Menu_DefaultKey( &s_playersettings.menu, key );
}


/*
=================
PlayerSettings_SetMenuItems
=================
*/
static void PlayerSettings_SetMenuItems( void ) {
	vec3_t	viewangles;
	int		c;
	int		h;

	// name
	Q_strncpyz( s_playersettings.name.field.buffer, UI_Cvar_VariableString("name"), sizeof(s_playersettings.name.field.buffer) );

	// effects color
	c = trap_Cvar_VariableValue( "color1" ) - 1;
	if( c < 0 || c > 6 ) {
		c = 6;
	}
	s_playersettings.effects = gamecodetoui[c];

	// model/skin
	memset( &s_playersettings.playerinfo, 0, sizeof(playerInfo_t) );

	viewangles[YAW]   = 180 - 30;
	viewangles[PITCH] = 0;
	viewangles[ROLL]  = 0;

	UI_PlayerInfo_SetModel( &s_playersettings.playerinfo, UI_Cvar_VariableString( "model" ) );
	UI_PlayerInfo_SetInfo( &s_playersettings.playerinfo, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse );
	trap_Cvar_VariableStringBuffer( "model", s_playersettings.playerModel, sizeof( s_playersettings.playerModel ) );

	// handicap
	h = Com_Clamp( 5, 100, trap_Cvar_VariableValue("handicap") );
	s_playersettings.handicap.curvalue = h;
}


/*
=================
PlayerSettings_MenuEvent
=================
*/
static void PlayerSettings_MenuEvent( void* ptr, int event ) {
	int		id;

	if( event != QM_ACTIVATED ) {
		return;
	}

	id = ((menucommon_s*)ptr)->id;
	switch( id ) {
	case ID_HANDICAP:
		trap_Cvar_SetValue( "handicap", (int)( s_playersettings.handicap.curvalue + 0.5f ) );
		break;

	case ID_MODEL:
		PlayerSettings_SaveChanges();
		UI_PlayerModelMenu();
		break;

	case ID_LOGIN:
		// the commands also make the key when it is wanted
		trap_Cmd_ExecuteText( EXEC_APPEND, s_playersettings.login.curvalue ? "login\n" : "logout\n" );
		break;


	default:
		if ( id >= ID_FX0 && id < ID_FX0 + 7 ) {
			s_playersettings.effects = id - ID_FX0;
			trap_Cvar_SetValue( "color1", uitogamecode[s_playersettings.effects] );
		}
		break;
	}
}


/*
=================
PlayerSettings_MenuInit
=================
*/
static void PlayerSettings_MenuInit( void ) {
	int		i, y;

	memset(&s_playersettings,0,sizeof(playersettings_t));

	s_playersettings.menu.key        = PlayerSettings_MenuKey;
	s_playersettings.menu.draw       = PlayerSettings_MenuDraw;
	s_playersettings.menu.wrapAround = qtrue;
	s_playersettings.menu.fullscreen = qtrue;
	UI_PlayerView_Init( &s_playersettings.view );


	y = SETTINGS_FORM_Y;
	s_playersettings.name.generic.type			= MTYPE_FIELD;
	s_playersettings.name.generic.flags			= QMF_NODEFAULTINIT;
	s_playersettings.name.generic.ownerdraw		= PlayerSettings_DrawName;
	s_playersettings.name.field.widthInChars	= MAX_NAMELENGTH;
	s_playersettings.name.field.maxchars		= MAX_NAMELENGTH;
	s_playersettings.name.generic.x				= PS_X;
	s_playersettings.name.generic.y				= y;
	s_playersettings.name.generic.left			= PS_X - 4;
	s_playersettings.name.generic.top			= y - 2;
	s_playersettings.name.generic.right			= PS_X - 4 + PS_FIELD_W;
	s_playersettings.name.generic.bottom		= y + SMALLCHAR_HEIGHT + 2;
	Menu_AddItem( &s_playersettings.menu, &s_playersettings.name );

	y += PS_ROW;
	s_playersettings.handicap.generic.type		= MTYPE_SLIDER;
	s_playersettings.handicap.generic.name		= "Handicap:";
	s_playersettings.handicap.generic.flags		= QMF_SMALLFONT;
	s_playersettings.handicap.generic.id		= ID_HANDICAP;
	s_playersettings.handicap.generic.callback	= PlayerSettings_MenuEvent;
	s_playersettings.handicap.generic.x			= PS_X;
	s_playersettings.handicap.generic.y			= y;
	s_playersettings.handicap.minvalue			= 5;
	s_playersettings.handicap.maxvalue			= 100;
	s_playersettings.handicap.step				= 5;
	s_playersettings.handicap.suffix			= "%";
	Menu_AddItem( &s_playersettings.menu, &s_playersettings.handicap );

	y += PS_ROW;
	for ( i = 0; i < 7; i++ ) {
		menutext_s	*t = &s_playersettings.fx[i];

		t->generic.type		= MTYPE_PTEXT;
		t->generic.flags	= QMF_LEFT_JUSTIFY;
		t->generic.id		= ID_FX0 + i;
		t->generic.x		= PS_X + i * ( PS_SWATCH + PS_SWATCH_GAP );
		t->generic.y		= y;
		t->generic.callback	= PlayerSettings_MenuEvent;
		t->generic.ownerdraw = PlayerSettings_SwatchDraw;
		t->string			= "";
		Menu_AddItem( &s_playersettings.menu, t );
		// the widget worked out its mouse area from its text: use the swatch
		t->generic.left		= PS_X + i * ( PS_SWATCH + PS_SWATCH_GAP );
		t->generic.right	= t->generic.left + PS_SWATCH - 1;
		t->generic.top		= y;
		t->generic.bottom	= y + PS_SWATCH - 1;
	}

	y += PS_ROW + 4;
	s_playersettings.model.generic.type			= MTYPE_PTEXT;
	s_playersettings.model.generic.flags		= QMF_LEFT_JUSTIFY;
	s_playersettings.model.generic.id			= ID_MODEL;
	s_playersettings.model.generic.x			= PS_X;
	s_playersettings.model.generic.y			= y;
	s_playersettings.model.generic.callback		= PlayerSettings_MenuEvent;
	s_playersettings.model.generic.ownerdraw	= PlayerSettings_ModelDraw;
	s_playersettings.model.string				= "";
	Menu_AddItem( &s_playersettings.menu, &s_playersettings.model );
	s_playersettings.model.generic.left			= PS_X;
	s_playersettings.model.generic.right		= PS_X + PS_BUTTON_W - 1;
	s_playersettings.model.generic.top			= y;
	s_playersettings.model.generic.bottom		= y + PS_BUTTON_H - 1;

	// the records account: only where the engine has the login (cl_login exists)
	s_playersettings.records = UI_Cvar_VariableString( "cl_login" )[0] != '\0';

	s_playersettings.login.generic.type		= MTYPE_RADIOBUTTON;
	s_playersettings.login.generic.name		= "Login:";
	s_playersettings.login.generic.flags	= QMF_SMALLFONT;
	s_playersettings.login.generic.id		= ID_LOGIN;
	s_playersettings.login.generic.callback	= PlayerSettings_MenuEvent;
	s_playersettings.login.generic.x		= PS_X;
	s_playersettings.login.generic.y		= PS_REC_LOGIN_Y;
	s_playersettings.login.curvalue			= trap_Cvar_VariableValue( "cl_login" ) != 0;
	Menu_AddItem( &s_playersettings.menu, &s_playersettings.login );

	if ( !s_playersettings.records ) {
		s_playersettings.login.generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
	}

	UI_Settings_AddRail( &s_playersettings.menu, SETP_PLAYER, PlayerSettings_SaveChanges );

	PlayerSettings_SetMenuItems();
}


/*
=================
UI_PlayerSettingsMenu
=================
*/
void UI_PlayerSettingsMenu( void ) {
	PlayerSettings_MenuInit();
	UI_PushMenu( &s_playersettings.menu );
}
