#ifndef X11_H
#define X11_H

#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>

unsigned long x11_get_window_property(Display* display,
                                      Window window,
                                      Atom property,
                                      Atom type,
                                      unsigned char** prop_return);

bool x11_has_wm(Display* display);
bool x11_is_ewmh_supported(Display* display);

void x11_fullscreen_screens(Display* display, int screen, Window window, int top, int bottom, int left, int right);
void x11_fullscreen(Display* display, int screen, Window window, bool enabled);

#endif // X11_H
