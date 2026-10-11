package su.xash.engine.ui.settings

import android.graphics.Typeface
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
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
 * v8: advanced SMA -> AMXX compiler.
 *
 * Same engine as [CompilerDialogFragment] (bundled amxxpc from jniLibs) but
 * exposes the compiler options: output file name, extra include folders,
 * constants (sym=val), debug info (-d2), verbosity (-v2), optimizations
 * toggle, warnings-as-errors (-E) and stack size (-S). Shows the full
 * compiler log below.
 */
class FullCompilerDialogFragment(val game: Game, val preselectedScript: String?) : DialogFragment() {
        private lateinit var logView: TextView
        private lateinit var compileButton: Button
        private lateinit var outputEdit: EditText
        private lateinit var includesEdit: EditText
        private lateinit var definesEdit: EditText
        private lateinit var stackEdit: EditText
        private lateinit var debugCheck: CheckBox
        private lateinit var verboseCheck: CheckBox
        private lateinit var warningsCheck: CheckBox
        private lateinit var optimCheck: CheckBox
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

                root.addView(TextView(ctx).apply {
                        text = getString(R.string.compiler_full_title)
                        textSize = 18f
                        gravity = Gravity.CENTER
                })

                root.addView(TextView(ctx).apply {
                        text = scriptsDir.absolutePath
                        textSize = 12f
                        setPadding(0, dp(6), 0, dp(4))
                })

                if (scripts.isEmpty()) {
                        root.addView(TextView(ctx).apply {
                                text = getString(R.string.compiler_no_scripts)
                                setPadding(0, dp(6), 0, dp(6))
                        })
                } else {
                        val list = ListView(ctx).apply {
                                choiceMode = ListView.CHOICE_MODE_SINGLE
                                adapter = ArrayAdapter(ctx, android.R.layout.simple_list_item_single_choice,
                                        scripts.map { it.name })
                                val pre = preselectedScript
                                val idx = scripts.indexOfFirst { it.name == pre }.coerceAtLeast(0)
                                setItemChecked(idx, true)
                                selectedScript = scripts[idx]
                                setOnItemClickListener { _, _, position, _ -> selectedScript = scripts[position] }
                                // keep the script list compact inside the dialog
                                isVerticalScrollBarEnabled = true
                        }
                        root.addView(list, LinearLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, dp(140)))
                }

                fun addEdit(hintRes: Int, init: String = ""): EditText {
                        val edit = EditText(ctx).apply {
                                hint = getString(hintRes)
                                setText(init)
                                setSingleLine(true)
                                textSize = 13f
                        }
                        root.addView(edit, LinearLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
                        return edit
                }

                fun addCheck(textRes: Int, checked: Boolean): CheckBox {
                        val box = CheckBox(ctx).apply {
                                text = getString(textRes)
                                isChecked = checked
                                textSize = 14f
                        }
                        root.addView(box, LinearLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
                        return box
                }

                outputEdit = addEdit(R.string.compiler_output_name,
                        selectedScript?.name?.removeSuffix(".sma")?.plus(".amxx") ?: "")
                includesEdit = addEdit(R.string.compiler_includes)
                definesEdit = addEdit(R.string.compiler_defines)
                stackEdit = addEdit(R.string.compiler_stack)
                debugCheck = addCheck(R.string.compiler_debug, false)
                verboseCheck = addCheck(R.string.compiler_verbose, false)
                warningsCheck = addCheck(R.string.compiler_warnings, false)
                optimCheck = addCheck(R.string.compiler_optimizations, true)

                compileButton = Button(ctx).apply {
                        text = getString(R.string.compiler_compile)
                        isEnabled = selectedScript != null
                        setOnClickListener { compileSelected() }
                }
                root.addView(compileButton)

                logView = TextView(ctx).apply {
                        textSize = 12f
                        typeface = Typeface.MONOSPACE
                        setPadding(dp(4), dp(8), dp(4), dp(8))
                        setTextIsSelectable(true)
                }
                root.addView(ScrollView(ctx).apply {
                        addView(logView)
                }, LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, dp(200)))

                return android.app.AlertDialog.Builder(ctx)
                        .setView(root)
                        .setNegativeButton(android.R.string.cancel, null)
                        .create()
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

                val scriptingDir = File(game.basedir, "addons/amxmodx/scripting")
                val includeDir = File(scriptingDir, "include")
                val pluginsDir = File(game.basedir, "addons/amxmodx/plugins")
                if (!pluginsDir.isDirectory) pluginsDir.mkdirs()

                val outName = outputEdit.text.toString().trim()
                        .ifEmpty { script.name.removeSuffix(".sma") + ".amxx" }
                if (!outName.endsWith(".amxx", true)) {
                        appendLog("! output name must end with .amxx\n")
                        return
                }
                val out = File(pluginsDir, outName)

                compileButton.isEnabled = false
                appendLog(">>> amxxpc (full) ${script.name}\n")

                thread {
                        try {
                                val args = mutableListOf(amxxpc.absolutePath)

                                args += "-i" + includeDir.absolutePath
                                includesEdit.text.toString().split(',')
                                        .map { it.trim() }
                                        .filter { it.isNotEmpty() }
                                        .forEach { args += "-i$it" }

                                definesEdit.text.toString().split(',')
                                        .map { it.trim() }
                                        .filter { it.isNotEmpty() }
                                        .forEach { def ->
                                                // pawn syntax: bare "sym=val" (or "sym=") arguments
                                                args += if (def.startsWith("-")) def else def
                                        }

                                if (debugCheck.isChecked) args += "-d2"
                                if (verboseCheck.isChecked) args += "-v2"
                                if (warningsCheck.isChecked) args += "-E"
                                if (!optimCheck.isChecked) args += "-O0"

                                val stack = stackEdit.text.toString().trim()
                                if (stack.isNotEmpty()) args += "-S$stack"

                                args += "-o" + out.absolutePath
                                args += script.absolutePath

                                appendLog("cmd: ${args.joinToString(" ")}\n")

                                val pb = ProcessBuilder(args)
                                pb.redirectErrorStream(true)
                                val proc = pb.start()
                                val stdout = proc.inputStream.bufferedReader()
                                var line: String?
                                while (run { line = stdout.readLine(); line != null }) {
                                        appendLog((line ?: "") + "\n")
                                }
                                val code = proc.waitFor()
                                // old amxxpc builds exited 0 even on
                                // fatal compile errors and could leave a header-only
                                // stub .amxx (~36 bytes). Validate before claiming OK.
                                if (code == 0 && (!out.isFile || out.length() < 256L)) {
                                        out.delete()
                                        appendLog("=== FAILED (no valid plugin produced)\n")
                                } else if (code == 0)
                                        appendLog("=== OK: ${out.name} (${out.length()} bytes)\n")
                                else
                                        appendLog("=== FAILED (exit code $code)\n")
                        } catch (t: Throwable) {
                                appendLog("! ${t.javaClass.simpleName}: ${t.message}\n")
                        } finally {
                                activity.runOnUiThread {
                                        compileButton.isEnabled = true
                                        Toast.makeText(activity, R.string.compiler_done, Toast.LENGTH_SHORT).show()
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
