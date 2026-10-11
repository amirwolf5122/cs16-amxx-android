#!/usr/bin/env python3
# rebuild the Android MOTD dialog as a 1:1 replica of the ORIGINAL
# xash3d-fwgs/hlsdk-portable VGUI MOTD window (cl_dll/vgui_MOTDWindow.cpp,
# CMessageWindowPanel) with real HTML rendering through the sandboxed WebView.
# Removes the MOTD.res parser and the custom CS-logo window.
import io, sys

PATH = "xash3d-fwgs-master/android/app/src/main/java/su/xash/engine/XashActivity.java"

with io.open(PATH, "r", encoding="utf-8", newline="") as f:
    src = f.read()

def cut(src, start_marker, end_marker, replacement, what):
    i = src.find(start_marker)
    j = src.find(end_marker)
    if i < 0 or j < 0 or j < i:
        print(f"FAIL markers for {what}: start={i} end={j}")
        sys.exit(1)
    return src[:i] + replacement + src[j:], src[i:j]

# ------------------------------------------------------- 1. JNI entry points
old, _ = None, None
for old, new in [
    (
        "        /** Called from native (JNI) when a client MOTD contains HTML.\n"
        "         *  Runs the dialog creation synchronously on the UI thread and\n"
        "         *  returns whether the dialog is actually on screen, so the client dll\n"
        "         *  can fall back to the classic HUD text renderer when the WebView\n"
        "         *  dialog cannot be shown. */\n"
        "        public boolean showMOTD( final byte[] htmlBytes ) {",
        "        /** Called from native (JNI) with the window title (the server name,\n"
        "         *  like the original HL1 VGUI MOTD window) and the raw MOTD payload.\n"
        "         *  Runs the dialog creation synchronously on the UI thread and\n"
        "         *  returns whether the dialog is actually on screen, so the client dll\n"
        "         *  can fall back to the classic HUD text renderer when the WebView\n"
        "         *  dialog cannot be shown. */\n"
        "        public boolean showMOTD( final byte[] titleBytes, final byte[] htmlBytes ) {",
    ),
    (
        "                                        shown[0] = showMOTDOnUiThread( htmlBytes );",
        "                                        shown[0] = showMOTDOnUiThread( titleBytes, htmlBytes );",
    ),
]:
    if old not in src:
        print(f"FAIL entry-point marker:\n{old[:120]}")
        sys.exit(1)
    src = src.replace(old, new, 1)

# ---------------------------------- 2. showMOTDOnUiThread + buildMOTDTitle
A = "        // orange-gold sampled from the PC CS 1.6 MOTD reference"
B = "        /** build the document loaded into the MOTD WebView."

