#!/bin/bash
# Assemble the two AMXX addons zips from stage/ trees + freshly built .so files:
#   out/cstrike-addons.zip  -> installed into <xash>/cstrike (shipped by cs16client APK as assets/addons.zip)
#   out/valve-addons.zip    -> installed into <xash>/valve   (shipped by engine APK as assets/valve-addons.zip)
#
# The stage trees contain everything EXCEPT the native .so files, which are
# copied in from the build outputs (glue/out/...).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)      # glue/
ROOT=$(dirname "$HERE")                  # repo root
STAGE="$ROOT/stage"
AMXXLIBS="$HERE/out/amxx/libs"
MMLIBS="$HERE/out/metamod"
OUT="$ROOT/out"
mkdir -p "$OUT"

install_libs() { # <stage-game-dir>
        local GAMEDIR=$1
        # metamod
        mkdir -p "$GAMEDIR/addons/metamod/dlls"
        cp "$MMLIBS/libmetamod_android_arm64.so"   "$GAMEDIR/addons/metamod/dlls/"
        cp "$MMLIBS/libmetamod_android_armv7a.so" "$GAMEDIR/addons/metamod/dlls/"
        # amxmodx core (abi-suffixed; the engine patch renames on copy to the private dir)
        mkdir -p "$GAMEDIR/addons/amxmodx/dlls" "$GAMEDIR/addons/amxmodx/modules"
        cp "$AMXXLIBS"/arm64-v8a/libmm_amxmodx.so     "$GAMEDIR/addons/amxmodx/dlls/libmm_amxmodx_arm64.so"
        cp "$AMXXLIBS"/armeabi-v7a/libmm_amxmodx.so   "$GAMEDIR/addons/amxmodx/dlls/libmm_amxmodx_armv7a.so"
        # modules
        for f in "$AMXXLIBS"/arm64-v8a/libamxx_*.so; do
                b=$(basename "$f" .so)            # libamxx_cstrike
                cp "$f" "$GAMEDIR/addons/amxmodx/modules/${b}_arm64.so"
        done
        for f in "$AMXXLIBS"/armeabi-v7a/libamxx_*.so; do
                b=$(basename "$f" .so)
                cp "$f" "$GAMEDIR/addons/amxmodx/modules/${b}_armv7a.so"
        done
}

rm -rf "$OUT/stage-cstrike" "$OUT/stage-valve"
cp -r "$STAGE/cstrike" "$OUT/stage-cstrike"
cp -r "$STAGE/valve"   "$OUT/stage-valve"

install_libs "$OUT/stage-cstrike"
install_libs "$OUT/stage-valve"

# ship AMXX gamedata (common.games / modules.games / ...) — fakemeta
# disables its get/set_ent_data* natives without it and many plugins rely
# on them. Lookup path is <gamedir>/addons/amxmodx/data/gamedata.
GAMEDATA_SRC="$ROOT/amxmodx-FWGS/gamedata"
for GAME in cstrike valve; do
        mkdir -p "$OUT/stage-$GAME/addons/amxmodx/data"
        rm -rf "$OUT/stage-$GAME/addons/amxmodx/data/gamedata"
        cp -r "$GAMEDATA_SRC" "$OUT/stage-$GAME/addons/amxmodx/data/gamedata"
done

# GeoIP database for the geoip module. geoip_main.cpp looks for
# <gamedir>/addons/amxmodx/data/GeoLite2-{City,Country}.mmdb — ship the
# Country database bundled in the module source tree.
GEOIP_DB="$ROOT/amxmodx-FWGS/modules/geoip/GeoLite2-Country.mmdb"
if [ -f "$GEOIP_DB" ]; then
        for GAME in cstrike valve; do
                mkdir -p "$OUT/stage-$GAME/addons/amxmodx/data"
                cp "$GEOIP_DB" "$OUT/stage-$GAME/addons/amxmodx/data/GeoLite2-Country.mmdb"
        done
fi

# the zips must carry the GAME DIRECTORY PREFIX. Both installers
# extract into <xash>/ root:
#   cs16client MainActivity  -> expects "cstrike/..." entries
#   engine ValveAddonsInstaller -> expects "valve/..." entries
# Without the prefix everything landed in <xash>/addons (root) — CS addons
# outside cstrike/ and valve addons never reaching valve/.
mkdir -p "$OUT/stage-cstrike/cstrike"
mv "$OUT/stage-cstrike/addons" "$OUT/stage-cstrike/cstrike/addons"
mkdir -p "$OUT/stage-valve/valve"
mv "$OUT/stage-valve/addons" "$OUT/stage-valve/valve/addons"

# player spray logo. The engine reads logos/<cl_logofile> (PC
# GoldSrc convention) on every connect and converts it to tempdecal.wad;
# the file ships as logos/AmirWolf512.bmp so it shows up in the Options
# logo picker; colors are preserved — no tinting/recoloring.
for GAME in cstrike valve; do
        if [ -d "$OUT/stage-$GAME/logos" ]; then
                mv "$OUT/stage-$GAME/logos" "$OUT/stage-$GAME/$GAME/logos"
        fi
        # complete motd.txt sample into the game dir root; servers
        # send it to joining players, exactly like PC CS 1.6
        if [ -f "$OUT/stage-$GAME/motd.txt" ]; then
                mv "$OUT/stage-$GAME/motd.txt" "$OUT/stage-$GAME/$GAME/motd.txt"
        fi
done

(cd "$OUT/stage-cstrike" && zip -qr9 "$OUT/cstrike-addons.zip" cstrike)
(cd "$OUT/stage-valve"   && zip -qr9 "$OUT/valve-addons.zip" valve)

# gameinfo.txt for valve (engine uses -dll @hl)
ls -la "$OUT"/*.zip
echo "DONE"
