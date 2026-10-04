#!/bin/bash
# run the REAL game chain on the host
#   xash3d-fwgs dedicated engine (linux, native arch)
#     -> metamod (loaded by the engine's COM_AMXX_Setup)
#       -> amxmodx core + ALL modules (fun/engine/fakemeta/cstrike/csx/
#          nvault/sockets/hamsandwich/cs_ham_bots_api)
#       -> cstrike gamedll = the REAL ReGameDLL-based libcs built from
#          cs16-client-main (same source as the Android libcs_android.so)
#       -> YaPB bot dll (real bots => real CBasePlayer entities)
#       -> every staged plugin + battery_test.amxx (exercises every module)
# Everything the battery prints is scanned from the engine console log.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE="$ROOT/xash3d-fwgs-master"
# arch-flexible names (cs_amd64.so on x86_64 hosts, cs_arm64.so on
# aarch64 CI runners -- same cmake of cs16-client-main BUILD_SERVER=ON)
CS=$(find "$ROOT/out/cs-build" -name "cs_*.so" | head -1)
YAPB=$(find "$ROOT/out/cs-build" -name "yapb_*.so" | head -1)
HOST="$ROOT/out/host"
TEST="$ROOT/out/cs-changelevel-test"
ARCH=$(uname -m)

[ -n "$CS" ]    || { echo "ERROR: cs gamedll not built" >&2; exit 1; }
[ -n "$YAPB" ]  || { echo "ERROR: yapb not built" >&2; exit 1; }
[ -d "$HOST/modules" ] || { echo "ERROR: host amxx chain not built" >&2; exit 1; }

rm -rf "$TEST"
mkdir -p "$TEST/cstrike" "$TEST/valve"

# ---------------- gamedir: cstrike ----------------
C="$TEST/cstrike"

# addons tree from the staged package (plugins, configs, data, includes)
cp -r "$ROOT/stage/cstrike/addons" "$C/addons"

# host-built libs
mkdir -p "$C/addons/amxmodx/dlls" "$C/addons/amxmodx/modules" "$C/addons/metamod/dlls"
cp "$HOST/libmm_amxmodx.so" "$C/addons/amxmodx/dlls/"
cp "$HOST"/modules/libamxx_*.so "$C/addons/amxmodx/modules/"
# engine Q_buildarch reports amd64/arm64 per host -- provide every name
for n in amd64 x86_64 arm64 aarch64; do
        cp "$HOST/libmetamod_android_$ARCH.so" "$C/addons/metamod/dlls/libmetamod_android_$n.so"
done

# changelevel test: NO battery (its Ham_Spawn hook on "player" calls a
# wrong vtable slot with the locally built ReGameDLL -- documented host-
# only mismatch, device gamedll verified fine) so the fake client can spawn
sed -i 's|^cs_ham_bots_api\.amxx$|;cs_ham_bots_api.amxx ; host-only: hamdata mismatch crashes host at fake-client spawn|' "$C/addons/amxmodx/configs/plugins.ini"

# statsx registers Ham_Spawn via hamdata.ini vtable offsets that do not
# match the locally built ReGameDLL (device arm64 gamedll is verified fine) —
# the first bot spawn then crashes calling the "original". Skip it here.
sed -i 's|^statsx.amxx|;statsx.amxx ; host-only: hamdata vtable mismatch with local ReGameDLL|' "$C/addons/amxmodx/configs/plugins.ini"

# gamedata + GeoIP db — same augmentation make_addons_zips.sh does for
# the shipped zip (fakemeta's ent_data natives need gamedata on disk; the
# geoip module only opens GeoLite2-*.mmdb, not legacy GeoIP.dat)
mkdir -p "$C/addons/amxmodx/data"
rm -rf "$C/addons/amxmodx/data/gamedata"
cp -r "$ROOT/amxmodx-FWGS/gamedata" "$C/addons/amxmodx/data/gamedata"
cp "$ROOT/amxmodx-FWGS/modules/geoip/GeoLite2-Country.mmdb" "$C/addons/amxmodx/data/"

# metamod plugin list: amxmodx (+ yapb on arm64 -- the DEVICE-like config;
# yapb's Bot constructor segfaults on x86_64 hosts, known host-only issue)
YAPBLIB=$(basename "$YAPB")
if [ "$ARCH" = "x86_64" ]; then
        cat > "$C/addons/metamod/plugins.ini" <<'EOF'
