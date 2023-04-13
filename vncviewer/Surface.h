/* Copyright 2016 Pierre Ossman for Cendio AB
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

#ifndef __SURFACE_H__
#define __SURFACE_H__

#if defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
// Apple headers conflict with FLTK, so redefine types here
typedef struct CGImage* CGImageRef;
#else
#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

//class Fl_RGB_Image;
class QImage;
class QQuickWindow;

class Surface {
public:
  Surface(int width, int height);
  Surface(const QImage *image);
  ~Surface();

  int width() { return w; }
  int height() { return h; }
#if defined(WIN32)
  HBITMAP hbitmap() { return bitmap; }
#endif

  void clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a=255);

  void draw(int src_x, int src_y, int dst_x, int dst_y,
            int dst_w, int dst_h);
  void draw(Surface* dst, int src_x, int src_y, int dst_x, int dst_y,
            int dst_w, int dst_h);

  void blend(int src_x, int src_y, int dst_x, int dst_y,
             int dst_w, int dst_h, int a=255);
  void blend(Surface* dst, int src_x, int src_y, int dst_x, int dst_y,
             int dst_w, int dst_h, int a=255);

protected:
  void alloc();
  void dealloc();
  void update(const QImage *image);

protected:
  int w, h;
  QQuickWindow *m_window;

#if defined(WIN32)
  RGBQUAD* data;
  HBITMAP bitmap;
#elif defined(__APPLE__)
  unsigned char* data;
#else
  Pixmap pixmap;
  Picture picture;
  XRenderPictFormat* visFormat;

  // cf. https://www.fltk.org/doc-1.3/osissues.html
  Display *fl_display;
  Window fl_window;
  GC fl_gc;
  int fl_screen;
  XVisualInfo *fl_visual;
  Colormap fl_colormap;

  Picture alpha_mask(int a);
#endif
};

#endif

