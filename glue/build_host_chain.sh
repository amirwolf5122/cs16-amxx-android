#!/bin/bash
# Build the AMXX chain for the HOST (linux, arm64 or x86_64) so the CI can
# run the REAL metamod -> amxmodx -> modules -> plugins chain natively on a
# real arm64 runner (same 64-bit-pointer/32-bit-cell code paths as the phone).
#
# Trick: everything is compiled with -D__ANDROID__ so the exact Android code
# paths (MM_GAMELIBDIR module loading, libamxx_<name>.so naming) are used —
# the binaries are real Linux ELF shared objects for the host arch.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)/glue
ROOT=$(dirname "$HERE")
AMXX="$ROOT/amxmodx-FWGS"
MM="$ROOT/metamod-fwgs"
HLSDK="$MM/hlsdk"
FSINC="$ROOT/filesystem_stdio_xash/include"
OUT="$ROOT/out/host"
BUILD=$(uname -m)
mkdir -p "$OUT/modules"

CXX=${CXX:-g++}
CC=${CC:-gcc}
COMMON="-O2 -g -fPIC -fpermissive -Wno-error -w -D__ANDROID__ -DLINUX -DHAVE_STDINT_H \
  -include $HERE/compat_types.h \
  -Dstricmp=strcasecmp -D_stricmp=strcasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf -D_vsnprintf=vsnprintf \
  -D_INCLUDE_SEQUENCE_H_ -D_GNU_SOURCE -std=gnu++11 \
  -I$AMXX/public/sdk -I$AMXX/public -I$AMXX/public/memtools -I$AMXX/public/amtl -I$AMXX/public/amtl/amtl \
  -I$AMXX/third_party/hashing -I$AMXX/third_party/utf8rewind -I$AMXX/third_party/pcre -I$AMXX/third_party/zlib \
  -I$AMXX/third_party/parson -I$AMXX/third_party -I$HERE -I$FSINC \
  -I$HLSDK/dlls -I$HLSDK/public -I$HLSDK/common -I$HLSDK/engine -I$HLSDK/pm_shared -I$MM/metamod -I$AMXX/amxmodx -I$HERE/android_shim"

echo "== building third_party static bits (hashing + utf8rewind) =="
HASHING=$(find $AMXX/third_party/hashing -name '*.c*' | grep -v test)
UTF8="$AMXX/third_party/utf8rewind/utf8rewind.c $AMXX/third_party/utf8rewind/unicodedatabase.c $AMXX/third_party/utf8rewind/internal/casemapping.c $AMXX/third_party/utf8rewind/internal/codepoint.c $AMXX/third_party/utf8rewind/internal/composition.c $AMXX/third_party/utf8rewind/internal/database.c $AMXX/third_party/utf8rewind/internal/decomposition.c $AMXX/third_party/utf8rewind/internal/seeking.c $AMXX/third_party/utf8rewind/internal/streaming.c"
THIRDPARTY_O=""
mkdir -p "$OUT/tp"
for f in $HASHING $UTF8; do
        o="$OUT/tp/$(basename $f | tr '/.' '__').o"
        $CXX $COMMON -c -o "$o" "$f"
        THIRDPARTY_O="$THIRDPARTY_O $o"
done

echo "== building amxmodx core =="
CORE="CCmd CDataPack CEvent CFlagManager CForward CGameConfigs CLang CLibrarySys CLogEvent CMenu CMisc CModule CPlugin CTask CTextParsers CVault CoreConfig CvarManager amx amxcore amxdbg amxmodx amxtime amxxfile amxxlog cvars datapacks datastructs debugger emsg fakemeta file float format gameconfigs libraries messages meta_api modules natives newmenus nongpl_matches optimizer power sorting srvcmd stackstructs string strptime textparse trie_natives util vault vector MemoryUtils detours asm mod_rehlds_api"
mkdir -p "$OUT/core"
CORE_O=""
for s in $CORE; do
        for src in "$AMXX/amxmodx/$s.cpp" "$AMXX/amxmodx/$s.c" "$AMXX/public/memtools/$s.cpp" "$AMXX/public/memtools/CDetour/$s.cpp" "$AMXX/public/memtools/CDetour/asm/$s.c" "$AMXX/public/resdk/$s.cpp"; do
                if [ -f "$src" ]; then
                        o="$OUT/core/$s.o"
                        $CXX $COMMON -c -o "$o" "$src"
                        CORE_O="$CORE_O $o"
                        break
                fi
        done
done
$CXX $COMMON -shared -o "$OUT/libmm_amxmodx.so" $CORE_O $THIRDPARTY_O -lz -lpthread -ldl
echo "  => $OUT/libmm_amxmodx.so"

