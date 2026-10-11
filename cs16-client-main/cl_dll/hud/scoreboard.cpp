/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Scoreboard.cpp
//
// implementation of CHudScoreboard class
//
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "com_weapons.h"
#include "cdll_dll.h"
#include "draw_util.h"
#include "vgui_parser.h"
#include "eventscripts.h"

hud_player_info_t   g_PlayerInfoList[MAX_PLAYERS+1]; // player info from the engine
extra_player_info_t	g_PlayerExtraInfo[MAX_PLAYERS+1]; // additional player info sent directly to the client dll
team_info_t         g_TeamInfo[MAX_TEAMS+1];
hostage_info_t      g_HostageInfo[MAX_HOSTAGES+1];
int g_iUser1;
int g_iUser2;
int g_iUser3;
int g_iTeamNumber;


// X positions

int xstart, xend;
int ystart, yend;

enum
{
	COL_NAME = 0,
	COL_ATTRIB,
	COL_HP,
	COL_MONEY,
	COL_KILLS,
	COL_DEATHS,
	COL_PING,
	TOTAL_COLUMNS
};

static struct Column
{
	int start, end;
	const char *name;

	Column() :
	    start( 0 ), end( 0 ), name( nullptr ) { }

	Column( int s, const char *n = nullptr, bool reverse = true )
	{
		name = n;
		end = 0;
		start = 0;

		if ( reverse )
		{
			start = s;
			if ( n )
				end = start - DrawUtils::HudStringLen( n );
		}
		else
		{
			start = s;
			if ( n )
				end = start + DrawUtils::HudStringLen( n );
		}
	}
} g_Columns[TOTAL_COLUMNS];

//#include "vgui_TeamFortressViewport.h"


// Y positions
#define ROW_GAP  15

// tab avatars -----------------------------------------------------------------
// the engine saves every player's steam avatar into media/avatars/ (keyed by
// the account number from *sid) and the customize menu lets you pick a
// personal one (cl_avatar). The scoreboard only draws what is on disk and
// probes again every couple of seconds while downloads are still landing.
#define AVATAR_PROBE_TIME 2.0f

static struct model_s *s_pAvatarModel[MAX_PLAYERS+1];
static float s_flAvatarNextProbe[MAX_PLAYERS+1];
static bool s_pAvatarWarned[MAX_PLAYERS+1];
static cvar_t *cl_avatar = NULL;

// mirror cl_avatar so a mid-game pick re-probes the sprites
static char s_pAvatarCvarCache[64];

// "0:905"/"0:0:905" -> account, last number = id, one before = auth bit
static bool Scoreboard_SidTailAccount( const char *s, int *account )
{
	const char *last, *num;
	char ybuf[16];
	size_t len;
	int z, y;

	last = strrchr( s, ':' );
	if( !last || last == s )
		return false;

	if( sscanf( last + 1, "%d", &z ) != 1 || z < 0 )
		return false;

	num = last;
	while( num > s && isdigit( (unsigned char)num[-1] ))
		num--;
	len = last - num;
	if( len <= 0 || len >= sizeof( ybuf ))
		return false;

	memcpy( ybuf, num, len );
	ybuf[len] = 0;
	y = atoi( ybuf );
	if( y < 0 )
		return false;

	*account = z * 2 + ( y & 1 );
	return true;
}

// reunion/dproto servers put the steamid into userinfo "*sid";
// fold whatever spelling it is to the bare account number
static bool Scoreboard_AvatarAccount( const char *sid, int *account )
{
	int y = 0;

	if( !sid || !sid[0] )
		return false;

	if( !strnicmp( sid, "STEAM_", 6 ))
	{
		return Scoreboard_SidTailAccount( sid + 6, account );
	}
	else if( sid[0] == '[' )
	{
		if( sscanf( sid, "[U:1:%d]", &y ) != 1 || y < 0 )
			return false;
		*account = y;
		return true;
	}
	else if( isdigit( (unsigned char)sid[0] ) && strchr( sid, ':' ))
	{
		return Scoreboard_SidTailAccount( sid, account );
	}
	else if( isdigit( (unsigned char)sid[0] ))
	{
		// bare steamid64 (reunion) or account number,
		unsigned long long v = strtoull( sid, NULL, 10 );

		if( v > 0xFFFFFFFFULL )
		{
			if( v <= 76561197960265728ULL )
				return false;
			v -= 76561197960265728ULL;
		}

		if( v == 0 || v > 0xFFFFFFFFULL )
			return false;

		*account = ( int )v;
		return true;
	}
	else
	{
		return false;
	}

	return *account > 0;
}

