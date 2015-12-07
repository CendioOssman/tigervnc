/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander
 * Copyright 2014 Pierre Ossman for Cendio AB
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
#ifndef __RFB_TIGHTENCODER_H__
#define __RFB_TIGHTENCODER_H__

#include <rdr/MemOutStream.h>
#include <rdr/ZlibOutStream.h>
#include <rfb/Encoder.h>

namespace rfb {

  class TightEncoder : public Encoder {
  public:
    TightEncoder();
    virtual ~TightEncoder();

    virtual bool isSupported(const ConnParams& cp);

    virtual void setCompressLevel(int level);

    virtual void writeRect(const PixelBuffer* pb,
                           const Palette& palette,
                           const ConnParams& cp,
                           rdr::OutStream* os);
    virtual void writeSolidRect(int width, int height,
                                const PixelFormat& pf,
                                const rdr::U8* colour,
                                const ConnParams& cp,
                                rdr::OutStream* os);

  protected:
    void writeMonoRect(const PixelBuffer* pb,
                       const Palette& palette,
                       rdr::OutStream* os);
    void writeIndexedRect(const PixelBuffer* pb,
                          const Palette& palette,
                          rdr::OutStream* os);
    void writeFullColourRect(const PixelBuffer* pb,
                             const Palette& palette,
                             rdr::OutStream* os);

    void writePixels(const rdr::U8* buffer, const PixelFormat& pf,
                     unsigned int count, rdr::OutStream* os);

    void writeCompact(rdr::OutStream* os, rdr::U32 value);

    rdr::OutStream* getZlibOutStream(rdr::OutStream* os, int streamId,
                                     int level, size_t length);
    void flushZlibOutStream(rdr::OutStream* zos, rdr::OutStream* os);

  protected:
    // Preprocessor generated, optimised methods
    void writeMonoRect(int width, int height,
                       const rdr::U8* buffer, int stride,
                       const PixelFormat& pf, const Palette& palette,
                       rdr::OutStream *os);
    void writeMonoRect(int width, int height,
                       const rdr::U16* buffer, int stride,
                       const PixelFormat& pf, const Palette& palette,
                       rdr::OutStream *os);
    void writeMonoRect(int width, int height,
                       const rdr::U32* buffer, int stride,
                       const PixelFormat& pf, const Palette& palette,
                       rdr::OutStream *os);

    void writeIndexedRect(int width, int height,
                          const rdr::U16* buffer, int stride,
                          const PixelFormat& pf, const Palette& palette,
                          rdr::OutStream *os);
    void writeIndexedRect(int width, int height,
                          const rdr::U32* buffer, int stride,
                          const PixelFormat& pf, const Palette& palette,
                          rdr::OutStream *os);

    rdr::ZlibOutStream zlibStreams[4];
    rdr::MemOutStream memStream;

    int idxZlibLevel, monoZlibLevel, rawZlibLevel;
  };

}

#endif
