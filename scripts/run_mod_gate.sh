#!/bin/bash
# run_mod_gate.sh — run ONE third-party mod inside the
# real host chain (engine -> metamod -> AMXX 1.8.3 -> ReGameDLL).
# usage: run_mod_gate.sh <name> <ini-file> <amxx-dir> [window-seconds]
set -u
ROOT=/home/z/work/cs16-amxx-android
NAME=$1; INI=$2; AMXXDIR=$3; WIN=${4:-110}
ENGINE=$ROOT/xash3d-fwgs-master
TEST=$ROOT/out/modgate-$NAME
ARCH=$(uname -m)

rm -rf "$TEST"
# fresh base tree (script builds out/cs-host-test but does not run when
# SKIP_RUN=1; falls back to reusing the last built tree if present)
if [ ! -d "$ROOT/out/cs-host-test/cstrike/addons/amxmodx/dlls" ]; then
        SKIP_RUN=1 bash "$ROOT/scripts/run_cstrike_host_test.sh" > /tmp/modgate_setup.log 2>&1 || true
fi
cp -r "$ROOT/out/cs-host-test" "$TEST"

C="$TEST/cstrike"
cp "$INI" "$C/addons/amxmodx/configs/plugins.ini"
cp "$AMXXDIR"/*.amxx "$C/addons/amxmodx/plugins/" 2>/dev/null

FS=$(find "$ENGINE/build" -name "filesystem_stdio.so" | head -1); FS=$(dirname "$FS")
export XASH3D_FAKECLIENT_CONNECT=1
export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
export XASH3D_GAMELIBDIR="$TEST/gamelibs"
export LD_LIBRARY_PATH="$FS:$TEST:$TEST/gamelibs:${LD_LIBRARY_PATH:-}"

cd "$TEST"
timeout -k 10 "$WIN" "$ENGINE/build/engine/xash" \
        -game cstrike -dev 2 -log -condebug \
        -dll "$TEST/gamelibs/libserver_hardfp.so" \
        +map amxx_test +hostname "modgate-$NAME" +sv_lan 1 \
        -noip -nojoy -nosteam > console.txt 2>&1 || true

PASS=1; FAIL=""
grep -aq "Metamod version" console.txt               || { PASS=0; FAIL="$FAIL metamod-not-loaded"; }
grep -aq "ReGameDLL version" console.txt             || { PASS=0; FAIL="$FAIL gamedll-not-loaded"; }
grep -aq "Crash: signal\|SIGSEGV\|Segmentation" console.txt && { PASS=0; FAIL="$FAIL CRASH"; }
grep -aqi "failed to load" console.txt               && { PASS=0; FAIL="$FAIL plugin-load-failure"; }
grep -aq "Run time error" console.txt                && { PASS=0; FAIL="$FAIL amxx-runtime-error"; }
LOADS=$(grep -ac "\[AMXX\] Loaded\|loaded.*plugin" console.txt || true)
echo "[$NAME] PLUGINS_LOADED_LINES=$LOADS"
[ "$PASS" = 1 ] && echo "RESULT: $NAME PASS" || echo "RESULT: $NAME FAIL:$FAIL"
exit $((1 - PASS))
