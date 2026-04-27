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

#include <limits.h>
#include <unistd.h>

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif
#include <QScreen>
#include <QWidget>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/XInput2.h>

#include <rfb/LogWriter.h>

#include "i18n.h"
#include "x11.h"

static rfb::LogWriter vlog("X11");

static Display* qt_display()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QX11Info::display();
#else
    return qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
}

bool x11_has_wm()
{
  Display* display = qt_display();

  Window* wmWindow;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;

  XGetWindowProperty(display, XRootWindow(display, DefaultScreen(display)),
                     XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False),
                     0, 1, False, XA_WINDOW,
                     &actual_type, &actual_format, &nitems,
                     &bytes_after, (unsigned char**)&wmWindow);
  if ((actual_type != XA_WINDOW) || (actual_format != 32) ||
      (nitems != 1) || (bytes_after != 0))
    return false;

  // Confirm WM is alive
  XGetWindowProperty(display, *wmWindow,
                     XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False),
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
  Display* display = qt_display();

  Atom* supported;
  Atom desired;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;

  if (!x11_has_wm())
    return false;

  XGetWindowProperty(display,
                     XRootWindow(display, DefaultScreen(display)),
                     XInternAtom(display, "_NET_SUPPORTED", False),
                     0, LONG_MAX, False, XA_ATOM,
                     &actual_type, &actual_format, &nitems,
                     &bytes_after, (unsigned char**)&supported);
  if ((actual_type != XA_ATOM) || (actual_format != 32))
    return false;

  desired = XInternAtom(display, atom, False);
  for (unsigned long n = 0; n < nitems; n++) {
    if (supported[n] == desired)
      return true;
  }

  return false;
}

static bool x11_is_qscreen(QScreen* qscreen,
                           XineramaScreenInfo* xscreen)
{
  return (qscreen->geometry().x() == xscreen->x_org &&
          qscreen->geometry().y() == xscreen->y_org &&
          qscreen->geometry().width() == xscreen->width &&
          qscreen->geometry().height() == xscreen->height);
}

bool x11_fullscreen_screens(QWidget* window,
                            QScreen* top, QScreen* bottom,
                            QScreen* left, QScreen* right)
{
  Display* display = qt_display();
  int screen = DefaultScreen(display);

  XineramaScreenInfo* screens;
  int i, number;

  XEvent event;
  int xtop, xbottom, xleft, xright;

  screens = XineramaQueryScreens(display, &number);
  if (number == 0)
    return false;

  xtop = xbottom = xleft = xright = -1;
  for (i = 0; i < number; i++) {
    if (x11_is_qscreen(top, &screens[i]))
      xtop = i;
    if (x11_is_qscreen(bottom, &screens[i]))
      xbottom = i;
    if (x11_is_qscreen(left, &screens[i]))
      xleft = i;
    if (x11_is_qscreen(right, &screens[i]))
      xright = i;
  }

  if ((xtop == -1) || (xbottom == -1) ||
      (xleft == -1) || (xright == -1))
    return false;

  event.xany.type = ClientMessage;
  event.xany.window = window->winId();
  event.xclient.message_type = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", 0);
  event.xclient.format = 32;
  event.xclient.data.l[0] = xtop;
  event.xclient.data.l[1] = xbottom;
  event.xclient.data.l[2] = xleft;
  event.xclient.data.l[3] = xright;
  event.xclient.data.l[4] = 0;
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &event);

  return true;
}

void x11_fullscreen(QWidget* window, bool enabled)
{
  Display* display = qt_display();
  int screen = DefaultScreen(display);
  XEvent event;

  event.xany.type = ClientMessage;
  event.xany.window = window->winId();
  event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
  event.xclient.format = 32;
  event.xclient.data.l[0] = enabled;
  event.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &event);
}

