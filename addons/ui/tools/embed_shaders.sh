#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out="$root/src/render/shaders.generated.h"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
{
    printf '#ifndef SIUI_SHADERS_GENERATED_H\n#define SIUI_SHADERS_GENERATED_H\n'
    for shader in rect.vert rect.frag text.vert text.frag solid.frag; do
        glslc "$root/src/render/shaders/$shader" -o "$tmp/$shader.spv"
        name=$(printf '%s' "$shader" | tr '.' '_')
        xxd -i -n "siui_${name}_spv" "$tmp/$shader.spv"
    done
    printf '#endif\n'
} > "$out"
