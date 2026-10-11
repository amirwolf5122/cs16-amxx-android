/*
android_nosdl.c - android backend
Copyright (C) 2016-2019 mittorn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "platform/platform.h"
#include "input.h"
#include "client.h"
#include "sound.h"
#include "errno.h"
#include <pthread.h>
#include <sys/prctl.h>

#include <android/log.h>
#include <jni.h>
#if XASH_SDL
#include <SDL.h>
#endif // XASH_SDL

struct jnimethods_s
{
        JNIEnv *env;
        jobject activity;
        jclass actcls;
        jmethodID loadAndroidID;
        jmethodID getAndroidID;
        jmethodID saveAndroidID;
        jmethodID showMOTD; // sandboxed HTML MOTD dialog
        jmethodID loadingShow; // CS 1.6 style loading window
        jmethodID loadingStatus; // phase text + overall bar fill
        jmethodID loadingDownload; // second bar: current file, its fill and counters
        jmethodID loadingHide;
        jmethodID avatarFetch; // steam avatar for the scoreboard
        jmethodID avatarPick; // customize avatar image picker
} jni;

// dialog visibility state so the client dll can
// hold back the team select menu until the user presses OK (like PC CS).
static qboolean g_motd_dialog_open = false;

// set from the UI thread when the player presses Cancel on the loading
// window; the engine picks it up on its own thread and disconnects
static qboolean g_loading_cancelled = false;

// set from the ui thread after the avatar picker saved the picture
// into the game's media/avatars, the engine thread picks it up and
static qboolean g_avatar_picked = false;

/*
========================
Android_LoadingShow

raise the CS 1.6 style loading window (olive VGUI panel,
segmented progress bar, Cancel). serveraddr keys the saved
server banner so reconnects show it right away.
========================
*/
void Android_LoadingShow( const char *serveraddr )
{
        jstring jstr;

        if( !jni.env || !jni.activity || !jni.loadingShow )
                return;

        jstr = (*jni.env)->NewStringUTF( jni.env, COM_StringEmpty( serveraddr ) ? "" : serveraddr );

        if( !jstr )
                return;

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.loadingShow, jstr );
        (*jni.env)->DeleteLocalRef( jni.env, jstr );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

/*
========================
Android_LoadingStatus

progress line + segmented bar fill; percent < 0 draws the bar idle.
========================
*/
void Android_LoadingStatus( const char *text, float percent )
{
        jstring jstr;

        if( !jni.env || !jni.activity || !jni.loadingStatus || COM_StringEmpty( text ))
                return;

        jstr = (*jni.env)->NewStringUTF( jni.env, text );

        if( !jstr )
                return;

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.loadingStatus, jstr, percent );
        (*jni.env)->DeleteLocalRef( jni.env, jstr );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

/*
========================
Android_LoadingDownload

the second bar of the loading window: the file the server is
sending right now, its own fill and the info lines around it.
file == NULL folds the block away, status/footer == NULL hides
those lines individually.
========================
*/
void Android_LoadingDownload( const char *file, float percent, const char *status, const char *footer )
{
        jstring jfile = NULL, jstatus = NULL, jfooter = NULL;

        if( !jni.env || !jni.activity || !jni.loadingDownload )
                return;

        if( !COM_StringEmptyOrNULL( file ))
        {
                jfile = (*jni.env)->NewStringUTF( jni.env, file );

                if( !jfile )
                        return;
        }

        if( jfile && !COM_StringEmptyOrNULL( status ))
                jstatus = (*jni.env)->NewStringUTF( jni.env, status );

        if( jfile && !COM_StringEmptyOrNULL( footer ))
                jfooter = (*jni.env)->NewStringUTF( jni.env, footer );

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.loadingDownload,
                jfile, percent, jstatus, jfooter );

        if( jfile )
                (*jni.env)->DeleteLocalRef( jni.env, jfile );
        if( jstatus )
                (*jni.env)->DeleteLocalRef( jni.env, jstatus );
        if( jfooter )
                (*jni.env)->DeleteLocalRef( jni.env, jfooter );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

/*
========================
Android_LoadingHide

window goes away, the game is about to take the screen.
========================
*/
void Android_LoadingHide( void )
{
        if( !jni.env || !jni.activity || !jni.loadingHide )
                return;

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.loadingHide );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

/*
========================
Android_AvatarFetch

steam avatar for a player, resolved and cached by the activity so
the client scoreboard can draw it from media/avatars.
========================
*/
void Android_AvatarFetch( uint64_t steamid64 )
{
        if( !jni.env || !jni.activity || !jni.avatarFetch )
                return;

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.avatarFetch, ( jlong )steamid64 );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

/*
========================
Android_AvatarPick

the customize menu asked for the image picker: the activity opens
the system chooser, saves the picked picture into
<gamedir>/media/avatars/avatar_custom.png and flags the result
back through Android_AvatarPickResult.
========================
*/
void Android_AvatarPick( void )
{
        if( !jni.env || !jni.activity || !jni.avatarPick )
                return;

        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.avatarPick );

        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
}

