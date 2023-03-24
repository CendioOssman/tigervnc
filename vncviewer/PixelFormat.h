/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
 * Copyright 2009-2014 Pierre Ossman for Cendio AB
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
//
// PixelFormat - structure to represent a pixel format.  Also has useful
// methods for reading & writing to streams, etc. Conversion to and from
// other formats are also handled by this class. We have three different
// representations that we refer to:
//
// a) Pixels - Unsigned native integers in the format specified by this
//             PixelFormat object.
// b) Buffer - Same thing as pixels, but in the appropriate byte stream
//             format. This involves endian conversion and padding.
// c) RGB - A byte stream of 8 bit red, green and blue elements, in that
//          order.
//

#ifndef __RFB_PIXELFORMAT_H__
#define __RFB_PIXELFORMAT_H__

#include "rfb/Pixel.h"
#include "rfb/Rect.h"

namespace rdr {
  class FdInStream;
  class FdOutStream;
}

class PixelFormat {
public:
    PixelFormat(int b, int d, bool e, bool t,
                int rm, int gm, int bm, int rs, int gs, int bs);
    PixelFormat();

    // Checks if the formats have identical buffer representation.
    // They might still have different pixel representation, endianness
    // or true colour state.
    bool equal(const PixelFormat& other) const;

    void read(rdr::FdInStream *is);
    void write(rdr::FdOutStream *os) const;

    bool is888(void) const;
    bool isBigEndian(void) const;
    bool isLittleEndian(void) const;

    inline rfb::Pixel pixelFromBuffer(const rdr::U8* buffer) const;
    inline void bufferFromPixel(rdr::U8* buffer, rfb::Pixel pixel) const;

    inline rfb::Pixel pixelFromRGB(rdr::U16 red, rdr::U16 green, rdr::U16 blue) const;
    inline rfb::Pixel pixelFromRGB(rdr::U8 red, rdr::U8 green, rdr::U8 blue) const;

    void bufferFromRGB(rdr::U8 *dst, const rdr::U8* src, int pixels) const;
    void bufferFromRGB(rdr::U8 *dst, const rdr::U8* src,
                       int w, int stride, int h) const;

    inline void rgbFromPixel(rfb::Pixel pix, rdr::U16 *r, rdr::U16 *g, rdr::U16 *b) const;
    inline void rgbFromPixel(rfb::Pixel pix, rdr::U8 *r, rdr::U8 *g, rdr::U8 *b) const;

    void rgbFromBuffer(rdr::U8* dst, const rdr::U8* src, int pixels) const;
    void rgbFromBuffer(rdr::U8* dst, const rdr::U8* src,
                       int w, int stride, int h) const;

    rfb::Pixel pixelFromPixel(const PixelFormat &srcPF, rfb::Pixel src) const;

    void bufferFromBuffer(rdr::U8* dst, const PixelFormat &srcPF,
                          const rdr::U8* src, int pixels) const;
    void bufferFromBuffer(rdr::U8* dst, const PixelFormat &srcPF,
                          const rdr::U8* src, int w, int h,
                          int dstStride, int srcStride) const;

    void print(char* str, int len) const;
    bool parse(const char* str);

protected:
    void updateState(void);
    bool isSane(void);

private:
    // Preprocessor generated, optimised methods

    void directBufferFromBufferFrom888(rdr::U8* dst, const PixelFormat &srcPF,
                                       const rdr::U8* src, int w, int h,
                                       int dstStride, int srcStride) const;
    void directBufferFromBufferFrom888(rdr::U16* dst, const PixelFormat &srcPF,
                                       const rdr::U8* src, int w, int h,
                                       int dstStride, int srcStride) const;
    void directBufferFromBufferFrom888(rdr::U32* dst, const PixelFormat &srcPF,
                                       const rdr::U8* src, int w, int h,
                                       int dstStride, int srcStride) const;

    void directBufferFromBufferTo888(rdr::U8* dst, const PixelFormat &srcPF,
                                     const rdr::U8* src, int w, int h,
                                     int dstStride, int srcStride) const;
    void directBufferFromBufferTo888(rdr::U8* dst, const PixelFormat &srcPF,
                                     const rdr::U16* src, int w, int h,
                                     int dstStride, int srcStride) const;
    void directBufferFromBufferTo888(rdr::U8* dst, const PixelFormat &srcPF,
                                     const rdr::U32* src, int w, int h,
                                     int dstStride, int srcStride) const;

public:
    int bpp;
    int depth;

