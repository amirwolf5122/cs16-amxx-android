package su.xash.engine.ui.library

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.navigation.fragment.findNavController
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import su.xash.engine.BuildConfig
import su.xash.engine.R
import su.xash.engine.adapters.GameAdapter
import su.xash.engine.databinding.FragmentLibraryBinding


class LibraryFragment : Fragment(), MenuProvider {
	private var _binding: FragmentLibraryBinding? = null
	private val binding get() = _binding!!

	// show the blocking storage dialog at most once per session
	private var promptedForStorage = false

	private val libraryViewModel: LibraryViewModel by activityViewModels()

	private val startActivityForResult =
		registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
			if (checkStoragePermissions(prompt = false)) {
				libraryViewModel.reloadGames(requireContext())
			}
		}

	private val requiredPermissions = arrayOf(
		Manifest.permission.READ_EXTERNAL_STORAGE,
		Manifest.permission.WRITE_EXTERNAL_STORAGE
	)

	private val requestPermissionLauncher = registerForActivityResult(
		ActivityResultContracts.RequestMultiplePermissions()
	) { permissions ->
		val granted = permissions.entries.all { it.value }
		if (granted) {
			libraryViewModel.reloadGames(requireContext())
		} else {
			// never automatically re-request after a denial
			// (this used to loop the system permission dialog)
			Toast.makeText(requireContext(), R.string.file_access_message, Toast.LENGTH_LONG).show()
		}
	}

	private fun checkStoragePermissions( prompt: Boolean = true ): Boolean {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (Environment.isExternalStorageManager()) {
				return true
			}

			// show the blocking dialog at most once per session
			// (onResume / onActivityResult used to re-show it in a loop)
			if (prompt) {
				MaterialAlertDialogBuilder(requireContext()).apply {
					setTitle(R.string.file_access_required)
					setMessage(R.string.file_access_message)
					setPositiveButton(R.string.open_settings) { _, _ ->
						startActivityForResult.launch(
							Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).setData(
								Uri.fromParts("package", BuildConfig.APPLICATION_ID, null)
							)
						)
					}
					setNeutralButton(R.string.done_check_permissions) { dialog, _ ->
						dialog.dismiss()
					}
					setCancelable(false)
					show()
				}
			}

			return false
		} else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			val permissionsNeeded = requiredPermissions.filter {
				ContextCompat.checkSelfPermission(
					requireContext(),
					it
				) != PackageManager.PERMISSION_GRANTED
			}.toTypedArray()

			if (!permissionsNeeded.isEmpty()) {
				if (prompt) {
					val showRationale = permissionsNeeded.any {
						ActivityCompat.shouldShowRequestPermissionRationale(requireActivity(), it)
					}

					MaterialAlertDialogBuilder(requireContext()).apply {
						setTitle(R.string.external_storage_required)
						setMessage(R.string.external_storage_message)
						setPositiveButton(R.string.open_settings) { _, _ ->
							if (showRationale) {
								requestPermissionLauncher.launch(permissionsNeeded)
							} else {
								val intent =
									Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
										data =
											Uri.fromParts("package", requireContext().packageName, null)
									}
								startActivity(intent)
							}
						}
						setNeutralButton(R.string.done_check_permissions) { dialog, _ ->
							dialog.dismiss()
						}
						setCancelable(false)
						show()
					}
				}

				return false
			} else {
				return true
			}
		} else {
			return true
		}
	}

	override fun onCreateView(
		inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
	): View {
		_binding = FragmentLibraryBinding.inflate(inflater, container, false)

		val adapter = GameAdapter(libraryViewModel)
		binding.gamesList.adapter = adapter

		requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

		return binding.root
	}

	override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
		binding.swipeRefresh.setOnRefreshListener { libraryViewModel.reloadGames(requireContext()) }

		libraryViewModel.isReloading.observe(viewLifecycleOwner) {
			binding.swipeRefresh.isRefreshing = it
		}

		libraryViewModel.installedGames.observe(viewLifecycleOwner) {
			(binding.gamesList.adapter as GameAdapter).submitList(it)
		}

		if (!checkStoragePermissions(prompt = !promptedForStorage)) {
			promptedForStorage = true
		} else {
			libraryViewModel.reloadGames(requireContext())
		}
	}

	override fun onDestroyView() {
		super.onDestroyView()
		_binding = null
	}

	override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
		menuInflater.inflate(R.menu.menu_library, menu)
	}

	override fun onMenuItemSelected(menuItem: MenuItem): Boolean {
		when (menuItem.itemId) {
			R.id.action_settings -> {
				findNavController().navigate(R.id.action_libraryFragment_to_appSettingsFragment)
			}
		}

		return false
	}

	override fun onResume() {
		super.onResume()

		if (!checkStoragePermissions(prompt = !promptedForStorage)) {
			promptedForStorage = true
		} else {
			libraryViewModel.reloadGames(requireContext())
		}
	}
}
