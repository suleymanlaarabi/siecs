#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(mktemp -d /tmp/siecs-features.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT

cc=${CC:-cc}
cxx=${CXX:-c++}
bake_home=$(bake env | sed -n 's/^BAKE_HOME=//p')
bake_target=$(bake env | sed -n 's/^BAKE_TARGET=//p')
dependency_libs="$bake_target/lib"
sources=$(rg --files "$repo_root/src" -g '*.c')

has_symbol() {
    nm -D --defined-only "$1" | rg -q "$2"
}

has_reference() {
    nm -D --undefined-only "$1" | rg -q "$2"
}

assert_no_symbol() {
    if has_symbol "$1" "$2"; then
        echo "unexpected symbol matching '$2' in $1" >&2
        exit 1
    fi
}

assert_no_reference() {
    if has_reference "$1" "$2"; then
        echo "unexpected reference matching '$2' in $1" >&2
        exit 1
    fi
}

check_config() {
    defs=$1
    expected=$2

    # shellcheck disable=SC2086
    "$cc" -std=c23 -I"$repo_root/include" $defs $expected \
        -fsyntax-only "$repo_root/test/standalone/config.c"
}

build_variant() {
    variant=$1
    defs=$2
    expected=$3
    libs=$4
    output="$build_dir/libsiecs_$variant.so"

    # shellcheck disable=SC2086
    "$cc" -std=c23 -fPIC -shared -Wl,-z,defs \
        -I"$repo_root/include" -I"$repo_root/src" -I"$bake_home/include" \
        $defs $sources -L"$dependency_libs" $libs -lpthread -o "$output"

    # shellcheck disable=SC2086
    "$cc" -std=c23 -I"$repo_root/include" -I"$bake_home/include" \
        $defs $expected "$repo_root/test/standalone/features.c" \
        -L"$build_dir" -Wl,-rpath,"$build_dir" -l"siecs_$variant" \
        -L"$dependency_libs" $libs -lpthread -o "$build_dir/$variant"

    LD_LIBRARY_PATH="$build_dir:$dependency_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$build_dir/$variant"
}

build_cpp_variant() {
    variant=$1
    defs=$2
    expected=$3
    libs=$4

    # shellcheck disable=SC2086
    "$cxx" -std=c++23 -I"$repo_root/include" -I"$bake_home/include" \
        $defs $expected "$repo_root/test/standalone/features.cpp" \
        -L"$build_dir" -Wl,-rpath,"$build_dir" -l"siecs_$variant" \
        -L"$dependency_libs" $libs -lpthread -o "$build_dir/${variant}_cpp"

    LD_LIBRARY_PATH="$build_dir:$dependency_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$build_dir/${variant}_cpp"
}

build_distr_variant() {
    variant=$1
    defs=$2
    expected=$3
    output="$build_dir/libsiecs_distr_$variant.so"

    # shellcheck disable=SC2086
    "$cc" -std=c23 -fPIC -shared -Wl,-z,defs \
        $defs "$repo_root/distr/siecs.c" -lpthread -o "$output"

    # shellcheck disable=SC2086
    "$cc" -std=c23 -I"$repo_root/distr" \
        $defs $expected "$repo_root/test/standalone/features.c" \
        -L"$build_dir" -Wl,-rpath,"$build_dir" -l"siecs_distr_$variant" \
        -lpthread -o "$build_dir/distr_$variant"

    LD_LIBRARY_PATH="$build_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$build_dir/distr_$variant"

    # shellcheck disable=SC2086
    "$cxx" -std=c++23 -I"$repo_root/distr" \
        $defs $expected "$repo_root/test/standalone/features.cpp" \
        -L"$build_dir" -Wl,-rpath,"$build_dir" -l"siecs_distr_$variant" \
        -lpthread -o "$build_dir/distr_${variant}_cpp"

    LD_LIBRARY_PATH="$build_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$build_dir/distr_${variant}_cpp"
}

check_config "" "-DEXPECT_NAMES=1 -DEXPECT_META=1 -DEXPECT_REST=1"
check_config "-DSIECS_NO_NAMES" "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=1"
check_config "-DSIECS_NO_META" "-DEXPECT_NAMES=1 -DEXPECT_META=0 -DEXPECT_REST=0"
check_config "-DSIECS_NO_REST" "-DEXPECT_NAMES=1 -DEXPECT_META=1 -DEXPECT_REST=0"
check_config "-DSIECS_CUSTOM_BUILD" "-DEXPECT_NAMES=0 -DEXPECT_META=0 -DEXPECT_REST=0"

