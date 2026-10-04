# Aggregator: builds AMX Mod X core + selected modules for Android.
# Repo version — all paths come from the ndk-build command line:
#   AMXX_SRC / XASH_FS / MM_SRC / HLSDK_SRC / GLUE_DIR
LOCAL_PATH := $(call my-dir)

# where the glue files live (command-line value wins)
GLUE_DIR ?= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
AMXX_SRC ?= $(GLUE_DIR)/../amxmodx-FWGS
XASH_FS ?= $(GLUE_DIR)/../xash3d-fwgs-master
MM_SRC ?= $(GLUE_DIR)/../metamod-fwgs
HLSDK_SRC ?= $(MM_SRC)/hlsdk

# a1ba's module makefiles include $(XASH3D_CONFIG) — point it at ours
XASH3D_CONFIG := $(GLUE_DIR)/XASH3D_CONFIG.mk

AMXXROOT := $(AMXX_SRC)

include $(AMXXROOT)/amxmodx/Android.mk
include $(AMXXROOT)/modules/engine/Android.mk
include $(AMXXROOT)/modules/fun/Android.mk
include $(AMXXROOT)/modules/fakemeta/Android.mk
include $(AMXXROOT)/modules/cstrike/cstrike/Android.mk
include $(AMXXROOT)/modules/cstrike/csx/Android.mk
include $(AMXXROOT)/modules/nvault/Android.mk
include $(AMXXROOT)/modules/sockets/Android.mk
include $(AMXXROOT)/modules/regex/Android.mk
include $(AMXXROOT)/modules/geoip/Android.mk
include $(AMXXROOT)/modules/sqlite/Android.mk
include $(AMXXROOT)/modules/json/Android.mk
include $(AMXXROOT)/modules/hamsandwich/Android.mk
# library anchor for the cs_ham_bots_api library (zombie plague mods);
# natives come from the staged Pawn plugin cs_ham_bots_api.amxx
include $(AMXXROOT)/modules/cs_ham_bots_api/Android.mk

$(call import-add-path,$(AMXXROOT))
$(call import-module,third_party)

# hamsandwich links against our libffcall-compatible ARM trampoline
# shim (LOCAL_STATIC_LIBRARIES += trampoline in modules/hamsandwich/Android.mk)
$(call import-add-path,$(GLUE_DIR))
$(call import-module,trampoline)
