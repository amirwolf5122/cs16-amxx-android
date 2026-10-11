// cs_ham_bots_api — Android library-anchor module.
//
// See moduleconfig.h for the rationale: the module exposes the library
// name "cs_ham_bots_api" through AMXX_Query (g_ModuleInfo.library), which
// the AMXX core adds to its library list when the .so is loaded
// (CModule::LoadModule -> AddLibrariesFromString). With the module present,
// plugins carrying "#pragma reqlib/loadlib cs_ham_bots_api" resolve their
// requirement at both the CALM (autoload) and Finalize (CheckModules)
// stages instead of falling into the "Can't find module file" bail-out.
//
// The runtime natives live in the Pawn plugin cs_ham_bots_api.amxx, which
// calls hamsandwich's RegisterHamFromEntity — identical to a PC AMXX
// install running Zombie Plague 5.0.
//
// amxxmodule.cpp (compiled from ../../public/sdk) supplies every export
// (AMXX_Query/AMXX_Attach/AMXX_Detach, g_ModuleInfo built from the
// MODULE_* macros above). No FN_AMXX_* hook is defined, so all lifecycle
// callbacks are no-ops — the module is deliberately inert.

// Keep a symbol in this translation unit so ndk-build always has a
// non-empty object for the module beyond the SDK TU.
extern "C" int amxx_cs_ham_bots_api_anchor(void)
{
	return 0;
}
