#!/bin/bash
# Assemble the host test environment, run the dedicated engine with the full
# AMXX chain and evaluate the log. This is the arm64 CI "full test":
# engine(patch) -> metamod -> amxmodx -> modules -> plugins -> map spawn.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE="$ROOT/xash3d-fwgs-master"
AMXX="$ROOT/amxmodx-FWGS"
OUT="$ROOT/out/host"
TEST="$ROOT/out/host-test"
ARCH=$(uname -m)
rm -rf "$TEST"
mkdir -p "$TEST/valve"

# ---- content: valve gamedir ----
cp -r "$ROOT/stage/valve/addons" "$TEST/valve/addons"
# the committed stage tree has no dlls/modules dirs (libs are added here)
mkdir -p "$TEST/valve/addons/amxmodx/dlls" "$TEST/valve/addons/amxmodx/modules"
cp "$OUT/libmm_amxmodx.so" "$TEST/valve/addons/amxmodx/dlls/"
cp "$OUT"/modules/libamxx_*.so "$TEST/valve/addons/amxmodx/modules/"
mkdir -p "$TEST/valve/addons/metamod/dlls"
# engine's Q_buildarch may report amd64/arm64 depending on the host — provide all names
for n in amd64 x86_64 arm64 aarch64; do
        cp "$OUT/libmetamod_android_$ARCH.so" "$TEST/valve/addons/metamod/dlls/libmetamod_android_$n.so"
done

# fakemeta now loads on the host too. It imports two ReGameDLL API
# symbols (RegamedllApi_Init / ReGameHookchains) that normally come from the
# CS gamedll; a stub library (Init=false) satisfies them so the REAL
# OnAmxxAttach -> LoadGameConfigFile -> SMC gamedata parse path runs here.
# cstrike/csx stay out (they need the actual ReGameDLL game dll).
g++ -O1 -g -fPIC -shared -o "$OUT/regamedll_stub.so" "$ROOT/glue/host_regamedll_stub.cpp"

# regression gate: the arm64 crash was TextParsers::ParseStream_SMC
# (parse_point[i-1] with i==0 wrapping to parse_point+4GB on LP64) while
# parsing the shipped gamedata — parse EVERY shipped gamedata file through
# the real core library here; any LP64 wrap reproduces deterministically.
g++ -O0 -g -std=gnu++11 -I"$AMXX/public" -o "$OUT/smc_repro" "$ROOT/scripts/smc_repro.cpp" -ldl
if ! "$OUT/smc_repro" "$OUT/libmm_amxmodx.so" "$AMXX/gamedata" > "$TEST/smc_repro.txt" 2>&1; then
        PASS_SMC=0
        tail -5 "$TEST/smc_repro.txt"
else
        PASS_SMC=1
        tail -1 "$TEST/smc_repro.txt"
fi

# test map + stand-in models
mkdir -p "$TEST/valve/maps" "$TEST/valve/models" "$TEST/valve/gfx"
python3 "$ROOT/scripts/make_minimal_map.py" "$TEST/valve/maps/amxx_test.bsp"
python3 "$ROOT/scripts/make_minimal_models.py" "$TEST/valve/models/player.mdl"
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/gfx.wad" conchars CONBACK LAMBDA
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/decals.wad" '{break1 '{bullet1
python3 "$ROOT/scripts/make_delta_lst.py" "$ENGINE/engine/common/net_encode.c" "$TEST/valve/delta.lst"

# exercise the scrollmsg crash path: amx_scrollmsg server command
# (plugin_srvcmd -> executeForwards -> amx_Exec -> amx_Callback -> set_task)
# the AMXX core execs amxx.cfg itself after plugins load — same as the phone flow
printf 'amx_scrollmsg "AMXX arm64 CI test message" 10\namx_scrollmsg "second message" 12\n' \
        > "$TEST/valve/addons/amxmodx/configs/amxx.cfg"

# gamedll (libhl built by waf) + liblist.gam
GAMELIB=$(find "$ENGINE/3rdparty/hlsdk-portable/build" -name "hl_*.so" | head -1)
if [ -z "$GAMELIB" ]; then
        echo "ERROR: hl gamedll not built" >&2; exit 1