linux addons/amxmodx/dlls/libmm_amxmodx.so
EOF
else
        cat > "$C/addons/metamod/plugins.ini" <<EOF
linux addons/amxmodx/dlls/libmm_amxmodx.so
linux addons/yapb/dlls/$YAPBLIB
EOF
fi

# yapb: dll + config tree + nav graphs
mkdir -p "$C/addons/yapb/dlls"
cp "$YAPB" "$C/addons/yapb/dlls/"
cp -r "$ROOT/cs16-client-main/3rdparty/yapb/cfg/addons/yapb/." "$C/addons/yapb/" 2>/dev/null || true
mkdir -p "$C/addons/yapb/data"
cp -r "$ROOT/out/cs-build/graphs/addons/yapb/data/graph" "$C/addons/yapb/data/" 2>/dev/null || true
# yapb refuses to spawn bots without a graph for the map — give it any real
# graph under the test map's name (waypoints won't match, don't care)
cp "$C/addons/yapb/data/graph/cs_747.graph" "$C/addons/yapb/data/graph/amxx_test.graph" 2>/dev/null || true
cp "$ROOT/cs16-client-main/3rdparty/cs16client-extras/BotProfile.db" "$C/addons/yapb/" 2>/dev/null || true
cp "$ROOT/cs16-client-main/3rdparty/cs16client-extras/BotChatter.db" "$C/addons/yapb/" 2>/dev/null || true

# filesystem_stdio (the android-shim dlopen looks for it in the cwd)
FSFILE=$(find "$ENGINE/build" -name "filesystem_stdio.so" | head -1)
cp "$FSFILE" "$TEST/"
cp "$FSFILE" "$C/"

# gamedll as metamod's android autodetect expects it: $XASH3D_GAMELIBDIR/libserver_hardfp.so
mkdir -p "$TEST/gamelibs"
cp "$CS" "$TEST/gamelibs/libserver_hardfp.so"
cp "$CS" "$TEST/cs_amd64_real.so"
cp "$CS" "$TEST/gamelibs/cs_amd64.so"

# liblist.gam (engine picks the gamedll via -dll, metamod gets it via XASH3D_GAMELIBDIR)
cat > "$C/liblist.gam" <<'EOF'
game "Counter-Strike"
gamedir "cstrike"
type "multiplayer_only"
dll "cs"
EOF

# AMXXDEV=1: reproduce the exact Android device condition for the amxmodx
# entry. On the phone, plugins.ini names a gamedir file that does not exist
# on disk (the engine copies the ABI-suffixed .so into the app-private dir
# under the neutral name and metamod dlopens it via MM_ADDONS_ROOT).
# Pre-seed the private tree and remove the gamedir copy so the ini pathname
# stat-fails while MM_ADDONS_ROOT holds the real file -- the state that made
# the changelevel ini refresh unload a running AMX Mod X.
if [ "${AMXXDEV:-0}" = "1" ]; then
        mkdir -p "$TEST/amxxpriv/addons/amxmodx/dlls" "$TEST/amxxpriv/addons/amxmodx/modules" "$TEST/amxxpriv/addons/metamod/dlls"
        cp "$HOST/libmm_amxmodx.so" "$TEST/amxxpriv/addons/amxmodx/dlls/"
        cp "$HOST"/modules/libamxx_*.so "$TEST/amxxpriv/addons/amxmodx/modules/"
        for n in amd64 x86_64 arm64 aarch64; do
                cp "$HOST/libmetamod_android_$ARCH.so" "$TEST/amxxpriv/addons/metamod/dlls/libmetamod_android_$n.so"
        done
        rm -f "$C/addons/amxmodx/dlls/libmm_amxmodx.so"
        # tier-3 equivalent of the phone: the neutral-named core also sits
        # in the APK lib dir (XASH3D_GAMELIBDIR) -- that is the tier the
        # device actually dlopens from
        cp "$HOST/libmm_amxmodx.so" "$TEST/gamelibs/"
        echo "AMXXDEV=1: gamedir libmm_amxmodx.so removed, MM_ADDONS_ROOT tree pre-seeded"
fi

