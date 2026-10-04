// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Fakemeta Module
//

#ifndef _INCLUDE_TR_H
#define _INCLUDE_TR_H

#include <amtl/am-string.h>

extern TraceResult *gfm_tr;

//these also don't fit in here but gaben does not care.  GABEN DOES NOT CARE!!!
extern TraceResult g_tr_2;
extern clientdata_t g_cd_glb;
extern clientdata_t *g_cd_hook;
extern entity_state_t g_es_glb;
extern entity_state_t *g_es_hook;
extern usercmd_t g_uc_glb;
extern usercmd_t *g_uc_hook;

// 64-bit-safe opaque handles for engine structs that cross
// the AMX boundary.  Pawn cells are 32-bit, so on 64-bit hosts (arm64) a raw
// struct pointer gets truncated when handed to a plugin through a forward,
// and any native casting that cell back to a pointer dereferences garbage
// (SIGSEGV inside get_uc during FM_CmdStart with real mods such as
// Zombie Plague 4.3).  Hooks and allocating natives (create_tr2/create_kvd)
// hand out slot cookies instead; the natives resolve the cookie back:
//   handle 0        -> classic global buffers (g_tr_2 / g_cd_glb / g_es_glb / g_uc_glb / g_kvd_glb)
//   handle 1..8     -> hook ring, short lived, generation guarded
//   handle 9..40    -> stable pool backing create_tr2 / create_kvd objects
// The generation counter is global to the module so a stale (slot, gen) pair
// can never collide with a live entry; stale handles resolve to nullptr and
// the natives report a clean AMX error instead of crashing.
struct FM_HandleSlot
{
        void     *ptr;
        uint32_t  gen;
        bool      used;      // meaningful for stable-pool slots only
};

enum
{
        FM_HANDLE_SLOTS    = 40,
        FM_HANDLE_RING     = 8,
        FM_HANDLE_GEN_BITS = 25,
        FM_HANDLE_GEN_MASK = (1 << FM_HANDLE_GEN_BITS) - 1
};

extern FM_HandleSlot g_fm_trace_slots[FM_HANDLE_SLOTS];  // TraceResult (hooks + create_tr2)
extern FM_HandleSlot g_fm_cdata_slots[FM_HANDLE_SLOTS];  // clientdata_t (FM_UpdateClientData)
extern FM_HandleSlot g_fm_estate_slots[FM_HANDLE_SLOTS]; // entity_state_t (AddToFullPack/CreateBaseline/...)
extern FM_HandleSlot g_fm_ucmd_slots[FM_HANDLE_SLOTS];   // usercmd_t (FM_CmdStart)
extern FM_HandleSlot g_fm_kvd_slots[FM_HANDLE_SLOTS];    // KeyValueData (FM_KeyValue + create_kvd)
extern FM_HandleSlot g_fm_info_slots[FM_HANDLE_SLOTS];   // infobuffer (EngFunc_GetInfoKeyBuffer NULL-edict case)

cell  FM_AcquireHookHandle(FM_HandleSlot *slots, const void *ptr);
cell  FM_AcquireStableHandle(FM_HandleSlot *slots, const void *ptr);
void *FM_ResolveHandle(FM_HandleSlot *slots, cell handle);
void  FM_ReleaseStableHandle(FM_HandleSlot *slots, cell handle);

// Typed resolvers shared by the natives, engfunc and dllfunc.  handle 0
// selects the classic global buffer; an unknown/stale handle logs a clean
// AMX error and returns nullptr so callers can bail out without crashing.
TraceResult   *FM_ResolveTrace(AMX *amx, cell handle);
clientdata_t  *FM_ResolveClientData(AMX *amx, cell handle);
entity_state_t *FM_ResolveEntityState(AMX *amx, cell handle);
usercmd_t     *FM_ResolveUserCmd(AMX *amx, cell handle);
KeyValueData  *FM_ResolveKvd(AMX *amx, cell handle);

// Infobuffer handles are encoded deterministically so a cached handle never
// goes stale: pfnGetInfoKeyBuffer(edict) == edict index + 1 (1..34).  Only
// the rare NULL-edict buffer (index -1) lives in the ring table and its
// handles are always >= 64, so the two namespaces never overlap.
#define FM_INFO_HANDLE(edict_index)   ((cell)((edict_index) + 1))
#define FM_INFO_HANDLE_IS_EDICT(h)    ((h) > 0 && (h) <= 34)

struct KVD_Wrapper
{
        KeyValueData kvd;
        ke::AString cls;
        ke::AString key;
        ke::AString val;
};

extern KVD_Wrapper g_kvd_glb;

