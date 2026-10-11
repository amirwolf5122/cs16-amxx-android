#!/bin/bash
# Assemble the minimal Counter-Strike content pack for the CI arm64 test:
#   testcontent/cstrike — gamedir with AMXX addons, test map, stand-in models
#   testcontent/valve   — minimal valve dir (engine wants it to exist)
# Everything is generated (no game assets are shipped).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
rm -rf testcontent
mkdir -p testcontent/cstrike testcontent/valve

# 1) addons: metamod + amxmodx full tree
mkdir -p testcontent/cstrike/addons
unzip -qo out/cstrike-addons.zip -d testcontent/cstrike/

# 2) test map
mkdir -p testcontent/cstrike/maps
python3 scripts/make_minimal_map.py testcontent/cstrike/maps/amxx_test.bsp

# 3) stand-in models (valid minimal studio models)
python3 scripts/make_minimal_models.py /tmp/minimal.mdl
MDL=/tmp/minimal.mdl
mkdir -p testcontent/cstrike/models/player
for t in urban terror sas gsg9 arctic guerilla vip leet militia spetsnaz; do
	mkdir -p "testcontent/cstrike/models/player/$t"
	cp "$MDL" "testcontent/cstrike/models/player/$t/$t.mdl"
done
mkdir -p testcontent/cstrike/models
for m in player player_cs; do
	cp "$MDL" "testcontent/cstrike/models/$m.mdl"
done
# default loadout weapon models (server precaches them when players spawn)
for w in knife usp glock18 p228 deagle fiveseven elite mp5navy tmp p90 mac10 ump45 ak47 m4a1 famas galil aug sg552 sg550 awp scout g3sg1 m249 m3 xm1014 vest vesthelm flash hegrenade smokegrenade kevlar; do
	cp "$MDL" "testcontent/cstrike/models/w_$w.mdl"
	cp "$MDL" "testcontent/cstrike/models/p_$w.mdl"
	cp "$MDL" "testcontent/cstrike/models/v_$w.mdl"
done
# misc studio models the gameDLL tends to touch
for m in shell PrimedC4 PrimedGrenade w_c4 p_c4 v_c4; do
	cp "$MDL" "testcontent/cstrike/models/$m.mdl"
done
# world models for items
for m in w_thighpack item_longjump; do
	cp "$MDL" "testcontent/cstrike/models/$m.mdl"
done
# hostage + chicken (CS content)
cp "$MDL" "testcontent/cstrike/models/hostage.mdl"
cp "$MDL" "testcontent/cstrike/models/hostage01.mdl"

# 4) sound placeholders — missing sounds are warnings only, but give the
#    most common radio ones real (silence) wav files to avoid spam
mkdir -p testcontent/cstrike/sound/radio
python3 - <<'PYEOF'
import struct, math
def wav(path, secs=0.2):
    sr = 11025
    n = int(sr * secs)
    data = b'\x80' * n
    hdr = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' + \
        struct.pack('<IHHIIHH', 16, 1, 1, sr, sr, 1, 8) + b'data' + struct.pack('<I', len(data))
    open(path, 'wb').write(hdr + data)
for s in ['blow', 'ctwin', 'rounddraw', 'twin', 'letsgo', 'fireinhole', 'moveout']:
    wav('testcontent/cstrike/sound/radio/%s.wav' % s)
PYEOF

# 5) minimal valve dir (liblist.gam so xash is happy if it probes valve)
cat > testcontent/valve/liblist.gam <<'EOF'
game "Half-Life"
gamedir "valve"
type "singleplayer_only"
dll "hl"
EOF

echo ">> test content:"
du -sh testcontent/cstrike testcontent/valve
find testcontent -type f | wc -l
