# AMXXOnAndroid
# Copyright (C) 2017 a1batross
#
# JSON module for Android. Upstream ships no
# Android.mk for json; parson (the bundled JSON backend) lives in
# third_party/parson and is compiled straight into this module.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(XASH3D_CONFIG)

LOCAL_C_INCLUDES += \
	$(HLSDK)/dlls \
	$(HLSDK)/public \
	$(HLSDK)/common \
	$(HLSDK)/engine \
	$(HLSDK)/pm_shared \
	$(METAMOD)/metamod \
	$(SRCPATH)/third_party/parson

LOCAL_MODULE := amxx_json

LOCAL_CFLAGS += -DHAVE_STDINT_H

LOCAL_SRC_FILES := \
	../../public/sdk/amxxmodule.cpp \
	JsonMngr.cpp \
	JsonNatives.cpp \
	../../third_party/parson/parson.c

include $(BUILD_SHARED_LIBRARY)