// load a picture quietly: a missing file must not print the hud
// sprite error on every probe, the picture may still be on its
// way over the network or the steam fetch may not have run yet
static struct model_s *Scoreboard_LoadAvatarSprite( const char *path )
{
	byte *test = ( byte * )gEngfuncs.COM_LoadFile(( char * )path, 5, NULL );
	struct model_s *sprite;

	if( !test )
		return NULL;

	gEngfuncs.COM_FreeFile( test );

	sprite = gEngfuncs.LoadMapSprite( path );

	return sprite;
}

static struct model_s *Scoreboard_GetAvatar( int slot, float flTime )
{
	char path[256], sid[128] = "";
	int account = 0;

	// cl_avatar can move mid-game (picker/console), drop the
	if( cl_avatar && strncmp( s_pAvatarCvarCache, cl_avatar->string,
		sizeof( s_pAvatarCvarCache )))
	{
		int i;

		for( i = 0; i <= MAX_PLAYERS; i++ )
		{
			s_pAvatarModel[i] = NULL;
			s_pAvatarWarned[i] = false;
		}

		strncpy( s_pAvatarCvarCache, cl_avatar->string, sizeof( s_pAvatarCvarCache ) - 1 );
		s_pAvatarCvarCache[sizeof( s_pAvatarCvarCache ) - 1] = 0;
	}

	if( s_pAvatarModel[slot] )
		return s_pAvatarModel[slot];

	if( flTime < s_flAvatarNextProbe[slot] )
		return NULL;

	s_flAvatarNextProbe[slot] = flTime + AVATAR_PROBE_TIME;

	strncpy( sid, gEngfuncs.PlayerInfo_ValueForKey( slot, "*sid" ), sizeof( sid ) - 1 );
	sid[sizeof( sid ) - 1] = 0;

	if( g_PlayerInfoList[slot].thisplayer )
	{
		// my row: the menu pick wins over the steam photo, path-like
		// values count as a direct file
		if( cl_avatar && cl_avatar->string[0]
			&& stricmp( cl_avatar->string, "(none)" ))
		{
			if( strchr( cl_avatar->string, '/' ) || strchr( cl_avatar->string, '\\' ))
			{
				snprintf( path, sizeof( path ), "%s", cl_avatar->string );
				s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( path );
			}
			else
			{
				// picker saves png but a manual jpg should
				static const char *exts[] = { "png", "bmp", "jpg", "tga" };
				int e;

				for( e = 0; e < 4 && !s_pAvatarModel[slot]; e++ )
				{
					snprintf( path, sizeof( path ), "media/avatars/%s.%s",
						cl_avatar->string, exts[e] );
					s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( path );
				}
			}
		}

		// the pick follows the player, the picker copies it into every game
		if( !s_pAvatarModel[slot] )
		{
			static const char *mine[] = { "media/avatars/avatar_custom.png",
				"media/avatars/avatar_custom.tga" };
			int m;

			for( m = 0; m < 2 && !s_pAvatarModel[slot]; m++ )
				s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( mine[m] );
		}

		// steam photo next, still no default mask here
		if( !s_pAvatarModel[slot] && Scoreboard_AvatarAccount( sid, &account ))
		{
			snprintf( path, sizeof( path ), "media/avatars/av_%d.png", account );
			s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( path );
		}
	}
	else
	{
		// other rows: own steam photo only, no photo on disk
		// = no draw, no default mask
		if( Scoreboard_AvatarAccount( sid, &account ))
		{
			snprintf( path, sizeof( path ), "media/avatars/av_%d.png", account );
			s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( path );

			// same picture as tga when the png never landed
			if( !s_pAvatarModel[slot] )
			{
				snprintf( path, sizeof( path ), "media/avatars/av_%d.tga", account );
				s_pAvatarModel[slot] = Scoreboard_LoadAvatarSprite( path );
			}
		}
	}

