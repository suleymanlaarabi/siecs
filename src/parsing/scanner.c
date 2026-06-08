#include "scanner.h"
#include <string.h>

void ecs_scanner_init(ecs_scanner_t *scanner, const char *str) {
    scanner->str = str;
    scanner->pos = 0;
    scanner->len = (uint32_t)strlen(str);
}
