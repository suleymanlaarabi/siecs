#ifndef SIECS_CONFIG_H
#define SIECS_CONFIG_H

#ifdef SICORE_NO_VEC
#error "SIECS requires SICORE_VEC"
#endif

#ifndef SICORE_VEC
#define SICORE_VEC 1
#endif

/*
 * SIECS builds with all optional features enabled by default.
 *
 * Define SIECS_CUSTOM_BUILD to start with optional metadata disabled, then
 * enable it only when it is needed:
 *
 *   SIECS_META   - component reflection metadata
 *
 * In the default build, individual features can be removed with:
 *
 *   SIECS_NO_META
 */

#if defined(SIECS_META) && defined(SIECS_NO_META)
#error "SIECS_META and SIECS_NO_META cannot be defined together"
#endif

#ifdef SIECS_CUSTOM_BUILD

#ifdef SIECS_META
#define SIECS_HAS_META 1
#else
#define SIECS_HAS_META 0
#endif

#else

#ifdef SIECS_NO_META
#define SIECS_HAS_META 0
#else
#define SIECS_HAS_META 1
#endif

#endif

#endif