void x11_win_may_grab(QWidget* win)
{
  Display* display = qt_display();
  int screen = DefaultScreen(display);
  XEvent e;

  e.xany.type = ClientMessage;
  e.xany.window = win->winId();
  e.xclient.message_type = XInternAtom (display, "_XWAYLAND_MAY_GRAB_KEYBOARD", 0);
  e.xclient.format = 32;
  e.xclient.data.l[0] = 1;
  e.xclient.data.l[1] = 0;
  e.xclient.data.l[2] = 0;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;
  XSendEvent(display, RootWindow(display, screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
}

bool x11_grab_keyboard(QWidget* win)
{
  Display* display = qt_display();
  int ret;

  ret = XGrabKeyboard(display, win->winId(), True,
                      GrabModeAsync, GrabModeAsync, CurrentTime);
  if (ret) {
    if (ret == AlreadyGrabbed) {
      // It seems like we can race with the WM in some cases, e.g. when
      // the WM holds the keyboard as part of handling Alt+Tab.
      // Repeat the request a few times and see if we get it...
      for (int attempt = 0; attempt < 5; attempt++) {
        usleep(100000);
        // Also throttle based on how busy the X server is
        XSync(display, False);
        ret = XGrabKeyboard(display, win->winId(), True,
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
  XUngrabKeyboard(qt_display(), CurrentTime);
}

bool x11_grab_pointer(QWidget* win)
{
  Display* display = qt_display();
  Window wnd;

  XIEventMask *curmasks;
  int num_masks;

  int ret, ndevices;

  XIDeviceInfo *devices, *device;
  bool gotGrab;

  wnd = win->winId();

  // We grab for the same events as the window is currently interested in
  curmasks = XIGetSelectedEvents(display, wnd, &num_masks);
  if (curmasks == nullptr) {
    if (num_masks == -1)
      vlog.error(_("Unable to get X Input 2 event mask for window 0x%08lx"), wnd);
    else
      vlog.error(_("Window 0x%08lx has no X Input 2 event mask"), wnd);

    return false;
  }

  // Our windows should only have a single mask, which allows us to
  // simplify all the code handling the masks
  if (num_masks > 1) {
    vlog.error(_("Window 0x%08lx has more than one X Input 2 event mask"), wnd);
    return false;
  }

  devices = XIQueryDevice(display, XIAllMasterDevices, &ndevices);

  // Iterate through available devices to find those which
  // provide pointer input, and attempt to grab all such devices.
  gotGrab = false;
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    curmasks[0].deviceid = device->deviceid;

    ret = XIGrabDevice(display,
                       device->deviceid,
                       wnd,
                       CurrentTime,
                       None,
                       XIGrabModeAsync,
                       XIGrabModeAsync,
                       True,
                       &(curmasks[0]));

    if (ret) {
      if (ret == XIAlreadyGrabbed) {
        continue;
      } else {
        vlog.error(_("Failure grabbing device %i"), device->deviceid);
        continue;
      }
    }

    gotGrab = true;
  }

  XIFreeDeviceInfo(devices);

         // Did we not even grab a single device?
  if (!gotGrab)
    return false;

  return true;
}

void x11_ungrab_pointer()
{
  Display* display = qt_display();

  int ndevices;
  XIDeviceInfo *devices, *device;

  devices = XIQueryDevice(display, XIAllMasterDevices, &ndevices);

  // Release all devices, hoping they are the same as when we
  // grabbed things
  for (int i = 0; i < ndevices; i++) {
    device = &devices[i];

    if (device->use != XIMasterPointer)
      continue;

    XIUngrabDevice(display, device->deviceid, CurrentTime);
  }

  XIFreeDeviceInfo(devices);
}

bool x11_is_pointer_on_same_screen(QWidget* win)
{
  Display* display = qt_display();
  Window root, child;
  int x, y, wx, wy;
  unsigned int mask;

  if (!XQueryPointer(display, win->winId(), &root, &child,
                     &x, &y, &wx, &wy, &mask))
    return false;

  return true;
}

void x11_bell()
{
  XBell(qt_display(), 0 /* volume */);
}
