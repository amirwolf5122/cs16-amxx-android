package su.xash.engine.ui.settings

import android.graphics.Typeface
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.DialogFragment
import su.xash.engine.R
import su.xash.engine.model.Game
import java.io.File
import kotlin.concurrent.thread

/**
 * v7: in-app SMA -> AMXX compiler.
 *
 * Lists *.sma scripts from <gamedir>/addons/amxmodx/scripting, compiles the
 * selected script with the bundled amxxpc binary (packaged as libamxxpc.so in
 * the engine jniLibs) against <gamedir>/addons/amxmodx/scripting/include and
 * writes the result into <gamedir>/addons/amxmodx/plugins/<name>.amxx.
 * The full compiler output (the log) is shown below the controls.
 */
class CompilerDialogFragment(val game: Game) : DialogFragment() {
        private lateinit var logView: TextView
        private lateinit var compileButton: Button
        private var allButtons: List<Button> = emptyList()
        private var selectedScript: File? = null

        override fun onCreateDialog(savedInstanceState: Bundle?): android.app.Dialog {
                val ctx = requireContext()
                val scriptsDir = File(game.basedir, "addons/amxmodx/scripting")
                val scripts = (scriptsDir.listFiles { f -> f.isFile && f.name.endsWith(".sma", true) }
                        ?: arrayOf()).sortedBy { it.name.lowercase() }

                val density = resources.displayMetrics.density
                fun dp(v: Int) = (v * density).toInt()

                val root = LinearLayout(ctx).apply {
                        orientation = LinearLayout.VERTICAL
                        setPadding(dp(16), dp(12), dp(16), dp(12))
                }

                val title = TextView(ctx).apply {
                        text = getString(R.string.compiler_title)
                        textSize = 18f
                        gravity = Gravity.CENTER
                }
                root.addView(title)

                val scriptsLabel = TextView(ctx).apply {
                        text = scriptsDir.absolutePath
                        textSize = 12f
                        setPadding(0, dp(8), 0, dp(4))
                }
                root.addView(scriptsLabel)

                if (scripts.isEmpty()) {
                        root.addView(TextView(ctx).apply {
                                text = getString(R.string.compiler_no_scripts)
                                setPadding(0, dp(8), 0, dp(8))
                        })
                } else {
                        val list = ListView(ctx).apply {
                                choiceMode = ListView.CHOICE_MODE_SINGLE
                                adapter = ArrayAdapter(ctx, android.R.layout.simple_list_item_single_choice,
                                        scripts.map { it.name })
                                setItemChecked(0, true)
                                selectedScript = scripts[0]
                                setOnItemClickListener { _, _, position, _ -> selectedScript = scripts[position] }
                        }
                        root.addView(list, LinearLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, dp(180)))
                }

                compileButton = Button(ctx).apply {
                        text = getString(R.string.compiler_compile)
                        isEnabled = selectedScript != null
                        setOnClickListener { compileSelected() }
                }
                root.addView(compileButton)

                // v8.1: compile every .sma script in one go
                val compileAllButton = Button(ctx).apply {
                        text = getString(R.string.compiler_compile_all)
                        isEnabled = scripts.isNotEmpty()
                        setOnClickListener { compileAll() }
                }
                root.addView(compileAllButton)
                allButtons = listOf(compileButton, compileAllButton)

                // v8: advanced mode with custom flags, defines, includes, output name
                root.addView(Button(ctx).apply {
                        text = getString(R.string.compiler_full)
                        isEnabled = selectedScript != null
                        setOnClickListener {
                                dismiss()
                                FullCompilerDialogFragment(game, selectedScript?.name)
                                        .show(parentFragmentManager, "sma_compiler_full")
                        }
                })