new_ui = '''        // the original HL1 VGUI colors — orange title/button text
        // (255,170,0) and the window border (178,119,0), straight from
        // hlsdk-portable cl_dll/vgui_MOTDWindow.cpp
        private static final int MOTD_VGUI_TEXT = 0xFFFFAA00;
        private static final int MOTD_VGUI_BORDER = 0xFFB37700;

        private boolean showMOTDOnUiThread( byte[] titleBytes, byte[] htmlBytes ) {
                try {
                        if ( mMotdDialog != null ) {
                                mMotdDialog.dismiss();
                                mMotdDialog = null;
                        }

                        String title = decodeMotdBytes( titleBytes );
                        String raw = new String( htmlBytes, "UTF-8" );
                        String base = mMotdBaseDir != null ? mMotdBaseDir
                                : Environment.getExternalStorageDirectory().getAbsolutePath() + "/xash";
                        String game = mMotdGameDir != null ? mMotdGameDir : "valve";
                        final File gameDir = new File( base, game );

                        final Dialog dialog = new Dialog( this, android.R.style.Theme_Black_NoTitleBar );
                        Window w = dialog.getWindow();
                        w.setBackgroundDrawable( new ColorDrawable( 0x00000000 ) );
                        w.setLayout( ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.MATCH_PARENT );

                        // the original window shades the whole screen behind
                        // itself with a 100/255 black veil (CMenuPanel)
                        FrameLayout root = new FrameLayout( this );
                        root.setBackgroundColor( 0x64000000 );

                        DisplayMetrics dm = getResources().getDisplayMetrics();
                        int screenW = dm.widthPixels;
                        int screenH = dm.heightPixels;

                        // 1:1 copy of the ORIGINAL xash3d-fwgs /
                        // hlsdk-portable MOTD window (CMessageWindowPanel,
                        // 640x480 coordinate space): a 424x312 window at
                        // (112,80) — opaque black, 1px LineBorder in
                        // (178,119,0), server-name title top-left, scrollable
                        // content, bottom-left OK button (160x30). XRES/YRES
                        // scale independently exactly like the PC game does
                        // on widescreen; phones additionally get a
                        // guaranteed-wide panel (74%..94% of the screen).
                        float sx = screenW / 640f;
                        float sy = screenH / 480f;

                        int panelW = Math.round( 424 * sx );
                        int minW = Math.round( screenW * 0.74f );
                        int maxW = Math.round( screenW * 0.94f );

                        if ( panelW < minW )
                                panelW = minW;

                        if ( panelW > maxW )
                                panelW = maxW;

                        int panelH = Math.round( 312 * sy );
                        int maxH = Math.round( screenH * 0.88f );

                        if ( panelH > maxH )
                                panelH = maxH;

                        LinearLayout panel = new LinearLayout( this );
                        panel.setOrientation( LinearLayout.VERTICAL );
                        panel.setBackground( makeMOTDWindowBackground( sy ));
                        FrameLayout.LayoutParams panelLp = new FrameLayout.LayoutParams( panelW, panelH );
                        panelLp.gravity = Gravity.CENTER;
                        root.addView( panel, panelLp );

                        // --- title: the server name (falls back to "MOTD"),
                        // the orange "Title Font" scheme, small, top-left ---
                        TextView titleView = new TextView( this );
                        titleView.setText(( title != null && !title.isEmpty()) ? title : "MOTD" );
                        titleView.setTextColor( MOTD_VGUI_TEXT );
                        titleView.setTextSize( TypedValue.COMPLEX_UNIT_SP, 14 );
                        titleView.setTypeface( Typeface.DEFAULT_BOLD );
                        titleView.setSingleLine( true );
                        titleView.setEllipsize( TextUtils.TruncateAt.END );
                        titleView.setGravity( Gravity.CENTER_VERTICAL );

                        LinearLayout.LayoutParams titleLp = new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT,
                                        Math.max( dp( 22 ), Math.round( 30 * sy )));
                        titleLp.setMargins( Math.round( 16 * sx ), Math.round( 12 * sy ),
                                        Math.round( 16 * sx ), 0 );
                        panel.addView( titleView, titleLp );

                        // --- content: the sandboxed WebView plays the role of
                        // the original ScrollPanel + TextPanel; HTML MOTDs
                        // render for real, plain text is wrapped game-styled ---
                        WebView wv = createMOTDWebView( gameDir );
                        wv.setBackgroundColor( 0x00000000 );
                        wv.loadDataWithBaseURL( "https://motd.local/", buildMOTDDocument( raw ),
                                        "text/html", "utf-8", null );

                        LinearLayout.LayoutParams contentLp = new LinearLayout.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f );
                        contentLp.setMargins( Math.round( 16 * sx ), Math.round( 6 * sy ),
                                        Math.round( 16 * sx ), 0 );
                        panel.addView( wv, contentLp );

                        // --- OK: the original CommandButton, bottom-left
                        // (16, tall - 16 - BUTTON_SIZE_Y), 160x30 in window
                        // units, orange label, thin orange border ---
                        Button ok = new Button( this );
                        ok.setText( "OK" );
                        ok.setAllCaps( false );
                        ok.setTextColor( MOTD_VGUI_TEXT );
                        ok.setTextSize( TypedValue.COMPLEX_UNIT_SP, 14 );
                        ok.setTypeface( Typeface.DEFAULT_BOLD );
                        ok.setBackground( makeMOTDButtonBackground());
                        ok.setStateListAnimator( null );
                        ok.setElevation( 0f );
                        ok.setPadding( dp( 12 ), 0, dp( 12 ), 0 );
                        ok.setOnClickListener( new View.OnClickListener() {
                                        @Override
                                        public void onClick( View v ) {
                                                dialog.dismiss();
                                        }
                        } );

                        int okW = Math.max( dp( 110 ), Math.round( 160 * sx ));
                        int okH = Math.max( dp( 38 ), Math.round( 30 * sy ));

                        LinearLayout.LayoutParams okLp = new LinearLayout.LayoutParams( okW, okH );
                        okLp.setMargins( Math.round( 16 * sx ), Math.round( 14 * sy ),
                                        Math.round( 16 * sx ), Math.round( 16 * sy ));
                        panel.addView( ok, okLp );

                        dialog.setContentView( root );
                        dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                                @Override
                                public void onDismiss( DialogInterface d ) {
                                        mMotdDialog = null;
                                        notifyMOTDClosed();
                                }
                        } );

                        mMotdDialog = dialog;
                        dialog.show();
                        return true;
                } catch ( Throwable t ) {
                        Log.w( TAG, "showMOTD failed", t );
                        return false;
                }
        }

        /** engine payloads are raw UTF-8 bytes (a malformed server
         *  string must never abort NewStringUTF); decode leniently. */
        private static String decodeMotdBytes( byte[] b ) {
                try {
                        return b == null ? "" : new String( b, "UTF-8" ).trim();
                } catch ( Throwable t ) {
                        return "";
                }
        }

'''