    // This only tracks if the client thinks it is in colour map mode.
    // In practice we are always in true colour mode.
    bool trueColour;

protected:
    bool bigEndian;
    int redMax;
    int greenMax;
    int blueMax;
    int redShift;
    int greenShift;
    int blueShift;

protected:
    /* Pre-computed values to keep algorithms simple */
    int redBits, greenBits, blueBits;
    int maxBits, minBits;
    bool endianMismatch;

    static rdr::U8 upconvTable[256*8];
    static rdr::U8 downconvTable[256*8];

    class Init;
    friend class Init;
    static Init _init;

    /* Only for testing this class */
    friend void makePixel(const PixelFormat &, rdr::U8 *);
    friend bool verifyPixel(const PixelFormat &,
                            const PixelFormat &,
                            const rdr::U8 *);
};

inline rfb::Pixel PixelFormat::pixelFromBuffer(const rdr::U8* buffer) const
{
  rfb::Pixel p;

  p = 0;

  if (bigEndian) {
    switch (bpp) {
    case 32:
      p |= ((rfb::Pixel)*(buffer++)) << 24;
      p |= ((rfb::Pixel)*(buffer++)) << 16;
    case 16:
      p |= ((rfb::Pixel)*(buffer++)) << 8;
    case 8:
      p |= *buffer;
    }
  } else {
    p |= buffer[0];
    if (bpp >= 16) {
      p |= ((rfb::Pixel)buffer[1]) << 8;
      if (bpp == 32) {
        p |= ((rfb::Pixel)buffer[2]) << 16;
        p |= ((rfb::Pixel)buffer[3]) << 24;
      }
    }
  }

  return p;
}


inline void PixelFormat::bufferFromPixel(rdr::U8* buffer, rfb::Pixel p) const
{
  if (bigEndian) {
    switch (bpp) {
    case 32:
      *(buffer++) = (p >> 24) & 0xff;
      *(buffer++) = (p >> 16) & 0xff;
    case 16:
      *(buffer++) = (p >>  8) & 0xff;
    case 8:
      *(buffer++) = (p >>  0) & 0xff;
    }
  } else {
    buffer[0] = (p >>  0) & 0xff;
    if (bpp >= 16) {
      buffer[1] = (p >>  8) & 0xff;
      if (bpp == 32) {
        buffer[2] = (p >> 16) & 0xff;
        buffer[3] = (p >> 24) & 0xff;
      }
    }
  }
}


inline rfb::Pixel PixelFormat::pixelFromRGB(rdr::U16 red, rdr::U16 green, rdr::U16 blue) const
{
  rfb::Pixel p;

  p = (rfb::Pixel)downconvTable[(redBits-1)*256 + (red >> 8)] << redShift;
  p |= (rfb::Pixel)downconvTable[(greenBits-1)*256 + (green >> 8)] << greenShift;
  p |= (rfb::Pixel)downconvTable[(blueBits-1)*256 + (blue >> 8)] << blueShift;

  return p;
}


inline rfb::Pixel PixelFormat::pixelFromRGB(rdr::U8 red, rdr::U8 green, rdr::U8 blue) const
{
  rfb::Pixel p;

  p = (rfb::Pixel)downconvTable[(redBits-1)*256 + red] << redShift;
  p |= (rfb::Pixel)downconvTable[(greenBits-1)*256 + green] << greenShift;
  p |= (rfb::Pixel)downconvTable[(blueBits-1)*256 + blue] << blueShift;

  return p;
}


inline void PixelFormat::rgbFromPixel(rfb::Pixel p, rdr::U16 *r, rdr::U16 *g, rdr::U16 *b) const
{
  rdr::U8 _r, _g, _b;

  _r = p >> redShift;
  _g = p >> greenShift;
  _b = p >> blueShift;

  _r = upconvTable[(redBits-1)*256 + _r];
  _g = upconvTable[(greenBits-1)*256 + _g];
  _b = upconvTable[(blueBits-1)*256 + _b];

  *r = _r << 8 | _r;
  *g = _g << 8 | _g;
  *b = _b << 8 | _b;
}


inline void PixelFormat::rgbFromPixel(rfb::Pixel p, rdr::U8 *r, rdr::U8 *g, rdr::U8 *b) const
{
  rdr::U8 _r, _g, _b;

  _r = p >> redShift;
  _g = p >> greenShift;
  _b = p >> blueShift;

  *r = upconvTable[(redBits-1)*256 + _r];
  *g = upconvTable[(greenBits-1)*256 + _g];
  *b = upconvTable[(blueBits-1)*256 + _b];
}

#endif
