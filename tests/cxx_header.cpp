// The declarations must be usable from C++ (extern "C").
#include "../virt.h"

int
main()
{
        virt_kb_tap(-1, KEY_A); // never linked: only compiled
        return 0;
}
