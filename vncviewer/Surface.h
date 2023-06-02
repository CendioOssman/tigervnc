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
class CGImage;
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XShm.h>
#endif

class QQuickWindow;

class Surface {
public:
  Surface(int width, int height);
  ~Surface();

  int width() { return w; }
  int height() { return h; }
#if defined(WIN32)
  HBITMAP hbitmap() { return bitmap; }
  RGBQUAD *framebuffer() { return data; }
#elif defined(__APPLE__)
  CGImage *bitmap() { return bitmap_; }
  unsigned char *framebuffer() { return data; }
#else
  XImage *ximage() { return xim; }
  XShmSegmentInfo *shmSegmentInfo() { return shminfo; }
  char *framebuffer() { return xim->data; }
#endif

protected:
  void alloc();
  void dealloc();

protected:
  int w, h;
  QQuickWindow *window_;

#if defined(WIN32)
  RGBQUAD* data;
  HBITMAP bitmap;
#elif defined(__APPLE__)
  unsigned char* data;
  CGImage *bitmap_;
#else
  XShmSegmentInfo *shminfo;
  XImage *xim;
  bool setupShm();
#endif
};

#endif

