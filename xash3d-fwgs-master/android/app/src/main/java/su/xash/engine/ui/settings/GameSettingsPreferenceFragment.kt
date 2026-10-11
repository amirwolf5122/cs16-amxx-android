package su.xash.engine.ui.settings

import android.content.Intent
import android.os.Bundle
import androidx.core.net.toUri
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import su.xash.engine.R
import su.xash.engine.model.Game
import su.xash.engine.model.GameLibDownloader
import java.io.File
import java.text.DateFormat
import java.util.Date

class GameSettingsPreferenceFragment(val game: Game) : PreferenceFragmentCompat() {
        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
                preferenceManager.sharedPreferencesName = game.basedir.name;
                setPreferencesFromResource(R.xml.game_preferences, rootKey);

                val packageList = findPreference<ListPreference>("package_name")!!
                packageList.entries = arrayOf(getString(R.string.app_name))
                packageList.entryValues = arrayOf(requireContext().packageName)

                if (packageList.value == null) {
                        packageList.setValueIndex(0);
                }

                val isCS = game.basedir.name.equals("cstrike", ignoreCase = true)
                                || game.basedir.name.equals("czero", ignoreCase = true)
                val isValve = game.basedir.name.equals("valve", ignoreCase = true)

                if (isCS || isValve) {
                        val enableAmxx = findPreference<SwitchPreferenceCompat>("enable_amxx")!!
                        enableAmxx.isVisible = true

                        val smaCompiler = findPreference<Preference>("sma_compiler")!!
                        smaCompiler.isVisible = true
                        smaCompiler.setOnPreferenceClickListener {
                                CompilerDialogFragment(game)
                                        .show(parentFragmentManager, "sma_compiler")
                                true
                        }
                }

                if (isCS) {
                        val enableAmxx = findPreference<SwitchPreferenceCompat>("enable_amxx")!!
                        val enableYaPBBots = findPreference<SwitchPreferenceCompat>("enable_yapb_bots")!!
                        enableYaPBBots.isVisible = true

                        // AMX Mod X and YaPB are fully independent now -- both
                        // can be enabled at the same time (metamod loads both from
                        // plugins.ini). Each switch only gates its own plugins.ini
                        // line through a marker file in the game dir, because
                        // plugins.ini itself is patched by the cs16client installer
                        // app, which cannot read this app's SharedPreferences.
                        enableAmxx.setOnPreferenceChangeListener { _, newValue ->
                                writeMetaMarker("amxmodx.disabled", newValue != true)
                                patchIniFromPrefs(newValue == true,
                                        preferenceManager.sharedPreferences
                                                ?.getBoolean("enable_yapb_bots", true) == true)
                                true
                        }
                        enableYaPBBots.setOnPreferenceChangeListener { _, newValue ->
                                writeMetaMarker("yapb.disabled", newValue != true)
                                patchIniFromPrefs(
                                        preferenceManager.sharedPreferences
                                                ?.getBoolean("enable_amxx", true) == true,
                                        newValue == true)
                                true
                        }
                }

                populateDownloadedBuildInfo()

                val separatePackages = findPreference<SwitchPreferenceCompat>("separate_libraries")!!
                val clientPackage = findPreference<ListPreference>("client_package")!!
                val serverPackage = findPreference<ListPreference>("server_package")!!
                separatePackages.setOnPreferenceChangeListener { _, newValue ->
                        if (newValue == true) {
                                packageList.isVisible = false
                                clientPackage.isVisible = true
                                serverPackage.isVisible = true
                        } else {
                                packageList.isVisible = true
                                clientPackage.isVisible = false
                                serverPackage.isVisible = false
                        }

                        true
                }
        }

        /** Create/remove a marker file under <gamedir>/addons/metamod
         *  so the cs16client plugins.ini patcher (a separate app that cannot
         *  read our SharedPreferences) can honour the per-game switches. */
        private fun writeMetaMarker(name: String, present: Boolean) {
                try {
                        val metaDir = File(game.basedir, "addons/metamod")
                        metaDir.mkdirs()
                        val marker = File(metaDir, name)
                        if (present) marker.createNewFile() else marker.delete()
                } catch (e: Exception) {
                        e.printStackTrace()
                }
        }

        /** Apply the new switch state to plugins.ini immediately (not
         *  only at the next launch), so bots can never keep spawning from a
         *  stale ini while the user believes YaPB is off. */
        private fun patchIniFromPrefs(amxxEnabled: Boolean, yapbEnabled: Boolean) {
                if (!amxxEnabled && !yapbEnabled)
                        return // metamod is bypassed entirely; ini content irrelevant

                su.xash.engine.model.MetamodIniPatcher.patch(
                        File(game.basedir, "addons/metamod"),
                        amxxEnabled, yapbEnabled, resolveCsNativeLibDir())
        }

        /** Find the cs16client APK native lib dir (holds the YaPB lib). */
        private fun resolveCsNativeLibDir(): String? {
                return try {
                        val pm = requireContext().packageManager
                        val candidates = arrayOf(
                                "su.xash.cs16clientamxx", "su.xash.cs16clientamxx.test",
                                "su.xash.cs16client.test", "su.xash.cs16client")
                        for (pn in candidates) {
                                try {
                                        pm.getPackageInfo(pn, 0).applicationInfo?.nativeLibraryDir
                                                ?.let { if (it.isNotEmpty()) it else null }
                                                ?.let { return it }
                                } catch (e: Exception) {
                                        // package not installed, try the next one
                                }
                        }
                        null
                } catch (e: Exception) {
                        null
                }
        }

        private fun populateDownloadedBuildInfo() {
                val downloader = GameLibDownloader(requireContext())
                val source = downloader.getSourceInfo(game.basedir.name) ?: return
                val downloadedAt = downloader.getDownloadTime(game.basedir.name)

                val urlPref = findPreference<Preference>("source_url")!!
                urlPref.isVisible = true
                urlPref.summary = source.url ?: "-"
                urlPref.isEnabled = source.url != null
                urlPref.setOnPreferenceClickListener {
                        source.url?.let {
                                startActivity(Intent(Intent.ACTION_VIEW, it.toUri()))
                        }
                        true
                }

                val branchPref = findPreference<Preference>("source_branch")!!
                branchPref.isVisible = true
                branchPref.summary = source.branch ?: "-"

                val commitPref = findPreference<Preference>("source_commit")!!
                commitPref.isVisible = true
                commitPref.summary = source.commit ?: "-"
                commitPref.isEnabled = source.commit != null && source.url != null
                commitPref.setOnPreferenceClickListener {
                        // FIXME: GitHub-styled URL!
                        val target = "${source.url!!.trimEnd('/')}/commit/${source.commit}"
                        startActivity(Intent(Intent.ACTION_VIEW, target.toUri()))
                        true
                }

                val timePref = findPreference<Preference>("downloaded_at")!!
                timePref.isVisible = true
                timePref.summary = if (downloadedAt > 0L)
                        DateFormat.getDateTimeInstance().format(Date(downloadedAt))
                else
                        "-"
        }
}
