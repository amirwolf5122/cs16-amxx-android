# AMXXOnAndroid
# Copyright (C) 2017 a1batross
# cs_ham_bots_api Android library-anchor module.
# The real natives ship as the Pawn plugin cs_ham_bots_api.amxx
# (see stage/*/addons/amxmodx/plugins). This module only publishes
# the library name so reqlib/loadlib resolution succeeds and the
# AMXX core stops logging "Can't find module file" on every
# zombie-mod plugin load.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(XASH3D_CONFIG)

LOCAL_MODULE := amxx_cs_ham_bots_api

LOCAL_SRC_FILES := \
	../../public/sdk/amxxmodule.cpp \
	amxxapi.cpp \

include $(BUILD_SHARED_LIBRARY)
