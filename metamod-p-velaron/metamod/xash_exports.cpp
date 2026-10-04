// vi: set ts=4 sw=4 :
// xash_exports.cpp - Xash3D-specific server-library exports for metamod.
//
// The Xash3D FWGS engine resolves several optional interfaces DIRECTLY on the
// server library handle (svgame.hInstance) using dlsym():
//
//   - Server_GetPhysicsInterface  (physics_interface_t: SV_CreateEntity, ...)
//   - Server_GetBlendingInterface (studio model blending interface)
//   - per-classname entity spawn functions (worldspawn, info_player_start, ...)
//
// When metamod is loaded as the server library, these lookups hit METAMOD,
// not the real game DLL.  The entity spawn mechanism on Xash3D Android works
// like this: dlsym(svgame.hInstance, classname) fails -> engine falls back to
// svgame.physFuncs.SV_CreateEntity (filled via Server_GetPhysicsInterface),
// which in turn dlsym's the classname export inside the real game DLL.
//
// If metamod does not forward Server_GetPhysicsInterface, physFuncs stays
// empty, NO entity can be spawned ("No spawn function for ..." spam) and
// g_pGameRules stays NULL, which crashes the server on first
// CHalfLifeMultiplay::ServerActivate (NULL->ReadMultiplayCvars, this+0x54).
//
// On desktop x86 metamod-P covers this with the dlsym hook
// (osdep_linkent_linux), which is disabled on Android/aarch64, so plain
// pass-through exports are used here instead: they forward the engine's call
// verbatim to the game DLL (e.g. the YaPB proxy, which implements the
// physics interface on top of ReGameDLL).

#include <dlfcn.h>

#include "extdll.h"                     // always
#include "h_export.h"           // me / export macros
#include "metamod.h"            // GameDLL, etc
#include "log_meta.h"           // META_LOG / META_WARNING
#include "osdep_p.h"            // platform helpers

// Opaque pass-through signatures (the real implementations validate the
// version and fill the structures; we never touch the contents ourselves).
typedef int (*PHYSICAPI_PASS)(int version, void *physics_api, void *table);
typedef int (*BLENDIFACE_PASS)(int version, void **ppinterface, void *pstudio,
                                float *rotationmatrix, float *bonetransform);

static void *gamedll_resolve(const char *name) {
        if(!GameDLL.handle) {
                META_WARNING("dll: game DLL not loaded yet; cannot resolve '%s'", name);
                return(NULL);
        }
        return(dlsym(GameDLL.handle, name));
}

// ---------------------------------------------------------------------------
// Xash3D custom physics interface.
//
// The ENGINE fills svgame.physFuncs from this export. The real game DLL
// (hl / ReGameDLL) usually does NOT export Server_GetPhysicsInterface at
// all, so blindly forwarding the call is useless: the engine would zero
// physFuncs and every map entity would die with
// "No spawn function for ...".
//
// Instead we provide our OWN SV_CreateEntity which dlsym's the per-classname
// spawn export (worldspawn, info_player_start, ...) inside the game DLL —
// exactly what metamod-fwgs' osdep_linkent_xash.cpp does, adapted for
// metamod-P. All other physFuncs stay NULL (engine built-ins are used).
// ---------------------------------------------------------------------------
#ifndef SV_PHYSICS_INTERFACE_VERSION
#define SV_PHYSICS_INTERFACE_VERSION 6
#endif

typedef void (*mm_linkentity_func)( entvars_t *pev );

// Leading fields of the engine's physics_interface_t (see hlsdk physint.h).
// Everything after SV_CreateEntity may differ between engine builds, so only
// these two are written; the engine zeroes the remainder before the call.
struct mm_physics_interface_head {
        int version;
        int (*SV_CreateEntity)( edict_t *pent, const char *szName );
};

static int mm_DispatchCreateEntity( edict_t *pent, const char *szName ) {
        mm_linkentity_func spawn;

        if(!GameDLL.handle)
                return(-1);

        spawn = (mm_linkentity_func) dlsym(GameDLL.handle, szName);
        if(!spawn)
                return(-1);

        spawn(&pent->v);
        return(0);      // handled
}

extern "C" __attribute__((visibility("default")))
int Server_GetPhysicsInterface(int version, void *physics_api, void *table) {
        mm_physics_interface_head *iface = (mm_physics_interface_head *) table;

        (void) physics_api;

        if(!iface || version != SV_PHYSICS_INTERFACE_VERSION)
                return(0);

        if(!GameDLL.handle) {
                META_WARNING("dll: game DLL not loaded yet; cannot provide physics interface");
                return(0);
        }

        iface->version = SV_PHYSICS_INTERFACE_VERSION;
        iface->SV_CreateEntity = &mm_DispatchCreateEntity;
        META_LOG("dll: provided Xash3D physics interface (SV_CreateEntity -> game dll exports)");
        return(1);
}

// Xash3D/GoldSrc studio blending interface (server-side model animation).
extern "C" __attribute__((visibility("default")))
int Server_GetBlendingInterface(int version, void **ppinterface, void *pstudio,
                float *rotationmatrix, float *bonetransform) {
        void *fn= gamedll_resolve("Server_GetBlendingInterface");

        if(!fn)
                return(0);

        return(((BLENDIFACE_PASS) fn)(version, ppinterface, pstudio,
                                        rotationmatrix, bonetransform));
}