src, removed = cut(src, A, B, new_ui, "showMOTDOnUiThread+buildMOTDTitle")
if "buildMOTDTitle" in src:
    print("FAIL: buildMOTDTitle still referenced")
    sys.exit(1)

# ------------------------- 3. plain-text wrapper -> Briefing Text scheme
old_style = (
    '                sb.append( "pre{margin:0;padding:12px;font-family:monospace;" );\n'
    '                sb.append( "font-size:14px;line-height:1.5;color:#ffb400;" );'
)
new_style = (
    '                // the plain-text wrapper uses the original HL1\n'
    '                // "Briefing Text" scheme color (196,181,119)\n'
    '                sb.append( "pre{margin:0;padding:14px;font-family:sans-serif;" );\n'
    '                sb.append( "font-size:14px;line-height:1.45;color:#c4b577;" );'
)
if old_style not in src:
    print("FAIL plain-text style marker")
    sys.exit(1)
src = src.replace(old_style, new_style, 1)

# ------------------- 4. drop MOTD.res parser + old drawables, add the new ones
C1 = "        // =====================================================================\n" \
     "        // resource/UI/MOTD.res parsing"
C2 = '        @SuppressLint("SetJavaScriptEnabled")'

new_drawables = '''        /** the original CMessageWindowPanel body — opaque black with
         *  the 1px LineBorder in (178,119,0) (vgui_MOTDWindow.cpp). */
        private Drawable makeMOTDWindowBackground( float sy ) {
                GradientDrawable d = new GradientDrawable();
                d.setColor( 0xFF000000 );
                d.setStroke( Math.max( 1, Math.round( sy )), MOTD_VGUI_BORDER );
                return d;
        }

        /** the original CommandButton — flat dark body, thin orange
         *  border, orange label; brighter when armed (pressed). */
        private StateListDrawable makeMOTDButtonBackground() {
                GradientDrawable normal = new GradientDrawable();
                normal.setColor( 0xFF1D1D1D );
                normal.setStroke( 1, MOTD_VGUI_BORDER );

                GradientDrawable pressed = new GradientDrawable();
                pressed.setColor( 0xFF241A00 );
                pressed.setStroke( 1, MOTD_VGUI_TEXT );

                StateListDrawable sld = new StateListDrawable();
                sld.addState( new int[] { android.R.attr.state_pressed }, pressed );
                sld.addState( new int[] { -android.R.attr.state_pressed }, normal );
                return sld;
        }

'''

src, removed2 = cut(src, C1, C2, new_drawables, "motd.res parser + old drawables")
for gone in ("parseMotdRes", "MotdLayout", "makeMOTDDivider", "makeMOTDTitleBarBackground", "readMotdResFile", "stripResComments", "applyMotdKey", "findMotdRes"):
    if gone in src:
        print(f"FAIL: {gone} still present")
        sys.exit(1)

# ------------------------------------------------------------ sanity checks
for must in ("decodeMotdBytes", "MOTD_VGUI_BORDER", "createMOTDWebView",
             "resolveInGameDir", "buildMOTDDocument", "escapeMOTDHtml",
             "notifyMOTDClosed", "showMOTDOnUiThread( titleBytes, htmlBytes )"):
    if must not in src:
        print(f"FAIL: missing {must}")
        sys.exit(1)

bal = src.count("{") - src.count("}")
par = src.count("(") - src.count(")")
if bal != 0 or par != 0:
    print(f"FAIL balance braces={bal} parens={par}")
    sys.exit(1)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print(f"OK XashActivity.java rewritten: {len(src)} bytes, removed {len(removed)}+{len(removed2)} bytes, balance 0/0")
