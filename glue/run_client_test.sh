#!/bin/bash
# FULL client-connect regression test.
#
# The device crash ("Trying to connect with modern protocol" -> signal 11
# code 1 at 0x0) never happened in CI because run_host_test.sh ran the engine
# in dedicated mode: no client ever connected. This script runs the engine as
# a real listen server (SDL dummy video/audio, software renderer) so the
# loopback client actually connects:
#
#   server: SV_ConnectClient -> metamod -> amxmodx ClientConnect/
#           client_authorized -> get_user_authid -> MF_GetPlayerAuthId
#           (crash cause: metamod-p EngineInfo rejected the Xash engine
#            module name -> signature funcs nulled -> call through NULL)
#   client: CL_ParseServerData -> HUD font (gfx/conchars) -> signon frames
#
# PASS requires: the admin.amxx "became an admin" line (proves
# pfnGetPlayerAuthId works through the metamod plugin table), serverdata
# parsed by the client, no crash, and the classic server-side checks.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE="$ROOT/xash3d-fwgs-master"
AMXX="$ROOT/amxmodx-FWGS"
OUT="$ROOT/out/host"
TEST="$ROOT/out/client-test"
ARCH=$(uname -m)
rm -rf "$TEST"
mkdir -p "$TEST/valve"

# ---- content: valve gamedir (same base as run_host_test.sh) ----
cp -r "$ROOT/stage/valve/addons" "$TEST/valve/addons"
mkdir -p "$TEST/valve/addons/amxmodx/dlls" "$TEST/valve/addons/amxmodx/modules"
cp "$OUT/libmm_amxmodx.so" "$TEST/valve/addons/amxmodx/dlls/"
cp "$OUT"/modules/libamxx_*.so "$TEST/valve/addons/amxmodx/modules/"
mkdir -p "$TEST/valve/addons/metamod/dlls"
for n in amd64 x86_64 arm64 aarch64; do
        cp "$OUT/libmetamod_android_$ARCH.so" "$TEST/valve/addons/metamod/dlls/libmetamod_android_$n.so"
done

# fakemeta needs the ReGameDLL API symbols; stub satisfies them
g++ -O1 -g -fPIC -shared -o "$OUT/regamedll_stub.so" "$ROOT/glue/host_regamedll_stub.cpp"
export LD_PRELOAD="$OUT/regamedll_stub.so"

# regression gate for the HUD font: valid WAD3 miptex conchars
mkdir -p "$TEST/valve/maps" "$TEST/valve/models" "$TEST/valve/gfx"
python3 "$ROOT/scripts/make_minimal_map.py" "$TEST/valve/maps/amxx_test.bsp"
python3 "$ROOT/scripts/make_minimal_models.py" "$TEST/valve/models/player.mdl"
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/gfx.wad" conchars CONBACK LAMBDA
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/decals.wad" '{break1 '{bullet1
python3 "$ROOT/scripts/make_delta_lst.py" "$ENGINE/engine/common/net_encode.c" "$TEST/valve/delta.lst"

printf 'amx_scrollmsg "AMXX arm64 CI test message" 10\n' \
        > "$TEST/valve/addons/amxmodx/configs/amxx.cfg"

# gamedll + client dll (waf hlsdk build makes both)
GAMELIB=$(find "$ENGINE/3rdparty/hlsdk-portable/build" -name "hl_*.so" | head -1)
CLIENTLIB=$(find "$ENGINE/3rdparty/hlsdk-portable/build" -name "client_*.so" | head -1)
if [ -z "$GAMELIB" ] || [ -z "$CLIENTLIB" ]; then
        echo "ERROR: hl gamedll or client dll not built" >&2; exit 1
fi
cp "$GAMELIB" "$TEST/"
mkdir -p "$TEST/valve/dlls" "$TEST/valve/cl_dlls"
for n in hl_amd64.so hl_i386.so hl_arm64.so libserver.so; do
        cp "$GAMELIB" "$TEST/valve/dlls/$n"
done
for n in client_amd64.so client_i386.so client_arm64.so libclient.so client.so; do
        cp "$CLIENTLIB" "$TEST/valve/cl_dlls/$n"
done
cat > "$TEST/valve/liblist.gam" <<'EOF'
game "Half-Life"
gamedir "valve"
type "singleplayer_only"
dll "hl"
EOF
# in HOST_NORMAL builds the engine only executes "+..." cmdline arguments
# when a .rc script calls "stuffcmds" (dedicated builds set stuffcmds_pending
# unconditionally — that's why run_host_test.sh never noticed this)
printf 'stuffcmds\n' > "$TEST/valve/valve.rc"
mkdir -p "$TEST/gamelibs"
cp "$GAMELIB" "$TEST/gamelibs/libserver_hardfp.so"

