#include "../script.h"

bool siecs_string_is_interpolated(const char *value) {
    for (size_t i = 0; value[i]; i++) {
        if (value[i] == '\\' && value[i + 1]) {
            i++;
        } else if (
            value[i] == '{' ||
            (value[i] == '$' && (isalnum((unsigned char)value[i + 1]) || value[i + 1] == '_'))
        ) {
            return true;
        }
    }
    return false;
}

char *siecs_string_escape(char *str) {
    char *write = str;
    for (const char *read = str; *read; read++) {
        char value = *read;
        if (value == '\\') {
            read++;
            switch (*read) {
            case 'n':
                value = '\n';
                break;
            case 'r':
                value = '\r';
                break;
            case 't':
                value = '\t';
                break;
            case '\\':
            case '"':
            case '\'':
            case '$':
            case '{':
            case '}':
                value = *read;
                break;
            default:
                return NULL;
            }
        }
        *write++ = value;
    }
    *write = '\0';
    return write;
}
