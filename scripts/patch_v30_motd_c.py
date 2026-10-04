#!/usr/bin/env python3
# pass the server name as the MOTD window title (original HL1 VGUI
# behaviour: CreateTextWindow/SHOW_MOTD uses m_szServerName as the title).
import io, sys

def patch(path, subs, description):
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        src = f.read()
    orig = src
    for old, new, count in subs:
        n = src.count(old)
        if n < count:
            print(f"FAIL {path}: pattern found {n}x (want >= {count}):\n---\n{old}\n---")
            sys.exit(1)
        src = src.replace(old, new, count)
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(src)
    print(f"OK {path}: {description} ({len(orig)} -> {len(src)} bytes)")

# ---------------------------------------------------------------- android.c
patch(
    "xash3d-fwgs-master/engine/platform/android/android.c",
    [
        # 1. JNI signature: showMOTD now takes the title bytes too
        (
            '\tjni.showMOTD = (*jni.env)->GetMethodID( jni.env, jni.actcls, "showMOTD", "([B)Z" );',
            '\t// (title, html) both as raw byte arrays, so a malformed\n'
            '\t// server string can never abort NewStringUTF\n'
            '\tjni.showMOTD = (*jni.env)->GetMethodID( jni.env, jni.actcls, "showMOTD", "([B[B)Z" );',
            1,
        ),
        # 2. Android_ShowMOTD: new title parameter
        (
            "qboolean Android_ShowMOTD( const char *html )\n"
            "{\n"
            "\tsize_t len;\n"
            "\tjbyteArray jbytes;\n"
            "\tjboolean shown;\n"
            "\n"
            "\tif( !jni.env || !jni.activity || !jni.showMOTD )\n"
            "\t\treturn false;\n"
            "\n"
            "\tlen = Q_strlen( html );\n"
            "\tjbytes = (*jni.env)->NewByteArray( jni.env, (jsize)len );\n"
            "\n"
            "\tif( !jbytes )\n"
            "\t\treturn false;\n"
            "\n"
            "\t(*jni.env)->SetByteArrayRegion( jni.env, jbytes, 0, (jsize)len, (const jbyte *)html );\n"
            "\tshown = (*jni.env)->CallBooleanMethod( jni.env, jni.activity, jni.showMOTD, jbytes );\n"
            "\t(*jni.env)->DeleteLocalRef( jni.env, jbytes );",
            "qboolean Android_ShowMOTD( const char *title, const char *html )\n"
            "{\n"
            "\tsize_t len;\n"
            "\tjbyteArray jbytes, jtitle;\n"
            "\tjboolean shown;\n"
            "\n"
            "\tif( !jni.env || !jni.activity || !jni.showMOTD )\n"
            "\t\treturn false;\n"
            "\n"
            "\t// the window title (server name, like the original HL1\n"
            "\t// VGUI MOTD window) is passed as a second raw byte array\n"
            "\tlen = Q_strlen( title );\n"
            "\tjtitle = (*jni.env)->NewByteArray( jni.env, (jsize)len );\n"
            "\n"
            "\tif( !jtitle )\n"
            "\t\treturn false;\n"
            "\n"
            "\t(*jni.env)->SetByteArrayRegion( jni.env, jtitle, 0, (jsize)len, (const jbyte *)title );\n"
            "\n"
            "\tlen = Q_strlen( html );\n"
            "\tjbytes = (*jni.env)->NewByteArray( jni.env, (jsize)len );\n"
            "\n"
            "\tif( !jbytes )\n"
            "\t{\n"
            "\t\t(*jni.env)->DeleteLocalRef( jni.env, jtitle );\n"
            "\t\treturn false;\n"
            "\t}\n"
            "\n"
            "\t(*jni.env)->SetByteArrayRegion( jni.env, jbytes, 0, (jsize)len, (const jbyte *)html );\n"
            "\tshown = (*jni.env)->CallBooleanMethod( jni.env, jni.activity, jni.showMOTD, jtitle, jbytes );\n"
            "\t(*jni.env)->DeleteLocalRef( jni.env, jbytes );\n"
            "\t(*jni.env)->DeleteLocalRef( jni.env, jtitle );",
            1,
        ),
        # 3. doc comment above Android_ShowMOTD
        (
            "render an HTML MOTD in a sandboxed WebView dialog\n"
            "managed by the activity. The payload is passed as a raw byte array so a\n"
            "malformed (non-modified-UTF-8) server string can't abort in NewStringUTF;\n"
            "Java decodes it as UTF-8 with replacement characters.",
            "Render an HTML MOTD in a sandboxed WebView dialog\n"
            "managed by the activity. The payload is passed as a raw byte array so a\n"
            "malformed (non-modified-UTF-8) server string can't abort in NewStringUTF;\n"
            "Java decodes it as UTF-8 with replacement characters.\n"
            "Also takes the window title (server name) as a second argument, exactly\n"
            "like the original HL1 VGUI MOTD window (vgui_MOTDWindow.cpp).",
            1,
        ),
    ],
    "Android_ShowMOTD title param + JNI sig",
)

# ---------------------------------------------------------------- platform.h
patch(
    "xash3d-fwgs-master/engine/platform/platform.h",
    [
        (
            "qboolean Android_ShowMOTD( const char *html ); // returns false when the dialog could not be shown",
            "qboolean Android_ShowMOTD( const char *title, const char *html ); // title = server name; returns false when the dialog could not be shown",
            1,
        ),
    ],
    "platform.h prototype",
)

# ---------------------------------------------------------------- cl_game.c
patch(
    "xash3d-fwgs-master/engine/client/dll_int/cl_game.c",
    [
        (
            "#if XASH_ANDROID\n"
            "\tif( Android_ShowMOTD( html ))\n"
            "\t\treturn true;",
            "#if XASH_ANDROID\n"
            "\t// the server name is the window title, exactly like the\n"
            "\t// original HL1 VGUI MOTD window (CreateTextWindow/SHOW_MOTD)\n"
            "\tif( Android_ShowMOTD( cls.servername, html ))\n"
            "\t\treturn true;",
            1,
        ),
    ],
    "pfnShowMOTD passes servername",
)

print("ALL C PATCHES OK")
