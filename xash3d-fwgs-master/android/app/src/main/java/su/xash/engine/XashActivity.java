package su.xash.engine;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings.Secure;
import android.text.Html;
import android.text.TextUtils;
import android.text.method.ScrollingMovementMethod;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import su.xash.engine.util.CrashReports;
import su.xash.engine.util.SoftKeyboardPan;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public class XashActivity extends SDLActivity {
        private boolean mUseVolumeKeys;
        private String mPackageName;
        private static final String TAG = "XashActivity";

        // sandboxed HTML MOTD (set in getArguments())
        private String mMotdBaseDir;
        private String mMotdGameDir;
        private Dialog mMotdDialog;
        // the MOTD WebView of the currently-open dialog. Every map
        // change makes the server re-send the MOTD, so without explicit
        // destroy() the old WebViews would pile up in the native renderer
        // and eventually crash the game after several map changes.
        private WebView mMotdWebView;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
                super.onCreate(savedInstanceState);

                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        //getWindow().addFlags(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
                        getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                }

                SoftKeyboardPan.assistActivity(this);
        }

        @Override
        public void onDestroy() {
                super.onDestroy();

                // Now that we don't exit from native code, we need to exit here, resetting
                // application state (actually global variables that we don't cleanup on exit)
                //
                // When the issue with global variables will be resolved, remove that exit() call
                System.exit(0);
        }

        @Override
        protected String[] getLibraries() {
                return new String[]{"SDL2", "xash"};
        }

        @SuppressLint("HardwareIds")
        private String getAndroidID() {
                return Secure.getString(getContentResolver(), Secure.ANDROID_ID);
        }

        @SuppressLint("ApplySharedPref")
        private void saveAndroidID(String id) {
                getSharedPreferences("xash_preferences", MODE_PRIVATE).edit().putString("xash_id", id).commit();
        }

        private String loadAndroidID() {
                return getSharedPreferences("xash_preferences", MODE_PRIVATE).getString("xash_id", "");
        }

        @Override
        public String getCallingPackage() {
                if (mPackageName != null) {
                        return mPackageName;
                }

                return super.getCallingPackage();
        }

        private AssetManager getAssets(boolean isEngine) {
                AssetManager am = null;

                if (isEngine) {
                        am = getAssets();
                } else {
                        try {
                                am = getPackageManager().getResourcesForApplication(getCallingPackage()).getAssets();
                        } catch (Exception e) {
                                Log.e(TAG, "Unable to load mod assets!");
                                e.printStackTrace();
                        }
                }

                return am;
        }

        private String[] getAssetsList(boolean isEngine, String path) {
                AssetManager am = getAssets(isEngine);

                // never let a NullPointerException reach the JNI caller
                if (am == null)
                        return new String[]{};

                try {
                        String[] list = am.list(path);
                        return list != null ? list : new String[]{};
                } catch (Exception e) {
                        e.printStackTrace();
                }

                return new String[]{};
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
                if (SDLActivity.mBrokenLibraries) {
                        return false;
                }

                int keyCode = event.getKeyCode();
                if (!mUseVolumeKeys) {
                        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_CAMERA || keyCode == KeyEvent.KEYCODE_ZOOM_IN || keyCode == KeyEvent.KEYCODE_ZOOM_OUT) {
                                return false;
                        }
                }

                return getWindow().superDispatchKeyEvent(event);
        }

        private static void appendStringExtra(StringBuilder sb, Intent intent, String key) {
                String value = intent.getStringExtra(key);
                if (value != null)
                        sb.append("  ").append(key).append(" = ").append(value).append('\n');
        }

        // record intent info, so that it could be consumed later for crash reporting
        private void recordLaunchInfo() {
                // do not overwrite current launch info with pending crash log, shouldn't happen but might
                File pendingCrash = new File(getFilesDir(), "crashes/" + CrashReports.STACKTRACE_NAME);
                if (pendingCrash.exists() && pendingCrash.length() > 0)
                        return;

                // write Android version, fingerprint, supported abis, etc
                CrashReports.writeSystemInfo(this);

                // now create intent info and pass it to crash reporting
                Intent intent = getIntent();
                if (intent == null)
                        return;
                StringBuilder sb = new StringBuilder();
                sb.append("Action: ").append(intent.getAction()).append('\n');
                sb.append("Data: ").append(intent.getDataString()).append('\n');
                sb.append("Calling package: ").append(getCallingPackage()).append('\n');
                sb.append("Extras:\n");
                // only write intent extras that we care about
                appendStringExtra(sb, intent, "gamedir");
                appendStringExtra(sb, intent, "gamelibdir");
                appendStringExtra(sb, intent, "pakfile");
                appendStringExtra(sb, intent, "basedir");
                appendStringExtra(sb, intent, "package");
                appendStringExtra(sb, intent, "argv");
                sb.append("  usevolume = ").append(intent.getBooleanExtra("usevolume", false)).append('\n');
                String[] env = intent.getStringArrayExtra("env");
                if (env != null)
                        sb.append("  env = ").append(Arrays.toString(env)).append('\n');
                CrashReports.writeIntentInfo(this, sb.toString());
        }

        // TODO: REMOVE LATER, temporary launchers support?
        @Override
        protected String[] getArguments() {
                File crashDir = new File(getFilesDir(), "crashes");
                crashDir.mkdirs();
                nativeSetenv("XASH3D_CRASH_DIR", crashDir.getAbsolutePath());

                recordLaunchInfo();

                String gamedir = getIntent().getStringExtra("gamedir");
                if (gamedir == null) gamedir = "valve";
                nativeSetenv("XASH3D_GAME", gamedir);

                String gamelibdir = getIntent().getStringExtra("gamelibdir");
                if (gamelibdir == null) {
                        // v7: fall back to our own nativeLibraryDir so metamod can find
                        // bundled server libraries (e.g. libhl_android_*.so for Half-Life)
                        // when running games without an external game app.
                        gamelibdir = getApplicationInfo().nativeLibraryDir;
                }
                if (gamelibdir != null) nativeSetenv("XASH3D_GAMELIBDIR", gamelibdir);

                String rodir = System.getenv("XASH3D_RODIR");
                if (rodir == null) {
                        // FIXME: we are using rodir as a supplier for downloaded game libraries
                        rodir = getFilesDir().getAbsolutePath() + "/gamelibs";
                        nativeSetenv("XASH3D_RODIR", rodir);
                }
                Log.i(TAG, "XASH3D_RODIR = " + rodir);

                String pakfile = getIntent().getStringExtra("pakfile");
                if (pakfile != null) nativeSetenv("XASH3D_EXTRAS_PAK2", pakfile);

                String basedir = getIntent().getStringExtra("basedir");
                if (basedir != null) {
                        nativeSetenv("XASH3D_BASEDIR", basedir);
                } else {
                        String rootPath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/xash";
                        nativeSetenv("XASH3D_BASEDIR", rootPath);
                        basedir = rootPath;
                }

                // remember the real game dirs for the MOTD WebView sandbox
                mMotdBaseDir = basedir;
                mMotdGameDir = gamedir;

                mUseVolumeKeys = getIntent().getBooleanExtra("usevolume", false);
                mPackageName = getIntent().getStringExtra("package");

                String[] env = getIntent().getStringArrayExtra("env");
                if (env != null) {
                        for (int i = 0; i < env.length; i += 2)
                                nativeSetenv(env[i], env[i + 1]);
                }

                String argv = getIntent().getStringExtra("argv");
                if (argv == null) argv = "-console -log";

                return argv.split(" ");
        }

        // =====================================================================
        // sandboxed HTML MOTD rendering ("Message of the
        // Day" like real CS 1.6). The engine hands us the raw "MOTD" user
        // message payload as bytes; we render it in a WebView that can only
        // read files inside the current game dir (valve/ or cstrike/), can
        // never reach addons/ (metamod/AMXX data, top15 stats) and has no
        // JavaScript, DOM storage, content:// or network access at all.
        // =====================================================================

        // implemented in the engine (libxash.so); tells
        // the client dll the MOTD dialog is gone so it can show the deferred
        // team select menu (PC CS 1.6 behaviour). Name kept by proguard.
        private static native void nativeMOTDClosed();

        private static void notifyMOTDClosed() {
                try {
                        nativeMOTDClosed();
                } catch ( Throwable t ) {
                        Log.w( TAG, "nativeMOTDClosed failed", t );
                }
        }

        // surface Java-side MOTD failures INSIDE the game console (the
        // engine's Con_Printf). Logcat-only diagnostics were invisible on
        // user devices: a bare "MOTD dialog failed" gives no clue WHY
        // (the dialog returning false with no reason anywhere).
        private static native void nativeConsolePrintf( String s );

        private static void consolePrintf( String s ) {
                try {
                        nativeConsolePrintf( s );
                } catch ( Throwable t ) {
                        Log.w( TAG, "consolePrintf failed", t );
                }
        }

        /** Called from native (JNI) with the window title (the server name,
         *  like the original HL1 VGUI MOTD window) and the raw MOTD payload.
         *  Runs the dialog creation synchronously on the UI thread and
         *  returns whether the dialog is actually on screen, so the client dll
         *  can fall back to the classic HUD text renderer when the WebView
         *  dialog cannot be shown. */
        public boolean showMOTD( final byte[] titleBytes, final byte[] htmlBytes ) {
                final boolean[] shown = new boolean[1];
                final java.util.concurrent.CountDownLatch latch = new java.util.concurrent.CountDownLatch( 1 );

                runOnUiThread( new Runnable() {
                        @Override
                        public void run() {
                                try {
                                        shown[0] = showMOTDOnUiThread( titleBytes, htmlBytes );
                                } catch ( Throwable t ) {
                                        Log.w( TAG, "showMOTD failed", t );
                                        shown[0] = false;
                                } finally {
                                        latch.countDown();
                                }
                        }
                } );

                try {
                        // the engine render thread blocks briefly here; the UI thread
                        // is independent, so this cannot deadlock
                        if ( !latch.await( 5, java.util.concurrent.TimeUnit.SECONDS ) ) {
                                // make the timeout visible — it is indistinguishable
                                // from a dead activity otherwise
                                consolePrintf( "MOTD: UI thread did not answer within 5s (dialog skipped)" );
                                return false;
                        }
                } catch ( InterruptedException e ) {
                        return false;
                }

                return shown[0];
        }

        // the original HL1 VGUI colors — orange title/button text
        // (255,170,0) and the window border (178,119,0), straight from
        // hlsdk-portable cl_dll/vgui_MOTDWindow.cpp
        private static final int MOTD_VGUI_TEXT = 0xFFFFAA00;
        private static final int MOTD_VGUI_BORDER = 0xFFB37700;

        /** Destroy the previous MOTD WebView after its dialog is fully
         *  detached (WebView.destroy() must never run while the view is still
         *  attached to a window). Posted so it is ordered after dismiss. */
        private void scheduleMotdWebViewDestroy() {
                if ( mMotdWebView == null )
                        return;

                final WebView wv = mMotdWebView;
                mMotdWebView = null;

                new android.os.Handler( android.os.Looper.getMainLooper()).post(
                        new Runnable() {
                                @Override
                                public void run() {
                                        try {
                                                wv.destroy();
                                        } catch ( Throwable t ) {
                                                Log.w( TAG, "MOTD WebView destroy failed", t );
                                        }
                                }
                        } );
        }

        private boolean showMOTDOnUiThread( byte[] titleBytes, byte[] htmlBytes ) {
                try {
                        if ( mMotdDialog != null ) {
                                mMotdDialog.dismiss();
                                mMotdDialog = null;
                        }

                        // the previous map's WebView goes away with its
                        // dialog — reclaim it before building a new one
                        scheduleMotdWebViewDestroy();

                        String title = decodeMotdBytes( titleBytes );
                        String raw = new String( htmlBytes, "UTF-8" );
                        Log.i( TAG, "MOTD dialog: title=\"" + title + "\" payloadBytes="
                                + ( htmlBytes != null ? htmlBytes.length : -1 ));
                        String base = mMotdBaseDir != null ? mMotdBaseDir
                                : Environment.getExternalStorageDirectory().getAbsolutePath() + "/xash";
                        String game = mMotdGameDir != null ? mMotdGameDir : "valve";
                        final File gameDir = new File( base, game );

                        final Dialog dialog = new Dialog( this, android.R.style.Theme_Black_NoTitleBar );
                        Window w = dialog.getWindow();
                        w.setBackgroundDrawable( new ColorDrawable( 0x00000000 ) );
                        w.setLayout( ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.MATCH_PARENT );

                        // NO fullscreen shade — the retail CS 1.6 MOTD
                        // (CMenuPanel( iShadeFullscreen=0 )) leaves the game
                        // at full brightness around the window, exactly like
                        // the PC original
                        FrameLayout root = new FrameLayout( this );
                        root.setBackgroundColor( 0x00000000 );

                        DisplayMetrics dm = getResources().getDisplayMetrics();
                        int screenW = dm.widthPixels;
                        int screenH = dm.heightPixels;

                        // 1:1 to the retail Steam CS 1.6 MOTD window
                        // (pixel-measured from the retail window):
                        // ~71.5% of the screen width x ~90% of the height,
                        // black at ~76% opacity (the game faintly shows
                        // through), Valve LineBorder (178,119,0) around it.
                        int panelW = Math.max( dp( 200 ), Math.round( screenW * 0.715f ));
                        int panelH = Math.max( dp( 140 ), Math.round( screenH * 0.90f ));

                        LinearLayout panel = new LinearLayout( this );
                        panel.setOrientation( LinearLayout.VERTICAL );
                        panel.setBackground( makeMOTDWindowBackground());
                        FrameLayout.LayoutParams panelLp = new FrameLayout.LayoutParams( panelW, panelH );
                        panelLp.gravity = Gravity.CENTER;
                        root.addView( panel, panelLp );

                        // title row like the reference — the CS soldier
                        // logo left, amber "Title Font" caption, sitting
                        // directly on the window background (no bar strip;
                        // the game shows through it), with a thin light
                        // separator line along its bottom edge.
                        int titleH = Math.round( panelH * 0.125f );
                        LinearLayout titleBar = new LinearLayout( this );
                        titleBar.setOrientation( LinearLayout.HORIZONTAL );
                        titleBar.setGravity( Gravity.CENTER_VERTICAL );

                        ImageView logo = new ImageView( this );
                        logo.setImageResource( R.drawable.cs_logo );
                        logo.setScaleType( ImageView.ScaleType.FIT_CENTER );
                        int logoSize = Math.round( titleH * 0.62f );
                        LinearLayout.LayoutParams logoLp = new LinearLayout.LayoutParams(
                                        logoSize, logoSize );
                        logoLp.setMargins( Math.round( panelW * 0.035f ), 0, Math.round( panelW * 0.02f ), 0 );
                        titleBar.addView( logo, logoLp );

                        TextView titleView = new TextView( this );
                        titleView.setText(( title != null && !title.isEmpty()) ? title : "Counter-Strike" );
                        titleView.setTextColor( MOTD_VGUI_TEXT );
                        titleView.setTextSize( TypedValue.COMPLEX_UNIT_PX, Math.round( titleH * 0.42f ));
                        titleView.setTypeface( Typeface.DEFAULT_BOLD );
                        titleView.setSingleLine( true );
                        titleView.setEllipsize( TextUtils.TruncateAt.END );
                        titleView.setGravity( Gravity.CENTER_VERTICAL );
                        titleBar.addView( titleView, new LinearLayout.LayoutParams(
                                        0, ViewGroup.LayoutParams.MATCH_PARENT, 1f ));

                        panel.addView( titleBar, new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT, titleH ));

                        // light 1px separator line under the title row
                        View titleSep = new View( this );
                        titleSep.setBackground( new ColorDrawable( 0x99B4B8BC ));
                        panel.addView( titleSep, new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT, Math.max( 1, dp( 1 )) ));

                        // --- content: the sandboxed WebView plays the role of
                        // the original ScrollPanel + TextPanel; HTML MOTDs
                        // render for real, plain text is wrapped game-styled.
                        // if WebView is unavailable (provider missing,
                        // device policy, ...) fall back to a styled TextView via
                        // Html.fromHtml — the dialog still returns true, so the
                        // client never degrades to raw-text HUD garbage.
                        View content;

                        // the content sits in a PURE BLACK box with no
                        // border of its own (the ScrollPanel client area of
                        // the PC window) — the framed look comes from the
                        // window chrome around it
                        LinearLayout contentWrap = new LinearLayout( this );
                        contentWrap.setOrientation( LinearLayout.VERTICAL );
                        contentWrap.setBackgroundColor( 0xFF000000 );

                        try {
                                WebView wv = createMOTDWebView( gameDir );
                                wv.setBackgroundColor( 0xFF000000 );
                                wv.loadDataWithBaseURL( "https://motd.local/", buildMOTDDocument( raw ),
                                                "text/html", "utf-8", null );
                                mMotdWebView = wv;
                                content = wv;
                        } catch ( Throwable wt ) {
                                consolePrintf( "MOTD: WebView unavailable (" + wt + "), using styled text" );

                                TextView tv = new TextView( this );
                                tv.setText( Html.fromHtml( buildMOTDTextHtml( raw ) ) );
                                tv.setMovementMethod( ScrollingMovementMethod.getInstance() );
                                tv.setTextColor( 0xFFDEDEDE );
                                tv.setTextSize( TypedValue.COMPLEX_UNIT_SP, 14 );
                                tv.setLinkTextColor( MOTD_VGUI_TEXT );
                                content = tv;
                        }

                        contentWrap.addView(( View ) content, new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.MATCH_PARENT ));

                        LinearLayout.LayoutParams contentLp = new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f );
                        contentLp.setMargins( Math.round( panelW * 0.023f ), Math.round( panelH * 0.085f ),
                                        Math.round( panelW * 0.023f ), 0 );
                        panel.addView( contentWrap, contentLp );

                        // --- the reference OK button — small, bottom-left:
                        // ~20.5% of the window wide, ~4.6% tall, near-black
                        // body with a thin LIGHT-GRAY frame (the VGUI
                        // CommandButton look) and an AMBER label; the empty
                        // window band below it matches the reference too.
                        int okW = Math.max( dp( 64 ), Math.round( panelW * 0.205f ));
                        int okH = Math.max( dp( 22 ), Math.round( panelH * 0.046f ));

                        Button ok = new Button( this );
                        ok.setText( "OK" );
                        ok.setAllCaps( false );
                        ok.setTextColor( MOTD_VGUI_TEXT );
                        ok.setTextSize( TypedValue.COMPLEX_UNIT_PX, Math.round( okH * 0.5f ));
                        ok.setBackground( makeMOTDButtonBackground());
                        ok.setStateListAnimator( null );
                        ok.setElevation( 0f );
                        ok.setPadding( dp( 6 ), 0, dp( 6 ), 0 );
                        ok.setOnClickListener( new View.OnClickListener() {
                                        @Override
                                        public void onClick( View v ) {
                                                dialog.dismiss();
                                        }
                        } );

                        LinearLayout.LayoutParams okLp = new LinearLayout.LayoutParams( okW, okH );
                        okLp.setMargins( Math.round( panelW * 0.112f ), Math.round( panelH * 0.02f ),
                                        0, Math.round( panelH * 0.16f ));
                        panel.addView( ok, okLp );

                        dialog.setContentView( root );
                        dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                                @Override
                                public void onDismiss( DialogInterface d ) {
                                        mMotdDialog = null;
                                        // reclaim the WebView when the user
                                        // closes the dialog with OK/back too
                                        scheduleMotdWebViewDestroy();
                                        notifyMOTDClosed();
                                }
                        } );

                        mMotdDialog = dialog;
                        dialog.show();
                        Log.i( TAG, "MOTD dialog shown (WebView HTML rendering)" );
                        return true;
                } catch ( Throwable t ) {
                        Log.w( TAG, "showMOTD failed", t );
                        // the reason must reach the game console, not only logcat
                        consolePrintf( "MOTD: dialog error: " + t );
                        return false;
                }
        }

        /** Engine payloads are raw UTF-8 bytes (a malformed server
         *  string must never abort NewStringUTF); decode leniently. */
        private static String decodeMotdBytes( byte[] b ) {
                try {
                        return b == null ? "" : new String( b, "UTF-8" ).trim();
                } catch ( Throwable t ) {
                        return "";
                }
        }

        /** Build the document loaded into the MOTD WebView. HTML pages
         *  render as-is (PC parity); plain-text MOTDs are wrapped into a
         *  game-styled document (black background, HL1 "Briefing Text" tan). */
        private static String buildMOTDDocument( String raw ) {
                String trimmed = raw == null ? "" : raw.trim();
                String lower = trimmed.toLowerCase( Locale.US );

                // broader tag list -- server MOTDs use every HTML tag in
                // the book (<head>, <title>, <style>, <meta>, <center>, <span>,
                // <h1>..<h6>, <li>, <b>/<i>/<u>, ...). A real HTML page always
                // carries at least one of these; plain chat-style text never
                // matches because "<" must be immediately followed by a letter
                // and form a known tag prefix.
                boolean looksHtml = lower.contains( "<html" ) || lower.contains( "<body" )
                        || lower.contains( "<head" ) || lower.contains( "<title" )
                        || lower.contains( "<meta" ) || lower.contains( "<style" )
                        || lower.contains( "<br" ) || lower.contains( "<p>" ) || lower.contains( "<p " )
                        || lower.contains( "<table" ) || lower.contains( "<div" ) || lower.contains( "<font" )
                        || lower.contains( "<img" ) || lower.contains( "<center" ) || lower.contains( "<span" )
                        || lower.contains( "<h1" ) || lower.contains( "<h2" ) || lower.contains( "<h3" )
                        || lower.contains( "<h4" ) || lower.contains( "<h5" ) || lower.contains( "<h6" )
                        || lower.contains( "<li" ) || lower.contains( "<pre" )
                        || lower.contains( "<b>" ) || lower.contains( "<i>" ) || lower.contains( "<u>" )
                        || lower.contains( "<em>" ) || lower.contains( "<strong" )
                        || lower.contains( "<hr" ) || lower.contains( "<a " ) || lower.contains( "<!doctype" );

                Log.i( TAG, "MOTD content: len=" + trimmed.length() + " html=" + looksHtml );

                if ( looksHtml )
                        return trimmed;

                StringBuilder sb = new StringBuilder();
                sb.append( "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" );
                sb.append( "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" );
                sb.append( "<style>html,body{margin:0;padding:0;background:#000;height:100%;}" );
                // plain text renders light-gray on black like the PC
                // CS 1.6 MOTD text panel (was the HL1 tan before)
                sb.append( "pre{margin:0;padding:14px;font-family:sans-serif;" );
                sb.append( "font-size:14px;line-height:1.45;color:#dedede;" );
                sb.append( "white-space:pre-wrap;word-wrap:break-word;}</style></head><body><pre>" );
                sb.append( escapeMOTDHtml( trimmed ) );
                sb.append( "</pre></body></html>" );
                return sb.toString();
        }

        private static String escapeMOTDHtml( String s ) {
                return s.replace( "&", "&amp;" ).replace( "<", "&lt;" ).replace( ">", "&gt;" );
        }

        /** Simplified document for the Html.fromHtml fallback path.
         *  fromHtml understands basic tags only (no CSS), so block elements
         *  are mapped to newlines and the body carries the HL1 tan color. */
        private static String buildMOTDTextHtml( String raw ) {
                String trimmed = raw == null ? "" : raw.trim();
                String lower = trimmed.toLowerCase( Locale.US );

                boolean looksHtml = lower.contains( "<html" ) || lower.contains( "<body" )
                        || lower.contains( "<head" ) || lower.contains( "<title" )
                        || lower.contains( "<meta" ) || lower.contains( "<style" )
                        || lower.contains( "<br" ) || lower.contains( "<p>" ) || lower.contains( "<p " )
                        || lower.contains( "<table" ) || lower.contains( "<div" ) || lower.contains( "<font" )
                        || lower.contains( "<img" ) || lower.contains( "<center" ) || lower.contains( "<span" )
                        || lower.contains( "<h1" ) || lower.contains( "<h2" ) || lower.contains( "<h3" )
                        || lower.contains( "<h4" ) || lower.contains( "<h5" ) || lower.contains( "<h6" )
                        || lower.contains( "<li" ) || lower.contains( "<pre" )
                        || lower.contains( "<b>" ) || lower.contains( "<i>" ) || lower.contains( "<u>" )
                        || lower.contains( "<em>" ) || lower.contains( "<strong" )
                        || lower.contains( "<hr" ) || lower.contains( "<a " ) || lower.contains( "<!doctype" );

                if ( looksHtml ) {
                        // drop <style>/<script> bodies — fromHtml would print them as text
                        String noCss = trimmed
                                .replaceAll( "(?is)<style[^>]*>.*?</style>", "" )
                                .replaceAll( "(?is)<script[^>]*>.*?</script>", "" );
                        return "<font color='#dedede'>" + noCss + "</font>";
                }

                return "<font color='#dedede'><pre>" + escapeMOTDHtml( trimmed ) + "</pre></font>";
        }

        /** The reference window — black at ~76% opacity (the game
         *  faintly shows through, like the PC retail MOTD over the map)
         *  with Valve's 1px LineBorder (178,119,0). */
        private Drawable makeMOTDWindowBackground() {
                GradientDrawable d = new GradientDrawable();
                d.setColor( 0xC2000000 );
                d.setStroke( Math.max( 1, dp( 1 )), MOTD_VGUI_BORDER );
                return d;
        }

        /** The small command button — near-black body, thin light-gray
         *  frame (the VGUI CommandButton look), amber when pressed. */
        private StateListDrawable makeMOTDButtonBackground() {
                GradientDrawable normal = new GradientDrawable();
                normal.setColor( 0xE6000000 );
                normal.setStroke( Math.max( 1, dp( 1 )), 0xFFC8C4BC );

                GradientDrawable pressed = new GradientDrawable();
                pressed.setColor( 0xFF3A2E10 );
                pressed.setStroke( Math.max( 1, dp( 1 )), MOTD_VGUI_TEXT );

                StateListDrawable sld = new StateListDrawable();
                sld.addState( new int[] { android.R.attr.state_pressed }, pressed );
                sld.addState( new int[] { -android.R.attr.state_pressed }, normal );
                return sld;
        }

        @SuppressLint("SetJavaScriptEnabled")
        private WebView createMOTDWebView( final File gameDir ) {
                WebView wv = new WebView( this );
                WebSettings s = wv.getSettings();

                // --- sandbox defaults ---------------------------------------
                // PC parity -- the retail CS 1.6 MOTD window (Steam
                // browser) renders REMOTE images and lets links navigate
                // inside the window. JS is enabled because modern server
                // rank/ban pages need it; there is no JS-to-native bridge,
                // so the sandbox holds: file:// and content:// stay blocked
                // and shouldInterceptRequest still gates every request.
                s.setJavaScriptEnabled( true );
                s.setDomStorageEnabled( true );
                s.setAllowFileAccess( false );
                s.setAllowContentAccess( false );
                s.setAllowFileAccessFromFileURLs( false );
                s.setAllowUniversalAccessFromFileURLs( false );
                s.setBlockNetworkLoads( false );
                s.setBlockNetworkImage( false );
                s.setSavePassword( false );
                s.setCacheMode( WebSettings.LOAD_NO_CACHE );
                s.setMediaPlaybackRequiresUserGesture( true );
                // the MOTD document is loaded from a fake https origin, so
                // plain-http <img>/<link> targets would be mixed content --
                // always allow, like the PC window did
                s.setMixedContentMode( WebSettings.MIXED_CONTENT_ALWAYS_ALLOW );

                // server MOTD pages are designed for the ~640px-wide PC
                // CS 1.6 window; lay them out wide and zoom to fit, so they
                // appear complete just like on PC
                s.setUseWideViewPort( true );
                s.setLoadWithOverviewMode( true );

                wv.setWebViewClient( new WebViewClient() {
                        @Override
                        public WebResourceResponse shouldInterceptRequest( WebView view, WebResourceRequest request ) {
                                Uri url = request.getUrl();
                                String scheme = url.getScheme();

                                if ( scheme == null )
                                        return emptyResponse();

                                // game-dir relative resources: served from disk by us
                                if ( scheme.equals( "https" ) && "motd.local".equals( url.getHost() ) ) {
                                        File f = resolveInGameDir( gameDir, url.getPath() );
                                        if ( f != null ) {
                                                try {
                                                        return new WebResourceResponse( guessMime( f.getName() ),
                                                                null, new FileInputStream( f ) );
                                                } catch ( Throwable t ) {
                                                        return emptyResponse();
                                                }
                                        }
                                        return emptyResponse();
                                }

                                if ( scheme.equals( "data" ) )
                                        return null; // inline data URIs are harmless

                                // real http(s) goes to the network (PC
                                // parity: server MOTDs embed remote images,
                                // rank/ban pages, web fonts)
                                if ( scheme.equals( "http" ) || scheme.equals( "https" ) )
                                        return null;

                                // file://, content:// and anything else is still blocked
                                return emptyResponse();
                        }

                        @Override
                        public boolean shouldOverrideUrlLoading( WebView view, WebResourceRequest request ) {
                                // PC parity -- http(s) links navigate INSIDE
                                // the MOTD window; everything else (file,
                                // content, mailto, intent, market) stays blocked
                                String scheme = request.getUrl().getScheme();
                                return !( "http".equals( scheme ) || "https".equals( scheme ));
                        }
                } );

                return wv;
        }

        /**
         * Resolve a URL path against the game dir with strict sandboxing:
         * no ".." traversal, no "addons" access, result must stay inside
         * the game dir and exist as a plain file.
         */
        private static File resolveInGameDir( File gameDir, String uriPath ) {
                try {
                        if ( uriPath == null || uriPath.isEmpty() )
                                return null;

                        String path = Uri.decode( uriPath );
                        if ( path.indexOf( '\0' ) >= 0 )
                                return null;

                        File root = gameDir.getCanonicalFile();
                        File cur = root;

                        for ( String seg : path.split( "/" ) ) {
                                if ( seg.isEmpty() || seg.equals( "." ) )
                                        continue;
                                if ( seg.equals( ".." ) )
                                        return null;
                                if ( seg.equalsIgnoreCase( "addons" ) )
                                        return null; // addons is off-limits (metamod/AMXX, top15 data)
                                cur = new File( cur, seg );
                        }

                        File resolved = cur.getCanonicalFile();
                        if ( !resolved.getPath().startsWith( root.getPath() + File.separator ) )
                                return null;
                        if ( !resolved.isFile() )
                                return null;
                        return resolved;
                } catch ( Throwable t ) {
                        return null;
                }
        }

        private static WebResourceResponse emptyResponse() {
                return new WebResourceResponse( "text/plain", "utf-8",
                        new ByteArrayInputStream( new byte[0] ) );
        }

        private static String guessMime( String name ) {
                String n = name.toLowerCase( Locale.US );
                if ( n.endsWith( ".html" ) || n.endsWith( ".htm" ) ) return "text/html";
                if ( n.endsWith( ".jpg" ) || n.endsWith( ".jpeg" ) ) return "image/jpeg";
                if ( n.endsWith( ".png" ) ) return "image/png";
                if ( n.endsWith( ".gif" ) ) return "image/gif";
                if ( n.endsWith( ".bmp" ) ) return "image/bmp";
                if ( n.endsWith( ".css" ) ) return "text/css";
                if ( n.endsWith( ".txt" ) ) return "text/plain";
                return "application/octet-stream";
        }

        private int dp( int v ) {
                return Math.round( v * getResources().getDisplayMetrics().density );
        }
}
