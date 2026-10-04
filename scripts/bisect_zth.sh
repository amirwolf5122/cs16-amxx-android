#!/bin/bash
# bisect: find which ZTH plugin segfaults the host at SV_FakeConnect.
# usage: bisect_zth.sh <listfile-with-plugin-names>   (one .amxx per line)
ROOT=/home/z/work/cs16-amxx-android
TEST=$ROOT/out/cs-host-test
ENGINE=$ROOT/xash3d-fwgs-master/build/engine/xash
INI=$TEST/cstrike/addons/amxmodx/configs/plugins.ini

run_once() # writes plugins.ini from stdin, runs, echoes exit code
{
        cat > "$INI"
        rm -rf "$TEST/amxxpriv"
        cd "$TEST"
        export XASH3D_FAKECLIENT_CONNECT=1
        export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
        export XASH3D_GAMELIBDIR="$TEST/gamelibs"
        export LD_LIBRARY_PATH="$ROOT/xash3d-fwgs-master/build:$TEST:$TEST/gamelibs:$LD_LIBRARY_PATH"
        timeout -k 5 55 "$ENGINE" -game cstrike -dev 2 -log -condebug -dedicated \
                -dll "$TEST/gamelibs/libserver_hardfp.so" \
                +map amxx_test +hostname "host-test" +sv_lan 1 -noip -nojoy -nosteam \
                > console.txt 2>&1
        echo $?
}

LIST=$(mktemp)
grep -v '^;' "$1" | grep -v '^\s*$' | grep -v fakeplayer_driver > "$LIST"
N=$(wc -l < "$LIST")
echo "total plugins: $N"

lo=0; hi=$N
# first: all plugins -> expect crash
exit_code=$( ( cat "$LIST"; echo fakeplayer_driver.amxx ) | run_once )
echo "ALL: exit=$exit_code"
[ "$exit_code" = "139" ] || { echo "no crash with ALL plugins -- bisect pointless"; exit 0; }

while (( hi - lo > 1 )); do
        mid=$(( (lo + hi) / 2 ))
        { head -n "$mid" "$LIST"; echo fakeplayer_driver.amxx; } | run_once > /dev/null
        code=$?
        echo "first $mid: exit=$code"
        if [ "$code" = "139" ]; then hi=$mid; else lo=$mid; fi
done
echo "=== minimal crashing prefix: first $hi plugins; offender = line $hi ==="
sed -n "${hi}p" "$LIST"
cp "$LIST" /tmp/zth_bisect_list.txt
