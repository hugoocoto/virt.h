/* demo.c - create a virtual keyboard and mouse, type a word and move the
   cursor in a square.

   Build: cc demo.c -o demo
   Run with write access to /dev/uinput (e.g. sudo ./demo), with the focus
   on a text field: it types "hello!" there (US layout).
*/

#define _DEFAULT_SOURCE // usleep, with -std=c99
#define INCLUDE_VIRT_IMPLEMENTATION
#include "../virt.h"

#include <stdio.h>

int
main(void)
{
        virt_dev kb = virt_kb_init("virt.h demo keyboard");
        if (kb < 0) {
                perror("virt_kb_init");
                return 1;
        }
        virt_dev mouse = virt_mouse_init("virt.h demo mouse");
        if (mouse < 0) {
                perror("virt_mouse_init");
                virt_kb_fini(kb);
                return 1;
        }
        virt_kb_ensure_connection();

        const int word[] = { KEY_H, KEY_E, KEY_L, KEY_L, KEY_O };
        for (size_t i = 0; i < sizeof(word) / sizeof(word[0]); i++)
                virt_kb_tap(kb, word[i]);

        // shift + 1: "!" on a US layout
        const int bang[] = { KEY_LEFTSHIFT, KEY_1 };
        virt_kb_combo(kb, bang, 2);

        const int dx[] = { 5, 0, -5, 0 };
        const int dy[] = { 0, 5, 0, -5 };
        for (int side = 0; side < 4; side++) {
                for (int step = 0; step < 40; step++) {
                        virt_mouse_move(mouse, dx[side], dy[side]);
                        usleep(5000);
                }
        }

        virt_mouse_fini(mouse);
        virt_kb_fini(kb);
        return 0;
}
