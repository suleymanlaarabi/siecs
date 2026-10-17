#include "physics_events.h"

void physics_api_emit_from_other_file(ecs::entity entity) { ecs::trigger<PhysicsApiEvent>(entity); }
