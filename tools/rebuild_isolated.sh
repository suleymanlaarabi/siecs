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
bake_bin=${BAKE:-bake}
host_bake_home=$("$bake_bin" env | sed -n 's/^BAKE_HOME=//p')
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
        test/cpp)
            mkdir -p "$work_dir/test"
            cp -R "$repo_root/test/cpp" "$work_dir/test/"
            ;;
        *)
            cp -R "$repo_root/$package" "$work_dir/"
            ;;
    esac
}

copy_common
copy_package

cd "$work_dir"

mkdir -p "$BAKE_HOME/include" "$BAKE_HOME/lib" "$BAKE_HOME/meta"
cp -R "$host_bake_home/include/." "$BAKE_HOME/include/"
cp "$host_bake_home"/lib/libbake_*.so "$BAKE_HOME/lib/"
for pkg in bake.amalgamate bake.lang.c bake.lang.cpp bake.test bake.util; do
    cp -R "$host_bake_home/meta/$pkg" "$BAKE_HOME/meta/$pkg"
done

"$bake_bin" rebuild . -r "$@" >/dev/null

if [ "$run" -eq 1 ]; then
    "$bake_bin" run "$package" "$@"
fi
