#ifndef X11_H
#define X11_H

class QWidget;

bool x11_has_wm();
bool x11_is_ewmh_supported();

void x11_fullscreen_screens(QWidget* window, int top, int bottom, int left, int right);
void x11_fullscreen(QWidget* window, bool enabled);

#endif // X11_H
