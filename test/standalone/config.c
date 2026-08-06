#include "siecs/config.h"

#ifdef EXPECT_META
_Static_assert(SIECS_HAS_META == EXPECT_META);
#endif
int main(void) { return 0; }