# minimal content: map + stand-in models + sounds
mkdir -p "$C/maps" "$C/models/player" "$C/sound/radio" "$C/overviews"
python3 "$ROOT/scripts/make_minimal_map.py" "$C/maps/amxx_test.bsp"
python3 "$ROOT/scripts/make_minimal_map.py" "$C/maps/de_test2.bsp"
python3 "$ROOT/scripts/make_minimal_models.py" /tmp/minimal.mdl
MDL=/tmp/minimal.mdl
for t in urban terror sas gsg9 arctic guerilla vip leet militia spetsnaz; do
        mkdir -p "$C/models/player/$t"
        cp "$MDL" "$C/models/player/$t/$t.mdl"
done
for m in player player_cs; do
        cp "$MDL" "$C/models/$m.mdl"
done
for w in knife usp glock18 p228 deagle fiveseven elite mp5navy tmp p90 mac10 ump45 ak47 m4a1 famas galil aug sg552 sg550 awp scout g3sg1 m249 m3 xm1014 vest vesthelm flash hegrenade smokegrenade kevlar; do
        cp "$MDL" "$C/models/w_$w.mdl"
        cp "$MDL" "$C/models/p_$w.mdl"
        cp "$MDL" "$C/models/v_$w.mdl"
done
for m in shell PrimedC4 PrimedGrenade w_c4 p_c4 v_c4 w_thighpack item_longjump hostage.mdl hostage01.mdl; do
        cp "$MDL" "$C/models/$m"
done
python3 - <<'PYEOF'
import struct
def wav(path, secs=0.2):
    sr = 11025
    n = int(sr * secs)
    data = b'\x80' * n
    hdr = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' + \
        struct.pack('<IHHIIHH', 16, 1, 1, sr, sr, 1, 8) + b'data' + struct.pack('<I', len(data))
    open(path, 'wb').write(hdr + data)
for s in ['blow', 'ctwin', 'rounddraw', 'twin', 'letsgo', 'fireinhole', 'moveout']:
    wav('out/cs-host-test/cstrike/sound/radio/%s.wav' % s)
PYEOF