	// one log line per slot and connect, missing vs broken load
	if( !s_pAvatarModel[slot] && !s_pAvatarWarned[slot] )
	{
		s_pAvatarWarned[slot] = true;
		gEngfuncs.Con_DPrintf( "Avatar: no picture for slot %d (sid '%s', cl_avatar '%s')\n",
			slot, sid, cl_avatar ? cl_avatar->string : "" );
	}

	return s_pAvatarModel[slot];
}

static void Scoreboard_AvatarsInit( void )
{
	cl_avatar = CVAR_CREATE( "cl_avatar", "", FCVAR_ARCHIVE );
	memset( s_pAvatarModel, 0, sizeof( s_pAvatarModel ));
	memset( s_flAvatarNextProbe, 0, sizeof( s_flAvatarNextProbe ));
	memset( s_pAvatarWarned, 0, sizeof( s_pAvatarWarned ));
}

static void Scoreboard_AvatarsReset( void )
{
	memset( s_pAvatarModel, 0, sizeof( s_pAvatarModel ));
	memset( s_flAvatarNextProbe, 0, sizeof( s_flAvatarNextProbe ));
	memset( s_pAvatarWarned, 0, sizeof( s_pAvatarWarned ));
}

// draws the avatar left of the name and returns the shifted name x
static int Scoreboard_DrawAvatar( int slot, int x, int ypos )
{
	struct model_s *avatar = Scoreboard_GetAvatar( slot, gHUD.m_flTime );
	int asz = ROW_GAP; // one row tall, the row pitch caps how far it grows

	if( !avatar )
		return x;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	gEngfuncs.pTriAPI->SpriteTexture( avatar, 0 );

	// TriAPI talks raw pixels while x/ypos are hud coords, scale
	// them like the radar does
	float s = gHUD.m_flScale;

	DrawUtils::Draw2DQuad( x * s, ypos * s, ( x + asz ) * s, ( ypos + asz ) * s );

	return x + asz + 4;
}

int CHudScoreboard :: Init( void )
{
	Scoreboard_AvatarsInit();

	gHUD.AddHudElem( this );

	// Hook messages & commands here
	HOOK_COMMAND( gHUD.m_Scoreboard, "+showscores", ShowScores );
	HOOK_COMMAND( gHUD.m_Scoreboard, "-showscores", HideScores );
	HOOK_COMMAND( gHUD.m_Scoreboard, "showscoreboard2", ShowScoreboard2 );
	HOOK_COMMAND( gHUD.m_Scoreboard, "hidescoreboard2", HideScoreboard2 );

	HOOK_MESSAGE( gHUD.m_Scoreboard, ScoreInfo );
	HOOK_MESSAGE( gHUD.m_Scoreboard, TeamScore );
	HOOK_MESSAGE( gHUD.m_Scoreboard, TeamInfo );

	InitHUDData();

	cl_showpacketloss = CVAR_CREATE( "cl_showpacketloss", "0", FCVAR_ARCHIVE );
	cl_showplayerversion = CVAR_CREATE( "cl_showplayerversion", "0", 0 );
	cl_show_scoreboard_on_death = CVAR_CREATE( "cl_show_scoreboard_on_death", "0", FCVAR_ARCHIVE );

	return 1;
}


int CHudScoreboard :: VidInit( void )
{
	xstart = ScreenWidth * 0.125f;
	xend = ScreenWidth - xstart;
	ystart = 100;
	yend = ScreenHeight - ystart;
	m_bForceDraw = false;

	// Load sprites here
	return 1;
}

void CHudScoreboard :: InitHUDData( void )
{
	Scoreboard_AvatarsReset();

	memset( g_PlayerExtraInfo, 0, sizeof g_PlayerExtraInfo );
	m_iLastKilledBy = 0;
	m_fLastKillTime = 0;
	m_iPlayerNum = 0;
	m_iNumTeams = 0;
	memset( g_TeamInfo, 0, sizeof g_TeamInfo );

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		// a1ba: get the cl.playernum from the engine
		// it shouldn't ever change during normal gameplay
		if( !m_iPlayerNum && EV_IsLocal( i ))
			m_iPlayerNum = i;

		g_PlayerExtraInfo[i].sb_health = -1;
		g_PlayerExtraInfo[i].sb_account = -1;
	}

	m_iFlags &= ~HUD_DRAW;  // starts out inactive

	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission
}

