/***
*
*       Copyright (c) 1999, Valve LLC. All rights reserved.
*       
*       This product contains software technology licensed from Id 
*       Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*       All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// MOTD.cpp
//
// for displaying a server-sent message of the day
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "kbutton.h"
#include "triangleapi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "draw_util.h"
#include "build.h"

#if XASH_WIN32 == 1 || XASH_PSVITA == 1
#define strcasestr strstr
#define strncasecmp _strnicmp
#endif

// strip HTML tags and entities so a MOTD can still be rendered as
// plain HUD text when the engine's sandboxed WebView dialog is not
// available (older engine, dialog failed, non-Android builds).
static void MOTD_StripHTML( CUtlString &text )
{
        const char *src = text.String();
        size_t n = strlen( src );
        char *out = (char *)malloc( n + 1 );
        size_t i = 0, o = 0;

        if( !out )
                return;

        while( i < n && o < n )
        {
                if( src[i] == '<' )
                {
                        const char *end = strchr( src + i, '>' );
                        const char *tag;
                        size_t taglen;

                        if( !end )
                                break; // unterminated tag, drop the rest

                        tag = src + i + 1;
                        taglen = (size_t)( end - tag );

                        // a <style>/<script> body is not text — skip the
                        // whole block, otherwise the HUD fallback prints the
                        // raw CSS as visible garbage (device screenshot)
                        if(( taglen >= 5 && !strncasecmp( tag, "style", 5 )) ||
                           ( taglen >= 6 && !strncasecmp( tag, "script", 6 )))
                        {
                                char closing[16];
                                const char *close;

                                snprintf( closing, sizeof( closing ), "</%.*s",
                                        (int)(( taglen >= 6 ) ? 6 : 5 ), tag );

                                close = strcasestr( end, closing );

                                if( close )
                                        i = (size_t)( close - src ) + strlen( closing );
                                else
                                        i = n; // unterminated block: drop the rest

                                // swallow the '>' that closes the end tag
                                if( i < n && src[i] == '>' )
                                        i++;

                                continue;
                        }

                        // block-level tags act as line breaks in the plain-text view
                        if(( taglen >= 2 && !strncasecmp( tag, "br", 2 )) ||
                           ( taglen >= 2 && !strncasecmp( tag, "/p", 2 )) ||
                           ( taglen >= 2 && !strncasecmp( tag, "p>", 2 )) ||
                           ( taglen >= 4 && !strncasecmp( tag, "/div", 4 )) ||
                           ( taglen >= 3 && !strncasecmp( tag, "/tr", 3 )) ||
                           ( taglen >= 2 && !strncasecmp( tag, "/h", 2 )))
                        {
                                if( o == 0 || out[o-1] != '\n' )
                                        out[o++] = '\n';
                        }

                        i += taglen + 2; // skip past '>'
                        continue;
                }

                if( src[i] == '&' )
                {
                        if( !strncasecmp( src + i, "&nbsp;", 6 )) { out[o++] = ' '; i += 6; continue; }
                        if( !strncasecmp( src + i, "&amp;", 5 )) { out[o++] = '&'; i += 5; continue; }
                        if( !strncasecmp( src + i, "&lt;", 4 )) { out[o++] = '<'; i += 4; continue; }
                        if( !strncasecmp( src + i, "&gt;", 4 )) { out[o++] = '>'; i += 4; continue; }
                        if( !strncasecmp( src + i, "&quot;", 6 )) { out[o++] = '"'; i += 6; continue; }
                }

                out[o++] = src[i++];
        }

        out[o] = '\0';
        text.Set( out );
        free( out );
}

int CHudMOTD :: Init( void )
{
        gHUD.AddHudElem( this );

        HOOK_MESSAGE( gHUD.m_MOTD, MOTD );

        cl_hide_motd = CVAR_CREATE("cl_hide_motd", "0", FCVAR_ARCHIVE); // hide motd
        Reset();

        return 1;
}

int CHudMOTD :: VidInit( void )
{
        // Load sprites here
        return 1;
}

void CHudMOTD :: Reset( void )
{
        m_iFlags &= ~HUD_DRAW;  // start out inactive
        m_szMOTD.Clear();
        m_iLines = 0;
        m_bShow = false;
}

#define LINE_HEIGHT  13
#define ROW_GAP  13
#define ROW_RANGE_MIN 30
#define ROW_RANGE_MAX ( ScreenHeight - 100 )

int CHudMOTD :: Draw( float fTime )
{
        gHUD.m_iNoConsolePrint &= ~( 1 << 1 );
        if( !m_bShow )
                return 1;

        if( cl_hide_motd->value )
        {
                Reset();
                return 1;
        }

        gHUD.m_iNoConsolePrint |= 1 << 1;
        // find the top of where the MOTD should be drawn,  so the whole thing is centered in the screen
        int ypos = (ScreenHeight - LINE_HEIGHT * m_iLines)/2; // shift it up slightly
        char *ch = m_szMOTD.Access();
        int xpos = (ScreenWidth - gHUD.GetCharWidth( 'M' ) * m_iMaxLength) / 2;
        if( xpos < 30 ) xpos = 30;
        int xmax = xpos + gHUD.GetCharWidth( 'M' ) * m_iMaxLength;
        int height = LINE_HEIGHT * m_iLines;
        int ypos_r=ypos;
        if( height > ROW_RANGE_MAX )
        {
                ypos = ROW_RANGE_MIN + 7 + scroll;
                if( ypos  > ROW_RANGE_MIN + 4 )
                        scroll-= (ypos - ( ROW_RANGE_MIN + 4))/3.0;
                if( ypos + height < ROW_RANGE_MAX )
                        scroll+= (ROW_RANGE_MAX - (ypos + height))/ 3.0;
                ypos_r = ROW_RANGE_MIN;
                height = ROW_RANGE_MAX;
        }
        if( xmax > ScreenWidth - 30 ) xmax = ScreenWidth - 30;
        char *next_line;
        DrawUtils::DrawRectangle(xpos-5, ypos_r - 5, xmax - xpos+10, height + 10);
        while ( *ch )
        {
                int line_length = 0;  // count the length of the current line
                for ( next_line = ch; *next_line != '\n' && *next_line != 0; next_line++ )
                        line_length += gHUD.GetCharWidth( (unsigned char)*next_line );
                char *top = next_line;
                if ( *top == '\n' )
                        *top = 0;
                else
                        top = NULL;

                // find where to start drawing the line
                if( (ypos > ROW_RANGE_MIN) && (ypos + LINE_HEIGHT <= ypos_r + height) )
                        DrawUtils::DrawHudString( xpos, ypos, xmax, ch, 255, 180, 0 );

                ypos += LINE_HEIGHT;

                if ( top )  // restore 
                        *top = '\n';
                ch = next_line;
                if ( *ch == '\n' )
                        ch++;

                if ( ypos > (ScreenHeight - 20) )
                        break;  // don't let it draw too low
        }
        
        return 1;
}

int CHudMOTD :: MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
        if( cl_hide_motd->value )
                return 1;

        if ( m_iFlags & HUD_DRAW )
        {
                Reset(); // clear the current MOTD in prep for this one
        }

        BufferReader reader( pszName, pbuf, iSize );

        int is_finished = reader.ReadByte();
        m_szMOTD.Append( reader.ReadString() );

        if ( is_finished )
        {
                // every server MOTD (HTML or plain text) opens the engine
                // dialog window, like the PC CS 1.6 MOTD window. HTML pages are
                // rendered by the sandboxed WebView; plain text is wrapped into
                // a game-styled document on the Android side. If the dialog
                // could not be shown, fall back to the classic HUD text
                // renderer with tags stripped, so the MOTD is never lost.
                if( gEngfuncs.pfnShowMOTD )
                {
                        // the MOTD window title is the SERVER NAME as of
                        // the moment the MOTD completes -- exactly like the
                        // retail CS 1.6 client. The gamedll sends
                        // ServerName(hostname) before the MOTD chunks
                        // (ReGameDLL CHalfLifeMultiplay::SendMOTDToClient) and
                        // AMXX show_motd() rewrites ServerName("<plugin title>")
                        // before the chunks and restores it after
                        // (amxmodx util.cpp UTIL_ShowMOTD), so top15/rank
                        // windows carry their own title. Passing only the html
                        // made the engine fall back to cls.servername
                        // ("localhost") for every MOTD.
                        if( gEngfuncs.pfnShowMOTD( gHUD.m_szServerName, m_szMOTD.String() ))
                        {
                                Reset();
                                return 1;
                        }

                        // make the fallback visible -- if users ever end up
                        // here, the engine log tells us the WebView dialog could
                        // not be shown (and this print marks the client side).
                        gEngfuncs.Con_Printf( "MOTD: engine dialog failed, using HUD text fallback\n" );
                        MOTD_StripHTML( m_szMOTD );
                }

                int length = 0;
                
                m_iMaxLength = 0;
                m_iFlags |= HUD_DRAW;


                for ( const char *sz = m_szMOTD.String(); *sz != 0; sz++ )  // count the number of lines in the MOTD
                {
                        if ( *sz == '\n' )
                        {
                                m_iLines++;
                                if( length > m_iMaxLength )
                                {
                                        m_iMaxLength = length;
                                        length = 0;
                                }
                        }
                        length++;
                }
                
                m_iLines++;
                if( length > m_iMaxLength )
                {
                        m_iMaxLength = length;
                        length = 0;
                }
                m_bShow = true;
        }

        return 1;
}
