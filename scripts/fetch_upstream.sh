#!/bin/bash
# fetch_upstream.sh — pull the real
# upstream sources, apply the minimal patch series from patches/<name>/, and
# FALL BACK to the in-tree copy (the "backup") whenever anything does not
# apply. Prints the chosen source dir on stdout.
#
#   usage: fetch_upstream.sh <name> [ref]
#     amxxmod     -> (see fetch_amxx_version.sh, kept separate for versions)
#     cs16client  -> github.com/Velaron/cs16-client
#     xash3d      -> github.com/FWGS/xash3d-fwgs
#     filesystem  -> github.com/FWGS/filesystem_stdio_xash
#     metamod-fwgs-> github.com/FWGS/metamod-fwgs
#     metamod-p   -> github.com/Bots-United/metamod-p
#
# Migration status:
#   - amxmodx: FULLY LIVE (scripts/fetch_amxx_version.sh, selectable)
#   - everything else: opt-in via USE_UPSTREAM=1; the in-tree copies carry
#     hundreds of device fixes (MOTD, fake-client chain, crash forensics,
#     metamod ini patcher, ...). The patch series for those is being
#     extracted step by step; until then upstream fetch falls back to the
#     in-tree copy unless the whole series applies cleanly.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NAME=$1; REF=${2:-}

case "$NAME" in
        cs16client)  REPO=https://github.com/Velaron/cs16-client ;;
        xash3d)      REPO=https://github.com/FWGS/xash3d-fwgs ;;
        filesystem)  REPO=https://github.com/FWGS/filesystem_stdio_xash ;;
        metamod-fwgs) REPO=https://github.com/FWGS/metamod-fwgs ;;
        metamod-p)   REPO=https://github.com/Bots-United/metamod-p ;;
        *) echo "ERROR: unknown repo '$NAME'" >&2; exit 1 ;;
esac

# canonical in-tree (backup) copies in this monorepo
case "$NAME" in
        cs16client)   INTREE="$ROOT/cs16-client-main" ;;
        xash3d)       INTREE="$ROOT/xash3d-fwgs-master" ;;
        filesystem)   INTREE="$ROOT/filesystem_stdio_xash" ;;
        metamod-fwgs) INTREE="$ROOT/metamod-fwgs" ;;
        metamod-p)    INTREE="$ROOT/metamod-p-velaron" ;;
esac

TARGET="/tmp/upstream-$NAME"
rm -rf "$TARGET"
if ! git clone ${REF:+--branch "$REF" --depth 1} --depth 1 "$REPO" "$TARGET" 2>/dev/null; then
        echo "WARN: fetch $NAME failed — using in-tree backup $INTREE" >&2
        echo "$INTREE"; exit 0
fi

# Safety gate: an upstream tree WITHOUT its full device patch series is
# NOT buildable for this project (the in-tree copies carry the MOTD,
# fake-client chain, crash forensics, ...). If no patch series exists yet,
# stay on the in-tree backup.
if ! ls "$ROOT/patches/$NAME"/*.patch >/dev/null 2>&1; then
        echo "INFO: no patch series for $NAME yet — using in-tree backup $INTREE" >&2
        echo "$INTREE"; exit 0
fi

FAILED=0
for p in "$ROOT/patches/$NAME"/*.patch; do
        [ -f "$p" ] || continue
        if git -C "$TARGET" apply --check "$p" 2>/dev/null; then
                git -C "$TARGET" apply "$p" || FAILED=$((FAILED+1))
        else
                echo "WARN: patch $(basename "$p") does not apply on upstream $NAME" >&2
                FAILED=$((FAILED+1))
        fi
done
if [ "$FAILED" -gt 0 ]; then
        echo "WARN: $FAILED patch(es) failed on upstream $NAME — using in-tree backup" >&2
        echo "$INTREE"; exit 0
fi

echo "INFO: upstream $NAME fetched with full patch series" >&2
echo "$TARGET"
