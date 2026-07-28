#include "siecs/config.h"

#ifdef EXPECT_NAMES
_Static_assert(SIECS_HAS_NAMES == EXPECT_NAMES);
#endif
#ifdef EXPECT_META
_Static_assert(SIECS_HAS_META == EXPECT_META);
#endif
#ifdef EXPECT_REST
_Static_assert(SIECS_HAS_REST == EXPECT_REST);
#endif

int main(void) { return 0; }