qboolean Android_AvatarPickResult( void )
{
        qboolean picked = g_avatar_picked;

        g_avatar_picked = false;
        return picked;
}

qboolean Android_LoadingCancelled( void )
{
        qboolean cancelled = g_loading_cancelled;

        g_loading_cancelled = false;
        return cancelled;
}

void Android_Init( void )
{
        memset( &jni, 0, sizeof( jni ));

#if XASH_SDL
        jni.env = (JNIEnv *)SDL_AndroidGetJNIEnv();
        jni.activity = (jobject)SDL_AndroidGetActivity();
        jni.actcls = (*jni.env)->GetObjectClass( jni.env, jni.activity );
        jni.loadAndroidID = (*jni.env)->GetMethodID( jni.env, jni.actcls, "loadAndroidID", "()Ljava/lang/String;" );
        jni.getAndroidID = (*jni.env)->GetMethodID( jni.env, jni.actcls, "getAndroidID", "()Ljava/lang/String;" );
        jni.saveAndroidID = (*jni.env)->GetMethodID( jni.env, jni.actcls, "saveAndroidID", "(Ljava/lang/String;)V" );
        // (title, html) both as raw byte arrays, so a malformed
        // server string can never abort NewStringUTF
        jni.showMOTD = (*jni.env)->GetMethodID( jni.env, jni.actcls, "showMOTD", "([B[B)Z" );
        jni.loadingShow = (*jni.env)->GetMethodID( jni.env, jni.actcls, "loadingShow", "(Ljava/lang/String;)V" );
        jni.loadingStatus = (*jni.env)->GetMethodID( jni.env, jni.actcls, "loadingStatus", "(Ljava/lang/String;F)V" );
        jni.loadingDownload = (*jni.env)->GetMethodID( jni.env, jni.actcls, "loadingDownload", "(Ljava/lang/String;FLjava/lang/String;Ljava/lang/String;)V" );
        jni.loadingHide = (*jni.env)->GetMethodID( jni.env, jni.actcls, "loadingHide", "()V" );
        jni.avatarFetch = (*jni.env)->GetMethodID( jni.env, jni.actcls, "avatarFetch", "(J)V" );
        jni.avatarPick = (*jni.env)->GetMethodID( jni.env, jni.actcls, "avatarPick", "()V" );
        // a failed lookup leaves a pending exception; clear it so
        // nothing downstream (filesystem assets, SDL) trips over it
        if( (*jni.env)->ExceptionCheck( jni.env ))
                (*jni.env)->ExceptionClear( jni.env );
#endif // !XASH_SDL
}

/*
========================
Android_GetNativeObject
========================
*/

void *Android_GetNativeObject( const char *name )
{
        if( !strcasecmp( name, "JNIEnv" ) )
        {
                return (void *)jni.env;
        }
        else if( !strcasecmp( name, "ActivityClass" ) )
        {
                return (void *)jni.actcls;
        }

        return NULL;
}

/*
========================
Android_GetAndroidID
========================
*/
const char *Android_GetAndroidID( void )
{
        static char id[32];

        if( !COM_StringEmpty( id ))
                return id;

        jstring resultJNIStr = (*jni.env)->CallObjectMethod( jni.env, jni.activity, jni.getAndroidID );
        const char *resultCStr = (*jni.env)->GetStringUTFChars( jni.env, resultJNIStr, NULL );
        Q_strncpy( id, resultCStr, sizeof( id ) );
        (*jni.env)->ReleaseStringUTFChars( jni.env, resultJNIStr, resultCStr );
        (*jni.env)->DeleteLocalRef( jni.env, resultJNIStr );

        return id;
}

/*
========================
Android_LoadID
========================
*/
const char *Android_LoadID( void )
{
        static char id[32];
        jstring resultJNIStr = (*jni.env)->CallObjectMethod( jni.env, jni.activity, jni.loadAndroidID );
        const char *resultCStr = (*jni.env)->GetStringUTFChars( jni.env, resultJNIStr, NULL );
        Q_strncpy( id, resultCStr, sizeof( id ) );
        (*jni.env)->ReleaseStringUTFChars( jni.env, resultJNIStr, resultCStr );
        (*jni.env)->DeleteLocalRef( jni.env, resultJNIStr );

        return id;
}

