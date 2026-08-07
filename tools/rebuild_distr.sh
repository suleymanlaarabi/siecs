#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out_root=${1:-$repo_root}
work_dir=$(mktemp -d /tmp/siecs-distr-build.XXXXXX)
bake_bin=${BAKE:-bake}

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

mkdir -p "$work_dir/distr"
cp -R "$repo_root/project.json" "$repo_root/include" "$repo_root/src" "$work_dir/"

cd "$work_dir"
"$bake_bin" . -r >/dev/null

mkdir -p "$out_root/distr" "$out_root/include/siecs"
cp distr/siecs.c "$out_root/distr/siecs.c"
cp distr/siecs.h "$out_root/distr/siecs.h"
cp include/siecs/bake_config.h "$out_root/include/siecs/bake_config.h"
