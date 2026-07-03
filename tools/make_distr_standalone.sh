#!/usr/bin/env sh
set -eu

out_dir=${1:-distr}
deps_dir=${2:-deps}

tmp_h=$(mktemp /tmp/siecs-h.XXXXXX)
tmp_c=$(mktemp /tmp/siecs-c.XXXXXX)

cleanup() {
    rm -f "$tmp_h" "$tmp_c"
}
trap cleanup EXIT

strip_dep_includes() {
    sed \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sireflect\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp\.h[">]/d' \
        "$1"
}

{
    printf '/* Embedded public dependency headers for standalone distribution. */\n'
    printf '#ifndef _POSIX_C_SOURCE\n'
    printf '#define _POSIX_C_SOURCE 200809L\n'
    printf '#endif\n'
    strip_dep_includes "$deps_dir/sireflect.h"
    strip_dep_includes "$deps_dir/sijson.h"
    strip_dep_includes "$deps_dir/sihttp.h"
    strip_dep_includes "$out_dir/siecs.h"
} > "$tmp_h"

{
    printf '#include "siecs.h"\n'
    printf '/* Embedded dependency implementations for standalone distribution. */\n'
    strip_dep_includes "$deps_dir/sireflect.c"
    strip_dep_includes "$deps_dir/sijson.c"
    strip_dep_includes "$deps_dir/sihttp.c"
    sed \
        -e '1{/^[[:space:]]*#[[:space:]]*include[[:space:]]*"siecs\.h"/d;}' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sireflect\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp\.h[">]/d' \
        "$out_dir/siecs.c"
} > "$tmp_c"

mv "$tmp_h" "$out_dir/siecs.h"
mv "$tmp_c" "$out_dir/siecs.c"
