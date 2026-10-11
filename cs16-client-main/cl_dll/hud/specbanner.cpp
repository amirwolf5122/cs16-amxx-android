/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology")  Id Technology (c) 1996 Id Software, Inc.
*   All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission of Valve LLC.
*
****/
//
// specbanner.cpp
//
// the DRC_CMD_BANNER picture: server ships a tga as a generic resource
// and names it through svc_director, pc client draws it top-left of the
// spectator panel. we draw it in the same corner while dead/spectating
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "triangleapi.h"

void CHudSpecBanner::SetBannerFile( const char *file )
{
	if( !file )
	{
		m_szBannerFile[0] = 0;
		return;
	}

	strncpy( m_szBannerFile, file, sizeof( m_szBannerFile ) - 1 );
	m_szBannerFile[sizeof( m_szBannerFile ) - 1] = 0;

	// printed on purpose, the console is the only way to tell a
	gEngfuncs.Con_Printf( "Spec banner: server named %s\n", m_szBannerFile );
	m_bWarnedNotFound = false;
}

int CHudSpecBanner::Init( void )
{
	// no HUD_DRAW = the redraw loop skips this element forever
	m_iFlags |= HUD_DRAW;

	gHUD.AddHudElem( this );

	m_pBanner = NULL;
	m_flNextProbe = 0.0f;
	m_szBannerFile[0] = 0;
	m_szLoadedFrom[0] = 0;
	m_bWarnedNotFound = false;

	return 1;
}

int CHudSpecBanner::VidInit( void )
{
	// probe timer rides on hud time
	m_flNextProbe = 0.0f;

	// vid init kills the map and its hud sprites, the cached banner
	m_pBanner = NULL;
	m_szLoadedFrom[0] = 0;
	m_bWarnedNotFound = false;

	return 1;
}

// fresh join must not keep the previous server's banner
void CHudSpecBanner::InitHUDData( void )
{
	m_pBanner = NULL;
	m_flNextProbe = 0.0f;
	m_szBannerFile[0] = 0;
	m_szLoadedFrom[0] = 0;
	m_bWarnedNotFound = false;
}

// tga header: 18 bytes, width at 12, height at 14 (LE u16)
static bool Banner_TgaSize( const char *path, int *w, int *h )
{
	byte *file = ( byte * )gEngfuncs.COM_LoadFile( ( char * )path, 5, NULL );

	if( !file )
		return false;

	bool ok = false;

	if( file[1] == 0 && ( file[2] == 2 || file[2] == 10 ) && ( file[16] == 24 || file[16] == 32 ))
	{
		int iw = file[12] | ( file[13] << 8 );
		int ih = file[14] | ( file[15] << 8 );

		if( iw > 0 && ih > 0 )
		{
			*w = iw;
			*h = ih;
			ok = true;
		}
	}

	gEngfuncs.COM_FreeFile( file );
	return ok;
}

int CHudSpecBanner::Draw( float flTime )
{
	// dead or spectating only, alive players never see it
	bool spectating = g_iUser1 != 0;
	int local = gHUD.m_Scoreboard.m_iPlayerNum;

	if( !spectating && local > 0 )
	{
		if( g_PlayerExtraInfo[local].dead )
			spectating = true;
		else if( g_PlayerExtraInfo[local].teamname[0]
			&& !stricmp( g_PlayerExtraInfo[local].teamname, "SPECTATOR" ))
			spectating = true;
	}

	if( !spectating && gHUD.m_Health.m_iHealth <= 0 )
		spectating = true;

	if( !spectating )
		return 0;

	if( !m_szBannerFile[0] )
		return 0;

	if( !m_pBanner || strcmp( m_szLoadedFrom, m_szBannerFile ))
	{
		if( flTime < m_flNextProbe )
			return 0;

		m_flNextProbe = flTime + 2.0f;

		// the tga comes through the resource download, so the first
		// frames after join may not have it yet
		m_pBanner = gEngfuncs.LoadMapSprite( m_szBannerFile );

		if( m_pBanner )
		{
			strncpy( m_szLoadedFrom, m_szBannerFile, sizeof( m_szLoadedFrom ) - 1 );
			m_szLoadedFrom[sizeof( m_szLoadedFrom ) - 1] = 0;
			gEngfuncs.Con_Printf( "Spec banner: showing %s\n", m_szBannerFile );
		}
		else if( !m_bWarnedNotFound )
		{
			// once per file, the probe repeats every 2s
			m_bWarnedNotFound = true;
			gEngfuncs.Con_Printf( "Spec banner: %s is not on disk yet (it may still be downloading)\n", m_szBannerFile );
		}
	}

	if( !m_pBanner )
		return 0;

	// fit the top left corner, keep the tga aspect
	float maxW = ScreenWidth * 0.28f;
	float maxH = ScreenHeight * 0.20f;
	float w = maxW, h = maxW / 4.0f;
	int iw, ih;
	int cols = 1, rows = 1;

	if( Banner_TgaSize( m_szBannerFile, &iw, &ih ))
	{
		// the engine chops map sprites into 128x128 tiles, a 256x64 tga
		// comes back as 2 frames (left/right half) - drawing frame 0
		// alone was the half banner. redraw all tiles in place
		int gw = iw - ( iw % 128 );
		int gh = ih - ( ih % 128 );

		if( gw < 128 ) gw = 128;
		if( gh < 128 ) gh = 128;

		cols = gw / 128;
		rows = gh / 128;

		w = maxW;
		h = w * ih / iw;

		if( h > maxH )
		{
			h = maxH;
			w = h * iw / ih;
		}
	}

	float x0 = 8.0f;
	float y0 = 8.0f;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// tiles go left to right then down, same order as the engine
	for( int i = 0; i < cols * rows; i++ )
	{
		int col = i % cols;
		int row = i / cols;

		if( !gEngfuncs.pTriAPI->SpriteTexture( m_pBanner, i ))
			break;

		DrawUtils::Draw2DQuad( x0 + col * w / cols, y0 + row * h / rows,
			x0 + ( col + 1 ) * w / cols, y0 + ( row + 1 ) * h / rows );
	}

	return 1;
}
