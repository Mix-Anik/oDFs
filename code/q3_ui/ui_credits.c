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
/*
=======================================================================

CREDITS

DeFRaG (ui 0x1d44 init, 0x1bc0 draw, 0x1bae key, 0x1b97 exit): an auto-scrolling credits roll (25 pixels per second) over
df_gfx/menu/credits_bg with a flickering red tint, intro music, and no mouse cursor.  Any key press (not a character) ends it
and restores s_musicvolume.  After 28.4 seconds the "music music/fla22k_01_loop" command is run once.
=======================================================================
*/


#include "ui_local.h"

extern vec4_t uiDfCreditHead;
extern vec4_t uiDfCreditBlank;
extern qboolean uiDfNoCursor;

vec4_t uiDfCreditHead	= { 0.53f, 0.77f, 1.00f, 1.00f };
vec4_t uiDfCreditBlank	= { 0.00f, 0.00f, 1.00f, 1.00f };

typedef struct {
	const char	*text;
	int			style;
	float		*color;
} creditLine_t;

static const creditLine_t creditLines[] = {
	{ "Defrag", 0x21, uiDfCreditHead },
	{ "", 0x11, uiDfCreditBlank },
	{ "", 0x11, uiDfCreditBlank },
	{ "Programming:", 0x11, uiDfCreditHead },
	{ "Cyril 'Cgg' Gantin", 0x11, color_white },
	{ "Cliff 'm00m1n' Rowley", 0x11, color_white },
	{ "John 'Ozone-Junkie' Mason", 0x11, color_white },
	{ "Robert 'BeRsErKeR' S.", 0x11, color_white },
	{ "Mark 'marky' M.", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "Levels Design:", 0x11, uiDfCreditHead },
	{ "Alexandre 'AciD' B.", 0x11, color_white },
	{ "Matthieu 'DonPichol' Simon", 0x11, color_white },
	{ "KJ 'Gambit' Hegerty", 0x11, color_white },
	{ "Medvedev 'Genosh' Roman", 0x11, color_white },
	{ "Gwendal 'Gwen' Le Coguic", 0x11, color_white },
	{ "Tobias 'Hitm4n' Gries", 0x11, color_white },
	{ "Octave 'Instazz' Boussaton", 0x11, color_white },
	{ "Nicolas 'Noskey' Martin", 0x11, color_white },
	{ "Daniel 'Space' Lundgren", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "Website, competition administration:", 0x11, uiDfCreditHead },
	{ "Eric 'Beamer' Leblanc", 0x11, color_white },
	{ "Alexander 'Boo' Zeillinger", 0x11, color_white },
	{ "Gwendal 'Gwen' Le Coguic", 0x11, color_white },
	{ "Marc 'Prophet' Wegner", 0x11, color_white },
	{ "Markus 'Mrks' H.", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "Additionnal code:", 0x11, uiDfCreditHead },
	{ "Challenge Promode", 0x11, color_white },
	{ "Piotr 'Camping Gaz' Z.", 0x11, color_white },
	{ "Firestarter", 0x11, color_white },
	{ "Iain McGinniss", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "Beta-testing:", 0x11, uiDfCreditHead },
	{ "Atomicoz", 0x11, color_white },
	{ "b3nt", 0x11, color_white },
	{ "BardoK", 0x11, color_white },
	{ "Justas 'CeTuS' V.", 0x11, color_white },
	{ "cheez", 0x11, color_white },
	{ "deffy", 0x11, color_white },
	{ "eckocx", 0x11, color_white },
	{ "iT.Edisdead", 0x11, color_white },
	{ "iT.Fallen", 0x11, color_white },
	{ "GunnarBot", 0x11, color_white },
	{ "huntz", 0x11, color_white },
	{ "KaffeeOnko", 0x11, color_white },
	{ "Markus", 0x11, color_white },
	{ "Eugene 'mc`uni' Kravchenko", 0x11, color_white },
	{ "Melgibbsem", 0x11, color_white },
	{ "Michael Flatley", 0x11, color_white },
	{ "m1tsu", 0x11, color_white },
	{ "novaa", 0x11, color_white },
	{ "oRCeN", 0x11, color_white },
	{ "orxata", 0x11, color_white },
	{ "own-age.com", 0x11, color_white },
	{ "prophet", 0x11, color_white },
	{ "sektor", 0x11, color_white },
	{ "shorrty", 0x11, color_white },
	{ "s0m", 0x11, color_white },
	{ "Ronald 'Symbi0nt' Locke", 0x11, color_white },
	{ "Tab", 0x11, color_white },
	{ "Tiff", 0x11, color_white },
	{ "T0t3r", 0x11, color_white },
	{ "Neil '-vertigo-' Cahill", 0x11, color_white },
	{ "Thomas 'UngamedPlayer' Hartvig", 0x11, color_white },
	{ "yttoP", 0x11, color_white },
	{ "Venom", 0x11, color_white },
	{ "shio", 0x11, color_white },
	{ "Patrick 'AMT-Morbus' Gassenbauer", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "On retirement:", 0x11, uiDfCreditHead },
	{ "Ackman", 0x11, color_white },
	{ "El*Nionio", 0x11, color_white },
	{ "Fredd", 0x11, color_white },
	{ "Polo2ro", 0x11, color_white },
	{ "Andrey 'Thrasher' P.", 0x11, color_white },
	{ "", 0x11, uiDfCreditBlank },
	{ "", 0x11, uiDfCreditBlank },
	{ "http://planetquake.com/defrag", 0x11, uiDfCreditHead },
	{ "#defrag on irc.quakenet.org", 0x11, uiDfCreditHead },
	{ "", 0x11, uiDfCreditBlank },
};

