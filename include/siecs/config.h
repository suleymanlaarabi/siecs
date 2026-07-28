#ifndef SIECS_CONFIG_H
#define SIECS_CONFIG_H

/*
 * SIECS builds with all optional features enabled by default.
 *
 * Define SIECS_CUSTOM_BUILD to start with every optional feature disabled,
 * then enable only the features that are needed:
 *
 *   SIECS_NAMES  - runtime name storage and name lookup APIs
 *   SIECS_META   - component reflection metadata
 *   SIECS_REST   - REST explorer (requires SIECS_META)
 *
 * In the default build, individual features can be removed with:
 *
 *   SIECS_NO_NAMES
 *   SIECS_NO_META
 *   SIECS_NO_REST
 */

#if defined(SIECS_NAMES) && defined(SIECS_NO_NAMES)
#error "SIECS_NAMES and SIECS_NO_NAMES cannot be defined together"
#endif

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

#ifdef SIECS_NAMES
#define SIECS_HAS_NAMES 1
#else
#define SIECS_HAS_NAMES 0
#endif

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

#ifdef SIECS_NO_NAMES
#define SIECS_HAS_NAMES 0
#else
#define SIECS_HAS_NAMES 1
#endif

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

#if SIECS_HAS_NAMES
#define SIECS_NAME_INIT(value) .name = (value),
#else
/* Public descriptors still accept .name, but generated descriptors omit it. */
#define SIECS_NAME_INIT(value)
#endif

#endif
