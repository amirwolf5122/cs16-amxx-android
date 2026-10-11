# AMXXOnAndroid
# Copyright (C) 2017 a1batross

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(XASH3D_CONFIG)

LOCAL_MODULE := amxx_cstrike

LOCAL_C_INCLUDES += \
        $(HLSDK)/dlls \
        $(HLSDK)/public \
        $(HLSDK)/common \
        $(HLSDK)/engine \
        $(HLSDK)/pm_shared \
        $(METAMOD)/metamod \
        
LOCAL_CFLAGS += -DHAVE_STDINT_H
# match the module's own CMakeLists.txt build. The
# "hacks" path dereferences HLDS/ReHLDS server_static_t/server_t globals,
# whose layout does not exist in the Xash3D engine (svs/sv are private and
# the structs differ) — on Xash that left cs_set_user_model broken (model
# queue never processed => zombie/team model changes never applied) and
# cs_set_user_model(.., update_index=true) returning 0. The NO_HACKS path
# uses pure engfuncs (SetClientKeyValue -> FCL_RESEND_USERINFO), which the
# Xash3D engine implements fully.
LOCAL_CFLAGS += -DNO_HACKS

LOCAL_SRC_FILES := \
        ../../../public/sdk/amxxmodule.cpp \
        CstrikeMain.cpp \
        CstrikePlayer.cpp \
        CstrikeNatives.cpp \
        CstrikeHacks.cpp \
        CstrikeUtils.cpp \
        CstrikeUserMessages.cpp \
        CstrikeItemsInfos.cpp \
        ../../../public/memtools/MemoryUtils.cpp \
        ../../../public/memtools/CDetour/detours.cpp \
        ../../../public/memtools/CDetour/asm/asm.c \
        ../../../public/resdk/mod_rehlds_api.cpp \
        ../../../public/resdk/mod_regamedll_api.cpp \

include $(BUILD_SHARED_LIBRARY)
