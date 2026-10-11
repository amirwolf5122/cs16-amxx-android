package su.xash.engine.model

import android.content.Context
import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.core.net.toUri
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import su.xash.engine.R
import su.xash.engine.XashActivity
import su.xash.engine.util.showDownloadProgressDialog
import java.io.File
import java.io.FileInputStream

class Game(val ctx: Context, val basedir: File, val gameInfoFile: File) {
        private var iconName = "game.ico"
        var title = "Unknown Game"
        var icon: Bitmap? = null
        var cover: Bitmap? = null

        val mobileHacksGames = arrayOf("aom", "bdlands", "biglolly", "bshift", "caseclosed",
                "hl_urbicide", "induction", "redempt", "secret",
                "sewer_beta", "tot", "valve", "vendetta")

        // a1ba: follow the behavior of Xash3D's game_launch.
        // for hl mods we put `valve` as game directory
        // for any other game that's not hl this string must be replaced with your
        // main game directory
        // mods always use -game command line parameter
        var defaultGameDir = "valve"

        private val pref = ctx.getSharedPreferences(basedir.name, Context.MODE_PRIVATE)

        // v7: environment variables built per launch (e.g. XASH3D_DISABLE_AMXX)
        private val envList = mutableListOf<String>()

        init {
                parseGameInfo(gameInfoFile)

                val iconFile = File(basedir, iconName)
                if (iconFile.exists()) {
                        icon = BitmapFactory.decodeFile(iconFile.path)
                }

                try {
                        cover = BackgroundBitmap.createBackground(basedir)
                } catch (e: Exception) {
                        e.printStackTrace()
                }
        }

