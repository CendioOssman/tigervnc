/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
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
// ZlibInStream streams from a compressed data stream ("underlying"),
// decompressing with zlib on the fly.
//

#ifndef ZLIBINSTREAM_H
#define ZLIBINSTREAM_H

#include "vncstream.h"

struct z_stream_s;

class ZlibInStream : public QVNCStream {

public:
    ZlibInStream();
    virtual ~ZlibInStream();

    void setUnderlying(QVNCStream *is, size_t bytesIn);
    void flushUnderlying();
    void reset();

private:
    void init();
    void deinit();

    virtual bool fillBuffer();

private:
    QVNCStream *underlying;
    z_stream_s *zs;
    size_t bytesIn;
};

#endif // ZLIBINSTREAM_H
