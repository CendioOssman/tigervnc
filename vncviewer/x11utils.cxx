#include "x11utils.h"
#include <climits>

#include <X11/Xlib.h>

namespace X11Utils {

unsigned long getWindowPropertyX11(Display* display, Window window, Atom property, Atom type, unsigned char** prop_return)
{
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;

  XGetWindowProperty(display,
                     window,
                     property,
                     0,
                     LONG_MAX,
                     false,
                     type,
                     &actual_type_return,
                     &actual_format_return,
                     &nitems_return,
                     &bytes_after_return,
                     prop_return);

  return nitems_return;
}

bool hasWM(Display* display)
{
  int screen = DefaultScreen(display);

  Window rootWindow = RootWindow(display, screen);

  Atom supportingWMCheck = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", false);

  Window* windowFromRoot = nullptr;
  auto supportingWMCheckOk = getWindowPropertyX11(display,
                                                  rootWindow,
                                                  supportingWMCheck,
                                                  XA_WINDOW,
                                                  (unsigned char**) &windowFromRoot);

  if (!supportingWMCheckOk) {
    return false;
  }

  return true;
}

bool isEWMHsupported(Display* display)
{
  int screen = DefaultScreen(display);

  Window rootWindow = RootWindow(display, screen);

  Atom supportingWMCheck = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", false);

  Window* windowFromRoot = nullptr;
  auto supportingWMCheckOk = getWindowPropertyX11(display,
                                                  rootWindow,
                                                  supportingWMCheck,
                                                  XA_WINDOW,
                                                  (unsigned char**) &windowFromRoot);

  if (!supportingWMCheckOk) {
    return false;
  }

  Atom netSupported = XInternAtom(display, "_NET_SUPPORTED", false);

  Atom* supportedAtoms = nullptr;
  auto atomCount = getWindowPropertyX11(display,
                                        rootWindow,
                                        netSupported,
                                        XA_ATOM,
                                        (unsigned char**) &supportedAtoms);

  Atom fullscreenMonitors = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", false);

  for (unsigned long i = 0;  i < atomCount;  i++) {
    if (supportedAtoms[i] == fullscreenMonitors)
      return true;
  }

  return false;
}

void fullscreen_screens(Display* display, int screen, Window window, int top, int bottom, int left, int right)
{
  XEvent event;
  event.xany.type = ClientMessage;
  event.xany.window = window;
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

void fullscreen(Display* display, int screen, Window window, bool enabled)
{
  XEvent event;
  event.xany.type = ClientMessage;
  event.xany.window = window;
  event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
  event.xclient.format = 32;
  event.xclient.data.l[0] = enabled;
  event.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &event);
}

};
