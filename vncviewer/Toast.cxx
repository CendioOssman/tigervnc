/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <time.h>

#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include <core/time.h>

#include "Surface.h"
#include "Toast.h"

// Time before we show a toast again
const time_t TOAST_REPEAT_TIMEOUT = 600;

Toast::Toast(Fl_Widget* parent_)
  : parent(parent_)
{
}

Toast::~Toast()
{
  Fl::remove_timeout(updateOverlay, this);

  while (!overlays.empty()) {
    delete overlays.front().surface;
    overlays.pop_front();
  }
}

void Toast::addRegular(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  std::map<std::string, time_t>::iterator iter;

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  // Purge all old entries
  for (iter = overlayTimes.begin(); iter != overlayTimes.end(); ) {
    if ((time(nullptr) - iter->second) >= TOAST_REPEAT_TIMEOUT)
      overlayTimes.erase(iter++);
    else
      iter++;
  }

  // Recently shown?
  if (overlayTimes.count(textbuf) > 0)
    return;

  overlayTimes[textbuf] = time(nullptr);

  addOverlay(textbuf);
}

void Toast::addPriority(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  addOverlay(textbuf);
}

bool Toast::shown()
{
  return !overlays.empty();
}

int Toast::width()
{
  if (overlays.empty())
    return 0;
  return overlays.front().surface->width();
}

int Toast::height()
{
  if (overlays.empty())
    return 0;
  return overlays.front().surface->height();
}

void Toast::draw(int src_x, int src_y, int dst_x, int dst_y,
                 int dst_w, int dst_h)
{
  Overlay* overlay;

  if (overlays.empty())
    return;

  overlay = &overlays.front();
  overlay->surface->blend(src_x, src_y, dst_x, dst_y, dst_w, dst_h,
                          overlay->alpha);
}

void Toast::draw(Surface* dst, int src_x, int src_y,
                 int dst_x, int dst_y, int dst_w, int dst_h)
{
  Overlay* overlay;

  if (overlays.empty())
    return;

  overlay = &overlays.front();
  overlay->surface->blend(dst, src_x, src_y, dst_x, dst_y, dst_w, dst_h,
                          overlay->alpha);
}

void Toast::addOverlay(const char* text)
{
  const Fl_Fontsize fontsize = 16;
  const int margin = 10;

  Fl_Image_Surface *surface;

  Fl_RGB_Image* imageText;
  Fl_RGB_Image* image;

  unsigned char* buffer;

  int x, y;
  int w, h;

  unsigned char* a;
  const unsigned char* b;

  struct Overlay overlay;

#if !defined(WIN32) && !defined(__APPLE__)
  // FLTK < 1.3.5 crashes if fl_gc is unset
  if (!fl_gc)
    fl_gc = XDefaultGC(fl_display, 0);
#endif

  fl_font(FL_HELVETICA, fontsize);
  w = 0;
  fl_measure(text, w, h);

  // Margins
  w += margin * 2 * 2;
  h += margin * 2;

  surface = new Fl_Image_Surface(w, h);
  surface->set_current();

  fl_rectf(0, 0, w, h, 0, 0, 0);

  fl_font(FL_HELVETICA, fontsize);
  fl_color(FL_WHITE);
  fl_draw(text, 0, 0, w, h, FL_ALIGN_CENTER);

  imageText = surface->image();
  delete surface;

  Fl_Display_Device::display_device()->set_current();

  buffer = new unsigned char[w * h * 4];
  image = new Fl_RGB_Image(buffer, w, h, 4);

  a = buffer;
  for (x = 0;x < image->w() * image->h();x++) {
    a[0] = a[1] = a[2] = 0x40;
    a[3] = 0xcc;
    a += 4;
  }

  a = buffer;
  b = (const unsigned char*)imageText->data()[0];
  for (y = 0;y < h;y++) {
    for (x = 0;x < w;x++) {
      unsigned char alpha;
      alpha = *b;
      a[0] = (unsigned)a[0] * (255 - alpha) / 255 + alpha;
      a[1] = (unsigned)a[1] * (255 - alpha) / 255 + alpha;
      a[2] = (unsigned)a[2] * (255 - alpha) / 255 + alpha;
      a[3] = 255 - (255 - a[3]) * (255 - alpha) / 255;
      a += 4;
      b += imageText->d();
    }
    if (imageText->ld() != 0)
      b += imageText->ld() - w * imageText->d();
  }

  delete imageText;

  overlay.surface = new Surface(image);
  overlay.alpha = 0;
  memset(&overlay.start, 0, sizeof(overlay.start));
  overlays.push_back(overlay);

  delete image;
  delete [] buffer;

  if (overlays.size() == 1)
    Fl::add_timeout(0.5, updateOverlay, this);
}

void Toast::updateOverlay(void *data)
{
  Toast *self;
  struct Overlay* overlay;
  unsigned elapsed;

  self = (Toast*)data;

  if (self->overlays.empty())
    return;

  overlay = &self->overlays.front();

  if (overlay->start.tv_sec == 0)
    gettimeofday(&overlay->start, nullptr);

  elapsed = core::msSince(&overlay->start);

  if (elapsed < 500) {
    overlay->alpha = (unsigned)255 * elapsed / 500;
    Fl::add_timeout(1.0/60, updateOverlay, self);
  } else if (elapsed < 3500) {
    overlay->alpha = 255;
    Fl::add_timeout(3.0, updateOverlay, self);
  } else if (elapsed < 4000) {
    overlay->alpha = (unsigned)255 * (4000 - elapsed) / 500;
    Fl::add_timeout(1.0/60, updateOverlay, self);
  } else {
    delete overlay->surface;
    self->overlays.pop_front();
    if (!self->overlays.empty())
      Fl::add_timeout(0.5, updateOverlay, self);
  }

  // FIXME: Only damage relevant area
  self->parent->damage(FL_DAMAGE_USER1);
}