fi
cp "$GAMELIB" "$TEST/"
# metamod autodetects the real gamedll from the gamedir (dlls/<name>.so)
mkdir -p "$TEST/valve/dlls"
for n in hl_amd64.so hl_i386.so hl_arm64.so libserver.so; do
        cp "$GAMELIB" "$TEST/valve/dlls/$n"
done
cat > "$TEST/valve/liblist.gam" <<'EOF'
game "Half-Life"
gamedir "valve"
type "singleplayer_only"
dll "hl"
EOF

# ---- run ----
cd "$TEST"
# metamod compiles with -D__ANDROID__ (exact phone code paths), so its
# gamedll autodetection goes through XASH3D_GAMELIBDIR looking for
# libserver_hardfp.so — same mechanism as the phone (where GAMELIBDIR is
# the game APK's nativeLibraryDir). Provide it, or the chain runs with no
# gamedll and every entity fails with "No spawn function".
mkdir -p "$TEST/gamelibs"
cp "$GAMELIB" "$TEST/gamelibs/libserver_hardfp.so"
export XASH3D_GAMELIBDIR="$TEST/gamelibs"
export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
export LD_LIBRARY_PATH="$ENGINE/build/filesystem:$ENGINE/3rdparty/hlsdk-portable/build/dlls:$LD_LIBRARY_PATH"
export LD_PRELOAD="$OUT/regamedll_stub.so"
timeout -k 10 60 "$ENGINE/build/engine/xash" \
        -dev 2 -log -condebug \
        -dll "$TEST/$(basename "$GAMELIB")" \
        +map amxx_test +meta list +amxx modules \
        -noip -nojoy -nosteam > console.txt 2>&1 || true
unset LD_PRELOAD

echo "================= LOG (tail) ================="
tail -40 console.txt
echo "================= EVALUATION ================="
PASS=1; FAIL=""
grep -q "Metamod version" console.txt           || { PASS=0; FAIL="$FAIL metamod-not-loaded"; }
grep -q "AMX Mod X version" console.txt         || { PASS=0; FAIL="$FAIL amxmodx-not-loaded"; }
grep -q "Mapchange to amxx_test\|Spawn Server" console.txt || { PASS=0; FAIL="$FAIL map-not-spawned"; }
grep -q "Scrolling message" console.txt         || { PASS=0; FAIL="$FAIL scrollmsg-not-run"; }
grep -q "Crash: signal\|SIGSEGV\|Segmentation" console.txt && { PASS=0; FAIL="$FAIL CRASH"; }
grep -q "was left pending" console.txt          && { PASS=0; FAIL="$FAIL module-left-pending"; }
grep -q "failed to load: Module" console.txt    && { PASS=0; FAIL="$FAIL plugin-module-missing"; }
# modules must actually ATTACH to metamod (LOAD_PLUGIN with an
# absolute path used to fail in resolve() and silently left every module
# unattached -> NULL api tables -> SIGSEGV in OnPluginsLoaded at spawn).
grep -q "registration failed\|LOAD_PLUGIN failed" console.txt && { PASS=0; FAIL="$FAIL module-metamod-attach"; }
MODULE_PLUGINS=$(grep -cE '\] .*RUN' console.txt || true)
[ "$MODULE_PLUGINS" -ge 3 ] || { PASS=0; FAIL="$FAIL meta-list-only-$MODULE_PLUGINS-plugins"; }
# the gamedata SMC parse regression gate (LP64 wrap fix)
[ "$PASS_SMC" = 1 ] || { PASS=0; FAIL="$FAIL smc-gamedata-parse"; }
# fakemeta must actually load now: its "Module path is" line is only logged
# on a successful module load, and it carries the gamedata attach path
FMMETA=$(grep -c "Module path is .*libamxx_fakemeta.so" console.txt || true)
[ "$FMMETA" -ge 1 ] || { PASS=0; FAIL="$FAIL fakemeta-not-running"; }
if [ "$PASS" = 1 ]; then echo "RESULT: PASS — full AMXX chain works on $ARCH ($MODULE_PLUGINS plugins incl. modules)"; else echo "RESULT: FAIL:$FAIL"; fi
exit $((1 - PASS))
