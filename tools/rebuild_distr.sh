#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d /tmp/siecs-distr-build.XXXXXX)

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

mkdir -p "$work_dir/distr"
cp -R "$repo_root/project.json" "$repo_root/include" "$repo_root/src" "$repo_root/tools" "$work_dir/"

cd "$work_dir"
sh tools/install_bake_deps.sh
bake rebuild . -r >/dev/null
sh tools/make_distr_standalone.sh

cp distr/siecs.c "$repo_root/distr/siecs.c"
cp distr/siecs.h "$repo_root/distr/siecs.h"