# ---------------- minimal valve dir ----------------
mkdir -p "$TEST/valve/gfx" "$TEST/cstrike/gfx"
cat > "$TEST/valve/liblist.gam" <<'EOF'
game "Half-Life"
gamedir "valve"
type "singleplayer_only"
dll "hl"
EOF
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/gfx.wad" conchars CONBACK LAMBDA
python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/valve/decals.wad" '{break1 '{bullet1
cp "$TEST/valve/decals.wad" "$TEST/cstrike/gfx/decals.wad" 2>/dev/null || \
        python3 "$ROOT/scripts/make_minimal_wad.py" "$TEST/cstrike/decals.wad" '{break1 '{bullet1
python3 "$ROOT/scripts/make_delta_lst.py" "$ENGINE/engine/common/net_encode.c" "$TEST/valve/delta.lst"

# ---------------- run ----------------
FS=$(find "$ENGINE/build" -name "*filesystem_stdio.so" | head -1)
[ -f "$FS" ] || { echo "ERROR: filesystem_stdio not found" >&2; exit 1; }
FS=$(dirname "$FS")

cd "$TEST"
# same env the phone flow has: a writable "private root" the engine copies
# the amxx chain into (COM_AMXX_CopyTree), the gamedll dir metamod's android
# autodetect scans, and the real gamedll handed over via MM_GAMEDLL
# drive the full gamedll connect chain for fake clients (engine patch)
export XASH3D_FAKECLIENT_CONNECT=1
export XASH3D_AMXX_LIBDIR="$TEST/amxxpriv"
export XASH3D_GAMELIBDIR="$TEST/gamelibs"
export LD_LIBRARY_PATH="$FS:$TEST:$TEST/gamelibs:$LD_LIBRARY_PATH"
# changelevel stress: 3 map changes driven through the server console.
# The commands are issued by a polling driver that watches engine.log for
# each map spawn instead of fixed sleeps -- slow runners spend a variable
# amount of time on boot and precache, and a fixed schedule fired `quit`
# while only 2 of the 4 maps had spawned.
FIFO="$TEST/cmdfifo"
rm -f "$FIFO"
mkfifo "$FIFO"

LOG="$TEST/engine.log"
rm -f "$LOG"

# wait_for_spawn <count> -- poll until <count> "Spawn Server" lines exist
wait_for_spawn() {
        local need=$1 waited=0 n
        while :; do
                n=$(grep -ac 'Spawn Server' "$LOG" 2>/dev/null)
                n=${n:-0}
                [ "$n" -ge "$need" ] && return 0
                sleep 2; waited=$((waited + 2))
                [ "$waited" -ge 180 ] && { echo "timeout waiting for spawn #$need"; return 1; }
        done
}

(
        # hold the fifo open for the whole session
        exec 9>"$FIFO"
        wait_for_spawn 1 && echo "changelevel de_test2" >&9
        wait_for_spawn 2 && echo "changelevel amxx_test" >&9
        wait_for_spawn 3 && echo "changelevel de_test2" >&9
        wait_for_spawn 4
        sleep 20
        echo "quit" >&9
) &
DRIVER_PID=$!

timeout -k 10 600 "$ENGINE/build/engine/xash" \
        -game cstrike -dev 2 -log -condebug -dedicated \
        -dll "$TEST/gamelibs/libserver_hardfp.so" \
        +map amxx_test +hostname "host-test" +sv_lan 1 +yb_quota 2 +mp_freezetime 0 +mp_roundtime 2 \
        -noip -nojoy -nosteam < "$FIFO" > console.txt 2>&1 || true

kill "$DRIVER_PID" 2>/dev/null || true
wait "$DRIVER_PID" 2>/dev/null || true
rm -f "$FIFO"

echo "================= BATTERY ================="
grep -a "\[BAT\]" console.txt | head -80
echo "================= AMXX ERRORS ================="
grep -a -E "Run time error|not available|hook unavailable|disabled" console.txt | head -20
echo "================= MODULES ================="
grep -a -A40 "Loaded modules\|amxx modules" console.txt | grep -a -E "fun|engine|fakemeta|cstrike|csx|nvault|hamsandwich|sockets|geoip|regex|sqlite|json" | head -20
echo "================= LOG TAIL ================="
tail -25 console.txt

# ---------------- evaluation ----------------
# console.txt loses everything after the first ~5s (stdio block buffering
# into a file) -- the engine's own -condebug engine.log is the full record.
LOG="$TEST/engine.log"
PASS=1; FAIL=""

SPAWNS=$(grep -ac "Spawn Server" "$LOG" 2>/dev/null || echo 0)
echo "map spawns: $SPAWNS (need 4 = initial + 3 changelevels)"
[ "$SPAWNS" -ge 4 ] || { PASS=0; FAIL="$FAIL map-spawns=$SPAWNS"; }

grep -aqiE "SIGSEGV|Segmentation|Sys_Crash|Crash: signal|Backtrace:" "$LOG" && { PASS=0; FAIL="$FAIL crash-log"; }
grep -aqiE "SIGSEGV|Segmentation|Sys_Crash|Crash: signal|Backtrace:" "$TEST/console.txt" 2>/dev/null && { PASS=0; FAIL="$FAIL crash-stdout"; }

# every map spawn must run the full module re-attach cycle: the cstrike
# module (reload-on-mapchange) has to detach cleanly and come back on each
# of the maps; this is exactly the path that used to leave dangling hooks
CSTRIKE_ATTACHES=$(grep -ac "Module path is .*libamxx_cstrike" "$LOG" || true)
echo "cstrike module attaches: $CSTRIKE_ATTACHES (need >= 4, one per map)"
[ "$CSTRIKE_ATTACHES" -ge 4 ] || { PASS=0; FAIL="$FAIL cstrike-attaches=$CSTRIKE_ATTACHES"; }
# amxx must stay attached across every changelevel: each map logs one
# "Mapchange to" line from C_ServerActivate while the amxx plugin lives;
# a stat-failed ini refresh (device bug) leaves only the first map's line
MAPCHANGES=$(grep -ac "Mapchange to" "$LOG" || true)
echo "amxx mapchange lines: $MAPCHANGES (need >= 3)"
[ "$MAPCHANGES" -ge 3 ] || { PASS=0; FAIL="$FAIL mapchange-lines=$MAPCHANGES"; }
grep -aq "can't be used twice on the hookchain" "$LOG" && { PASS=0; FAIL="$FAIL hookchain-duplicate"; }
grep -aqi "failed to load" "$LOG" && { PASS=0; FAIL="$FAIL plugin-load-failure"; }
grep -aq "Run time error" "$LOG" && { PASS=0; FAIL="$FAIL amxx-runtime-error"; }

if [ "$PASS" = 1 ]; then
        echo "RESULT: PASS -- $SPAWNS map spawns (3 changelevels) with the full module chain, no crashes on $ARCH"
else
        echo "RESULT: FAIL:$FAIL"
fi
exit $((1 - PASS))
