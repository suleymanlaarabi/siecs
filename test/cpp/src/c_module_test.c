#include "c_module_test.h"

int cpp_c_module_import_calls;
int cpp_c_module_last_gravity;

ECS_MODULE_DEFINE(cpp_c_module);

void cpp_c_module_import(const cpp_c_module_props_t *props) {
    cpp_c_module_import_calls++;
    cpp_c_module_last_gravity = props->gravity;
}
