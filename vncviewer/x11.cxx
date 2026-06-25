/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>

#include <FL/x.H>

#include "x11.h"

#define _NET_WM_STATE_ADD           1  /* add/set property */

bool x11_has_wm()
{
  Window* wmWindow;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;

  XGetWindowProperty(fl_display, XRootWindow(fl_display, fl_screen),
                     XInternAtom(fl_display, "_NET_SUPPORTING_WM_CHECK", False),
                     0, 1, False, XA_WINDOW,
                     &actual_type, &actual_format, &nitems,
                     &bytes_after, (unsigned char**)&wmWindow);
  if ((actual_type != XA_WINDOW) || (actual_format != 32) ||
      (nitems != 1) || (bytes_after != 0))
    return false;

  // Confirm WM is alive
  XGetWindowProperty(fl_display, *wmWindow,
                     XInternAtom(fl_display, "_NET_SUPPORTING_WM_CHECK", False),
                     0, 1, False, XA_WINDOW,
                     &actual_type, &actual_format, &nitems,
                     &bytes_after, (unsigned char**)&wmWindow);
  if ((actual_type != XA_WINDOW) || (actual_format != 32) ||
      (nitems != 1) || (bytes_after != 0))
    return false;

  return true;
}

bool x11_wm_supports(const char* atom)
{
  Atom* supported;
  Atom desired;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;

  if (!x11_has_wm())
    return false;

  XGetWindowProperty(fl_display, XRootWindow(fl_display, fl_screen),
                     XInternAtom(fl_display, "_NET_SUPPORTED", False),
                     0, LONG_MAX, False, XA_ATOM,
                     &actual_type, &actual_format, &nitems,
                     &bytes_after, (unsigned char**)&supported);
  if ((actual_type != XA_ATOM) || (actual_format != 32))
    return false;

  desired = XInternAtom(fl_display, atom, False);
  for (unsigned long n = 0; n < nitems; n++) {
    if (supported[n] == desired)
      return true;
  }

  return false;
}

