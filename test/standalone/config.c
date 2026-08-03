#include "siecs/config.h"

#ifdef EXPECT_META
_Static_assert(SIECS_HAS_META == EXPECT_META);
#endif
#ifdef EXPECT_REST
_Static_assert(SIECS_HAS_REST == EXPECT_REST);
#endif

int main(void) { return 0; }