bool CHudScoreboard :: ShouldDrawScoreboard() const
{
	if( m_bForceDraw )
		return true;

	if( m_bShowscoresHeld || gHUD.m_iIntermission )
		return true;

	if( cl_show_scoreboard_on_death && cl_show_scoreboard_on_death->value && gHUD.m_Health.m_iHealth <= 0 )
		return true;

	return false;
}

// Y positions
int CHudScoreboard :: Draw( float flTime )
{
	if( !ShouldDrawScoreboard( ))
		return 1;

	if( !m_bForceDraw )
	{
		xstart     = 0.125f * ScreenWidth;
		xend       = ScreenWidth - xstart;
		ystart     = 90;
		yend       = ScreenHeight - ystart;
		m_colors.r = 0;
		m_colors.g = 0;
		m_colors.b = 0;
		m_colors.a = 153;
		m_bDrawStroke = true;
	}

	return DrawScoreboard(flTime);
}

int CHudScoreboard :: DrawScoreboard( float fTime )
{
	GetAllPlayersInfo();
	char ServerName[90];

//	Packetloss removed on Kelly 'shipping nazi' Bailey's orders
//	if ( cl_showpacketloss && cl_showpacketloss->value && ( ScreenWidth >= 400 ) )
//	{
//		can_show_packetloss = 1;
//	}

	// just sort the list on the fly
	// list is sorted first by frags, then by deaths
	float list_slot = 0;

	// calculate columns sizes
	g_Columns[COL_PING] = Column( xend - 15, Localize( "#PlayerPing" ) );
	g_Columns[COL_PING].end = min( g_Columns[COL_PING].end, g_Columns[COL_PING].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_DEATHS] = Column( g_Columns[COL_PING].end - 10, Localize( "#PlayerDeath" ) );
	g_Columns[COL_DEATHS].end = min( g_Columns[COL_DEATHS].end, g_Columns[COL_DEATHS].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_KILLS] = Column( g_Columns[COL_DEATHS].end - 10, Localize( "#PlayerScore" ) );
	g_Columns[COL_KILLS].end = min( g_Columns[COL_KILLS].end, g_Columns[COL_KILLS].start - DrawUtils::HudStringLen( "9999" ) );

	g_Columns[COL_MONEY] = Column( g_Columns[COL_KILLS].end - 10, Localize( "#Cstrike_ACCOUNT" ) );
	g_Columns[COL_MONEY].end = min( g_Columns[COL_MONEY].end, g_Columns[COL_MONEY].start - DrawUtils::HudStringLen( "$999999" ) );

	g_Columns[COL_HP] = Column( g_Columns[COL_MONEY].end - 10, Localize( "#Cstrike_HEALTH" ) );
	g_Columns[COL_HP].end = min( g_Columns[COL_HP].end, g_Columns[COL_HP].start - DrawUtils::HudStringLen( "999999" ) );

	g_Columns[COL_ATTRIB] = Column( g_Columns[COL_HP].end - 10 );
	g_Columns[COL_ATTRIB].end = g_Columns[COL_ATTRIB].start - DrawUtils::HudStringLen( "#Cstrike_DEFUSE_KIT" );

	g_Columns[COL_NAME] = Column( xstart + 15, nullptr, false );
	g_Columns[COL_NAME].end = g_Columns[COL_ATTRIB].end - 10;

	// print the heading line

	DrawUtils::DrawRectangle(xstart, ystart, xend - xstart, yend - ystart,
		m_colors.r, m_colors.g, m_colors.b, m_colors.a, m_bDrawStroke);

	int ypos = ystart + (list_slot * ROW_GAP) + 5;

	if( gHUD.m_szServerName[0] )
		// snprintf( ServerName, 80, "%s", (char*)(gHUD.m_Teamplay ? "TEAMS" : "PLAYERS"), gHUD.m_szServerName );
		strncpy( ServerName, gHUD.m_szServerName, 80 );
	else
		strncpy( ServerName, gHUD.m_Teamplay ? "TEAMS" : "PLAYERS", 80 );

	DrawUtils::DrawHudString( g_Columns[COL_NAME].start, ypos, g_Columns[COL_NAME].end, ServerName, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_HP].start, ypos, g_Columns[COL_HP].end, g_Columns[COL_HP].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_MONEY].start, ypos, g_Columns[COL_MONEY].end, g_Columns[COL_MONEY].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, g_Columns[COL_KILLS].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_DEATHS].start, ypos, g_Columns[COL_DEATHS].end, g_Columns[COL_DEATHS].name, 255, 140, 0 );
	DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, g_Columns[COL_PING].name, 255, 140, 0 );

	list_slot += 2;
	ypos = ystart + (list_slot * ROW_GAP);
	FillRGBA( xstart, ypos, xend - xstart, 1, 255, 140, 0, 255);  // draw the separator line

	list_slot += 0.8;

	if ( gHUD.m_Teamplay )
	{
		DrawTeams( list_slot );
	}
	else
	{
		// it's not teamplay,  so just draw a simple player list
		DrawPlayers( list_slot );
	}
	return 1;
}

