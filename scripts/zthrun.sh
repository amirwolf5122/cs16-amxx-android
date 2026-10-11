#!/bin/bash
# lean repro harness: zthrun.sh <plugins-ini> <seconds> <tag>
ROOT=/home/z/work/cs16-amxx-android
TEST=$ROOT/out/cs-host-test
ENGINE=$ROOT/xash3d-fwgs-master/build/engine/xash
INI=$TEST/cstrike/addons/amxmodx/configs/plugins.ini
SECS=${2:-45}
TAG=${3:-run}

cp "$1" "$INI" || exit 1
rm -rf "$TEST/amxxpriv"
rm -f "$TEST/console.txt" "$TEST/engine.log"
cd "$TEST"
export XASH3D_FAKECLIENT_CONNECT=1
export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
export XASH3D_GAMELIBDIR="$TEST/gamelibs"
export LD_LIBRARY_PATH="$ROOT/xash3d-fwgs-master/build:$TEST:$TEST/gamelibs:$LD_LIBRARY_PATH"
timeout -k 3 "$SECS" "$ENGINE" -game cstrike -dev 2 -log -condebug -dedicated \
        -dll "$TEST/gamelibs/libserver_hardfp.so" \
        +map amxx_test +hostname "host-test" +sv_lan 1 -noip -nojoy -nosteam \
        < /dev/null > "$TEST/console.txt" 2>&1
CODE=$?
mv "$TEST/console.txt" "/tmp/$TAG-console.txt"
mv "$TEST/engine.log" "/tmp/$TAG-engine.log" 2>/dev/null
echo "exit=$CODE log=/tmp/$TAG-engine.log"
exit 0