/*
========================
Android_ShowMOTD

render an HTML MOTD in a sandboxed WebView dialog
managed by the activity. The payload is passed as a raw byte array so a
malformed (non-modified-UTF-8) server string can't abort in NewStringUTF;
Java decodes it as UTF-8 with replacement characters.
takes the window title (server name) as a second argument, exactly
like the original HL1 VGUI MOTD window (vgui_MOTDWindow.cpp).
========================
*/
qboolean Android_ShowMOTD( const char *title, const char *html )
{
        size_t len;
        jbyteArray jbytes, jtitle;
        jboolean shown;

        if( !jni.env || !jni.activity || !jni.showMOTD )
        {
                // silent false here looked exactly like
                // a broken MOTD on user devices ("MOTD dialog failed" with no
                // reason). The most common cause: an outdated engine app whose
                // XashActivity predates the showMOTD Java method.
                Con_Printf( S_WARN "Android_ShowMOTD: JNI not ready (env=%p activity=%p showMOTD=%p) - engine app too old for HTML MOTD dialogs?\n",
                        jni.env, jni.activity, jni.showMOTD );
                return false;
        }

        // the window title (server name, like the original HL1
        // VGUI MOTD window) is passed as a second raw byte array
        len = Q_strlen( title );
        jtitle = (*jni.env)->NewByteArray( jni.env, (jsize)len );

        if( !jtitle )
        {
                Con_Printf( S_WARN "Android_ShowMOTD: NewByteArray(title) failed\n" );
                return false;
        }

        (*jni.env)->SetByteArrayRegion( jni.env, jtitle, 0, (jsize)len, (const jbyte *)title );

        len = Q_strlen( html );
        jbytes = (*jni.env)->NewByteArray( jni.env, (jsize)len );

        if( !jbytes )
        {
                (*jni.env)->DeleteLocalRef( jni.env, jtitle );
                Con_Printf( S_WARN "Android_ShowMOTD: NewByteArray(html) failed\n" );
                return false;
        }

        (*jni.env)->SetByteArrayRegion( jni.env, jbytes, 0, (jsize)len, (const jbyte *)html );
        shown = (*jni.env)->CallBooleanMethod( jni.env, jni.activity, jni.showMOTD, jtitle, jbytes );
        (*jni.env)->DeleteLocalRef( jni.env, jbytes );
        (*jni.env)->DeleteLocalRef( jni.env, jtitle );

        if( shown )
                g_motd_dialog_open = true;

        // showMOTD catches Throwable internally, but never leave a
        // pending exception behind just in case
        if( (*jni.env)->ExceptionCheck( jni.env ))
        {
                (*jni.env)->ExceptionClear( jni.env );
                Con_Printf( S_WARN "Android_ShowMOTD: pending JNI exception cleared (dialog shown=%d)\n", shown );
                // the dialog may already be on screen; trust the Java
                // answer instead of discarding it
                return shown ? true : false;
        }

        return shown ? true : false;
}

qboolean Android_IsMOTDDialogOpen( void )
{
        return g_motd_dialog_open;
}

void Android_MOTDDialogClosed( void )
{
        g_motd_dialog_open = false;
}

// called by XashActivity when the MOTD dialog is dismissed (OK button,
// cancel or back button). Static native resolved by symbol lookup in
// the loaded native libraries.
JNIEXPORT void JNICALL Java_su_xash_engine_XashActivity_nativeMOTDClosed( JNIEnv *env, jclass clazz )
{
        (void)env;
        (void)clazz;
        Android_MOTDDialogClosed();
}

// Cancel button on the loading window (UI thread). Static native
// resolved by symbol lookup, like nativeMOTDClosed.
JNIEXPORT void JNICALL Java_su_xash_engine_XashActivity_nativeLoadingCancelled( JNIEnv *env, jclass clazz )
{
        (void)env;
        (void)clazz;
        g_loading_cancelled = true;
}

// avatar image picker finished (ui thread). static native,
// resolved by symbol lookup like nativeLoadingCancelled
JNIEXPORT void JNICALL Java_su_xash_engine_XashActivity_nativeAvatarPicked( JNIEnv *env, jclass clazz )
{
        (void)env;
        (void)clazz;
        g_avatar_picked = true;
}

/*
========================
Java_su_xash_engine_XashActivity_nativeConsolePrintf

let the activity surface diagnostics (MOTD dialog
failures, WebView fallbacks, ...) inside the game console, where users can
actually see and report them. Logcat-only messages were invisible.
========================
*/
JNIEXPORT void JNICALL Java_su_xash_engine_XashActivity_nativeConsolePrintf( JNIEnv *env, jclass clazz, jstring str )
{
        char buf[1024];
        const char *p;

        (void)clazz;

        if( !str )
                return;

        p = (*env)->GetStringUTFChars( env, str, NULL );

        if( !p )
                return;

        Q_strncpy( buf, p, sizeof( buf ));
        (*env)->ReleaseStringUTFChars( env, str, p );

        Con_Printf( "%s\n", buf );
}

/*
========================
Android_SaveID
========================
*/
void Android_SaveID( const char *id )
{
        jstring JStr = (*jni.env)->NewStringUTF( jni.env, id );
        (*jni.env)->CallVoidMethod( jni.env, jni.activity, jni.saveAndroidID, JStr );
        (*jni.env)->DeleteLocalRef( jni.env, JStr );
}

/*
========================
Android_ShellExecute
========================
*/
void Platform_ShellExecute( const char *path, const char *parms )
{
#if XASH_SDL
        SDL_OpenURL( path );
#endif // XASH_SDL
}