void x11_win_maximize(Fl_Window* win)
{
  Atom net_wm_state = XInternAtom (fl_display, "_NET_WM_STATE", 0);
  Atom net_wm_state_maximized_vert = XInternAtom (fl_display, "_NET_WM_STATE_MAXIMIZED_VERT", 0);
  Atom net_wm_state_maximized_horz = XInternAtom (fl_display, "_NET_WM_STATE_MAXIMIZED_HORZ", 0);

  XEvent e;
  e.xany.type = ClientMessage;
  e.xany.window = fl_xid(win);
  e.xclient.message_type = net_wm_state;
  e.xclient.format = 32;
  e.xclient.data.l[0] = _NET_WM_STATE_ADD;
  e.xclient.data.l[1] = net_wm_state_maximized_vert;
  e.xclient.data.l[2] = net_wm_state_maximized_horz;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;
  XSendEvent(fl_display, RootWindow(fl_display, fl_screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
}

bool x11_win_is_maximized(Fl_Window* win)
{
  bool maximized;

  Atom net_wm_state = XInternAtom (fl_display, "_NET_WM_STATE", 0);
  Atom net_wm_state_maximized_vert = XInternAtom (fl_display, "_NET_WM_STATE_MAXIMIZED_VERT", 0);
  Atom net_wm_state_maximized_horz = XInternAtom (fl_display, "_NET_WM_STATE_MAXIMIZED_HORZ", 0);

  Atom type;
  int format;
  unsigned long nitems, remain;
  Atom *atoms;

  XGetWindowProperty(fl_display, fl_xid(win), net_wm_state, 0, 1024,
                     False, XA_ATOM, &type, &format, &nitems, &remain,
                     (unsigned char**)&atoms);

  maximized = false;
  for (unsigned long n = 0;n < nitems;n++) {
    if ((atoms[n] == net_wm_state_maximized_vert) ||
        (atoms[n] == net_wm_state_maximized_horz)) {
      maximized = true;
      break;
    }
  }

  XFree(atoms);

  return maximized;
}

void x11_win_get_coords(Fl_Window* win, int* x, int* y, int* w, int* h)
{
  XWindowAttributes actual;
  Window cr;

  assert(x);
  assert(y);
  assert(w);
  assert(h);

  XGetWindowAttributes(fl_display, fl_xid(win), &actual);

  *x = actual.x;
  *y = actual.y;

  XTranslateCoordinates(fl_display, fl_xid(win), actual.root,
                        0, 0, w, h, &cr);
}

void x11_win_may_grab(Fl_Window* win)
{
  XEvent e;

  e.xany.type = ClientMessage;
  e.xany.window = fl_xid(win);
  e.xclient.message_type = XInternAtom (fl_display, "_XWAYLAND_MAY_GRAB_KEYBOARD", 0);
  e.xclient.format = 32;
  e.xclient.data.l[0] = 1;
  e.xclient.data.l[1] = 0;
  e.xclient.data.l[2] = 0;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;
  XSendEvent(fl_display, RootWindow(fl_display, fl_screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
}

bool x11_grab_keyboard(Fl_Window* win)
{
  int ret;

  ret = XGrabKeyboard(fl_display, fl_xid(win), True,
                      GrabModeAsync, GrabModeAsync, CurrentTime);
  if (ret) {
    if (ret == AlreadyGrabbed) {
      // It seems like we can race with the WM in some cases, e.g. when
      // the WM holds the keyboard as part of handling Alt+Tab.
      // Repeat the request a few times and see if we get it...
      for (int attempt = 0; attempt < 5; attempt++) {
        usleep(100000);
        // Also throttle based on how busy the X server is
        XSync(fl_display, False);
        ret = XGrabKeyboard(fl_display, fl_xid(win), True,
                            GrabModeAsync, GrabModeAsync, CurrentTime);
        if (ret != AlreadyGrabbed)
          break;
      }
    }
  }

  return ret == Success;
}

void x11_ungrab_keyboard()
{
  XUngrabKeyboard(fl_display, CurrentTime);
}

static int xi_major;

bool x11_has_xinput22()
{
  static bool checked = false;
  static bool has_xinput22 = false;

  int ev, err;
  int major_ver, minor_ver;

  if (checked)
    return has_xinput22;

  checked = true;

  fl_open_display();

  if (!XQueryExtension(fl_display, "XInputExtension", &xi_major, &ev, &err))
    return false;

  major_ver = 2;
  minor_ver = 2;
  if (XIQueryVersion(fl_display, &major_ver, &minor_ver) != Success)
    return false;

  if ((major_ver == 2) && (minor_ver < 2))
    return false;

  has_xinput22 = true;

  return true;
}

int x11_xinput_major()
{
  if (!x11_has_xinput22())
    return -1;

  return xi_major;
}

bool x11_grab_pointer(Fl_Window* win)
{
  Window window;

  XIEventMask *curmasks;
  int num_masks;

  int ret, ndevices;

  XIDeviceInfo *devices, *device;
  bool gotGrab;

  window = fl_xid(win);

  if (!x11_has_xinput22()) {
    int status;

    status = XGrabPointer(fl_display, window, True,
                          ButtonPressMask | ButtonReleaseMask |
                          ButtonMotionMask | PointerMotionMask,
                          GrabModeAsync, GrabModeAsync,
                          None, None, CurrentTime);
    return status == Success;
  }

  // We grab for the same events as the window is currently interested in
  curmasks = XIGetSelectedEvents(fl_display, window, &num_masks);
  if (curmasks == nullptr)
    return false;

  // Our windows should only have a single mask, which allows us to
  // simplify all the code handling the masks
  if (num_masks > 1) {
    XFree(curmasks);
    return false;
  }

  // We need to remove XI_TouchOwnership from our event masks while
  // grabbing as otherwise we will get double events (one for the grab,
  // and one because of XI_TouchOwnership) with no way of telling them
  // apart. See XInputTouchHandler constructor for why we use this
  // event.
  XIClearMask(curmasks[0].mask, XI_TouchOwnership);
  XISelectEvents(fl_display, window, curmasks, 1);

  devices = XIQueryDevice(fl_display, XIAllMasterDevices, &ndevices);

  // Iterate through available devices to find those which
  // provide pointer input, and attempt to grab all such devices.
  gotGrab = false;
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    curmasks[0].deviceid = device->deviceid;

    ret = XIGrabDevice(fl_display,
                       device->deviceid,
                       window,
                       CurrentTime,
                       None,
                       XIGrabModeAsync,
                       XIGrabModeAsync,
                       True,
                       &(curmasks[0]));

    if (ret)
      continue;

    gotGrab = true;
  }

  XIFreeDeviceInfo(devices);

  // Did we not even grab a single device?
  if (!gotGrab) {
    XISetMask(curmasks[0].mask, XI_TouchOwnership);
    XISelectEvents(fl_display, window, curmasks, 1);
    XFree(curmasks);
    return false;
  }

  XFree(curmasks);

  return true;
}

void x11_ungrab_pointer(Fl_Window* win)
{
  Window window;

  int ndevices;
  XIDeviceInfo *devices, *device;

  XIEventMask *curmasks;
  int num_masks;

  window = fl_xid(win);

  if (!x11_has_xinput22()) {
    XUngrabPointer(fl_display, CurrentTime);
    return;
  }

  devices = XIQueryDevice(fl_display, XIAllMasterDevices, &ndevices);

  // Release all devices, hoping they are the same as when we
  // grabbed things
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    XIUngrabDevice(fl_display, device->deviceid, CurrentTime);
  }

  XIFreeDeviceInfo(devices);

  // Restore XI_TouchOwnership now that the grab is gone
  curmasks = XIGetSelectedEvents(fl_display, window, &num_masks);
  if (curmasks == nullptr)
    return;

  // Our windows should only have a single mask, which allows us to
  // simplify all the code handling the masks
  if (num_masks > 1) {
    XFree(curmasks);
    return;
  }

  XISetMask(curmasks[0].mask, XI_TouchOwnership);
  XISelectEvents(fl_display, window, curmasks, 1);

  XFree(curmasks);
}

void x11_warp_pointer(unsigned x, unsigned y)
{
  Window rootwindow = DefaultRootWindow(fl_display);
  XWarpPointer(fl_display, rootwindow, rootwindow, 0, 0, 0, 0, x, y);
}

bool x11_is_pointer_on_same_screen(Fl_Window* win)
{
  Window root, child;
  int x, y, wx, wy;
  unsigned int mask;

  if (XQueryPointer(fl_display, fl_xid(win), &root, &child,
                    &x, &y, &wx, &wy, &mask) &&
      (root != XRootWindow(fl_display, fl_screen)))
    return false;

  return true;
}
