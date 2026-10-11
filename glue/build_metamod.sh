#!/bin/bash
# Build Metamod-P for Android (arm64-v8a + armeabi-v7a) with plain clang++
# (same recipe as the original build_metamod_amxx.sh, repo-parametrized).
# Usage: build_metamod.sh [NDK_PATH] [OUT_DIR]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")

NDK="${1:-${ANDROID_NDK_HOME:-}}"
[ -n "$NDK" ] || NDK=$(ls -d "${ANDROID_HOME:-/usr/local/lib/android/sdk}"/ndk/* 2>/dev/null | sort -V | tail -1)
[ -d "$NDK" ] || { echo "ERROR: NDK path not found" >&2; exit 1; }
OUT="${2:-$HERE/out/metamod}"
SRCDIR="$ROOT/metamod-p-velaron/metamod"
PREBUILT=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin

SRCFILES="api_hook api_info commands_meta conf_meta dllapi engine_api engineinfo \
game_support game_autodetect h_export linkgame linkplug log_meta meta_eiface \
metamod mlist mplayer mplugin mqueue mreg mutil osdep osdep_p reg_support \
sdk_util studioapi support_meta thread_logparse vdate \
osdep_linkent_linux osdep_detect_gamedll_linux xash_exports"

INCLUDES=(-I"$SRCDIR" -I"$SRCDIR/../hlsdk/engine" -I"$SRCDIR/../hlsdk/common" -I"$SRCDIR/../hlsdk/dlls" -I"$SRCDIR/../hlsdk/pm_shared" -I"$SRCDIR/../hlsdk")
COMMON_FLAGS=(-O2 -g -std=gnu++98 -fPIC -fno-exceptions -fno-rtti -fvisibility=hidden
  -Wno-unknown-pragmas -Wno-attributes -Wno-write-strings -Wno-unused-variable
  -Wno-unused-but-set-variable -Wno-format -Wno-deprecated-declarations
  -D__METAMOD_BUILD__ '-DCOMPILE_TZ="UTC"')

build_abi() {
  local TRIPLET=$1 ARCHNAME=$2
  local OBJDIR=$OUT/obj_$ARCHNAME
  mkdir -p "$OBJDIR"

  local CXX=$PREBUILT/${TRIPLET}-clang++

  echo "=== building metamod for $TRIPLET (libmetamod_android_$ARCHNAME.so) ==="
  for s in $SRCFILES; do
    if [ ! -f "$OBJDIR/$s.o" ] || [ "$SRCDIR/$s.cpp" -nt "$OBJDIR/$s.o" ]; then
      $CXX -c "${COMMON_FLAGS[@]}" "${INCLUDES[@]}" -o "$OBJDIR/$s.o" "$SRCDIR/$s.cpp" 2>"$OBJDIR/$s.warn" || { echo "FAILED: $s.cpp"; cat "$OBJDIR/$s.warn"; exit 1; }
    fi
  done

  # -static-libstdc++: link libc++ statically so the lib has no
  # libc++_shared.so DT_NEEDED (it is dlopen'ed from the game/engine
  # namespaces where libc++_shared.so is not visible — otherwise
  # "dlopen failed: library libc++_shared.so not found").
  $CXX -shared -o "$OUT/libmetamod_android_$ARCHNAME.so" $OBJDIR/*.o -ldl -lm -static-libstdc++ -Wl,--build-id=sha1
  echo "  => $OUT/libmetamod_android_$ARCHNAME.so"
}

build_abi aarch64-linux-android24 arm64
build_abi armv7a-linux-androideabi24 armv7a

ls -la $OUT/*.so
echo "DONE"
