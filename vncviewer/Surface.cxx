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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Surface.h"

Surface::Surface(int width, int height)
  : w(width)
  , h(height)
#if !defined(__APPLE__) && !defined(WIN32)
  #if 0
  , m_pixmap(0)
  , m_picture(0)
  , m_display(nullptr)
  #endif
  , shminfo(nullptr)
  , xim(nullptr)
#endif
{
  alloc();
}

Surface::~Surface()
{
  dealloc();
}
