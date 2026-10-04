# Common config included by every AMX Mod X module Android.mk
# (a1ba's AMXXOnAndroid build layout) — repo version: all paths are
# passed in from the ndk-build command line, nothing is hardcoded.
#
# Required variables (set by glue/Android.mk or the ndk-build invocation):
#   AMXX_SRC  — absolute path to amxmodx-FWGS sources
#   XASH_FS   — absolute path to xash3d-fwgs-master (engine) sources
#   MM_SRC    — absolute path to metamod-fwgs sources (metamod SDK headers)
#   HLSDK_SRC — absolute path to hlsdk-portable (vendored inside the engine 3rdparty)
#   GLUE_DIR  — absolute path to this glue directory

GLUE_DIR ?= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
AMXX_SRC ?= $(GLUE_DIR)/../amxmodx-FWGS
XASH_FS ?= $(GLUE_DIR)/../xash3d-fwgs-master
MM_SRC ?= $(GLUE_DIR)/../metamod-fwgs
FS_SRC ?= $(GLUE_DIR)/../filesystem_stdio_xash
HLSDK_SRC ?= $(MM_SRC)/hlsdk

# a1ba's module makefiles reference these legacy names:
#   SRCPATH — the amxmodx source root (geoip: libmaxminddb, sqlite: bundled
#   sqlite3, json: parson)
#   HLSDK / METAMOD — hlsdk-portable and metamod SDK headers (were empty
#   before; the aggregate includes from XASH3D_CONFIG made most builds work,
#   but modules that reach into third_party through SRCPATH failed)
SRCPATH := $(AMXX_SRC)
HLSDK := $(HLSDK_SRC)
METAMOD := $(MM_SRC)

LOCAL_C_INCLUDES += \
        $(AMXX_SRC)/public/sdk \
        $(AMXX_SRC)/public \
        $(AMXX_SRC)/public/memtools \
        $(AMXX_SRC)/public/amtl \
        $(AMXX_SRC)/public/amtl/amtl \
        $(AMXX_SRC)/third_party/hashing \
        $(AMXX_SRC)/third_party/utf8rewind \
        $(AMXX_SRC)/third_party/pcre \
        $(AMXX_SRC)/third_party/zlib \
        $(AMXX_SRC)/third_party/parson \
        $(AMXX_SRC)/third_party \
        $(GLUE_DIR) \
        $(FS_SRC)/include \
        $(HLSDK_SRC)/dlls \
        $(HLSDK_SRC)/public \
        $(HLSDK_SRC)/common \
        $(HLSDK_SRC)/engine \
        $(HLSDK_SRC)/pm_shared \
        $(MM_SRC)/metamod

LOCAL_CFLAGS += \
        -include $(GLUE_DIR)/compat_types.h \
        -fpermissive \
        -Wno-shorten-64-to-32 \
        -Dstricmp=strcasecmp \
        -D_stricmp=strcasecmp \
        -Dstrnicmp=strncasecmp \
        -D_snprintf=snprintf \
        -D_vsnprintf=vsnprintf

LOCAL_CPPFLAGS += -std=c++0x

LOCAL_SHORT_COMMANDS := true

# NDK r29 clang promotes some legacy diagnostics to hard errors; older NDKs
# built these files with them as warnings only. Restore that behavior.
LOCAL_CFLAGS += -Wno-error -Wno-error=shorten-64-to-32 -Wno-error=incompatible-function-pointer-types
LOCAL_CPPFLAGS += -Wno-error -Wno-error=shorten-64-to-32 -Wno-error=incompatible-function-pointer-types

# AMXX core TUs get the sequence structs from compat_types.h; prevent the
# (differently shaped) engine/Sequence.h from redefining them under NDK r29.
LOCAL_CFLAGS += -D_INCLUDE_SEQUENCE_H_
LOCAL_CPPFLAGS += -D_INCLUDE_SEQUENCE_H_
