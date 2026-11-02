/* Derived from Flecs. Copyright (c) 2025 Sander Mertens.
 * Portions Copyright (c) Meta Platforms, Inc. and affiliates.
 * Distributed under the MIT license in addons/script/LICENSE.flecs. */
/* Ported from Flecs expr/util.c; see LICENSE.flecs. */
#include "../script.h"

bool siecs_string_is_interpolated(
    const char *value)
{
    const char *ptr = value;

    for (ptr = strchr(ptr, '$'); ptr; ptr = strchr(ptr + 1, '$')) {
        if (ptr != value) {
            if (ptr[-1] == '\\') {
                continue; /* Escaped */
            }
        }

        if (isspace((unsigned char)ptr[1]) || !ptr[1]) {
            continue; /* $ by itself */
        }

        return true;
    }

    ptr = value;

    for (ptr = strchr(ptr, '{'); ptr; ptr = strchr(ptr + 1, '{')) {
        if (ptr != value) {
            if (ptr[-1] == '\\') {
                continue; /* Escaped */
            }
        }

        return true;
    }

    return false;
}

char* siecs_string_escape(
    char *str)
{
    const char *ptr;
    char *out = str, ch;

    for (ptr = str; ptr[0]; ) {
        if (ptr[0] == '\\') {
            if (ptr[1] == '{') { /* Escape string interpolation delimiter */
                ch = '{';
                ptr += 2;
            } else if (ptr[1] == '$') { /* Escape string interpolation var */
                ch = '$';
                ptr += 2;
            } else {
                ptr = siecs_chrparse(ptr, &ch);
                if (!ptr) {
                    ecs_err("invalid escape sequence in string '%s'", str);
                    return NULL;
                }
            }
        } else {
            ch = ptr[0];
            ptr ++;
        }

        out[0] = ch;
        out ++;
    }

    out[0] = '\0';

    return out + 1;
}
