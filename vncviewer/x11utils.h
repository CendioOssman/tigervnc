#ifndef X11UTILS_H
#define X11UTILS_H

#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>

namespace X11Utils {

unsigned long getWindowPropertyX11(Display* display,
                                   Window window,
                                   Atom property,
                                   Atom type,
                                   unsigned char** prop_return);

bool hasWM(Display* display);
bool isEWMHsupported(Display* display);

void fullscreen_screens(Display* display, int screen, Window window, int top, int bottom, int left, int right);
void fullscreen(Display* display, int screen, Window window, bool enabled);

};

#endif // X11UTILS_H
