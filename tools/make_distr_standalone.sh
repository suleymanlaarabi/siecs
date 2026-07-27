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
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson_internal\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_buffer\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_internal\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp_route\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<][^">]*\/bake_config\.h[">]/d' \
        "$1"
}

strip_internal_includes() {
    sed \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]siecs\.h[>"]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*"[^"]*"/d' \
        "$1"
}

embed_internal_headers() {
    # Keep dependencies before users: local includes are removed below because
    # standalone output contains only siecs.c and siecs.h.
    for header in \
        src/helper.h \
        src/datastructure/idmap.h \
        src/datastructure/vec.h \
        src/datastructure/arena.h \
        src/type.h \
        src/storage/resource_index.h \
        src/storage/module_index.h \
        src/storage/system_index.h \
        src/storage/entity_index.h \
        src/storage/component_index.h \
        src/table.h \
        src/storage/query_index.h \
        src/storage/observer_index.h \
        src/storage/table_index.h \
        src/command_buffer.h \
        src/table_migration.h \
        src/module.h \
        src/world_internal.h \
        src/addons/addons.h \
        src/addons/rest_internal.h \
        src/utils.h; do
        strip_internal_includes "$header"
    done
}

{
    printf '/* Embedded public dependency headers for standalone distribution. */\n'
    printf '#ifndef _POSIX_C_SOURCE\n'
    printf '#define _POSIX_C_SOURCE 200809L\n'
    printf '#endif\n'
    printf '#ifndef SIREFLECT_API\n'
    printf '#define SIREFLECT_API\n'
    printf '#endif\n'
    printf '#ifndef SIJSON_API\n'
    printf '#define SIJSON_API\n'
    printf '#endif\n'
    printf '#ifndef SIHTTP_API\n'
    printf '#define SIHTTP_API\n'
    printf '#endif\n'
    strip_dep_includes "$deps_dir/sireflect.h"
    strip_dep_includes "$deps_dir/sijson.h"
    strip_dep_includes "$deps_dir/sihttp.h"
    strip_dep_includes "$out_dir/siecs.h"
} > "$tmp_h"

{
    printf '#include "siecs.h"\n'
    printf '/* Embedded internal headers for standalone distribution. */\n'
    embed_internal_headers
    printf '/* Embedded dependency implementations for standalone distribution. */\n'
    strip_dep_includes "$deps_dir/sireflect.c"
    strip_dep_includes "$deps_dir/sijson.c"
    strip_dep_includes "$deps_dir/sihttp.c"
    sed \
        -e '1{/^[[:space:]]*#[[:space:]]*include[[:space:]]*"siecs\.h"/d;}' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]siecs\.h[>"]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*"[^"]*"/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sireflect\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sijson\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<]sihttp\.h[">]/d' \
        -e '/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<][^">]*\/bake_config\.h[">]/d' \
        "$out_dir/siecs.c"
} > "$tmp_c"

mv "$tmp_h" "$out_dir/siecs.h"
mv "$tmp_c" "$out_dir/siecs.c"
