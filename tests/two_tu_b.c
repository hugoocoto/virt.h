#include "../virt.h"

void two_tu_b(void);

void
two_tu_b(void)
{
        // -1 is not a device: the write fails and is ignored
        virt_kb_tap(-1, KEY_A);
}
