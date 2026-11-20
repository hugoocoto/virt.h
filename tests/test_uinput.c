/* test_uinput.c - real devices through /dev/uinput, end to end.

   Creates a keyboard and a mouse, finds their /dev/input/event* nodes by
   name and reads back what the library sends. Needs write access to
   /dev/uinput and read access to /dev/input/event* (usually root).

   Without that access it skips (exit 77), unless VIRT_REQUIRE_UINPUT is
   set in the environment: then it fails, so CI cannot skip it silently.

   Only harmless events are sent: KEY_F24 and a 1 pixel cursor move.
*/

#define _DEFAULT_SOURCE // usleep, with -std=c99
#define INCLUDE_VIRT_IMPLEMENTATION
#include "../virt.h"

#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK(cond)                                                          \
        do {                                                                 \
                if (!(cond)) {                                               \
                        fprintf(stderr, "%s:%d: CHECK(%s) failed\n",         \
                                __FILE__, __LINE__, #cond);                  \
                        failures++;                                          \
                }                                                            \
        } while (0)

// opens the event node of the input device called name, -1 if not found
static int
open_event_node(const char *name)
{
        // udev may take a moment to create the node
        for (int tries = 0; tries < 50; tries++, usleep(100000)) {
                DIR *dir = opendir("/sys/class/input");
                if (!dir) return -1;
                struct dirent *e;
                while ((e = readdir(dir))) {
                        if (strncmp(e->d_name, "event", 5) != 0) continue;
                        char path[512], got[256] = { 0 };
                        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", e->d_name);
                        FILE *f = fopen(path, "r");
                        if (!f) continue;
                        if (!fgets(got, sizeof(got), f)) got[0] = 0;
                        fclose(f);
                        got[strcspn(got, "\n")] = 0;
                        if (strcmp(got, name) != 0) continue;

                        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
                        int fd = open(path, O_RDONLY | O_NONBLOCK);
                        if (fd >= 0) {
                                closedir(dir);
                                return fd;
                        }
                }
                closedir(dir);
        }
        return -1;
}

// reads the next event, waiting up to a second; 0 on success
static int
next_event(int fd, struct input_event *ev)
{
        struct pollfd p = { fd, POLLIN, 0 };
        if (poll(&p, 1, 1000) <= 0) return -1;
        return read(fd, ev, sizeof(*ev)) == (ssize_t) sizeof(*ev) ? 0 : -1;
}

static void
expect(int fd, int type, int code, int value)
{
        struct input_event ev;
        if (next_event(fd, &ev) < 0) {
                fprintf(stderr, "no event, want (%d %d %d)\n", type, code, value);
                failures++;
                return;
        }
        if (ev.type != type || ev.code != code || ev.value != value) {
                fprintf(stderr, "event (%d %d %d), want (%d %d %d)\n",
                        ev.type, ev.code, ev.value, type, code, value);
                failures++;
        }
}

int
main(void)
{
        int required = getenv("VIRT_REQUIRE_UINPUT") != NULL;

        virt_dev kb = virt_kb_init("virt.h test keyboard");
        if (kb < 0) {
                fprintf(stderr, "virt_kb_init: %s\n", strerror(errno));
                if (required) return 1;
                fprintf(stderr, "no access to /dev/uinput, skipping\n");
                return 77;
        }
        virt_dev mouse = virt_mouse_init("virt.h test mouse");
        CHECK(mouse >= 0);

        int kb_in    = open_event_node("virt.h test keyboard");
        int mouse_in = open_event_node("virt.h test mouse");
        CHECK(kb_in >= 0);
        CHECK(mouse_in >= 0);

        if (kb_in >= 0) {
                virt_kb_tap(kb, KEY_F24);
                expect(kb_in, EV_KEY, KEY_F24, 1);
                expect(kb_in, EV_SYN, SYN_REPORT, 0);
                expect(kb_in, EV_KEY, KEY_F24, 0);
                expect(kb_in, EV_SYN, SYN_REPORT, 0);
                close(kb_in);
        }
        if (mouse >= 0 && mouse_in >= 0) {
                virt_mouse_move(mouse, 1, -1);
                expect(mouse_in, EV_REL, REL_X, 1);
                expect(mouse_in, EV_REL, REL_Y, -1);
                expect(mouse_in, EV_SYN, SYN_REPORT, 0);
                close(mouse_in);
        }

        if (mouse >= 0) virt_mouse_fini(mouse);
        virt_kb_fini(kb);

        if (failures) {
                fprintf(stderr, "%d check(s) failed\n", failures);
                return 1;
        }
        printf("all tests passed\n");
        return 0;
}
