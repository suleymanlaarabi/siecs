#ifndef SIECS_CPP_C_MODULE_TEST_H
#define SIECS_CPP_C_MODULE_TEST_H

#include <siecs.h>

ECS_MODULE_DECLARE(cpp_c_module, {
    int gravity;
});

#ifdef __cplusplus
extern "C" {
#endif

extern int cpp_c_module_import_calls;
extern int cpp_c_module_last_gravity;

#ifdef __cplusplus
}
#endif

#endif
