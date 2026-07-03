#!/usr/bin/env sh
set -eu

run=0
if [ "${1:-}" = "--run" ]; then
    run=1
    shift
fi

if [ "$#" -lt 1 ]; then
    echo "usage: $0 [--run] <package> [bake args...]" >&2
    exit 1
fi

package=$1
shift

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work_dir=$(mktemp -d /tmp/siecs-bake-build.XXXXXX)
export BAKE_HOME="$work_dir/.bake"

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT

copy_common() {
    cp -R "$repo_root/project.json" "$repo_root/include" "$repo_root/src" "$repo_root/tools" "$work_dir/"
}

copy_package() {
    case "$package" in
        .)
            ;;
        test)
            cp -R "$repo_root/test" "$work_dir/"
            ;;
        example/c)
            mkdir -p "$work_dir/example"
            cp -R "$repo_root/example/c" "$work_dir/example/"
            ;;
        addons/siecs_cpp/test)
            mkdir -p "$work_dir/addons"
            cp -R "$repo_root/addons/siecs_cpp" "$work_dir/addons/"
            ;;
        *)
            cp -R "$repo_root/$package" "$work_dir/"
            ;;
    esac
}

copy_common
copy_package

cd "$work_dir"
sh tools/install_bake_deps.sh
bake rebuild . -r "$@" >/dev/null

if [ "$run" -eq 1 ]; then
    bake run "$package" "$@"
fi
