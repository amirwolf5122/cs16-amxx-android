#!/bin/bash
# Master native build for CI (and local): builds amxmodx, metamod and the
# SMA compiler, then populates the gradle projects:
#   - cs16client jniLibs (libamxx_* + libmm_amxmodx + libmetamod per ABI)
#   - cs16client assets (base content from cs16client-extras + addons.zip)
#   - engine jniLibs (libamxxpc)
#   - engine assets (valve-addons.zip)
#   - out/*.zip (standalone addons packages)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

echo "==================================================="
echo "== 1/4 amxmodx core + modules (version: ${AMXX_VERSION:-1.8.3})"
echo "==================================================="
# selectable AMXX version: fetch upstream tree when requested
# (1.8.2 | 1.9 | 1.10); default = the in-tree 1.8.3-dev FWGS fork.
AMXX_LABEL="1.8.3-dev-fwgs"
if [ -n "${AMXX_VERSION:-}" ] && [ "${AMXX_VERSION}" != "1.8.3" ]; then
        FETCH_OUT=$(bash scripts/fetch_amxx_version.sh "$AMXX_VERSION")
        AMXX_SRC=$(echo "$FETCH_OUT" | head -1)
        AMXX_LABEL=$(echo "$FETCH_OUT" | tail -1)
        export AMXX_SRC
        echo ">> AMXX source: $AMXX_SRC (label $AMXX_LABEL)"
fi
bash glue/build_amxx.sh

echo "==================================================="
echo "== 2/4 metamod"
echo "==================================================="
bash glue/build_metamod.sh

echo "==================================================="
echo "== 3/4 libamxxpc (SMA compiler)"
echo "==================================================="
bash glue/build_amxxpc.sh

echo "==================================================="
echo "== 4/4 staging"
echo "==================================================="
bash glue/make_addons_zips.sh
# labeled copies of the addons zips for the per-version release
for z in cstrike-addons valve-addons; do
        [ -f "out/$z.zip" ] && cp "out/$z.zip" "out/${z}-amxx-${AMXX_LABEL}.zip"
done

CS16="$ROOT/cs16-client-main/android/app/src/main"
ENGINE="$ROOT/xash3d-fwgs-master/android/app/src/main"

# ---- cs16client jniLibs: unsuffixed names per ABI dir (engine's APK-path
# ---- fallback + the game's nativeLibraryDir both look these names up)
mkdir -p "$CS16/jniLibs/arm64-v8a" "$CS16/jniLibs/armeabi-v7a"
for abi in arm64-v8a armeabi-v7a; do
        cp glue/out/amxx/libs/$abi/libamxx_*.so          "$CS16/jniLibs/$abi/"
        cp glue/out/amxx/libs/$abi/libmm_amxmodx.so      "$CS16/jniLibs/$abi/"
done
cp glue/out/metamod/libmetamod_android_arm64.so   "$CS16/jniLibs/arm64-v8a/"
cp glue/out/metamod/libmetamod_android_armv7a.so  "$CS16/jniLibs/armeabi-v7a/"

# ---- cs16client assets: base content + addons.zip
mkdir -p "$CS16/assets"
cp -r cs16-client-main/3rdparty/cs16client-extras/. "$CS16/assets/"
rm -rf "$CS16/assets/addons"   # never ship the extracted dir, only the zip
cp out/cstrike-addons.zip "$CS16/assets/addons.zip"
echo "amxx-1.8.3-r3" > "$CS16/assets/addons_version.txt"

# ---- engine jniLibs: libamxxpc (SMA compiler executable, packaged as
# ---- lib*.so so Android allows exec from nativeLibraryDir)
mkdir -p "$ENGINE/jniLibs/arm64-v8a" "$ENGINE/jniLibs/armeabi-v7a"
for abi in arm64-v8a armeabi-v7a; do
        SRC_PC="$ROOT/glue/out/amxxpc/libs/$abi/libamxxpc"
        [ -f "$SRC_PC" ] || SRC_PC="$ROOT/glue/out/amxxpc/libs/$abi/libamxxpc.so"
        cp "$SRC_PC" "$ENGINE/jniLibs/$abi/libamxxpc.so"
done

# ---- bundle metamod + amxmodx core into the engine APK as well.
# ---- Valve mode runs entirely inside the engine app; the library loader's
# ---- tiers and metamod's own DLOPEN fallback both look in XASH3D_GAMELIBDIR
# ---- (= this app's nativeLibraryDir), so the binaries must be present there
# ---- even though the private-dir copy normally wins.
cp glue/out/metamod/libmetamod_android_arm64.so  "$ENGINE/jniLibs/arm64-v8a/"
cp glue/out/metamod/libmetamod_android_armv7a.so "$ENGINE/jniLibs/armeabi-v7a/"
cp glue/out/amxx/libs/arm64-v8a/libmm_amxmodx.so      "$ENGINE/jniLibs/arm64-v8a/"
cp glue/out/amxx/libs/armeabi-v7a/libmm_amxmodx.so    "$ENGINE/jniLibs/armeabi-v7a/"

# ---- engine assets: valve addons zip
mkdir -p "$ENGINE/assets"
cp out/valve-addons.zip "$ENGINE/assets/valve-addons.zip"

# ---- pin the effective engine build date -----------------------------------
# cs16-client aborts with "Xash3D FWGS version check failed!" when the engine
# buildnum < MIN_XASH_VERSION (4190 = 2026-09-20 in upstream's date scheme).
# public/wscript honours XASH_BUILD_COMMIT_DATE; default it here so plain
# local builds also pass the gate.
export XASH_BUILD_COMMIT_DATE="${XASH_BUILD_COMMIT_DATE:-2026-10-01}"

# ---- make git metadata visible to the engine's waf build ------------------
# waf (public/wscript) looks for .git inside xash3d-fwgs-master only. In this
# monorepo .git lives at the repo root, so without a pointer file the engine
# is built with an empty commit date -> Q_buildnum() == -1 -> cs16-client
# aborts with "Xash3D FWGS version check failed!" (g_iXash < 4190).
if [ ! -d "$ROOT/xash3d-fwgs-master/.git" ]; then
        if GITDIR=$(git -C "$ROOT" rev-parse --absolute-git-dir 2>/dev/null); then
                echo "gitdir: $GITDIR" > "$ROOT/xash3d-fwgs-master/.git"
                echo ">> wrote $ROOT/xash3d-fwgs-master/.git -> $GITDIR"
        else
                rm -f "$ROOT/xash3d-fwgs-master/.git"
        fi
fi

echo ">> staging complete"
ls -la "$CS16/jniLibs/arm64-v8a" "$ENGINE/jniLibs/arm64-v8a" | head -20
