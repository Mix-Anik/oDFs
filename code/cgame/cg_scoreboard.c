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
// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"
qboolean CG_DF_DrawScoreboard( void );
extern vmCvar_t df_scoreboard;


#define	SCOREBOARD_X		(0)

#define SB_HEADER			86
#define SB_TOP				(SB_HEADER+32)

// Where the status bar starts, so we don't overwrite it
#define SB_STATUSBAR		420

#define SB_NORMAL_HEIGHT	40
#define SB_INTER_HEIGHT		16 // interleaved height

#define SB_MAXCLIENTS_NORMAL  ((SB_STATUSBAR - SB_TOP) / SB_NORMAL_HEIGHT)
#define SB_MAXCLIENTS_INTER   ((SB_STATUSBAR - SB_TOP) / SB_INTER_HEIGHT - 1)

// Used when interleaved



#define SB_LEFT_BOTICON_X	(SCOREBOARD_X+0)
#define SB_LEFT_HEAD_X		(SCOREBOARD_X+32)
#define SB_RIGHT_BOTICON_X	(SCOREBOARD_X+64)
#define SB_RIGHT_HEAD_X		(SCOREBOARD_X+96)
// Normal
#define SB_BOTICON_X		(SCOREBOARD_X+32)
#define SB_HEAD_X			(SCOREBOARD_X+64)

#define SB_SCORELINE_X		112

#define SB_RATING_WIDTH	    (6 * BIGCHAR_WIDTH) // width 6
#define SB_SCORE_X			(SB_SCORELINE_X + BIGCHAR_WIDTH) // width 6
#define SB_RATING_X			(SB_SCORELINE_X + 6 * BIGCHAR_WIDTH) // width 6
#define SB_PING_X			(SB_SCORELINE_X + 12 * BIGCHAR_WIDTH + 8) // width 5
#define SB_TIME_X			(SB_SCORELINE_X + 17 * BIGCHAR_WIDTH + 8) // width 5
#define SB_NAME_X			(SB_SCORELINE_X + 22 * BIGCHAR_WIDTH) // width 15

// The new and improved score board
//
// In cases where the number of clients is high, the score board heads are interleaved
// here's the layout

//
//	0   32   80  112  144   240  320  400   <-- pixel position
//  bot head bot head score ping time name
//  
//  wins/losses are drawn on bot icon now

static qboolean localClient; // true if local client has been displayed


							 /*
=================
CG_DrawScoreboard
=================
*/
static void CG_DrawClientScore( int y, score_t *score, float *color, float fade, qboolean largeFormat ) {
	char	string[1024];
	vec3_t	headAngles;
	clientInfo_t	*ci;
	int iconx, headx;

	if ( score->client < 0 || score->client >= cgs.maxclients ) {
		Com_Printf( "Bad score->client: %i\n", score->client );
		return;
	}
	
	ci = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + (SB_RATING_WIDTH / 2);
	headx = SB_HEAD_X + (SB_RATING_WIDTH / 2);

	// draw the handicap or bot skill marker (unless player has flag)
	if ( ci->powerups & ( 1 << PW_NEUTRALFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_FREE, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_FREE, qfalse );
		}
	} else if ( ci->powerups & ( 1 << PW_REDFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_RED, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_RED, qfalse );
		}
	} else if ( ci->powerups & ( 1 << PW_BLUEFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_BLUE, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_BLUE, qfalse );
		}
	} else {
		if ( ci->botSkill > 0 && ci->botSkill <= 5 ) {
			if ( cg_drawIcons.integer ) {
				if( largeFormat ) {
					CG_DrawPic( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, cgs.media.botSkillShaders[ ci->botSkill - 1 ] );
				}
				else {
					CG_DrawPic( iconx, y, 16, 16, cgs.media.botSkillShaders[ ci->botSkill - 1 ] );
				}
			}
		} else if ( ci->handicap < 100 ) {
			Com_sprintf( string, sizeof( string ), "%i", ci->handicap );
			if ( cgs.gametype == GT_TOURNAMENT )
				CG_DrawSmallStringColor( iconx, y - SMALLCHAR_HEIGHT/2, string, color );
			else
				CG_DrawSmallStringColor( iconx, y, string, color );
		}

		// draw the wins / losses
		if ( cgs.gametype == GT_TOURNAMENT ) {
			Com_sprintf( string, sizeof( string ), "%i/%i", ci->wins, ci->losses );
			if( ci->handicap < 100 && !ci->botSkill ) {
				CG_DrawSmallStringColor( iconx, y + SMALLCHAR_HEIGHT/2, string, color );
			}
			else {
				CG_DrawSmallStringColor( iconx, y, string, color );
			}
		}
	}

	// draw the face
	VectorClear( headAngles );
	headAngles[YAW] = 180;
	if( largeFormat ) {
		CG_DrawHead( headx, y - ( ICON_SIZE - BIGCHAR_HEIGHT ) / 2, ICON_SIZE, ICON_SIZE, 
			score->client, headAngles );
	}
	else {
		CG_DrawHead( headx, y, 16, 16, score->client, headAngles );
	}

