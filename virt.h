/* virt.h - v1.0 - public domain virtual keyboard and mouse - Hugo Coto Florez 2026

   To use this library, do this in *one* C file:
      #define INCLUDE_VIRT_IMPLEMENTATION
      #include "virt.h"

   In other source files, just include the header normally:
      #include "virt.h"

   No library to link: it only uses the kernel uinput interface (Linux 4.5
   or newer, for UI_DEV_SETUP).

TABLE OF CONTENTS

  Table of Contents
  License
  Documentation
  Notes
  Credits

LICENSE

  Placed in the public domain and also MIT licensed.
  See end of file for detailed license information.

DOCUMENTATION

  The library creates virtual input devices through /dev/uinput and
  writes key, button and movement events to them. Every device is the file
  descriptor of its /dev/uinput handle, so it can also be used with the
  uinput ioctls directly.

  Create a device with virt_kb_init() or virt_mouse_init(), and always
  release it with virt_kb_fini() / virt_mouse_fini() when done.

  Functions:

    virt_kb_init
      Creates a virtual keyboard named name (as shown by evtest, libinput,
      ...) that can send every keyboard key (codes from KEY_ESC up to, not
      including, BTN_MISC).
      Returns the device, or -1 on failure.

    virt_kb_ensure_connection
      Sleeps one second so userspace has time to notice the new devices
      before the first event is sent. Call it once after creating them.

    virt_kb_press / virt_kb_release
      Sends a key down / key up event, each in its own report.

    virt_kb_tap
      Sends a key down followed by a key up.

    virt_kb_combo
      Taps the last key of keys[] while holding the ones before it, e.g.
      { KEY_LEFTALT, KEY_SPACE }. Each step is its own report, so the
      modifiers are already down when the key arrives.

    virt_kb_fini
      Destroys a virtual keyboard.

    virt_mouse_init
      Creates a virtual mouse named name with relative X/Y movement and
      left and right buttons. Returns the device, or -1 on failure.

    virt_mouse_move
      Moves the cursor by dx, dy (relative). Zero axes are not sent.

    virt_mouse_button
      Presses (val 1) or releases (val 0) a button, e.g. BTN_LEFT.

    virt_mouse_fini
      Destroys a virtual mouse.

  Key and button codes are the ones from <linux/input-event-codes.h>
  (KEY_*, BTN_*), which this header includes.

NOTES

  * Creating uinput devices needs write access to /dev/uinput (usually
    root, or a udev rule / the 'input' group).

  * Write errors on events are ignored.

  * Devices are not thread-safe: do not write to the same device from
    several threads at once.

CREDITS

  Hugo Coto Florez -- design and implementation

*/

#ifndef VIRT_H_
#define VIRT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/input-event-codes.h>
#include <linux/uinput.h>

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef int virt_dev; // a device: the fd of its /dev/uinput handle

/* API */

virt_dev virt_kb_init(const char *name);                     // Create a virtual keyboard, -1 on failure
void virt_kb_ensure_connection(void);                        // Give userspace time to see the new devices
void virt_kb_press(virt_dev dev, int kp);                    // Key down
void virt_kb_release(virt_dev dev, int kp);                  // Key up
void virt_kb_tap(virt_dev dev, int kp);                      // Key down then up
void virt_kb_combo(virt_dev dev, const int *keys, size_t n); // Tap keys[n-1] holding the others
void virt_kb_fini(virt_dev dev);                             // Destroy a virtual keyboard
virt_dev virt_mouse_init(const char *name);                  // Create a virtual mouse, -1 on failure
void virt_mouse_move(virt_dev dev, int dx, int dy);          // Relative cursor movement
void virt_mouse_button(virt_dev dev, int btn, int val);      // val: 1 press, 0 release
void virt_mouse_fini(virt_dev dev);                          // Destroy a virtual mouse

#if defined(INCLUDE_VIRT_IMPLEMENTATION)

