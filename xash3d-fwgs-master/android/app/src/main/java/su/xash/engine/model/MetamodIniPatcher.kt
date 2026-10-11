package su.xash.engine.model

import android.os.Build
import java.io.File

/**
 * Single source of truth for the metamod plugins.ini content.
 *
 * Bug this fixes: the ini used to be rewritten ONLY by the cs16client
 * installer app (MainActivity.patchPluginsIni). Launching cstrike through
 * the ENGINE app icon (where the AMXX/YaPB switches live) never touched
 * the ini, so a stale "linux .../libyapb_android_arm64.so" line from an
 * earlier launch survived and YaPB kept adding bots even with the YaPB
 * switch turned off. The same hole existed for CZeroActivity (czero).
 *
 * Now the engine app - the owner of the switches - rewrites the ini at
 * every game launch and at every switch change. The cs16client patcher
 * still runs on its own launch path with identical logic, so both entry
 * points converge on the same content.
 */
object MetamodIniPatcher {
        @JvmStatic
        fun patch(metaDir: File, amxxEnabled: Boolean, yapbEnabled: Boolean,
                        csNativeLibDir: String?): Boolean {
                return try {
                        val sb = StringBuilder()
                        sb.append( ";;;\n" )
                        sb.append( "; Metamod plugin list for Android (xash3d-fwgs)\n" )
                        sb.append( "; AMX Mod X core:\n" )
                        sb.append( ";;;\n" )

                        if( amxxEnabled ) {
                                sb.append( "linux addons/amxmodx/dlls/libmm_amxmodx.so\n" )
                        } else {
                                sb.append( "; disabled by the AMX Mod X switch: addons/metamod/amxmodx.disabled\n" )
                        }

                        if( yapbEnabled ) {
                                // YaPB ships inside the cs16client APK; its library
                                // path is only known through that app's nativeLibraryDir.
                                if( csNativeLibDir != null ) {
                                        val abis = Build.SUPPORTED_ABIS
                                        val yapbName = if( abis != null && abis.isNotEmpty() && abis[0].contains( "arm64" ) )
                                                "libyapb_android_arm64.so" else "libyapb_android_armv7l.so"
                                        val yapb = File( csNativeLibDir, yapbName )

                                        if( yapb.isFile ) {
                                                sb.append( ";\n; YaPB bot (as metamod plugin):\n" )
                                                sb.append( "linux " ).append( yapb.absolutePath ).append( "\n" )
                                        }
                                }
                        } else {
                                sb.append( "; YaPB bots disabled by the YaPB switch: addons/metamod/yapb.disabled\n" )
                        }

                        metaDir.mkdirs()
                        File( metaDir, "plugins.ini" ).writeText( sb.toString() )
                        true
                } catch( e: Exception ) {
                        e.printStackTrace()
                        false
                }
        }
}
