#!/bin/bash
# Build the SMA->AMXX compiler driver for the LOCAL HOST (Linux x86_64).
# Same recipe as glue/amxxpc/jni/Android.mk (libpc300 static + amxxpc driver
# + amx_shim), just compiled with the system toolchain so we can compile
# .sma plugins locally without the NDK.
#
# Output: glue/out/amxxpc-host/amxxpc
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
AMXX_SRC="$ROOT/amxmodx-FWGS"
PC300="$AMXX_SRC/compiler/libpc300"
AMXXPC="$AMXX_SRC/compiler/amxxpc"
OUT="$HERE/out/amxxpc-host"
CC=${CC:-gcc}
CXX=${CXX:-g++}

mkdir -p "$OUT/obj"

PC300_CFLAGS="-O2 -w -DNDEBUG -DLINUX -DENABLE_BINRELOC -DNO_MAIN -DPAWNC_DLL -DHAVE_STDINT_H -D_GNU_SOURCE -I$PC300"

echo "== 1/2 libpc300 (pawn compiler core, static) =="
PC300_OBJS=""
for src in sc1.c sc2.c sc3.c sc4.c sc5.c sc6.c sc7.c scvars.c scmemfil.c \
           scstate.c sclist.c sci18n.c libpawnc.c prefix.c \
           memfile.c sp_symhash.c; do
        obj="$OUT/obj/$(basename "$src" .c).o"
        [ -f "$obj" ] || "$CC" $PC300_CFLAGS -c "$PC300/$src" -o "$obj"
        PC300_OBJS="$PC300_OBJS $obj"
done

echo "== 2/2 amxxpc driver =="
"$CXX" -O2 -w -DNDEBUG -DAMX_ANSIONLY -DHAVE_STDINT_H -DAMXXPC_NO_DLOPEN -include cstddef \
        -I"$AMXXPC" -I"$AMXX_SRC/public" -I"$PC300" \
        "$HERE/amxxpc/amx_shim.cpp" \
        "$AMXXPC/amxxpc.cpp" \
        "$AMXXPC/Binary.cpp" \
        $PC300_OBJS \
        -o "$OUT/amxxpc" -lz -ldl -lm

echo ">> host amxxpc built:"
ls -la "$OUT/amxxpc"