int CHudScoreboard :: DrawTeams( float list_slot )
{
	int j;
	int ypos = ystart + (list_slot * ROW_GAP) + 5;

	// clear out team scores
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !g_TeamInfo[i].scores_overriden )
			g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
		g_TeamInfo[i].sumping = 0;
		g_TeamInfo[i].players = 0;
		g_TeamInfo[i].already_drawn = FALSE;
	}

	// recalc the team scores, then draw them
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
			continue; // empty player slot, skip

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// find what team this player is in
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )  // player is not in a team, skip to the next guy
			continue;

		if ( !g_TeamInfo[j].scores_overriden )
		{
			g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
			g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
		}

		g_TeamInfo[j].sumping += g_PlayerInfoList[i].ping;

		if ( g_PlayerInfoList[i].thisplayer )
			g_TeamInfo[j].ownteam = TRUE;
		else
			g_TeamInfo[j].ownteam = FALSE;

		g_TeamInfo[j].players++;
	}

	// Draw the teams
	int iSpectatorPos = -1;

	while( true )
	{
		int highest_frags = -99999; int lowest_deaths = 99999;
		int best_team = 0;

		for ( int i = 1; i <= m_iNumTeams; i++ )
		{
			// don't draw team without players
			if ( g_TeamInfo[i].players <= 0 )
				continue;

			if (!strnicmp(g_TeamInfo[i].name, "SPECTATOR", MAX_TEAM_NAME))
			{
				iSpectatorPos = i;
				continue;
			}

			if ( !g_TeamInfo[i].already_drawn && g_TeamInfo[i].frags >= highest_frags )
			{
				if ( g_TeamInfo[i].frags > highest_frags || g_TeamInfo[i].deaths < lowest_deaths )
				{
					best_team = i;
					lowest_deaths = g_TeamInfo[i].deaths;
					highest_frags = g_TeamInfo[i].frags;
				}
			}
		}

		// draw the best team on the scoreboard
		if ( !best_team )
		{
			// if spectators is found and still not drawn
			if( iSpectatorPos != -1 && g_TeamInfo[iSpectatorPos].already_drawn == FALSE )
				best_team = iSpectatorPos;
			else break;
		}
		// draw out the best team
		team_info_t *team_info = &g_TeamInfo[best_team];

		// don't draw team without players
		if ( team_info->players <= 0 )
			continue;

		ypos = ystart + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > yend )  // don't draw to close to the lower border
			break;

		int r, g, b;
		char teamName[64];

		char numPlayers[16];
		sprintf( numPlayers, "%d", team_info->players );

		char fmtString[32];
		const char *fmtStringName = team_info->players == 1 ? "#Cstrike_ScoreBoard_Player" : "#Cstrike_ScoreBoard_Players";
		strncpy( fmtString, Localize( fmtStringName ), sizeof( fmtString ) );
		fmtString[sizeof( fmtString ) - 1] = 0;

		if ( !strcmp( fmtString, fmtStringName ) )
		{
			const char *fallback = team_info->players == 1 ? "%s1    -   %s2 player" : "%s1    -   %s2 players";
			strncpy( fmtString, fallback, sizeof( fmtString ) );
			fmtString[sizeof( fmtString ) - 1] = 0;
		}

		GetTeamColor( r, g, b, team_info->teamnumber );
		switch ( team_info->teamnumber )
		{
		case TEAM_TERRORIST:
		{
			const char *args[2] = { Localize( "#Cstrike_ScoreBoard_Ter" ), numPlayers };
			Localize_Format( teamName, sizeof( teamName ), fmtString, args, 2 );
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, team_info->frags, r, g, b );
			break;
		}
		case TEAM_CT:
		{
			const char *args[2] = { Localize( "#Cstrike_ScoreBoard_CT" ), numPlayers };
			Localize_Format( teamName, sizeof( teamName ), fmtString, args, 2 );
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, team_info->frags, r, g, b );
			break;
		}
		case TEAM_SPECTATOR:
		case TEAM_UNASSIGNED:
			strncpy( teamName, Localize( "#Spectators" ), sizeof( teamName ) );
			break;
		}

		DrawUtils::DrawHudString( g_Columns[COL_NAME].start, ypos, g_Columns[COL_NAME].end, teamName, r, g, b );
		DrawUtils::DrawHudNumberString( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, team_info->sumping / team_info->players, r, g, b );

		team_info->already_drawn = TRUE;  // set the already_drawn to be TRUE, so this team won't get drawn again

		// draw underline
		list_slot += 1.2f;
		FillRGBA( xstart, ystart + (list_slot * ROW_GAP), xend - xstart, 1, r, g, b, 255);

		list_slot += 0.4f;
		// draw all the players that belong to this team, indented slightly
		list_slot = DrawPlayers( list_slot, 10, team_info->name );
	}

	// draw all the players who are not in a team
	list_slot += 4.0f;
	DrawPlayers( list_slot, 0, "" );

	return 1;
}

