# virt.h

Single-header C library to create a virtual keyboard and mouse on Linux and
send key presses, combos, clicks and cursor movement. It only uses the kernel
uinput interface: nothing to link, works under X11, Wayland and the console.

## Usage

Copy `virt.h` into your project. In **one** C file:

```c
#define INCLUDE_VIRT_IMPLEMENTATION
#include "virt.h"
```

Everywhere else just `#include "virt.h"`.

```c
virt_dev kb    = virt_kb_init("my keyboard");
virt_dev mouse = virt_mouse_init("my mouse");
if (kb < 0 || mouse < 0) return 1;
virt_kb_ensure_connection();            // let userspace see the new devices

virt_kb_tap(kb, KEY_A);

const int keys[] = { KEY_LEFTALT, KEY_TAB };
virt_kb_combo(kb, keys, 2);             // Alt+Tab

virt_mouse_move(mouse, 10, -5);
virt_mouse_button(mouse, BTN_LEFT, 1);
virt_mouse_button(mouse, BTN_LEFT, 0);

virt_mouse_fini(mouse);
virt_kb_fini(kb);
```

Key and button codes are the `KEY_*` / `BTN_*` constants from
`<linux/input-event-codes.h>`, which the header includes.

A complete program is in [`examples/demo.c`](examples/demo.c).
The full API reference is the comment at the top of [`virt.h`](virt.h).

## Requirements

- Linux 4.5 or newer (`UI_DEV_SETUP`) with the `uinput` module
  (`sudo modprobe uinput` if `/dev/uinput` is missing).
- Write access to `/dev/uinput`: root, or a udev rule / group that grants it.

## Limitations

- The mouse has relative X/Y movement and left/right buttons only: no wheel,
  middle button or absolute positioning.
- Write errors on events are ignored.
- A device must not be written from several threads at once.

## Tests

```sh
make check            # no root needed
sudo make test-uinput # real devices through /dev/uinput
```

`make check` builds the example with strict warnings, checks the header from
C++ and from two translation units, and runs `tests/test_events.c` under
ASan/UBSan: the device is replaced by a pipe and the exact events written are
checked. `make test-uinput` creates real devices and reads their events back
from `/dev/input/event*` (it only sends `KEY_F24` and a 1 pixel move). CI runs
both.

## License

Public domain or MIT, your choice. See [LICENSE](LICENSE).
