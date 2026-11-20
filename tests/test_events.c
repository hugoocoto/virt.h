/* test_events.c - the events virt.h writes, without /dev/uinput.

   The event functions only write struct input_event to the device fd, so a
   pipe stands in for the device: the test reads back what was written and
   compares it with the expected sequence.
*/

#define INCLUDE_VIRT_IMPLEMENTATION
#include "../virt.h"

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                          \
        do {                                                                 \
                if (!(cond)) {                                               \
                        fprintf(stderr, "%s:%d: CHECK(%s) failed\n",         \
                                __FILE__, __LINE__, #cond);                  \
                        failures++;                                          \
                }                                                            \
        } while (0)

struct ev {
        int type, code, value;
};

#define SYN { EV_SYN, SYN_REPORT, 0 }

static int pipefd[2]; // [1] is the device, [0] what the kernel would read

static void
setup(void)
{
        if (pipe(pipefd) < 0) {
                perror("pipe");
                _exit(2);
        }
}

/* Reads everything written since setup() and compares it with want[n].
   Closes the pipe. */
static void
expect(const char *what, const struct ev *want, size_t n)
{
        struct input_event got[64];
        close(pipefd[1]);
        ssize_t bytes = read(pipefd[0], got, sizeof(got));
        close(pipefd[0]);

        if (bytes < 0 || bytes % sizeof(got[0]) != 0) {
                fprintf(stderr, "%s: read %zd bytes, not whole events\n", what, bytes);
                failures++;
                return;
        }
        size_t count = bytes / sizeof(got[0]);
        if (count != n) {
                fprintf(stderr, "%s: %zu events, want %zu\n", what, count, n);
                failures++;
                return;
        }
        for (size_t i = 0; i < n; i++) {
                if (got[i].type != want[i].type || got[i].code != want[i].code ||
                    got[i].value != want[i].value) {
                        fprintf(stderr, "%s: event %zu is (%d %d %d), want (%d %d %d)\n",
                                what, i, got[i].type, got[i].code, got[i].value,
                                want[i].type, want[i].code, want[i].value);
                        failures++;
                }
                // the kernel fills in the time
                CHECK(got[i].input_event_sec == 0 && got[i].input_event_usec == 0);
        }
}

#define EXPECT(what, ...)                                                    \
        do {                                                                 \
                const struct ev want_[] = { __VA_ARGS__ };                   \
                expect(what, want_, sizeof(want_) / sizeof(want_[0]));       \
        } while (0)

static void
test_keyboard(void)
{
        setup();
        virt_kb_press(pipefd[1], KEY_A);
        EXPECT("press", { EV_KEY, KEY_A, 1 }, SYN);

        setup();
        virt_kb_release(pipefd[1], KEY_A);
        EXPECT("release", { EV_KEY, KEY_A, 0 }, SYN);

        setup();
        virt_kb_tap(pipefd[1], KEY_ENTER);
        EXPECT("tap", { EV_KEY, KEY_ENTER, 1 }, SYN, { EV_KEY, KEY_ENTER, 0 }, SYN);

        // modifiers down in order, then up in reverse, each its own report
        const int keys[] = { KEY_LEFTCTRL, KEY_LEFTALT, KEY_T };
        setup();
        virt_kb_combo(pipefd[1], keys, 3);
        EXPECT("combo",
               { EV_KEY, KEY_LEFTCTRL, 1 }, SYN,
               { EV_KEY, KEY_LEFTALT, 1 }, SYN,
               { EV_KEY, KEY_T, 1 }, SYN,
               { EV_KEY, KEY_T, 0 }, SYN,
               { EV_KEY, KEY_LEFTALT, 0 }, SYN,
               { EV_KEY, KEY_LEFTCTRL, 0 }, SYN);

        // a single key is a plain tap
        setup();
        virt_kb_combo(pipefd[1], keys + 2, 1);
        EXPECT("combo of one", { EV_KEY, KEY_T, 1 }, SYN, { EV_KEY, KEY_T, 0 }, SYN);

        // no keys, no events
        setup();
        virt_kb_combo(pipefd[1], keys, 0);
        expect("combo of none", NULL, 0);
}

static void
test_mouse(void)
{
        setup();
        virt_mouse_move(pipefd[1], 3, -4);
        EXPECT("move", { EV_REL, REL_X, 3 }, { EV_REL, REL_Y, -4 }, SYN);

        // zero axes are not sent
        setup();
        virt_mouse_move(pipefd[1], 0, 7);
        EXPECT("move y", { EV_REL, REL_Y, 7 }, SYN);

        setup();
        virt_mouse_move(pipefd[1], -2, 0);
        EXPECT("move x", { EV_REL, REL_X, -2 }, SYN);

        setup();
        virt_mouse_move(pipefd[1], 0, 0);
        EXPECT("move none", SYN);

        setup();
        virt_mouse_button(pipefd[1], BTN_LEFT, 1);
        virt_mouse_button(pipefd[1], BTN_LEFT, 0);
        EXPECT("button", { EV_KEY, BTN_LEFT, 1 }, SYN, { EV_KEY, BTN_LEFT, 0 }, SYN);
}

static void
test_write_errors_ignored(void)
{
        // a closed device must not crash anything (errors are ignored)
        setup();
        close(pipefd[0]);
        close(pipefd[1]);
        virt_kb_tap(pipefd[1], KEY_A);
        virt_mouse_move(pipefd[1], 1, 1);
}

int
main(void)
{
        test_keyboard();
        test_mouse();
        test_write_errors_ignored();

        if (failures) {
                fprintf(stderr, "%d check(s) failed\n", failures);
                return 1;
        }
        printf("all tests passed\n");
        return 0;
}
