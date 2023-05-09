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

#if defined(WIN32)
#include <windows.h>
#endif

#include <rdr/Exception.h>
#include "Surface.h"

void Surface::alloc()
{
  BITMAPINFOHEADER bih;
  memset(&bih, 0, sizeof(bih));
  bih.biSize         = sizeof(BITMAPINFOHEADER);
  bih.biBitCount     = 32;
  bih.biPlanes       = 1;
  bih.biWidth        = width();
  bih.biHeight       = -height(); // Negative to get top-down
  bih.biCompression  = BI_RGB;

  bitmap = CreateDIBSection(nullptr, (BITMAPINFO*)&bih,
                            DIB_RGB_COLORS, (void**)&data, nullptr, 0);
  if (!bitmap) {
    throw rdr::SystemException("CreateDIBSection", GetLastError());
  }
}

void Surface::dealloc()
{
  DeleteObject(bitmap);
}
