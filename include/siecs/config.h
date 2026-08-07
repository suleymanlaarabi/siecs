#ifndef SIECS_CONFIG_H
#define SIECS_CONFIG_H

#ifdef SICORE_NO_VEC
#error "SIECS requires SICORE_VEC"
#endif

#ifndef SICORE_VEC
#define SICORE_VEC 1
#endif

/* Reflection metadata is part of every SIECS build. */
#define SIECS_HAS_META 1

#endif
