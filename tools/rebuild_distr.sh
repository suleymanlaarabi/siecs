#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out_root=${1:-$repo_root}
work_dir=$(mktemp -d /tmp/siecs-distr-build.XXXXXX)
bake_bin=${BAKE:-bake}
host_bake_home=$("$bake_bin" env | sed -n 's/^BAKE_HOME=//p')
export BAKE_HOME="$work_dir/.bake"

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

mkdir -p "$work_dir/distr"
cp -R "$repo_root/project.json" "$repo_root/include" "$repo_root/src" "$work_dir/"

cd "$work_dir"

mkdir -p "$BAKE_HOME/include" "$BAKE_HOME/lib" "$BAKE_HOME/meta" "$BAKE_HOME/src"
cp -R "$host_bake_home/include/." "$BAKE_HOME/include/"
cp "$host_bake_home"/lib/libbake_*.so "$BAKE_HOME/lib/"
for pkg in \
    bake.amalgamate bake.lang.c bake.lang.cpp bake.test bake.util \
    sireflect sijson sihttp
do
    cp -R "$host_bake_home/meta/$pkg" "$BAKE_HOME/meta/$pkg"
done

for pkg in sireflect sijson sihttp; do
    dependency_source=$(sed -n '1p' "$host_bake_home/meta/$pkg/source.txt")
    mkdir -p "$BAKE_HOME/src/$pkg"
    cp -R \
        "$dependency_source/project.json" \
        "$dependency_source/include" \
        "$dependency_source/src" \
        "$BAKE_HOME/src/$pkg/"
    if [ -f "$dependency_source/dependee.json" ]; then
        cp "$dependency_source/dependee.json" "$BAKE_HOME/src/$pkg/"
    fi
    printf '%s\n' "$BAKE_HOME/src/$pkg" \
        > "$BAKE_HOME/meta/$pkg/source.txt"
done

"$bake_bin" . -r >/dev/null

mkdir -p "$out_root/distr" "$out_root/include/siecs"
cp distr/siecs.c "$out_root/distr/siecs.c"
cp distr/siecs.h "$out_root/distr/siecs.h"
cp distr/siecs_no_addons.c "$out_root/distr/siecs_no_addons.c"
cp distr/siecs_no_addons.h "$out_root/distr/siecs_no_addons.h"
cp include/siecs/bake_config.h "$out_root/include/siecs/bake_config.h"
