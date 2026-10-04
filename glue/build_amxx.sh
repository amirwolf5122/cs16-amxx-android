#!/bin/bash
# Build AMX Mod X core + modules for Android (ndk-build, both ABIs).
# Output: glue/out/amxx/libs/<abi>/{libmm_amxmodx_*.so, libamxx_*_*.so}
#
# Env / args:
#   NDK_BUILD  — path to ndk-build (default: $ANDROID_NDK_HOME/ndk-build or from ANDROID_NDK_HOME)
#   SDK paths are auto-detected from the repo layout.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")

: "${NDK_BUILD:=$(ls -d "${ANDROID_NDK_HOME:-/opt/ndk}"*/ndk-build 2>/dev/null | head -1)}"
: "${NDK_BUILD:=$(ls -d "${ANDROID_HOME:-/usr/local/lib/android/sdk}"/ndk/*/ndk-build 2>/dev/null | sort -V | tail -1)}"
[ -x "$NDK_BUILD" ] || { echo "ERROR: ndk-build not found (set ANDROID_NDK_HOME)" >&2; exit 1; }
echo ">> using $(dirname "$NDK_BUILD")"

OUT="$HERE/out/amxx"
mkdir -p "$OUT"

"$NDK_BUILD" \
	NDK_PROJECT_PATH="$OUT" \
	NDK_APPLICATION_MK="$HERE/Application.mk" \
	APP_BUILD_SCRIPT="$HERE/Android.mk" \
	NDK_OUT="$OUT/obj" \
	NDK_LIBS_OUT="$OUT/libs" \
	AMXX_SRC="${AMXX_SRC:-$ROOT/amxmodx-FWGS}" \
	XASH_FS="$ROOT/xash3d-fwgs-master" \
	FS_SRC="$ROOT/filesystem_stdio_xash" \
	MM_SRC="$ROOT/metamod-fwgs" \
	HLSDK_SRC="$ROOT/metamod-fwgs/hlsdk" \
	GLUE_DIR="$HERE" \
	-j"$(nproc)"

echo ">> amxmodx built:"
ls -la "$OUT"/libs/*/
