#ifndef SIGPU_INPUT_H
#define SIGPU_INPUT_H

#include <SDL3/SDL.h>
#include <sigpu.h>

enum { SiInputEdgeDown, SiInputEdgeUp, SiInputEdgeWheel, SiInputEdgeCancel };

#define SIINPUT_POINTER_QUEUE_CAPACITY 64

typedef struct {
    uint64_t timestamp_ns;
    uint32_t pointer_id;
    uint32_t buttons;
    float x;
    float y;
    float wheel_x;
    float wheel_y;
    uint8_t kind;
    uint8_t button;
    uint8_t clicks;
    uint8_t pointer_type;
} siinput_pointer_edge_t;

void sigpu_input_init(void);
ecs_system_id_t sigpu_input_system(void);
uint16_t sigpu_input_resource_id(const char *name);
sireflect_handle_t sigpu_input_resource_type(const char *name);
bool sigpu_input_had_motion(void);
uint16_t sigpu_input_modifiers(void);
const siinput_pointer_edge_t *sigpu_input_pointer_edges(uint32_t *count);

#endif
