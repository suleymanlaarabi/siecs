#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out_root=${1:-$repo_root}
work_dir=$(mktemp -d /tmp/siecs-distr-build.XXXXXX)
host_bake_home=$(bake env | sed -n 's/^BAKE_HOME=//p')
export BAKE_HOME="$work_dir/.bake"

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

mkdir -p "$work_dir/distr"
cp -R "$repo_root/project.json" "$repo_root/include" "$repo_root/src" "$repo_root/tools" "$work_dir/"

cd "$work_dir"

mkdir -p "$BAKE_HOME/include" "$BAKE_HOME/lib" "$BAKE_HOME/meta"
cp -R "$host_bake_home/include/." "$BAKE_HOME/include/"
cp "$host_bake_home"/lib/libbake_*.so "$BAKE_HOME/lib/"
for pkg in bake.amalgamate bake.lang.c bake.lang.cpp bake.test bake.util; do
    cp -R "$host_bake_home/meta/$pkg" "$BAKE_HOME/meta/$pkg"
done
mkdir -p "$BAKE_HOME/src"
for pkg in sireflect sijson sihttp; do
    if [ -d "$host_bake_home/src/$pkg" ]; then
        cp -R "$host_bake_home/src/$pkg" "$BAKE_HOME/src/$pkg"
    elif [ -d "$host_bake_home/src/$pkg.git" ]; then
        cp -R "$host_bake_home/src/$pkg.git" "$BAKE_HOME/src/$pkg.git"
    fi
done

sh tools/install_bake_deps.sh >/dev/null
bake rebuild . -r >/dev/null

strip_embedded_include() {
    sed \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sireflect\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson_internal\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_buffer\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_internal\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_route\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<][^">]*\/bake_config\.h[">]/d' \
        "$1"
}

dep_dir() {
    id=$1

    if [ -d "$BAKE_HOME/src/$id" ]; then
        printf '%s\n' "$BAKE_HOME/src/$id"
        return
    fi

    if [ -d "$BAKE_HOME/src/$id.git" ]; then
        printf '%s\n' "$BAKE_HOME/src/$id.git"
        return
    fi

    printf 'missing dependency source: %s\n' "$id" >&2
    exit 1
}

build_standalone_deps() {
    mkdir -p deps
    sireflect_dir=$(dep_dir sireflect)
    sijson_dir=$(dep_dir sijson)
    sihttp_dir=$(dep_dir sihttp)

    cp "$sireflect_dir/distr/sireflect.h" deps/sireflect.h
    cp "$sireflect_dir/distr/sireflect.c" deps/sireflect.c

    strip_embedded_include "$sijson_dir/include/sijson.h" > deps/sijson.h
    {
        strip_embedded_include "$sijson_dir/src/sijson_internal.h"
        strip_embedded_include "$sijson_dir/src/sijson_error.c"
        strip_embedded_include "$sijson_dir/src/sijson_parser.c"
        strip_embedded_include "$sijson_dir/src/sijson_reflect.c"
        strip_embedded_include "$sijson_dir/src/sijson_value.c"
        strip_embedded_include "$sijson_dir/src/sijson_writer.c"
    } > deps/sijson.c

    strip_embedded_include "$sihttp_dir/include/sihttp.h" > deps/sihttp.h
    {
        strip_embedded_include "$sihttp_dir/src/sihttp_buffer.h"
        strip_embedded_include "$sihttp_dir/src/sihttp_internal.h"
        strip_embedded_include "$sihttp_dir/src/sihttp_route.h"
        strip_embedded_include "$sihttp_dir/src/sihttp_buffer.c"
        strip_embedded_include "$sihttp_dir/src/sihttp_request.c"
        strip_embedded_include "$sihttp_dir/src/sihttp_response.c"
        strip_embedded_include "$sihttp_dir/src/sihttp_route.c"
        strip_embedded_include "$sihttp_dir/src/siformat.c"
        strip_embedded_include "$sihttp_dir/src/sihttp.c"
    } > deps/sihttp.c
}

build_standalone_deps
sh tools/make_distr_standalone.sh

mkdir -p "$out_root/distr" "$out_root/include/siecs"
cp distr/siecs.c "$out_root/distr/siecs.c"
cp distr/siecs.h "$out_root/distr/siecs.h"
cp include/siecs/bake_config.h "$out_root/include/siecs/bake_config.h"