// returns the ypos where it finishes drawing
int CHudScoreboard :: DrawPlayers( float list_slot, int nameoffset, const char *team )
{
	// draw the players, in order,  and restricted to team if set
	while ( 1 )
	{
		// Find the top ranking player
		int highest_frags = -99999;	int lowest_deaths = 99999;
		int best_player = 0;

		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			if ( g_PlayerInfoList[i].name && g_PlayerExtraInfo[i].frags >= highest_frags )
			{
				if ( !(team && stricmp(g_PlayerExtraInfo[i].teamname, team)) )  // make sure it is the specified team
				{
					extra_player_info_t *pl_info = &g_PlayerExtraInfo[i];
					if ( pl_info->frags > highest_frags || pl_info->deaths < lowest_deaths )
					{
						best_player = i;
						lowest_deaths = pl_info->deaths;
						highest_frags = pl_info->frags;
					}
				}
			}
		}

		if ( !best_player )
			break;

		// draw out the best player
		hud_player_info_t *pl_info = &g_PlayerInfoList[best_player];

		int ypos = ystart + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > yend )  // don't draw to close to the lower border
			break;

		int r = 255, g = 255, b = 255;
		float *colors = GetClientColor( best_player );
		r *= colors[0];
		g *= colors[1];
		b *= colors[2];

		if(pl_info->thisplayer) // hey, it's me!
		{
			FillRGBABlend( xstart, ypos, xend - xstart, ROW_GAP, 255, 255, 255, 15 );
		}

		{
			int nameX = Scoreboard_DrawAvatar( best_player, g_Columns[COL_NAME].start + nameoffset, ypos );

			DrawUtils::DrawHudString( nameX, ypos, g_Columns[COL_NAME].start + 350, pl_info->name, r, g, b );
		}

		if( cl_showplayerversion->value == 0.0f )
		{
			if( team && stricmp( team, "SPECTATOR" ))
			{
				// draw bomb( if player have the bomb )
				if( g_PlayerExtraInfo[best_player].dead )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_DEAD" ), r, g, b );
				else if( g_PlayerExtraInfo[best_player].has_c4 )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_BOMB" ), r, g, b );
				else if( g_PlayerExtraInfo[best_player].vip )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_VIP" ),  r, g, b );
				else if (g_PlayerExtraInfo[best_player].has_defuse_kit )
					DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, Localize( "#Cstrike_DEFUSE_KIT" ),  r, g, b );
			}
		}
		else
		{
			DrawUtils::DrawHudStringReverse( g_Columns[COL_ATTRIB].start, ypos, g_Columns[COL_ATTRIB].end, gEngfuncs.PlayerInfo_ValueForKey( best_player, "cscl_ver" ),  r, g, b );
		}

		if ( g_PlayerExtraInfo[best_player].sb_health >= 0 && !g_PlayerExtraInfo[best_player].dead )
		{
			if ( gHUD.m_pShowHealth->value )
			{
				static char buf[64];
				sprintf( buf, "%d", g_PlayerExtraInfo[best_player].sb_health );
				DrawUtils::DrawHudStringReverse( g_Columns[COL_HP].start, ypos, g_Columns[COL_HP].end, buf, r, g, b );
			}
		}

		if ( g_PlayerExtraInfo[best_player].sb_account >= 0 )
		{
			if ( gHUD.m_pShowMoney->value )
			{
				static char buf[64];
				sprintf( buf, "$%d", g_PlayerExtraInfo[best_player].sb_account );
				DrawUtils::DrawHudStringReverse( g_Columns[COL_MONEY].start, ypos, g_Columns[COL_MONEY].end, buf, r, g, b );
			}
		}

		// draw kills (right to left)
		if( team && stricmp( team, "SPECTATOR" ) )
		{
			DrawUtils::DrawHudNumberString( g_Columns[COL_KILLS].start, ypos, g_Columns[COL_KILLS].end, g_PlayerExtraInfo[best_player].frags, r, g, b );

			// draw deaths
			DrawUtils::DrawHudNumberString( g_Columns[COL_DEATHS].start, ypos, g_Columns[COL_DEATHS].end, g_PlayerExtraInfo[best_player].deaths, r, g, b );
		}

		// draw ping & packetloss
		const char *value;
		if( pl_info->ping <= 5  // must be 0, until Xash's bug not fixed
			&& ( value = gEngfuncs.PlayerInfo_ValueForKey( best_player, "*bot" ) )
			&& atoi( value ) > 0 )
		{
			DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, "BOT", r, g, b );
		}
		else
		{
			static char buf[64];
			sprintf( buf, "%d", pl_info->ping );
			DrawUtils::DrawHudStringReverse( g_Columns[COL_PING].start, ypos, g_Columns[COL_PING].end, buf, r, g, b );
		}

		pl_info->name = NULL;  // set the name to be NULL, so this client won't get drawn again
		list_slot++;
	}

	list_slot += 2.0f;

	return list_slot;
}


