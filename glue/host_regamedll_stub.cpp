/*
 * host_regamedll_stub.cpp — host-only stand-in for the two ReGameDLL API
 * symbols that fakemeta imports and that normally come from the CS gamedll
 * (ReGameDLL-CS). The host CI chain runs against the HL gamedll, which does
 * not export them, so without this stub dlopen(fakemeta) fails and the
 * gamedata parsing path (the arm64 crash reported from the field) is never
 * exercised.
 *
 * RegamedllApi_Init() returning false makes fakemeta fall back to the
 * g_pGameRules gamedata lookup and gracefully disable the gamerules natives
 * — exactly the "no regamedll" behavior on non-CS games.
 */
extern "C" {

__attribute__((visibility("default"))) bool RegamedllApi_Init()
{
        return false;
}

/* Only dereferenced when RegamedllApi_Init() returned true — never here. */
__attribute__((visibility("default"))) void *ReGameHookchains = nullptr;

} // extern "C"
