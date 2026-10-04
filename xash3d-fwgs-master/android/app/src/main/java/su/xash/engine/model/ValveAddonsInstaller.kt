package su.xash.engine.model

import android.content.Context
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipInputStream

/**
 * v7: installs the bundled Valve (Half-Life) AMX Mod X addons.
 *
 * The engine APK ships assets/valve-addons.zip containing:
 *   valve/gameinfo.txt       - minimal gamedir skeleton (only written when
 *                              the device has no Valve data at all, so the
 *                              game shows up in the library)
 *   valve/addons/metamod/... - Metamod + plugins.ini
 *   valve/addons/amxmodx/... - AMX Mod X core, modules, configs, plugins,
 *                              scripting (sources + includes) for the
 *                              in-app SMA compiler
 *
 * The archive is extracted into <xashdir>/valve on first launch and after
 * every package update that bumps [CURRENT_VERSION] (same flow as the
 * cs16client launcher uses for its addons.zip).
 */
object ValveAddonsInstaller {
        private const val TAG = "ValveAddons"
        private const val PREFS = "valve_addons"
        private const val KEY_VERSION = "version"

        // the zip now carries the "valve/" directory prefix (previous
        // packs were missing it, so the addons landed in <xash>/addons
        // instead of <xash>/valve/addons and the game never saw them).
        const val CURRENT_VERSION = "v40"

        fun ensureInstalled(ctx: Context) {
                val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                val root = File(
                        ctx.getSharedPreferences("app_preferences", Context.MODE_PRIVATE)
                                .getString("game_path", null)
                                ?: (Environment.getExternalStorageDirectory().absolutePath + "/xash")
                )

                try {
                        val valveDir = File(root, "valve")

                        val metamodDir = File(valveDir, "addons/metamod/dlls")
                        val done = prefs.getString(KEY_VERSION, null) == CURRENT_VERSION &&
                                metamodDir.isDirectory && metamodDir.listFiles()?.isNotEmpty() == true

                        if (done)
                                return

                        val asset = ctx.assets.open("valve-addons.zip")
                        installZip(asset, root)
                        prefs.edit().putString(KEY_VERSION, CURRENT_VERSION).apply()
                        Log.i(TAG, "valve addons installed into ${valveDir.absolutePath}")
                } catch (e: Throwable) {
                        // never crash the launcher because of an optional addon pack,
                        // but leave a trace the user can send us for diagnosis
                        Log.e(TAG, "valve addons installation failed", e)
                        logError(root, "valve-addons (engine app)", e)
                }
        }

        /**
         * v9: append installation problems to <xash>/amxx-install-log.txt so the
         * user can send us the exact reason (permission denied, missing asset, ...)
         * instead of a generic "it gave an error".
         */
        internal fun logError(root: File, source: String, e: Throwable) {
                try {
                        val dir = if (root.isDirectory) root
                        else File(Environment.getExternalStorageDirectory(), "xash")
                        dir.mkdirs()

                        java.io.PrintWriter(java.io.FileWriter(File(dir, "amxx-install-log.txt"), true)).use { pw ->
                                pw.appendLine("--- ${java.util.Date()} ---")
                                pw.appendLine("source: $source")
                                pw.appendLine("${e.javaClass.name}: ${e.message}")
                                e.stackTrace.take(8).forEach { pw.appendLine("    at $it") }
                                if (e.cause != null) {
                                        pw.appendLine("cause: ${e.cause!!.javaClass.name}: ${e.cause!!.message}")
                                }
                                pw.appendLine()
                        }
                } catch (ignored: Throwable) {
                        // logging must never crash anything
                }
        }

        private fun installZip(input: java.io.InputStream, root: File) {
                input.use { stream ->
                        val zis = ZipInputStream(stream.buffered())
                        while (true) {
                                val entry = zis.nextEntry ?: break

                                if (entry.isDirectory) {
                                        File(root, entry.name).mkdirs()
                                        zis.closeEntry()
                                        continue
                                }

                                val outFile = File(root, entry.name)
                                outFile.parentFile?.mkdirs()

                                // preserve any real Half-Life data the user already has:
                                // our gameinfo.txt is only a fallback skeleton
                                if (outFile.name.equals("gameinfo.txt", ignoreCase = true) &&
                                        outFile.parentFile?.let { parent ->
                                                File(parent, "liblist.gam").isFile ||
                                                        File(parent, "gameinfo.txt").isFile
                                        } == true
                                ) {
                                        zis.closeEntry()
                                        continue
                                }

                                FileOutputStream(outFile).use { fos ->
                                        zis.copyTo(fos)
                                }
                                zis.closeEntry()
                        }
                }
        }
}
