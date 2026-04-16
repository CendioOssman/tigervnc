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
#include <QWidget>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "x11.h"

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

void x11_fullscreen_screens(QWidget* window, int top, int bottom, int left, int right)
{
  Display* display = qt_display();
  int screen = DefaultScreen(display);
  XEvent event;

  event.xany.type = ClientMessage;
  event.xany.window = window->winId();
  event.xclient.message_type = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", 0);
  event.xclient.format = 32;
  event.xclient.data.l[0] = top;
  event.xclient.data.l[1] = bottom;
  event.xclient.data.l[2] = left;
  event.xclient.data.l[3] = right;
  event.xclient.data.l[4] = 0;
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &event);
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

void x11_bell()
{
  XBell(qt_display(), 0 /* volume */);
}