static void
virt_emit(virt_dev dev, int type, int code, int val)
{
        struct input_event ev = { 0 }; // the kernel fills in the time
        ev.type               = type;
        ev.code               = code;
        ev.value              = val;
        ssize_t n             = write(dev, &ev, sizeof(ev));
        (void) n; // errors are ignored, see NOTES
}

// sets the name and ids and creates the device; closes fd on failure
static virt_dev
virt_create(int fd, const char *name, unsigned short product)
{
        struct uinput_setup setup = { 0 };
        setup.id.bustype          = BUS_USB;
        setup.id.vendor           = 0x1234; /* sample vendor */
        setup.id.product          = product;
        strncpy(setup.name, name, UINPUT_MAX_NAME_SIZE - 1);

        if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
                close(fd);
                return -1;
        }
        return fd;
}

virt_dev
virt_kb_init(const char *name)
{
        int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) return -1;

        /*
         * Enable key events, in this case every keyboard key (codes from
         * BTN_MISC up are mouse/joystick buttons).
         */
        if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) goto err;
        for (int code = KEY_ESC; code < BTN_MISC; code++)
                if (ioctl(fd, UI_SET_KEYBIT, code) < 0) goto err;

        return virt_create(fd, name, 0x5678 /* sample product */);
err:
        close(fd);
        return -1;
}

void
virt_kb_ensure_connection(void)
{
        /*
         * On device creation the kernel will create the device node for this
         * device. We are inserting a pause here so that userspace has time
         * to detect, initialize the new device, and can start listening to
         * the event, otherwise it will not notice the event we are about
         * to send.
         */
        sleep(1);
}

void
virt_kb_fini(virt_dev dev)
{
        ioctl(dev, UI_DEV_DESTROY);
        close(dev);
}

void
virt_kb_press(virt_dev dev, int kp)
{
        virt_emit(dev, EV_KEY, kp, 1);         // key press
        virt_emit(dev, EV_SYN, SYN_REPORT, 0); // report
}

void
virt_kb_release(virt_dev dev, int kp)
{
        virt_emit(dev, EV_KEY, kp, 0);         // key release
        virt_emit(dev, EV_SYN, SYN_REPORT, 0); // report
}

void
virt_kb_tap(virt_dev dev, int kp)
{
        virt_emit(dev, EV_KEY, kp, 1);         // key press
        virt_emit(dev, EV_SYN, SYN_REPORT, 0); // report
        virt_emit(dev, EV_KEY, kp, 0);         // key release
        virt_emit(dev, EV_SYN, SYN_REPORT, 0); // report
}

void
virt_kb_combo(virt_dev dev, const int *keys, size_t n)
{
        for (size_t i = 0; i < n; i++)
                virt_kb_press(dev, keys[i]);
        for (size_t i = n; i-- > 0;)
                virt_kb_release(dev, keys[i]);
}

virt_dev
virt_mouse_init(const char *name)
{
        int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) return -1;

        if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) goto err;
        if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0) goto err;
        if (ioctl(fd, UI_SET_RELBIT, REL_Y) < 0) goto err;
        if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) goto err;
        if (ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0) goto err;
        if (ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT) < 0) goto err;

        return virt_create(fd, name, 0x5679 /* sample product */);
err:
        close(fd);
        return -1;
}

void
virt_mouse_move(virt_dev dev, int dx, int dy)
{
        if (dx) virt_emit(dev, EV_REL, REL_X, dx);
        if (dy) virt_emit(dev, EV_REL, REL_Y, dy);
        virt_emit(dev, EV_SYN, SYN_REPORT, 0);
}

void
virt_mouse_button(virt_dev dev, int btn, int val)
{
        virt_emit(dev, EV_KEY, btn, val);
        virt_emit(dev, EV_SYN, SYN_REPORT, 0);
}

void
virt_mouse_fini(virt_dev dev)
{
        ioctl(dev, UI_DEV_DESTROY);
        close(dev);
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* VIRT_H_ */

/*
-----------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
-----------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Hugo Coto Florez
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-----------------------------------------------------------------------------
*/