build_variant minimal \
    "-DSIECS_CUSTOM_BUILD" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=0 -DEXPECT_REST=0" \
    ""
assert_no_symbol "$build_dir/libsiecs_minimal.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find|ecs_rest_|init_rest|Name__"
assert_no_reference "$build_dir/libsiecs_minimal.so" "sihttp_|sijson_|sireflect_"
build_cpp_variant minimal \
    "-DSIECS_CUSTOM_BUILD" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=0 -DEXPECT_REST=0" \
    ""

build_variant names \
    "-DSIECS_CUSTOM_BUILD -DSIECS_NAMES" \
    "-DEXPECT_NAMES=1 -DEXPECT_META=0 -DEXPECT_REST=0" \
    ""
has_symbol "$build_dir/libsiecs_names.so" "ecs_entity_name"
assert_no_reference "$build_dir/libsiecs_names.so" "sihttp_|sijson_|sireflect_"
build_cpp_variant names \
    "-DSIECS_CUSTOM_BUILD -DSIECS_NAMES" \
    "-DEXPECT_NAMES=1 -DEXPECT_META=0 -DEXPECT_REST=0" \
    ""

build_variant meta \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=0" \
    "-lsijson -lsireflect"
assert_no_symbol "$build_dir/libsiecs_meta.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find|ecs_rest_|init_rest"
assert_no_reference "$build_dir/libsiecs_meta.so" "sihttp_"
has_reference "$build_dir/libsiecs_meta.so" "sireflect_"
build_cpp_variant meta \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=0" \
    "-lsijson -lsireflect"

build_variant rest \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META -DSIECS_REST" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=1" \
    "-lsihttp -lsijson -lsireflect"
has_symbol "$build_dir/libsiecs_rest.so" "ecs_rest_"
assert_no_symbol "$build_dir/libsiecs_rest.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find"
has_reference "$build_dir/libsiecs_rest.so" "sihttp_"
build_cpp_variant rest \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META -DSIECS_REST" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=1" \
    "-lsihttp -lsijson -lsireflect"

build_distr_variant minimal \
    "-DSIECS_CUSTOM_BUILD" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=0 -DEXPECT_REST=0"
assert_no_symbol "$build_dir/libsiecs_distr_minimal.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find|ecs_rest_|init_rest|Name__|sihttp_|sijson_|sireflect_"

build_distr_variant names \
    "-DSIECS_CUSTOM_BUILD -DSIECS_NAMES" \
    "-DEXPECT_NAMES=1 -DEXPECT_META=0 -DEXPECT_REST=0"
has_symbol "$build_dir/libsiecs_distr_names.so" "ecs_entity_name"
assert_no_symbol "$build_dir/libsiecs_distr_names.so" \
    "ecs_rest_|init_rest|sihttp_|sijson_|sireflect_"

build_distr_variant meta \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=0"
has_symbol "$build_dir/libsiecs_distr_meta.so" "sireflect_"
assert_no_symbol "$build_dir/libsiecs_distr_meta.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find|ecs_rest_|init_rest|sihttp_"

build_distr_variant rest \
    "-DSIECS_CUSTOM_BUILD -DSIECS_META -DSIECS_REST" \
    "-DEXPECT_NAMES=0 -DEXPECT_META=1 -DEXPECT_REST=1"
has_symbol "$build_dir/libsiecs_distr_rest.so" "ecs_rest_"
has_symbol "$build_dir/libsiecs_distr_rest.so" "sihttp_"
assert_no_symbol "$build_dir/libsiecs_distr_rest.so" \
    "ecs_(component|entity|module|resource|system)_name|ecs_resource_find"

if "$cc" -std=c23 -DSIECS_CUSTOM_BUILD -DSIECS_REST \
    -I"$repo_root/include" -fsyntax-only "$repo_root/test/standalone/config.c" \
    >/dev/null 2>&1; then
    echo "SIECS_REST unexpectedly compiled without SIECS_META" >&2
    exit 1
fi
