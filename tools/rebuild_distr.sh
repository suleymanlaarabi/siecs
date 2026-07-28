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
cp -R "$repo_root/tools/distr_deps" "$work_dir/"
cp "$repo_root/tools/assemble_distr.c" "$work_dir/"

cd "$work_dir"

mkdir -p "$BAKE_HOME/include" "$BAKE_HOME/lib" "$BAKE_HOME/meta"
cp -R "$host_bake_home/include/." "$BAKE_HOME/include/"
cp "$host_bake_home"/lib/libbake_*.so "$BAKE_HOME/lib/"
for pkg in bake.amalgamate bake.lang.c bake.lang.cpp bake.test bake.util; do
    cp -R "$host_bake_home/meta/$pkg" "$BAKE_HOME/meta/$pkg"
done
"$bake_bin" rebuild . -r >/dev/null
"$bake_bin" rebuild distr_deps/meta -r >/dev/null
"$bake_bin" rebuild distr_deps/rest -r >/dev/null

${CC:-cc} -std=c23 "$work_dir/assemble_distr.c" \
    -o "$work_dir/assemble_distr"
"$work_dir/assemble_distr" \
    "$work_dir/include/siecs/config.h" \
    "$work_dir/distr/siecs.h" \
    "$work_dir/distr_deps/meta/distr/siecs.h" \
    "$work_dir/distr_deps/rest/distr/siecs.h" \
    "$work_dir/distr/siecs.c" \
    "$work_dir/distr_deps/meta/distr/siecs.c" \
    "$work_dir/distr_deps/rest/distr/siecs.c" \
    "$work_dir/distr/siecs.final.h" \
    "$work_dir/distr/siecs.final.c"

mkdir -p "$out_root/distr" "$out_root/include/siecs"
cp distr/siecs.final.c "$out_root/distr/siecs.c"
cp distr/siecs.final.h "$out_root/distr/siecs.h"
cp include/siecs/bake_config.h "$out_root/include/siecs/bake_config.h"