echo "== building modules =="
build_module() { # <name> <dir> <srcs...>  (MODULE_CFLAGS may hold extra flags)
        local name=$1 dir=$2; shift 2
        local objs="" src=""
        mkdir -p "$OUT/obj_$name"
        # SDK glue + detour machinery is part of every module (matches the
        # Android AMBuild: engine module dlopens only if CDetourManager is in)
        set -- "$@" amxxmodule MemoryUtils detours asm
        for s in "$@"; do
                src=""
                if [ -f "$s" ]; then
                        src="$s"          # absolute paths (trampoline shim)
                else
                        for cand in "$AMXX/modules/$dir/$s.cpp" "$AMXX/public/sdk/$s.cpp" \
                                    "$AMXX/public/memtools/$s.cpp" \
                                    "$AMXX/public/memtools/CDetour/$s.cpp" \
                                    "$AMXX/public/memtools/CDetour/asm/$s.c" \
                                    "$AMXX/public/memtools/MemoryUtils.cpp"; do
                                if [ -f "$cand" ]; then
                                        src="$cand"
                                        break
                                fi
                        done
                fi
                if [ -n "$src" ]; then
                        o="$OUT/obj_$name/$(basename $s)_$name.o"
                        $CXX $COMMON $MODULE_CFLAGS -I"$AMXX/modules/$dir" -c -o "$o" "$src"
                        objs="$objs $o"
                else
                        echo "  !! $name: source for '$s' not found, skipped" >&2
                fi
        done
        $CXX $COMMON -shared -o "$OUT/modules/libamxx_$name.so" $objs $THIRDPARTY_O
        echo "  => libamxx_$name.so"
}

build_module fun fun fun
build_module engine engine amxxapi engine entity forwards globals
# fakemeta carries its own resdk glue (see cstrike above)
build_module fakemeta fakemeta dllfunc engfunc fakemeta_amxx fm_tr fm_tr2 forward glb misc pdata pdata_entities pdata_gamerules pev \
                "$AMXX/public/resdk/mod_regamedll_api.cpp"
# -DNO_HACKS matches the module's official CMakeLists build and is
# required on Xash3D (no HLDS svs/sv globals; pure-engfuncs model updates).
# mod_rehlds_api/mod_regamedll_api are compiled in statically (same as the
# Android Android.mk) so the module resolves the ReGameDLL/ReHLDS APIs via
# metamod's gamedll path instead of importing undefined symbols.
MODULE_CFLAGS="-DNO_HACKS" \
        build_module cstrike cstrike/cstrike CstrikeHacks CstrikeItemsInfos CstrikeMain CstrikeNatives CstrikePlayer CstrikeUserMessages CstrikeUtils \
                "$AMXX/public/resdk/mod_rehlds_api.cpp" "$AMXX/public/resdk/mod_regamedll_api.cpp"
build_module csx cstrike/csx CMisc CRank meta_api rank usermsg
build_module nvault nvault Binary Journal NVault amxxapi
build_module sockets sockets sockets

# hamsandwich on the host -- on x86_64 use the upstream generic
# trampoline (Trampolines.h) so hooks are REAL; on arm64 hosts (CI) keep
# the hand-written ARM libffcall shim (the generic emitter is x86-only)
# the x86_64 host builds host_v33_hook_callbacks.cpp instead. The
# hook_callbacks.cpp rewrite (per-callback `Hook *hook = ::hook;` snapshots
# + gDoForwards reentrancy restructure) was written for and is verified on
# the libffcall/Android path only; compiled in generic-trampoline mode it
# segfaults inside SV_FakeConnect on the host. The Android (device) build
# is untouched.
if [ "$BUILD" = "x86_64" ]; then
        build_module hamsandwich hamsandwich amxx_api config_parser \
                "$HERE/host_v33_hook_callbacks.cpp" hook_native srvcmd call_funcs hook_create \
                DataHandler pdata hook_specialbot
else
        MODULE_CFLAGS="-DUSE_LIBFFCALL -I$HERE/trampoline" \
                build_module hamsandwich hamsandwich amxx_api config_parser \
                hook_callbacks hook_native srvcmd call_funcs hook_create \
                DataHandler pdata hook_specialbot "$HERE/trampoline/trampoline.c"
fi

# cs_ham_bots_api library-anchor module (zombie plague mods; the
# natives live in hamsandwich + the staged cs_ham_bots_api.amxx plugin)
build_module cs_ham_bots_api cs_ham_bots_api amxxapi

