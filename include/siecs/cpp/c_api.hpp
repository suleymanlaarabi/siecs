#pragma once

#ifndef SIECS_NO_CPP
#define SIECS_NO_CPP
#define SIECS_CPP_RESTORE_CPP_INCLUDE
#endif

#include "siecs.h"

#ifdef SIECS_CPP_RESTORE_CPP_INCLUDE
#undef SIECS_CPP_RESTORE_CPP_INCLUDE
#undef SIECS_NO_CPP
#endif
