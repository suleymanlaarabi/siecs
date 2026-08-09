#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
config=${1:-debug}
case "$config" in
    debug)
        opt_level=-O0
        debug_flags=-g3
        link_flags=-sASSERTIONS=2
        ;;
    release)
        opt_level=-O3
        debug_flags=
        link_flags=-sASSERTIONS=0
        ;;
    *)
        echo "usage: $0 [debug|release]" >&2
        exit 2
        ;;
esac

command -v emcc >/dev/null 2>&1 || {
    echo "build_wasm.sh: emcc is required" >&2
    exit 1
}
command -v em++ >/dev/null 2>&1 || {
    echo "build_wasm.sh: em++ is required" >&2
    exit 1
}

bake_home=${BAKE_HOME:-}
if [ -z "$bake_home" ] && command -v bake >/dev/null 2>&1; then
    bake_home=$(bake env | sed -n 's/^BAKE_HOME=//p')
fi
deps_flags=
if [ -n "$bake_home" ]; then
    deps_flags="-I$bake_home/include"
fi

out_root="$repo_root/build-wasm/$config"
rm -rf "$out_root"
mkdir -p "$out_root/c" "$out_root/cpp"

sh "$repo_root/tools/rebuild_distr.sh" "$repo_root"

common_flags="-std=c17 $opt_level $debug_flags -I$repo_root/distr"
module_flags="-sMODULARIZE=1 -sENVIRONMENT=web,node"

# These exports are test entry points, not a new JavaScript-facing SIECS API.
emcc $common_flags \
    "$repo_root/distr/siecs.c" "$repo_root/test/wasm/smoke.c" \
    $module_flags \
    $link_flags \
    -sEXPORT_NAME=createSiecsC \
    -sEXPORTED_FUNCTIONS=_siecs_wasm_smoke \
    -o "$out_root/c/siecs_wasm.js"

emcc $common_flags -c "$repo_root/distr/siecs.c" -o "$out_root/cpp/siecs.o"
em++ -std=c++20 $opt_level $debug_flags \
    -I"$repo_root/distr" -I"$repo_root/include" $deps_flags \
    "$repo_root/test/wasm/smoke.cpp" "$out_root/cpp/siecs.o" \
    $module_flags \
    $link_flags \
    -sEXPORT_NAME=createSiecsCpp \
    -sEXPORTED_FUNCTIONS=_siecs_wasm_cpp_smoke \
    -o "$out_root/cpp/siecs_wasm.js"