        fun startEngine(ctx: Context) {
                val packageNames = getPackageNamesForGameDir(basedir.name)
                var externalGame = false
                var commandLineArgs = ""
                envList.clear()

                // v9: valve (Half-Life) AMX addons are installed by the ENGINE app
                // itself, exactly when the user launches the valve game -- cs16client
                // is fully separated from the valve addon pack.
                if (basedir.name.equals("valve", ignoreCase = true)) {
                        ValveAddonsInstaller.ensureInstalled(ctx.applicationContext)
                }

                // AMX Mod X per-game toggle. AMX Mod X and YaPB are
                // independent switches -- both can be on at the same time
                // (metamod loads both from plugins.ini). Each switch gates its
                // own plugins.ini line through a marker file next to it, because
                // plugins.ini is patched by the cs16client installer app, which
                // cannot read this app's SharedPreferences. Only when BOTH are
                // off does the engine bypass metamod entirely and load the
                // original server library directly. YaPB is a CS-only concept:
                // for other game dirs (valve) the old single-switch behaviour
                // is kept.
                val isCS = basedir.name.equals("cstrike", ignoreCase = true)
                                || basedir.name.equals("czero", ignoreCase = true)
                val enableAmxx = pref.getBoolean("enable_amxx", true)
                val enableYapb = isCS && pref.getBoolean("enable_yapb_bots", true)

                fun writeMetaMarker(name: String, present: Boolean) {
                        try {
                                val metaDir = File(basedir, "addons/metamod")
                                metaDir.mkdirs()
                                val marker = File(metaDir, name)
                                if (present) marker.createNewFile() else marker.delete()
                        } catch (e: Exception) {
                                e.printStackTrace()
                        }
                }

                if (!enableAmxx && !enableYapb)
                        envList += arrayOf("XASH3D_DISABLE_AMXX", "1")

                if (isCS) {
                        writeMetaMarker("amxmodx.disabled", !enableAmxx)
                        writeMetaMarker("yapb.disabled", !enableYapb)

                        // rewrite plugins.ini on THIS launch path too. It
                        // used to be patched only by the cs16client launcher, so
                        // starting the game from the engine icon kept a stale
                        // YaPB line and bots joined even with the switch off.
                        val csNativeLibDir = getPackageNamesForGameDir(basedir.name)
                                ?.firstNotNullOfOrNull { pn ->
                                        try {
                                                getGameLibDir(ctx, pn)
                                        } catch (e: Exception) {
                                                null
                                        }
                                }
                        MetamodIniPatcher.patch(
                                File(basedir, "addons/metamod"),
                                enableAmxx, enableYapb, csNativeLibDir)
                }

                if (basedir.name != defaultGameDir)
                        commandLineArgs += "-game ${basedir.name} "

                if (packageNames != null) {
                        if (packageNames.contains("su.xash.engine")) {
                                commandLineArgs += "-dll @hl "
                        } else if (packageNames.any { it.startsWith("su.xash.cs16client") }) {
                                // do NOT pass "-dll @yapb" when AMX Mod X is active.
                                // The engine resolves entity spawn functions with
                                // dlsym(svgame.hInstance, "worldspawn"), but the chain
                                // metamod -> yapb-wrapper breaks that: the wrapper does
                                // not re-export the entity symbols, so every map entity
                                // failed with "No spawn function". YaPB is now loaded as
                                // a metamod plugin instead (plugins.ini, patched by the
                                // cs16client installer with the absolute lib path), and
                                // metamod chains directly to libcs (which exports the
                                // entity symbols).
                                externalGame = true
                        }
                }

                commandLineArgs += pref.getString("arguments", "-console -log") ?: ""

                // engine-only games (valve/HL): the bundled libhl_android_<arch>.so
                // lives in THIS app's nativeLibraryDir; expose it as XASH3D_GAMELIBDIR so
                // the library loader's first tier and metamod's (-D__ANDROID__) gamedll
                // autodetection find it. Without this, metamod runs with no gamedll and
                // every map entity fails with "No spawn function". External games
                // (cs16client) already pass their own dir via the "gamelibdir" extra.
                if (!externalGame) {
                        envList += arrayOf("XASH3D_GAMELIBDIR", ctx.applicationInfo.nativeLibraryDir)
                }

                if (externalGame && packageNames != null) {
                        var packageName: String? = null
                        var gameLibDir: String? = null

                        for (pn in packageNames) {
                                gameLibDir = try {
                                        getGameLibDir(ctx, pn)
                                } catch (e: PackageManager.NameNotFoundException) {
                                        null
                                } catch (e: Exception) {
                                        e.printStackTrace()
                                        null
                                }

                                if (gameLibDir != null) {
                                        packageName = pn
                                        break
                                }
                        }

                        if (gameLibDir == null) {
                                MaterialAlertDialogBuilder(ctx).apply {
                                        setTitle(R.string.game_apk_required)
                                        setMessage(R.string.game_apk_message)
                                        setPositiveButton(R.string.game_apk_install) { _, _ ->
                                                val intent = Intent(Intent.ACTION_VIEW,
                                                        getDownloadPageForGameDir(basedir.name).toUri())
                                                ctx.startActivity(intent)
                                        }
                                        show()
                                }
                                return
                        }

                        launchEngine(ctx, commandLineArgs, packageName = packageName, gameLibDir = gameLibDir)
                        return
                }

                if (packageNames == null) {
                        // Unknown game - try to use downloaded libraries from hlsdk-mega-build
                        val downloader = GameLibDownloader(ctx)
                        val args = commandLineArgs

                        if (downloader.isDownloaded(basedir.name)) {
                                downloader.logExistingLibs(basedir.name)
                                launchEngine(ctx, args)
                                return
                        }

                        val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
                        scope.launch {
                                when (val r = downloader.lookupBuild(basedir.name)) {
                                        is GameLibDownloader.Lookup.Available -> showDownloadDialog(ctx, downloader, args)
                                        is GameLibDownloader.Lookup.NotInManifest -> launchEngine(ctx, args)
                                        is GameLibDownloader.Lookup.Error -> showManifestErrorDialog(ctx, args, r.cause)
                                }
                        }
                        return
                }

                launchEngine(ctx, commandLineArgs)
        }

        private fun showDownloadDialog(ctx: Context, downloader: GameLibDownloader, commandLineArgs: String) {
                showDownloadProgressDialog(
                        ctx = ctx,
                        titleRes = R.string.downloading_game_libs,
                        cancelable = true,
                        scope = CoroutineScope(Dispatchers.Main + SupervisorJob()),
                        download = { onProgress -> downloader.download(basedir.name, onProgress) },
                        onSuccess = { launchEngine(ctx, commandLineArgs) },
                )
        }