void CHudScoreboard :: GetAllPlayersInfo( void )
{
	memset( &g_PlayerInfoList[0], 0, sizeof( g_PlayerInfoList[0] ));

	for( int i = 1; i < MAX_PLAYERS; i++ )
		GetPlayerInfo( i, &g_PlayerInfoList[i] );
}

int CHudScoreboard :: MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_DRAW;

	BufferReader reader( pszName, pbuf, iSize );
	short cl = reader.ReadByte();
	short frags = reader.ReadShort();
	short deaths = reader.ReadShort();
	short playerclass = reader.ReadShort();
	reader.ReadShort();

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		g_PlayerExtraInfo[cl].playerclass = playerclass;

		//gViewPort->UpdateOnPlayerInfo();
	}

	return 1;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
int CHudScoreboard :: MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	short cl = reader.ReadByte();
	int teamNumber = 0;

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		// set the players team
		char teamName[MAX_TEAM_NAME];
		strncpy( teamName, reader.ReadString(), MAX_TEAM_NAME );
		teamName[MAX_TEAM_NAME-1] = 0;

		if( !strcmp( teamName, "TERRORIST") )
			teamNumber = TEAM_TERRORIST;
		else if( !strcmp( teamName, "CT") )
			teamNumber = TEAM_CT;
		else if( !strcmp( teamName, "SPECTATOR" ) )
		{
			teamNumber = TEAM_SPECTATOR;
		}
		else if( !strcmp( teamName, "UNASSIGNED" ) )
		{
			teamNumber = TEAM_UNASSIGNED;
			strncpy( teamName, "SPECTATOR", MAX_TEAM_NAME );
		}
		// just in case
		else teamNumber = TEAM_UNASSIGNED;

		strncpy( g_PlayerExtraInfo[cl].teamname, teamName, MAX_TEAM_NAME );
		g_PlayerExtraInfo[cl].teamnumber = teamNumber;
	}

	// rebuild the list of teams

	// clear out player counts from teams
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	GetAllPlayersInfo();
	m_iNumTeams = 0;

	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		int j;
		//if ( g_PlayerInfoList[i].name == NULL )
		//	continue;

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// is this player in an existing team?
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( g_TeamInfo[j].name[0] == '\0' )
				break;

			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )
		{
			// they aren't in a listed team, so make a new one
			for ( j = 1; j <= m_iNumTeams; j++ )
			{
				if ( g_TeamInfo[j].name[0] == '\0' )
					break;
			}


			m_iNumTeams = max( j, m_iNumTeams );

			strncpy( g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME );
			g_TeamInfo[j].teamnumber = g_PlayerExtraInfo[i].teamnumber;
			g_TeamInfo[j].players = 0;
		}

		g_TeamInfo[j].players++;
	}

	// clear out any empty teams
	for ( int i = 1; i <= m_iNumTeams; i++ )
	{
		if ( g_TeamInfo[i].players < 1 )
			memset( &g_TeamInfo[i], 0, sizeof(team_info_t) );
	}

	return 1;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudScoreboard :: MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	char *TeamName = reader.ReadString();
	int i;

	// find the team matching the name
	for ( i = 0; i < m_iNumTeams; i++ )
	{
		if ( !stricmp( TeamName, g_TeamInfo[i].name ) )
			break;
	}
	if ( i > m_iNumTeams )
	{
		reader.Flush();
		return 1;
	}

	// use this new score data instead of combined player scores
	g_TeamInfo[i].scores_overriden = TRUE;
	g_TeamInfo[i].frags = reader.ReadShort();
	// g_TeamInfo[i].deaths = reader.ReadShort();

	return 1;
}