#ifdef MISSIONPACK
	// draw the team task
	if ( ci->teamTask != TEAMTASK_NONE ) {
		if ( ci->teamTask == TEAMTASK_OFFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.assaultShader );
		}
		else if ( ci->teamTask == TEAMTASK_DEFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.defendShader );
		}
	}
#endif
	// draw the score line
	if ( score->ping == -1 ) {
		Com_sprintf(string, sizeof(string),
			" connecting    %s", ci->name);
	} else if ( ci->team == TEAM_SPECTATOR ) {
		Com_sprintf(string, sizeof(string),
			" SPECT %3i      %s", score->ping, ci->name);
	} else {
		// DeFRaG ranks by best finish time (score->score holds it in ms, 0 = no time yet)
		Com_sprintf(string, sizeof(string),
			"%6s %4i      %s", score->score ? CG_DF_FormatTime( score->score ) : "-", score->ping, ci->name);
	}

	// highlight your position
	if ( score->client == cg.snap->ps.clientNum ) {
		float	hcolor[4];
		int		rank;

		localClient = qtrue;

		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR 
			|| cgs.gametype >= GT_TEAM ) {
			rank = -1;
		} else {
			rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;
		}
		if ( rank == 0 ) {
			hcolor[0] = 0;
			hcolor[1] = 0;
			hcolor[2] = 0.7f;
		} else if ( rank == 1 ) {
			hcolor[0] = 0.7f;
			hcolor[1] = 0;
			hcolor[2] = 0;
		} else if ( rank == 2 ) {
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0;
		} else {
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0.7f;
		}

		hcolor[3] = fade * 0.7;
		CG_FillRect( SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2), y, 
			640 - SB_SCORELINE_X - BIGCHAR_WIDTH, BIGCHAR_HEIGHT+1, hcolor );
	}

	CG_DrawBigString( SB_SCORELINE_X + (SB_RATING_WIDTH / 2), y, string, fade );

	// add the "ready" marker for intermission exiting
	if ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) {
		CG_DrawBigStringColor( iconx, y, "READY", color );
	}
}