enum
{
        TR_AllSolid,
        TR_StartSolid,
        TR_InOpen,
        TR_InWater,
        TR_flFraction,
        TR_vecEndPos,
        TR_flPlaneDist,
        TR_vecPlaneNormal,
        TR_pHit,
        TR_iHitgroup,
};

enum KeyValue
{
        KV_ClassName,
        KV_KeyName,
        KV_Value,
        KV_fHandled
};

enum ClientData
{
        CD_Origin,
        CD_Velocity,
        CD_ViewModel,
        CD_PunchAngle,
        CD_Flags,
        CD_WaterLevel,
        CD_WaterType,
        CD_ViewOfs,
        CD_Health,
        CD_bInDuck,
        CD_Weapons,
        CD_flTimeStepSound,
        CD_flDuckTime,
        CD_flSwimTime,
        CD_WaterJumpTime,
        CD_MaxSpeed,
        CD_FOV,
        CD_WeaponAnim,
        CD_ID,
        CD_AmmoShells,
        CD_AmmoNails,
        CD_AmmoCells,
        CD_AmmoRockets,
        CD_flNextAttack,
        CD_tfState,
        CD_PushMsec,
        CD_DeadFlag,
        CD_PhysInfo,
        CD_iUser1,
        CD_iUser2,
        CD_iUser3,
        CD_iUser4,
        CD_fUser1,
        CD_fUser2,
        CD_fUser3,
        CD_fUser4,
        CD_vUser1,
        CD_vUser2,
        CD_vUser3,
        CD_vUser4
};

enum EntityState
{
        // Fields which are filled in by routines outside of delta compression
        ES_EntityType,
        // Index into cl_entities array for this entity
        ES_Number,
        ES_MsgTime,

        // Message number last time the player/entity state was updated
        ES_MessageNum,

        // Fields which can be transitted and reconstructed over the network stream
        ES_Origin,
        ES_Angles,

        ES_ModelIndex,
        ES_Sequence,
        ES_Frame,
        ES_ColorMap,
        ES_Skin,
        ES_Solid,
        ES_Effects,
        ES_Scale,
        ES_eFlags,

        // Render information
        ES_RenderMode,
        ES_RenderAmt,
        ES_RenderColor,
        ES_RenderFx,

        ES_MoveType,
        ES_AnimTime,
        ES_FrameRate,
        ES_Body,
        ES_Controller,
        ES_Blending,
        ES_Velocity,

        // Send bbox down to client for use during prediction
        ES_Mins,
        ES_Maxs,

        ES_AimEnt,
        // If owned by a player, the index of that player (for projectiles)
        ES_Owner,

        // Friction, for prediction
        ES_Friction,
        // Gravity multiplier
        ES_Gravity,

        // PLAYER SPECIFIC
        ES_Team,
        ES_PlayerClass,
        ES_Health,
        ES_Spectator,
        ES_WeaponModel,
        ES_GaitSequence,
        // If standing on conveyor, e.g.
        ES_BaseVelocity,
        // Use the crouched hull, or the regular player hull
        ES_UseHull,
        // Latched buttons last time state updated
        ES_OldButtons,
        // -1 = in air, else pmove entity number
        ES_OnGround,
        ES_iStepLeft,
        // How fast we are falling
        ES_flFallVelocity,

        ES_FOV,
        ES_WeaponAnim,

        // Parametric movement overrides
        ES_StartPos,
        ES_EndPos,
        ES_ImpactTime,
        ES_StartTime,

        // For mods
        ES_iUser1,
        ES_iUser2,
        ES_iUser3,
        ES_iUser4,
        ES_fUser1,
        ES_fUser2,
        ES_fUser3,
        ES_fUser4,
        ES_vUser1,
        ES_vUser2,
        ES_vUser3,
        ES_vUser4
};

enum UserCmd
{
        UC_LerpMsec,            // Interpolation time on client
        UC_Msec,                        // Duration in ms of command
        UC_ViewAngles,          // Command view angles

        // Intended velocities
        UC_ForwardMove,         // Forward velocity
        UC_SideMove,            // Sideways velocity
        UC_UpMove,                      // Upward velocity
        UC_LightLevel,          // Light level at spot where we are standing
        UC_Buttons,                     // Attack buttons
        UC_Impulse,                     // Impulse command issued
        UC_WeaponSelect,        // Current weapon id

        // Experimental player impact stuff
        UC_ImpactIndex,
        UC_ImpactPosition
};

extern AMX_NATIVE_INFO tr_Natives[];
extern AMX_NATIVE_INFO ext2_natives[];

#endif //_INCLUDE_TR_H