void CHudScoreboard :: DeathMsg( int killer, int victim )
{
	// if we were the one killed,  or the world killed us, set the scoreboard to indicate suicide
	if ( victim == m_iPlayerNum || killer == 0 )
	{
		m_iLastKilledBy = killer ? killer : m_iPlayerNum;
		m_fLastKillTime = gHUD.m_flTime + 10;	// display who we were killed by for 10 seconds

		if ( killer == m_iPlayerNum )
			m_iLastKilledBy = m_iPlayerNum;
	}
}



void CHudScoreboard :: UserCmd_ShowScores( void )
{
	m_bForceDraw = false;
	m_bShowscoresHeld = true;
}

void CHudScoreboard :: UserCmd_HideScores( void )
{
	m_bForceDraw = m_bShowscoresHeld = false;
}


void CHudScoreboard	:: UserCmd_ShowScoreboard2()
{
	if( gEngfuncs.Cmd_Argc() != 9 )
	{
		ConsolePrint("showscoreboard2 <xstart> <xend> <ystart> <yend> <r> <g> <b> <a>");
	}

	xstart     = atof(gEngfuncs.Cmd_Argv(1)) * ScreenWidth;
	xend       = atof(gEngfuncs.Cmd_Argv(2)) * ScreenWidth;
	ystart     = atof(gEngfuncs.Cmd_Argv(3)) * ScreenHeight;
	yend       = atof(gEngfuncs.Cmd_Argv(4)) * ScreenHeight;
	m_colors.r = atoi(gEngfuncs.Cmd_Argv(5));
	m_colors.b = atoi(gEngfuncs.Cmd_Argv(6));
	m_colors.b = atoi(gEngfuncs.Cmd_Argv(7));
	m_colors.a = atoi(gEngfuncs.Cmd_Argv(8));
	m_bDrawStroke = false;
	m_bForceDraw = true;
}

void CHudScoreboard :: UserCmd_HideScoreboard2()
{
	m_bForceDraw = m_bShowscoresHeld = false; // and disable it
}