/*
=================
CG_TeamScoreboard
=================
*/
static int CG_TeamScoreboard( int y, team_t team, float fade, int maxClients, int lineHeight ) {
	int		i;
	score_t	*score;
	float	color[4];
	int		count;
	clientInfo_t	*ci;

	color[0] = color[1] = color[2] = 1.0;
	color[3] = fade;

	count = 0;
	for ( i = 0 ; i < cg.numScores && count < maxClients ; i++ ) {
		score = &cg.scores[i];
		ci = &cgs.clientinfo[ score->client ];

		if ( team != ci->team ) {
			continue;
		}

		CG_DrawClientScore( y + lineHeight * count, score, color, fade, lineHeight == SB_NORMAL_HEIGHT );

		count++;
	}

	return count;
}

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
qboolean CG_DrawOldScoreboard( void ) {
	int		x, y, w, i, n1, n2;
	float	fade;
	float	*fadeColor;
	const char	*s;
	int maxClients;
	int lineHeight;
	int topBorderSize, bottomBorderSize;

	// don't draw amuthing if the menu or console is up
	if ( cg_paused.integer ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	if ( cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores ) {
		return qfalse;
	}

	if ( df_scoreboard.integer ) {		// DeFRaG race scoreboard (cgame 0x668c)
		return CG_DF_DrawScoreboard();
	}

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ||
		 cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		fade = 1.0;
		fadeColor = colorWhite;
	} else {
		fadeColor = CG_FadeColor( cg.scoreFadeTime, FADE_TIME );
		
		if ( !fadeColor ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			return qfalse;
		}
		fade = *fadeColor;
	}


	// fragged by ... line
	if ( cg.killerName[0] ) {
		s = va("Fragged by %s", cg.killerName );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 40;
		CG_DrawBigString( x, y, s, fade );
	}

	// current rank
	if ( cgs.gametype < GT_TEAM) {
		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
			s = va("%s place with %i",
				CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),
				cg.snap->ps.persistant[PERS_SCORE] );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
			x = ( SCREEN_WIDTH - w ) / 2;
			y = 60;
			CG_DrawBigString( x, y, s, fade );
		}
	} else {
		if ( cg.teamScores[0] == cg.teamScores[1] ) {
			s = va("Teams are tied at %i", cg.teamScores[0] );
		} else if ( cg.teamScores[0] >= cg.teamScores[1] ) {
			s = va("Red leads %i to %i",cg.teamScores[0], cg.teamScores[1] );
		} else {
			s = va("Blue leads %i to %i",cg.teamScores[1], cg.teamScores[0] );
		}

		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 60;
		CG_DrawBigString( x, y, s, fade );
	}

	// scoreboard
	y = SB_HEADER;

	CG_DrawPic( SB_SCORE_X + (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardScore );
	CG_DrawPic( SB_PING_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardPing );
	CG_DrawPic( SB_TIME_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardTime );
	CG_DrawPic( SB_NAME_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardName );

	y = SB_TOP;

	// If there are more than SB_MAXCLIENTS_NORMAL, use the interleaved scores
	if ( cg.numScores > SB_MAXCLIENTS_NORMAL ) {
		maxClients = SB_MAXCLIENTS_INTER;
		lineHeight = SB_INTER_HEIGHT;
		topBorderSize = 8;
		bottomBorderSize = 16;
	} else {
		maxClients = SB_MAXCLIENTS_NORMAL;
		lineHeight = SB_NORMAL_HEIGHT;
		topBorderSize = 16;
		bottomBorderSize = 16;
	}

	localClient = qfalse;

	if ( cgs.gametype >= GT_TEAM ) {
		//
		// teamplay scoreboard
		//
		y += lineHeight/2;

		if ( cg.teamScores[0] >= cg.teamScores[1] ) {
			n1 = CG_TeamScoreboard( y, TEAM_RED, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED );
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard( y, TEAM_BLUE, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE );
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;
		} else {
			n1 = CG_TeamScoreboard( y, TEAM_BLUE, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE );
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard( y, TEAM_RED, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED );
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;
		}
		n1 = CG_TeamScoreboard( y, TEAM_SPECTATOR, fade, maxClients, lineHeight );
		y += (n1 * lineHeight) + BIGCHAR_HEIGHT;

	} else {
		//
		// free for all scoreboard
		//
		n1 = CG_TeamScoreboard( y, TEAM_FREE, fade, maxClients, lineHeight );
		y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
		n2 = CG_TeamScoreboard( y, TEAM_SPECTATOR, fade, maxClients - n1, lineHeight );
		y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
	}

	if (!localClient) {
		// draw local client at the bottom
		for ( i = 0 ; i < cg.numScores ; i++ ) {
			if ( cg.scores[i].client == cg.snap->ps.clientNum ) {
				CG_DrawClientScore( y, &cg.scores[i], fadeColor, fade, lineHeight == SB_NORMAL_HEIGHT );
				break;
			}
		}
	}

	// DeFRaG footer (cgame 0x668c): speed award
	if ( cg.speedAwardUps > 0 ) {
		const char *holder = cg.speedAwardName[0] ? cg.speedAwardName
			: ( cg.speedAwardClient >= 0 ? cgs.clientinfo[cg.speedAwardClient].name : "" );
		CG_DrawBigString( 8, SCREEN_HEIGHT - 24, va( "Speed Award: %s ^7: %i ups", holder, cg.speedAwardUps ), fade );
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 10 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}

//================================================================================

/*
================
CG_CenterGiantLine
================
*/
static void CG_CenterGiantLine( float y, const char *string ) {
	float		x;
	vec4_t		color;

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	x = 0.5 * ( 640 - GIANT_WIDTH * CG_DrawStrlen( string ) );

	CG_DrawStringExt( x, y, string, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
}

/*
=================
CG_DrawTourneyScoreboard

Draw the oversize scoreboard for tournements
=================
*/
void CG_DrawTourneyScoreboard( void ) {
	const char		*s;
	vec4_t			color;
	int				min, tens, ones;
	clientInfo_t	*ci;
	int				y;
	int				i;

	// request more scores regularly
	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );
	}

	// draw the dialog background
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;
	CG_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color );

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	// print the mesage of the day
	s = CG_ConfigString( CS_MOTD );
	if ( !s[0] ) {
		s = "Scoreboard";
	}

	// print optional title
	CG_CenterGiantLine( 8, s );

	// print server time
	ones = cg.time / 1000;
	min = ones / 60;
	ones %= 60;
	tens = ones / 10;
	ones %= 10;
	s = va("%i:%i%i", min, tens, ones );

	CG_CenterGiantLine( 64, s );


	// print the two scores

	y = 160;
	if ( cgs.gametype >= GT_TEAM ) {
		//
		// teamplay scoreboard
		//
		CG_DrawStringExt( 8, y, "Red Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[0] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		
		y += 64;

		CG_DrawStringExt( 8, y, "Blue Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[1] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
	} else {
		//
		// free for all scoreboard
		//
		for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
			ci = &cgs.clientinfo[i];
			if ( !ci->infoValid ) {
				continue;
			}
			if ( ci->team != TEAM_FREE ) {
				continue;
			}

			CG_DrawStringExt( 8, y, ci->name, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			s = va("%i", ci->score );
			CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			y += 64;
		}
	}


}

// ---- DeFRaG scoreboard (cgame 0x668c; disasm/CGAME/scoreboard_draw.md) ----
// A race table instead of the vanilla one: Player / Score (best time) / Ping, a "Spectators Info" panel (who follows whom), the speed award box and
// a version footer. df_hud_scoreboard_size (0/1) selects 8x10 / 10x12 characters, df_hud_scoreboard_showSpectators the spectator panel.
// Enabled by df_scoreboard (not a DeFRaG cvar; 0 = the vanilla layout).
vmCvar_t	df_scoreboard, df_hud_scoreboard_size, df_hud_scoreboard_showSpectators;

static const vec4_t	dsbBorder = { 1, 1, 1, 1 }, dsbOwn = { 0.3f, 0.3f, 0.3f, 0.9f }, dsbHead = { 0.2f, 0.3f, 0.4f, 0.9f },
					dsbHeadText = { 0.7f, 0.9f, 1, 1 }, dsbRow = { 0.2f, 0.2f, 0.2f, 0.9f }, dsbWhite = { 1, 1, 1, 1 }, dsbGrey = { 0.75f, 0.75f, 0.75f, 1 };
static qhandle_t	dsbEye, dsbCam, dsbAward;
static qboolean		dsbReg;

static const char *DSB_Pad( const char *s, int width ) {		// 0x663a: right-align in `width` columns
	static char	buf[40];
	int			len = strlen( s ), pad;

	if ( width > 32 ) {
		width = 31;
	}
	pad = width - len;
	if ( pad < 0 ) {
		pad = 0;
	}
	Com_Memset( buf, ' ', pad );
	Q_strncpyz( buf + pad, s, width - pad + 1 );
	return buf;
}

qboolean CG_DF_DrawScoreboard( void ) {
	const int	cw = df_hud_scoreboard_size.integer ? 10 : 8, ch = df_hud_scoreboard_size.integer ? 12 : 10;
	const int	colA = ( cw << 4 ) + 6, colB = ( cw << 3 ) + 6, colC = ( cw << 2 ) + 6, rh = ch + 6, W = colA + colB + colC;
	const int	half = ( ( W - 6 ) / 2 ) / cw - 1;
	qboolean	anyFollowed = qfalse, wide;
	float		x, y = 68;
	int			i, j, self = cg.snap ? cg.snap->ps.clientNum : 0;
	char		buf[128];

	if ( !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION ) {
		return qfalse;
	}
	if ( !dsbReg ) {
		dsbEye = trap_R_RegisterShader( "df_gfx/scoreboard/eye.tga" );
		dsbCam = trap_R_RegisterShader( "df_gfx/scoreboard/cam.tga" );
		dsbAward = trap_R_RegisterShader( "df_gfx/scoreboard/award_speed.tga" );
		dsbReg = qtrue;
	}
	for ( i = 0; i < cg.numScores; i++ ) {
		if ( cg.scores[i].scoreFlags != -1 ) {		// someone follows scores[j]
			anyFollowed = qtrue;
		}
	}
	wide = anyFollowed && df_hud_scoreboard_showSpectators.integer && cg.numScores >= 8;

	if ( !cg.demoPlayback ) {		// date / server name line
		qtime_t	t;
		int		len;
		char	svname[256];

		trap_RealTime( &t );
		trap_Cvar_VariableStringBuffer( "defrag_servername", svname, sizeof( svname ) );		// 0x11d390 = cvar defrag_servername (cached by 0x3c33)
		Com_sprintf( buf, sizeof( buf ), "%s %04i-%02i-%02i %02i:%02i", svname,
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min );
		len = strlen( buf );
		CG_DrawStringExt( 320 - ( len << 3 ) / 2, 57, buf, dsbGrey, qtrue, qtrue, 8, 8, 0 );
	}

	if ( wide ) {
		x = 320 - W - ( df_hud_scoreboard_size.integer ? 10 : 18 ) / 2;
	} else {
		x = 320 - W / 2;
	}

	// header
	CG_FillRect( x, y, W, rh, dsbHead );
	CG_DrawRect( x, y, colA, rh, 1, dsbBorder );
	CG_DrawStringExt( x + 3, y + 3, "Player", dsbHeadText, qfalse, qfalse, cw, ch, 16 );
	CG_DrawRect( x + colA, y, colB, rh, 1, dsbBorder );
	CG_DrawStringExt( x + colA + 3, y + 3, "   Score", dsbHeadText, qfalse, qfalse, cw, ch, 8 );
	CG_DrawRect( x + colA + colB, y, colC, rh, 1, dsbBorder );
	CG_DrawStringExt( x + colA + colB + 3, y + 3, "Ping", dsbHeadText, qfalse, qfalse, cw, ch, 4 );
	y += rh;

	for ( i = 0; i < cg.numScores; i++ ) {
		const score_t		*sc = &cg.scores[i];
		const clientInfo_t	*ci = &cgs.clientinfo[sc->client];
		float				ix;
		qboolean			followed = qfalse;

		for ( j = 0; j < cg.numScores; j++ ) {
			if ( cg.scores[j].scoreFlags == sc->client ) {
				followed = qtrue;
				break;
			}
		}
		CG_FillRect( x, y, W, rh, sc->client == self ? dsbOwn : dsbRow );
		CG_DrawRect( x, y, colA, rh, 1, dsbBorder );
		CG_DrawStringExt( x + 3, y + 3, ci->name, dsbWhite, qfalse, qfalse, cw, ch, 16 );
		if ( sc->ping == -1 ) {
			CG_DrawRect( x + colA, y, W - colA, rh, 1, dsbBorder );
			CG_DrawStringExt( x + colA + 3, y + 3, "Connecting...", dsbWhite, qfalse, qfalse, cw, ch, 0 );
		} else {
			ix = x - ch - 3;
			trap_R_SetColor( NULL );
			if ( followed ) {
				CG_DrawPic( ix, y + 3, ch, ch, dsbCam );
				ix -= ch + 3;
			} else if ( ci->team == TEAM_SPECTATOR ) {
				CG_DrawPic( ix, y + 3, ch, ch, dsbEye );
				ix -= ch + 3;
			}
			if ( sc->client == cg.speedAwardClient ) {
				CG_DrawPic( ix, y + 3, ch, ch, dsbAward );
			}
			CG_DrawRect( x + colA, y, colB, rh, 1, dsbBorder );
			CG_DrawStringExt( x + colA + 3, y + 3, DSB_Pad( sc->score ? CG_DF_FormatTime( sc->score ) : "-", 8 ), dsbWhite, qfalse, qfalse, cw, ch, 8 );
			CG_DrawRect( x + colA + colB, y, colC, rh, 1, dsbBorder );
			Com_sprintf( buf, sizeof( buf ), "%i", sc->ping );
			CG_DrawStringExt( x + colA + colB + 3, y + 3, DSB_Pad( ci->team != TEAM_SPECTATOR ? buf : "-", 4 ), dsbWhite, qfalse, qfalse, cw, ch, 4 );
		}
		y += rh;
	}

	if ( wide ) {
		y = 68;
		x = 320 + ( df_hud_scoreboard_size.integer ? 10 : 18 ) / 2;
	} else {
		y += rh;
	}

	if ( anyFollowed && df_hud_scoreboard_showSpectators.integer ) {
		CG_FillRect( x, y, W, rh, dsbHead );
		CG_DrawRect( x, y, W, rh, 1, dsbBorder );
		CG_DrawStringExt( x + 3, y + 3, "Spectators Info", dsbHeadText, qfalse, qfalse, cw, ch, 0 );
		y += rh;
		for ( i = 0; i < cg.numScores; i++ ) {
			const score_t		*sc = &cg.scores[i];
			const clientInfo_t	*ci = &cgs.clientinfo[sc->client];
			int					len;

			if ( ci->team != TEAM_SPECTATOR || sc->scoreFlags == -1 ) {
				continue;
			}
			CG_FillRect( x, y, W, rh, dsbRow );
			CG_DrawRect( x, y, W, rh, 1, dsbBorder );
			CG_DrawStringExt( x + 3, y + 3, ci->name, dsbWhite, qfalse, qfalse, cw, ch, half );
			len = CG_DrawStrlen( ci->name );
			if ( len > half ) {
				len = half;
			}
			CG_DrawStringExt( x + 3 + len * cw, y + 3, ":", dsbWhite, qfalse, qfalse, cw, ch, half );
			CG_DrawStringExt( x + W - 3 - half * cw, y + 3, cgs.clientinfo[sc->scoreFlags & 63].name, dsbWhite, qfalse, qfalse, cw, ch, half );
			y += rh;
		}
	}

	if ( cg.speedAwardUps ) {
		const char *name = cg.speedAwardClient == -1 ? cg.speedAwardName : cgs.clientinfo[cg.speedAwardClient].name;
		int len;

		y += rh;
		CG_FillRect( x, y, W, rh, dsbHead );
		CG_FillRect( x, y + rh, W, rh, dsbRow );
		CG_DrawRect( x, y, W, ( ch << 1 ) + 12, 1, dsbBorder );
		CG_DrawStringExt( x + 3, y + 3, "Speed Award", dsbHeadText, qfalse, qfalse, cw, ch, 0 );
		y += rh;
		CG_DrawStringExt( x + 3, y + 3, name, dsbWhite, qfalse, qfalse, cw, ch, 16 );
		len = CG_DrawStrlen( name );
		if ( len > 16 ) {
			len = 16;
		}
		CG_DrawStringExt( x + 3 + len * cw, y + 3, va( "^7: %i ups", cg.speedAwardUps ), dsbWhite, qfalse, qfalse, cw, ch, 0 );
	}

	// footer: map [physics], server / client versions
	{
		int ver = atoi( CG_DF_Serverinfo( "defrag_vers" ) );

		if ( ver < 1000 ) {		// 0x3c33: short versions are stored x100
			ver *= 100;
		}

		Com_sprintf( buf, sizeof( buf ), "%s [%s]", Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ),
			atoi( CG_DF_Serverinfo( "df_promode" ) ) ? "cpm" : "vq3" );
		CG_DrawStringExt( 472, 456, DSB_Pad( buf, 28 ), dsbWhite, qtrue, qfalse, 6, 8, 28 );
		CG_DrawStringExt( 520, 464, DSB_Pad( va( "sv:%i.%02i.%02i", ver / 10000, ( ver / 100 ) % 100, ver % 100 ), 20 ), dsbWhite, qtrue, qfalse, 6, 8, 20 );
		CG_DrawStringExt( 520, 472, DSB_Pad( "cl:" DF_VERSION_STRING, 20 ), dsbWhite, qtrue, qfalse, 6, 8, 20 );
	}
	return qtrue;
}
