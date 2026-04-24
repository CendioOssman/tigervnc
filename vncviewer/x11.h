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

#ifndef __X11_H__
#define __X11_H__

class QWidget;

bool x11_has_wm();
bool x11_wm_supports(const char* atom);

void x11_fullscreen_screens(QWidget* window, int top, int bottom, int left, int right);
void x11_fullscreen(QWidget* window, bool enabled);

bool x11_grab_keyboard(QWidget* win);
void x11_ungrab_keyboard();

bool x11_grab_pointer(QWidget* win);
void x11_ungrab_pointer();

bool x11_is_pointer_on_same_screen(QWidget* win);

void x11_bell();

#endif