        private fun showManifestErrorDialog(ctx: Context, commandLineArgs: String, cause: Throwable) {
                MaterialAlertDialogBuilder(ctx)
                        .setTitle(R.string.manifest_error_title)
                        .setMessage(ctx.getString(R.string.manifest_error_message, cause.message ?: cause.javaClass.simpleName))
                        .setPositiveButton(R.string.launch_anyway) { _, _ -> launchEngine(ctx, commandLineArgs) }
                        .setNegativeButton(android.R.string.cancel, null)
                        .show()
        }

        private fun launchEngine(
                ctx: Context,
                commandLineArgs: String,
                packageName: String? = null,
                gameLibDir: String? = null,
                env: List<String> = envList
        ) {
                ctx.startActivity(Intent(ctx, XashActivity::class.java).apply {
                        flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK

                        putExtra("gamedir", defaultGameDir)
                        putExtra("argv", commandLineArgs)
                        putExtra("usevolume", pref.getBoolean("use_volume_buttons", false))
                        putExtra("basedir", basedir.parent)

                        if (gameLibDir != null) putExtra("gamelibdir", gameLibDir)
                        if (packageName != null) putExtra("package", packageName)
                        if (env.isNotEmpty()) putExtra("env", env.toTypedArray())
                })
        }

        private fun parseGameInfo(file: File) {
                FileInputStream(file).use { inputStream ->
                        inputStream.bufferedReader().use { reader ->
                                reader.forEachLine {
                                        val tokens = it.split("\\s+".toRegex(), limit = 2)
                                        if (tokens.size >= 2) {
                                                val k = tokens[0]
                                                val v = tokens[1].trim('"')

                                                if (k == "title" || k == "game") title = v
                                                if (k == "icon") iconName = v
                                        }
                                }
                        }
                }
        }

        private fun getPackageNamesForGameDir(gamedir: String): Array<String>? {
                if (gamedir.equals("cstrike", ignoreCase = true)
                        || gamedir.equals("czero", ignoreCase = true))
                        // v8.1: accept both the AMXX build and the vanilla build,
                        // whichever is installed (AMXX variant is preferred)
                        return arrayOf("su.xash.cs16clientamxx", "su.xash.cs16clientamxx.test",
                                "su.xash.cs16client.test", "su.xash.cs16client")

                if (gamedir.equals("tfc", ignoreCase = true))
                        return arrayOf("su.xash.tf15client.test", "su.xash.tf15client")

                // mobile_hacks hlsdk-portable branch allows us to have few more mods out of the box
                if (mobileHacksGames.any { it.equals(gamedir, ignoreCase = true) })
                        return arrayOf("su.xash.engine")

                return null
        }

        private fun getDownloadPageForGameDir(gamedir: String): String {
                if (gamedir.equals("cstrike", ignoreCase = true)
                        || gamedir.equals("czero", ignoreCase = true))
                        return "https://github.com/Velaron/cs16-client/releases/download/continuous/CS16Client-Android.apk"

                if (gamedir.equals("tfc", ignoreCase = true))
                        return "https://github.com/Velaron/tf15-client/releases/download/continuous/TF15Client-Android.apk"

                // just so we don't return null
                return "https://github.com/FWGS/xash3d-fwgs/releases/download/continuous/xash3d-fwgs-android.apk"
        }

        private fun getGameLibDir(ctx: Context, packageName: String): String? {
                val packageInfo: PackageInfo = ctx.packageManager.getPackageInfo(packageName, 0)
                return packageInfo.applicationInfo?.nativeLibraryDir
        }

        companion object {
                fun getGames(ctx: Context, root: File): List<Game> {
                        val games = mutableListOf<Game>()

                        root.listFiles()?.forEach {
                                if (it.isDirectory) {
                                        val subDirGameInfoFile = checkIfGamedir(it)
                                        if (subDirGameInfoFile != null) {
                                                games.add(Game(ctx, it, subDirGameInfoFile))
                                        }
                                }
                        }

                        return games
                }

                fun checkIfGamedir(gamedir: File): File? {
                        gamedir.listFiles()?.forEach {
                                if (it.isFile) {
                                        if (it.name.equals("liblist.gam", ignoreCase = true))
                                                return it

                                        if (it.name.equals("gameinfo.txt", ignoreCase = true))
                                                return it
                                }
                        }

                        return null
                }
        }
}
