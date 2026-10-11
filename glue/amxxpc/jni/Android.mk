# Builds libamxxpc.so — the in-app SMA->AMXX compiler driver used by the
# engine launcher (CompilerDialogFragment / FullCompilerDialogFragment).
#
# libamxxpc.so = amxxpc driver (amxxpc.cpp + Binary.cpp + amx_shim.cpp)
#                + libpc300 (pawn compiler 3.x, static, PAWNC_DLL mode)
#
# The upstream layout (compiler/amxxpc/Android.mk + compiler/libpc300/Android.mk)
# links amx.cpp from the driver — but amx.cpp does not compile on 64-bit with
# 32-bit cells; the driver only needs amx_Align16/32 (no-ops on little-endian),
# provided by glue/amxxpc/amx_shim.cpp.

LOCAL_PATH := $(call my-dir)
AMXX_SRC := $(AMXX_SRC)
PC300 := $(AMXX_SRC)/compiler/libpc300
AMXXPC := $(AMXX_SRC)/compiler/amxxpc

include $(CLEAR_VARS)
LOCAL_MODULE := amxxpc32
LOCAL_C_INCLUDES += $(PC300)
LOCAL_CFLAGS += -DLINUX -DENABLE_BINRELOC -DNO_MAIN -DPAWNC_DLL -DHAVE_STDINT_H -D_GNU_SOURCE
include $(GLUE_DIR)/XASH3D_CONFIG.mk
LOCAL_SRC_FILES := \
        $(PC300)/sc1.c \
        $(PC300)/sc2.c \
        $(PC300)/sc3.c \
        $(PC300)/sc4.c \
        $(PC300)/sc5.c \
        $(PC300)/sc6.c \
        $(PC300)/sc7.c \
        $(PC300)/scvars.c \
        $(PC300)/scmemfil.c \
        $(PC300)/scstate.c \
        $(PC300)/sclist.c \
        $(PC300)/sci18n.c \
        $(PC300)/pawncc.c \
        $(PC300)/libpawnc.c \
        $(PC300)/prefix.c \
        $(PC300)/memfile.c \
        $(PC300)/sp_symhash.c
LOCAL_EXPORT_C_INCLUDES := $(PC300)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := amxxpc
LOCAL_C_INCLUDES += $(AMXXPC) $(AMXX_SRC)/public
LOCAL_CXXFLAGS += -fexceptions
LOCAL_CFLAGS += -DAMX_ANSIONLY -DHAVE_STDINT_H
include $(GLUE_DIR)/XASH3D_CONFIG.mk
LOCAL_SRC_FILES := \
        $(GLUE_DIR)/amxxpc/amx_shim.cpp \
        $(AMXXPC)/amxxpc.cpp \
        $(AMXXPC)/Binary.cpp
LOCAL_LDLIBS := -lz -ldl -lm
LOCAL_STATIC_LIBRARIES := amxxpc32
# this must be a REAL executable, not a shared library:
# CompilerDialogFragment / FullCompilerDialogFragment run it through
# ProcessBuilder (execve). A shared object has no ELF entry point, so
# exec'ing it crashed (exit 133) and the in-app compiler never worked.
# Output name libamxxpc; build_native.sh copies it to the engine jniLibs
# as libamxxpc.so (Android only execs lib*.so from nativeLibraryDir).
LOCAL_MODULE_FILENAME := libamxxpc
include $(BUILD_EXECUTABLE)
