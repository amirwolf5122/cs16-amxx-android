# minimal libffcall-compatible trampoline library
# for Android ARM. GNU libffcall has no aarch64 backend and its arm32
# backend needs anonymous RWX memory (blocked on Android 10+ for apps
# targeting API 29+), so hamsandwich could never load there.
#
# This is a drop-in replacement exposing the two entry points hamsandwich
# uses: alloc_trampoline() / free_trampoline(). It generates a tiny
# "store hook pointer; tail-jump to callback" thunk in native ARM machine
# code and allocates it through a W^X-safe path (memfd-backed RX mapping
# when anonymous RWX is denied).
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := trampoline
LOCAL_SRC_FILES := trampoline.c
LOCAL_CFLAGS := -O2 -Wall
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

include $(BUILD_STATIC_LIBRARY)
