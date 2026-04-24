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

class QWidget;

void cocoa_beep();

int cocoa_capture_displays(QWidget* win);
void cocoa_release_displays(QWidget* win);

bool cocoa_screens_have_separate_spaces();

void cocoa_set_presentation_default();
void cocoa_set_presentation_full_screen();

int cocoa_scrollbar_size();

void cocoa_prevent_native_full_screen(QWidget* win);

void cocoa_event_delay(double seconds);

void cocoa_set_cursor_pos(int x, int y);

#endif