# engine binaries: FULL build (client + ref_soft), not the dedicated one
for f in game_launch/xash3d engine/libxash.so filesystem/filesystem_stdio.so; do
        [ -f "$ENGINE/build-full/$f" ] || { echo "ERROR: full engine build missing ($f) — build with: python3 waf configure -T release -8 && python3 waf build" >&2; exit 1; }
done
cp "$ENGINE/build-full/game_launch/xash3d" "$TEST/xash3d"
cp "$ENGINE/build-full/engine/libxash.so" "$TEST/"
cp "$ENGINE/build-full/filesystem/filesystem_stdio.so" "$TEST/"
find "$ENGINE/build-full" -name "libref_soft.so" -exec cp {} "$TEST/" \;

# ---- run as a listen server, client connects over loopback ----
cd "$TEST"
export XASH3D_GAMELIBDIR="$TEST/gamelibs"
export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
export LD_LIBRARY_PATH="$TEST:$ENGINE/3rdparty/hlsdk-portable/build/dlls:$LD_LIBRARY_PATH"

# a real (virtual) display is required: with SDL_VIDEODRIVER=dummy the client
# stalls before executing startup commands and never connects
if [ -z "$DISPLAY" ]; then
        XVFB_DISPLAY=":77"
        if ! xdpyinfo -display "$XVFB_DISPLAY" > /dev/null 2>&1; then
                Xvfb "$XVFB_DISPLAY" -screen 0 1280x720x24 > /dev/null 2>&1 &
                XVFB_PID=$!
                trap '[ -n "${XVFB_PID:-}" ] && kill "${XVFB_PID}" 2>/dev/null; true' EXIT
                sleep 1
        fi
        export DISPLAY="$XVFB_DISPLAY"
fi

timeout -k 10 75 ./xash3d \
        -dev 2 -log -condebug \
        -ref soft -dll "$TEST/$(basename "$GAMELIB")" \
        +map amxx_test +meta list \
        -noip -nojoy -nosteam > console.txt 2>&1 || true
unset LD_PRELOAD

echo "================= LOG (tail) ================="
tail -30 console.txt
echo "================= EVALUATION ================="
PASS=1; FAIL=""
grep -q "Metamod version" console.txt           || { PASS=0; FAIL="$FAIL metamod-not-loaded"; }
grep -q "AMX Mod X version" console.txt         || { PASS=0; FAIL="$FAIL amxmodx-not-loaded"; }
grep -q "Spawn Server" console.txt              || { PASS=0; FAIL="$FAIL map-not-spawned"; }
grep -q "Trying to connect with modern protocol" console.txt || { PASS=0; FAIL="$FAIL client-never-connected"; }
# THE critical gate: admin.amxx client_authorized -> get_user_authid ->
# MF_GetPlayerAuthId — the exact call that hit the NULL engine func (0x0)
# on the phone. The "became an admin" log line proves it works.
grep -q "became an admin" console.txt           || { PASS=0; FAIL="$FAIL authid-null-call"; }
# client side parsed the serverdata and reached signon
grep -q "Serverdata packet received" console.txt || { PASS=0; FAIL="$FAIL no-serverdata"; }
grep -q "Scrolling message" console.txt         || { PASS=0; FAIL="$FAIL scrollmsg-not-run"; }
grep -q "Crash: signal\|SIGSEGV\|Segmentation" console.txt && { PASS=0; FAIL="$FAIL CRASH"; }
grep -q "failed to load HUD font" console.txt   && { PASS=0; FAIL="$FAIL hud-font"; }
grep -q "registration failed\|LOAD_PLUGIN failed" console.txt && { PASS=0; FAIL="$FAIL module-metamod-attach"; }
grep -q "No spawn function" console.txt         && { PASS=0; FAIL="$FAIL no-spawn-func"; }
MODULE_PLUGINS=$(grep -cE '\] .*RUN' console.txt || true)
[ "$MODULE_PLUGINS" -ge 3 ] || { PASS=0; FAIL="$FAIL meta-list-only-$MODULE_PLUGINS-plugins"; }
if [ "$PASS" = 1 ]; then
        echo "RESULT: PASS — full client connect works on $ARCH ($MODULE_PLUGINS plugins, authid path exercised)"
else
        echo "RESULT: FAIL:$FAIL"
fi
exit $((1 - PASS))