#define CREDIT_LINES	( sizeof( creditLines ) / sizeof( creditLines[0] ) )

typedef struct {
	menuframework_s	menu;
	qhandle_t		bg;
	float			savedVolume;
	int				startTime;
	int				flickerNext;
	int				flickerLen;
	int				endCmdDone;
} creditsmenu_t;

static creditsmenu_t	s_credits;
static vec4_t			creditTint = { 1.0f, 0.3f, 0.3f, 0.0f };

/*
===============
UI_CreditMenu_Exit (0x1b97)
===============
*/
static void UI_CreditMenu_Exit( void ) {
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "s_musicvolume %f; music sound/null\n", s_credits.savedVolume ) );
	UI_PopMenu();
}

/*
===============
UI_CreditMenu_Key (0x1bae)
===============
*/
static sfxHandle_t UI_CreditMenu_Key( int key ) {
	if( key & K_CHAR_FLAG ) {
		return 0;
	}
	UI_CreditMenu_Exit();
	return 0;
}

/*
===============
UI_CreditMenu_Draw (0x1bc0)
===============
*/
static void UI_CreditMenu_Draw( void ) {
	float	y;
	int		i, len;

	uiDfNoCursor = qtrue;

	// red tint that flickers: a sine bump every 2 - 6 seconds, never darker than 0.4
	if ( !s_credits.flickerNext ) {
		s_credits.flickerNext = uis.realtime + 2000 + ( rand() % 4000 );
	}
	if ( uis.realtime >= s_credits.flickerNext ) {
		if ( !s_credits.flickerLen ) {
			s_credits.flickerLen = ( rand() % 500 ) + 900;
		}
		creditTint[3] = sin( (float)( uis.realtime - s_credits.flickerNext ) / (float)s_credits.flickerLen );
		if ( creditTint[3] < 0 ) {
			s_credits.flickerNext = 0;
			s_credits.flickerLen = 0;
		}
	}
	if ( creditTint[3] < 0.4f ) {
		creditTint[3] = 0.4f;
	}
	UI_SetColor( creditTint );

	if ( !s_credits.endCmdDone && uis.realtime - s_credits.startTime >= 28400 ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "music music/fla22k_01_loop\n" );
		s_credits.endCmdDone = 1;
	}

	UI_DrawHandlePic( 0, 0, 640, 480, s_credits.bg );
	UI_SetColor( NULL );

	y = (int)( 480.0f - ( 2.5f * (float)( uis.realtime - s_credits.startTime ) ) / 100.0f );
	for ( i = 0; i < (int)CREDIT_LINES; i++ ) {
		if ( !creditLines[i].text ) {
			if ( y < -16 ) {
				UI_CreditMenu_Exit();
			}
			break;
		}
		len = strlen( creditLines[i].text );
		if ( len != 1 && y > -36 ) {
			UI_DrawProportionalString( 320, (int)y, creditLines[i].text, creditLines[i].style, creditLines[i].color );
		}
		if ( len == 1 ) {
			// a single character line is a spacer and takes no room (0x1cce)
			continue;
		}
		if ( creditLines[i].style & UI_SMALLFONT ) {
			y = (int)( y + 20.25f );
		} else if ( creditLines[i].style & UI_BIGFONT ) {
			y += 27;
		} else if ( creditLines[i].style & UI_GIANTFONT ) {
			y = (int)( y + 36.0f );
		}
		if ( y > 480 ) {
			break;
		}
	}
}

/*
===============
UI_CreditMenu (0x1d44)
===============
*/
void UI_CreditMenu( void ) {
	memset( &s_credits, 0 ,sizeof(s_credits) );

	s_credits.menu.draw = UI_CreditMenu_Draw;
	s_credits.menu.key = UI_CreditMenu_Key;
	s_credits.menu.fullscreen = qtrue;
	UI_PushMenu ( &s_credits.menu );

	s_credits.startTime = uis.realtime;
	s_credits.savedVolume = trap_Cvar_VariableValue( "s_musicvolume" );
	if ( s_credits.savedVolume < 0.5f ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "s_musicvolume 0.5\n" );
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, "music music/fla22k_01_intro\n" );
	s_credits.bg = trap_R_RegisterShaderNoMip( "df_gfx/menu/credits_bg" );
	creditTint[3] = 0.4f;
}