# regex / geoip / json / sqlite — the remaining staged modules. These
# mix C third-party sources (pcre/maxminddb/parson/sqlite3) with C++ module
# code, so compile C with gcc and C++ with g++ instead of via build_module.
build_c_module() { # <name> <dir> <cflags> <c-srcs...> -- <cxx-srcs...>
        local name=$1 dir=$2 cflags=$3; shift 3
        local csrcs=() cxxsrcs=() in_cxx=0 o="" src=""
        for s in "$@"; do
                if [ "$s" = "--" ]; then in_cxx=1; continue; fi
                if [ $in_cxx -eq 0 ]; then csrcs+=("$s"); else cxxsrcs+=("$s"); fi
        done
        mkdir -p "$OUT/obj_$name"
        local objs=""
        for src in "${csrcs[@]}"; do
                o="$OUT/obj_$name/$(basename "$src" | tr '/.' '__').o"
                gcc $COMMON $cflags -std=gnu99 -c -o "$o" "$src"
                objs="$objs $o"
        done
        for src in "${cxxsrcs[@]}"; do
                o="$OUT/obj_$name/$(basename "$src" | tr '/.' '__').o"
                if [ -f "$src" ]; then :; else src="$AMXX/modules/$dir/$src.cpp"; fi
                $CXX $COMMON $cflags -I"$AMXX/modules/$dir" -c -o "$o" "$src"
                objs="$objs $o"
        done
        $CXX $COMMON -shared -o "$OUT/modules/libamxx_$name.so" $objs $THIRDPARTY_O
        echo "  => libamxx_$name.so"
}

PCRE=$(ls "$AMXX"/third_party/pcre/pcre_*.c | grep -v dftables)
build_c_module regex regex "-DPCRE_STATIC -DHAVE_CONFIG_H" $PCRE -- "$AMXX/public/sdk/amxxmodule.cpp" CRegEx module utils
build_c_module geoip geoip "-I$AMXX/third_party/libmaxminddb" \
        "$AMXX/third_party/libmaxminddb/maxminddb.c" -- "$AMXX/public/sdk/amxxmodule.cpp" geoip_main geoip_natives geoip_util
build_c_module json json "" \
        "$AMXX/third_party/parson/parson.c" -- "$AMXX/public/sdk/amxxmodule.cpp" JsonMngr JsonNatives
build_c_module sqlite sqlite "-I$AMXX/modules/sqlite/sqlitepp -I$AMXX/modules/sqlite/thread -DSM_DEFAULT_THREADER -pthread" \
        "$AMXX/third_party/sqlite/sqlite3.c" -- "$AMXX/public/sdk/amxxmodule.cpp" basic_sql handles module threading oldcompat_sql \
        thread/BaseWorker thread/ThreadWorker sqlitepp/SqliteQuery sqlitepp/SqliteResultSet \
        sqlitepp/SqliteDatabase sqlitepp/SqliteDriver thread/PosixThreads

echo "== building metamod (host) =="
MMSRC="$ROOT/metamod-p-velaron/metamod"
mkdir -p "$OUT/obj_mm"
MMFLAGS="-O2 -g -fPIC -std=gnu++98 -fpermissive -fno-exceptions -fno-rtti -w -D__METAMOD_BUILD__ -DCOMPILE_TZ=\"UTC\" -D__ANDROID__ \
  -I$MMSRC -I$HLSDK/engine -I$HLSDK/common -I$HLSDK/dlls -I$HLSDK/pm_shared -I$HLSDK"
MMOBJS=""
for s in api_hook api_info commands_meta conf_meta dllapi engine_api engineinfo game_support game_autodetect \
        h_export linkgame linkplug log_meta meta_eiface metamod mlist mplayer mplugin mqueue mreg mutil osdep \
        osdep_p reg_support sdk_util studioapi support_meta thread_logparse vdate \
        osdep_linkent_linux osdep_detect_gamedll_linux xash_exports; do
        $CXX $MMFLAGS -c -o "$OUT/obj_mm/$s.o" "$MMSRC/$s.cpp"
        MMOBJS="$MMOBJS $OUT/obj_mm/$s.o"
done
$CXX -shared -o "$OUT/libmetamod_android_$BUILD.so" $MMOBJS -ldl -lm
echo "  => libmetamod_android_$BUILD.so"

echo "== hlsdk-portable server (gamedll) =="
if [ ! -f "$OUT/libserver.so" ]; then
        cmake -S "$ROOT/xash3d-fwgs-master/3rdparty/hlsdk-portable" -B "$OUT/hlsdk-build" \
                -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=OFF -DBUILD_LIBMENU=OFF -DBUILD_GAMEUI=OFF > /dev/null
        cmake --build "$OUT/hlsdk-build" -j"$(nproc)" > /dev/null 2>&1 || cmake --build "$OUT/hlsdk-build" -j"$(nproc)"
find "$OUT/hlsdk-build" -name "libserver.so" -exec cp {} "$OUT/libserver.so" \;
fi
ls -la "$OUT"/lib*.so "$OUT"/modules/ | head -15
echo "HOST BUILD DONE ($BUILD)"
