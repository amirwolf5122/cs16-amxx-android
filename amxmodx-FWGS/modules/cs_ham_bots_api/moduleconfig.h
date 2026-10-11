// vim: set ts=4 sw=4 tw=99 noet:
//
// Module Config — cs_ham_bots_api (Android port)
//
// This module does not implement any natives. It only publishes the
// library name "cs_ham_bots_api" so that plugins compiled with
//   #pragma reqlib cs_ham_bots_api   (zp50_*.amxx, zombieplague.amxx, ...)
//   #pragma loadlib cs_ham_bots_api
// resolve their library requirement without the AMXX core logging
// "Can't find module file: .../libamxx_cs_ham_bots_api.so" for every
// zombie-plugin load.
//
// The actual natives (RegisterHamBots / EnableHamForwardBots /
// DisableHamForwardBots) are provided by the companion Pawn plugin
// addons/amxmodx/plugins/cs_ham_bots_api.amxx ("[CS] Ham Hooks for
// Bots API" by WiLS, shipped enabled at the top of configs/plugins.ini),
// which implements them on top of the hamsandwich module
// (RegisterHamFromEntity) — the same architecture every PC ZP 5.0
// server uses.
//
#ifndef __MODULECONFIG_H__
#define __MODULECONFIG_H__

#include <amxmodx_version.h>

// Module info
#define MODULE_NAME "CS Ham Bots API"
#define MODULE_VERSION AMXX_VERSION
#define MODULE_AUTHOR "AMX Mod X Dev Team / WiLS"
#define MODULE_URL "https://www.amxmodx.org/"
#define MODULE_LOGTAG "CSHAMBOT"
#define MODULE_LIBRARY "cs_ham_bots_api"
#define MODULE_LIBCLASS ""

#endif // __MODULECONFIG_H__
