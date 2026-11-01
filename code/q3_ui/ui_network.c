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
/*
=======================================================================

NETWORK OPTIONS MENU

=======================================================================
*/

#include "ui_local.h"


#define ID_RATE				14
#define ID_ALLOWDOWNLOAD	15


static const char *rate_items[] = {
	"<= 28.8K",
	"33.6K",
	"56K",
	"ISDN",
	"LAN/Cable/xDSL",
	NULL
};

typedef struct {
	menuframework_s	menu;


	menulist_s		rate;
	menuradiobutton_s	allowdownload;

} networkOptionsInfo_t;

static networkOptionsInfo_t	networkOptionsInfo;


/*
=================
UI_NetworkOptionsMenu_Event
=================
*/
static void UI_NetworkOptionsMenu_Event( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_ALLOWDOWNLOAD:
		// DeFRaG 0x12cc3: only the client side cvar is set (stock Q3 also sets sv_allowDownload)
		trap_Cvar_SetValue( "cl_allowDownload", networkOptionsInfo.allowdownload.curvalue );
		break;

	case ID_RATE:
		if( networkOptionsInfo.rate.curvalue == 0 ) {
			trap_Cvar_SetValue( "rate", 2500 );
		}
		else if( networkOptionsInfo.rate.curvalue == 1 ) {
			trap_Cvar_SetValue( "rate", 3000 );
		}
		else if( networkOptionsInfo.rate.curvalue == 2 ) {
			trap_Cvar_SetValue( "rate", 4000 );
		}
		else if( networkOptionsInfo.rate.curvalue == 3 ) {
			trap_Cvar_SetValue( "rate", 5000 );
		}
		else if( networkOptionsInfo.rate.curvalue == 4 ) {
			trap_Cvar_SetValue( "rate", 25000 );
		}
		break;

	}
}


/*
===============
UI_NetworkOptionsMenu_Init
===============
*/
static void UI_NetworkOptionsMenu_Init( void ) {
	int		y;
	int		rate;

	memset( &networkOptionsInfo, 0, sizeof(networkOptionsInfo) );

	UI_NetworkOptionsMenu_Cache();
	networkOptionsInfo.menu.wrapAround = qtrue;
	networkOptionsInfo.menu.fullscreen = qtrue;


	y = SETTINGS_FORM_Y;
	networkOptionsInfo.rate.generic.type		= MTYPE_SPINCONTROL;
	networkOptionsInfo.rate.generic.name		= "Data Rate:";
	networkOptionsInfo.rate.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	networkOptionsInfo.rate.generic.callback	= UI_NetworkOptionsMenu_Event;
	networkOptionsInfo.rate.generic.id			= ID_RATE;
	networkOptionsInfo.rate.generic.x			= SETTINGS_FORM_X;
	networkOptionsInfo.rate.generic.y			= y;
	networkOptionsInfo.rate.itemnames			= rate_items;
	y += SETTINGS_ROW_H;

	networkOptionsInfo.allowdownload.generic.type		= MTYPE_RADIOBUTTON;
	networkOptionsInfo.allowdownload.generic.name		= "Automatic Downloading:";
	networkOptionsInfo.allowdownload.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	networkOptionsInfo.allowdownload.generic.callback	= UI_NetworkOptionsMenu_Event;
	networkOptionsInfo.allowdownload.generic.id			= ID_ALLOWDOWNLOAD;
	networkOptionsInfo.allowdownload.generic.x			= SETTINGS_FORM_X;
	networkOptionsInfo.allowdownload.generic.y			= y;


	Menu_AddItem( &networkOptionsInfo.menu, ( void * ) &networkOptionsInfo.rate );
	Menu_AddItem( &networkOptionsInfo.menu, ( void * ) &networkOptionsInfo.allowdownload );
	UI_Settings_AddRail( &networkOptionsInfo.menu, SETP_NETWORK, NULL );

	networkOptionsInfo.allowdownload.curvalue = trap_Cvar_VariableValue( "cl_allowDownload" ) != 0;

	rate = trap_Cvar_VariableValue( "rate" );
	if( rate <= 2500 ) {
		networkOptionsInfo.rate.curvalue = 0;
	}
	else if( rate <= 3000 ) {
		networkOptionsInfo.rate.curvalue = 1;
	}
	else if( rate <= 4000 ) {
		networkOptionsInfo.rate.curvalue = 2;
	}
	else if( rate <= 5000 ) {
		networkOptionsInfo.rate.curvalue = 3;
	}
	else {
		networkOptionsInfo.rate.curvalue = 4;
	}
}


/*
===============
UI_NetworkOptionsMenu_Cache
===============
*/
void UI_NetworkOptionsMenu_Cache( void ) {
}


/*
===============
UI_NetworkOptionsMenu
===============
*/
void UI_NetworkOptionsMenu( void ) {
	UI_NetworkOptionsMenu_Init();
	UI_PushMenu( &networkOptionsInfo.menu );
}
