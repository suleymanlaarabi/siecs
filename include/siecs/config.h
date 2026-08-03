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
 * Define SIECS_CUSTOM_BUILD to start with every optional feature disabled,
 * then enable only the features that are needed:
 *
 *   SIECS_META   - component reflection metadata
 *   SIECS_REST   - REST explorer (requires SIECS_META)
 *
 * In the default build, individual features can be removed with:
 *
 *   SIECS_NO_META
 *   SIECS_NO_REST
 */

#if defined(SIECS_META) && defined(SIECS_NO_META)
#error "SIECS_META and SIECS_NO_META cannot be defined together"
#endif

#if defined(SIECS_REST) && defined(SIECS_NO_META)
#error "SIECS_REST cannot be enabled when SIECS_META is disabled"
#endif

#if defined(SIECS_REST) && defined(SIECS_NO_REST)
#error "SIECS_REST and SIECS_NO_REST cannot be defined together"
#endif

#ifdef SIECS_CUSTOM_BUILD

#ifdef SIECS_META
#define SIECS_HAS_META 1
#else
#define SIECS_HAS_META 0
#endif

#ifdef SIECS_REST
#define SIECS_HAS_REST 1
#else
#define SIECS_HAS_REST 0
#endif

#else

#ifdef SIECS_NO_META
#define SIECS_HAS_META 0
#else
#define SIECS_HAS_META 1
#endif

#if defined(SIECS_NO_REST) || !SIECS_HAS_META
#define SIECS_HAS_REST 0
#else
#define SIECS_HAS_REST 1
#endif

#endif

#if SIECS_HAS_REST && !SIECS_HAS_META
#error "SIECS_REST requires SIECS_META"
#endif

#endif