                logView = TextView(ctx).apply {
                        textSize = 12f
                        typeface = Typeface.MONOSPACE
                        setPadding(dp(4), dp(8), dp(4), dp(8))
                        setTextIsSelectable(true)
                }
                root.addView(ScrollView(ctx).apply {
                        addView(logView)
                }, LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, dp(220)))

                return android.app.AlertDialog.Builder(ctx)
                        .setView(root)
                        .setNegativeButton(android.R.string.cancel, null)
                        .create()
        }

        private fun setBusy(busy: Boolean) {
                activity?.runOnUiThread {
                        allButtons.forEach { it.isEnabled = !busy }
                }
        }

        /** Blocking single-script compile; returns amxxpc exit code. */
        private fun compileOne(amxxpc: File, includeDir: File, script: File): Int {
                val pluginsDir = File(game.basedir, "addons/amxmodx/plugins")
                if (!pluginsDir.isDirectory) pluginsDir.mkdirs()
                val out = File(pluginsDir, script.name.removeSuffix(".sma") + ".amxx")

                appendLog(">>> amxxpc ${script.name}\n")

                val args = mutableListOf(
                        amxxpc.absolutePath,
                        "-i" + includeDir.absolutePath,
                        "-o" + out.absolutePath,
                        script.absolutePath,
                )
                val pb = ProcessBuilder(args)
                pb.redirectErrorStream(true)
                val proc = pb.start()
                val stdout = proc.inputStream.bufferedReader()
                var line: String?
                while (run { line = stdout.readLine(); line != null }) {
                        appendLog((line ?: "") + "\n")
                }
                val code = proc.waitFor()
                // old amxxpc builds exited 0 even on fatal compile
                // errors and could leave a header-only stub .amxx (~36 bytes).
                // Only report OK when a plausible plugin file actually exists.
                if (code == 0 && (!out.isFile || out.length() < 256L)) {
                        out.delete()
                        appendLog("=== FAILED: ${script.name} (no valid plugin produced)\n")
                        return 1
                }
                appendLog(if (code == 0)
                        "=== OK: ${out.name} (${out.length()} bytes)\n"
                else
                        "=== FAILED: ${script.name} (exit code $code)\n")
                return code
        }

        private fun compileSelected() {
                val activity = activity ?: return
                val script = selectedScript ?: return

                val nativeDir = requireContext().applicationInfo.nativeLibraryDir
                val amxxpc = File(nativeDir, "libamxxpc.so")
                if (!amxxpc.isFile) {
                        appendLog("! compiler binary not found: ${amxxpc.absolutePath}\n")
                        return
                }

                val includeDir = File(game.basedir, "addons/amxmodx/scripting/include")

                setBusy(true)
                appendLog(">>> amxxpc ${script.name}\n")

                thread {
                        try {
                                compileOne(amxxpc, includeDir, script)
                        } catch (t: Throwable) {
                                appendLog("! ${t.javaClass.simpleName}: ${t.message}\n")
                        } finally {
                                setBusy(false)
                                activity.runOnUiThread {
                                        Toast.makeText(activity, R.string.compiler_done, Toast.LENGTH_SHORT).show()
                                }
                        }
                }
        }

        /** v8.1: compile every .sma in the scripting dir, one after another. */
        private fun compileAll() {
                val activity = activity ?: return
                val scriptsDir = File(game.basedir, "addons/amxmodx/scripting")
                val scripts = (scriptsDir.listFiles { f -> f.isFile && f.name.endsWith(".sma", true) }
                        ?: arrayOf()).sortedBy { it.name.lowercase() }
                if (scripts.isEmpty()) return

                val nativeDir = requireContext().applicationInfo.nativeLibraryDir
                val amxxpc = File(nativeDir, "libamxxpc.so")
                if (!amxxpc.isFile) {
                        appendLog("! compiler binary not found: ${amxxpc.absolutePath}\n")
                        return
                }

                val includeDir = File(game.basedir, "addons/amxmodx/scripting/include")

                setBusy(true)
                appendLog(">>> compiling ALL ${scripts.size} scripts...\n")

                thread {
                        var ok = 0
                        var failed = 0
                        try {
                                for (script in scripts) {
                                        try {
                                                if (compileOne(amxxpc, includeDir, script) == 0) ok++ else failed++
                                        } catch (t: Throwable) {
                                                failed++
                                                appendLog("! ${script.name}: ${t.javaClass.simpleName}: ${t.message}\n")
                                        }
                                }
                                appendLog("=== DONE: $ok succeeded, $failed failed (of ${scripts.size})\n")
                        } finally {
                                setBusy(false)
                                activity.runOnUiThread {
                                        Toast.makeText(activity,
                                                "${activity.getString(R.string.compiler_done)}: $ok/${scripts.size}",
                                                Toast.LENGTH_SHORT).show()
                                }
                        }
                }
        }

        private fun appendLog(text: String) {
                if (!isAdded) return
                activity?.runOnUiThread {
                        logView.append(text)
                }
        }
}
