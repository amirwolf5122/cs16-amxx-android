#!/bin/bash
# fetch_amxx_version.sh — make the shipped AMX Mod X
# version selectable at build time (1.8.0 .. latest, each
# version its own zip, default 1.8.3).
#
#   1.8.2  -> alliedmodders/amxmodx tag 1.8.2
#   1.8.3  -> the in-tree amxmodx-FWGS fork (default; carries our core
#             fixes: cvar auto-create, 64-bit-safe fakemeta handles, ...)
#   1.9    -> alliedmodders/amxmodx branch 1.9
#   1.10   -> alliedmodders/amxmodx master (1.10/1.11 dev)
#
# Minimal patches live in patches/amxx/<ver>/*.patch and are applied with
# `git apply`. If ANY patch fails to apply the build FALLS BACK to the
# in-tree fork (the "backup" the user asked for) with a loud warning —
# a guaranteed-build is worth more than an exact upstream snapshot.
#
# Output: writes the chosen source dir to stdout (AMXX_SRC) and
# AMXX_VERSION_LABEL to stdout line 2.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VER=${1:-1.8.3}
FWGS="$ROOT/amxmodx-FWGS"

case "$VER" in
        1.8.3|1.8.3-dev|"")
                echo "$FWGS"
                echo "1.8.3-dev-fwgs"
                exit 0
                ;;
        1.8.2)
                REPO_REF="--branch 1.8.2 --depth 1"
                LABEL="1.8.2"
                ;;
        1.9)
                REPO_REF="--branch 1.9 --depth 1"
                LABEL="1.9"
                ;;
        1.10|master|latest)
                REPO_REF="--depth 1"
                LABEL="1.10"
                ;;
        *)
                echo "ERROR: unsupported AMXX version '$VER' (use 1.8.2|1.8.3|1.9|1.10)" >&2
                exit 1
                ;;
esac

TARGET="/tmp/amxx-$LABEL"
rm -rf "$TARGET"
if ! git clone $REPO_REF https://github.com/alliedmodders/amxmodx "$TARGET" 2>/dev/null; then
        echo "WARN: could not fetch upstream amxmodx $LABEL — falling back to in-tree 1.8.3-dev fork" >&2
        echo "$FWGS"; echo "1.8.3-dev-fwgs-fallback-fetch"
        exit 0
fi

# minimal patches (per version) — the FWGS fork's device fixes
PATCHDIR="$ROOT/patches/amxx/$LABEL"
APPLIED=0; FAILED=0
if [ -d "$PATCHDIR" ]; then
        for p in "$PATCHDIR"/*.patch; do
                [ -f "$p" ] || continue
                if git -C "$TARGET" apply --check "$p" 2>/dev/null; then
                        git -C "$TARGET" apply "$p" && APPLIED=$((APPLIED+1))
                else
                        echo "WARN: patch $(basename "$p") does not apply on $LABEL" >&2
                        FAILED=$((FAILED+1))
                fi
        done
fi
if [ "$FAILED" -gt 0 ]; then
        echo "WARN: $FAILED patch(es) failed on $LABEL — falling back to in-tree 1.8.3-dev fork (backup)" >&2
        echo "$FWGS"; echo "1.8.3-dev-fwgs-fallback-patches"
        exit 0
fi

echo "INFO: upstream amxmodx $LABEL fetched, $APPLIED patch(es) applied" >&2
echo "$TARGET"
echo "$LABEL"
