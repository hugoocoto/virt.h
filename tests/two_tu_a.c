/* two_tu_a.c, two_tu_b.c - the header included from two translation units,
   the implementation in only one. Linking fails on duplicate symbols. */

#define INCLUDE_VIRT_IMPLEMENTATION
#include "../virt.h"

void two_tu_b(void);

int
main(void)
{
        two_tu_b();
        return 0;
}
