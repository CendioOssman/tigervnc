/* Copyright 2011-2016 Pierre Ossman for Cendio AB
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

#include <assert.h>
#include <stdlib.h>
#include <QTextStream>
#include <QEvent>
#include <QCursor>

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include <rfb/LogWriter.h>
#include <rdr/Exception.h>
#include "Surface.h"
#include "PlatformPixelBuffer.h"
#include "appmanager.h"

#include <QDebug>

static rfb::LogWriter vlog("PlatformPixelBuffer");

PlatformPixelBuffer::PlatformPixelBuffer(int width, int height) :
#if defined(__APPLE__)
  FullFramePixelBuffer(rfb::PixelFormat(32, 24, false, true,
                                        255, 255, 255, 0, 8, 16),
                       0, 0, nullptr, 0),
#else
  FullFramePixelBuffer(rfb::PixelFormat(32, 24, false, true,
                                        255, 255, 255, 16, 8, 0),
                       0, 0, nullptr, 0),
#endif
  Surface(width, height)
{
  setBuffer(width, height, (uint8_t*)framebuffer(), width);
}

PlatformPixelBuffer::~PlatformPixelBuffer()
{
}

void PlatformPixelBuffer::commitBufferRW(const rfb::Rect& r)
{
  FullFramePixelBuffer::commitBufferRW(r);
  mutex.lock();
  damage.assign_union(rfb::Region(r));
  mutex.unlock();

  AppManager::instance()->invalidate(r.tl.x, r.tl.y, r.br.x, r.br.y);
//  qDebug() << "PlatformPixelBuffer::commitBufferRW(): Rect=(" << r.tl.x << "," << r.tl.y << ")-(" << r.br.x << "," << r.br.y << ")";
}

rfb::Rect PlatformPixelBuffer::getDamage(void)
{
  rfb::Rect r;

  mutex.lock();
  r = damage.get_bounding_rect();
  damage.clear();
  mutex.unlock();
  return r;
}
