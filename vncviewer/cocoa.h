/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __VNCVIEWER_COCOA_H__
#define __VNCVIEWER_COCOA_H__

class NSWindow;
class NSView;
class NSCursor;
class CGImage;
class CGContext;
class QWidget;
class QCursor;

NSView *cocoa_create_view(QWidget *parent, CGImage *bitmap);
void cocoa_beep();
void cocoa_resize(NSView *view, CGImage *iref);
NSCursor *cocoa_set_cursor(NSView *view, const QCursor *cursor);
CGImage *cocoa_create_bitmap(int width, int height, unsigned char *framebuffer);
void cocoa_draw(NSView *view, int x, int y, int w, int h);
void cocoa_invalidate_region(NSView *view, int x, int y, int w, int h);

int cocoa_capture_displays(QWidget *parent);
void cocoa_release_displays(QWidget *parent);

typedef struct CGColorSpace *CGColorSpaceRef;

CGColorSpaceRef cocoa_win_color_space(NSView *view);

bool cocoa_win_is_zoomed(QWidget *parent);
void cocoa_win_zoom(QWidget *parent);

int cocoa_is_keyboard_sync(const void *event);
int cocoa_is_keyboard_event(const void *event);

int cocoa_is_key_press(const void *event);
bool cocoa_is_mouse_entered(const void *event);
bool cocoa_is_mouse_exited(const void *event);
bool cocoa_is_mouse_moved(const void *event);

int cocoa_event_keycode(const void *event);
int cocoa_event_keysym(const void *event);

int cocoa_set_caps_lock_state(bool on);
int cocoa_set_num_lock_state(bool on);

int cocoa_get_caps_lock_state(bool *on);
int cocoa_get_num_lock_state(bool *on);

void cocoa_get_mouse_properties(const void *event, int *x, int *y, int *buttonMask);

#endif
