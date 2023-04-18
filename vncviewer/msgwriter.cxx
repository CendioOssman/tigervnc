/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2009-2019 Pierre Ossman for Cendio AB
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

#include <QMutex>
#include <QDebug>

#include <stdio.h>

#include <rdr/OutStream.h>
#include <rdr/MemOutStream.h>
#include <rdr/ZlibOutStream.h>

#include <rfb/msgTypes.h>
#include <rfb/fenceTypes.h>
#include <rfb/qemuTypes.h>
#include <rfb/clipboardTypes.h>
#include <rfb/Exception.h>
#include <rfb/PixelFormat.h>
#include <rfb/Rect.h>
#include <rfb/ServerParams.h>
#include "msgwriter.h"

using namespace rfb;

QMsgWriter::QMsgWriter(ServerParams* server_, rdr::OutStream* os_)
  : server(server_), os(os_), m_mutex(new QMutex)
{
}

QMsgWriter::~QMsgWriter()
{
  delete m_mutex;
}

void QMsgWriter::writeClientInit(bool shared)
{
  QMutexLocker locker(m_mutex);
  os->writeU8(shared);
  endMsg();
}

void QMsgWriter::writeSetPixelFormat(const PixelFormat& pf)
{
  QMutexLocker locker(m_mutex);
  qDebug() << "QMsgWriter::writeSetPixelFormat";
  startMsg(msgTypeSetPixelFormat);
  os->pad(3);
  pf.write(os);
  endMsg();
}

void QMsgWriter::writeSetEncodings(const std::list<rdr::U32> encodings)
{
  QMutexLocker locker(m_mutex);
  qDebug() << "QMsgWriter::writeSetEncodings";
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  for (auto encoding : encodings) {
    qDebug().nospace() << "encoding=" << (int)encoding << ", 0x" << Qt::hex << encoding;
  }
#endif
  std::list<rdr::U32>::const_iterator iter;
  startMsg(msgTypeSetEncodings);
  os->pad(1);
  os->writeU16(encodings.size());
  for (iter = encodings.begin(); iter != encodings.end(); ++iter)
    os->writeU32(*iter);
  endMsg();
}

void QMsgWriter::writeSetDesktopSize(int width, int height,
                                     const ScreenSet& layout)
{
  if (!server->supportsSetDesktopSize)
    throw Exception("Server does not support SetDesktopSize");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeSetDesktopSize);
  os->pad(1);

  os->writeU16(width);
  os->writeU16(height);

  os->writeU8(layout.num_screens());
  os->pad(1);

  qDebug() << "QMsgWriter::writeSetDesktopSize: w=" << width << ", h=" << height;
  ScreenSet::const_iterator iter;
  for (iter = layout.begin();iter != layout.end();++iter) {
    qDebug() << "QMsgWriter::writeSetDesktopSize: id=" << iter->id << ", iter->dimensions.tl.x=" << iter->dimensions.tl.x << ", iter->dimensions.tl.y=" << iter->dimensions.tl.y << ", iter->dimensions.width()=" << iter->dimensions.width() << ", iter->dimensions.height()=" << iter->dimensions.height();
    os->writeU32(iter->id);
    os->writeU16(iter->dimensions.tl.x);
    os->writeU16(iter->dimensions.tl.y);
    os->writeU16(iter->dimensions.width());
    os->writeU16(iter->dimensions.height());
    os->writeU32(iter->flags);
  }

  endMsg();
}

void QMsgWriter::writeFramebufferUpdateRequest(const Rect& r, bool incremental)
{
  QMutexLocker locker(m_mutex);
  qDebug() << "QMsgWriter::writeFramebufferUpdateRequest: x=" << r.tl.x << ", y=" << r.tl.y << ", w=" << (r.br.x-r.tl.x) << ", h=" << (r.br.y-r.tl.y);
  startMsg(msgTypeFramebufferUpdateRequest);
  os->writeU8(incremental);
  os->writeU16(r.tl.x);
  os->writeU16(r.tl.y);
  os->writeU16(r.width());
  os->writeU16(r.height());
  endMsg();
}

void QMsgWriter::writeEnableContinuousUpdates(bool enable,
                                              int x, int y, int w, int h)
{
  qDebug() << "QMsgWriter::writeEnableContinuousUpdates";
  if (!server->supportsContinuousUpdates)
    throw Exception("Server does not support continuous updates");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeEnableContinuousUpdates);

  os->writeU8(!!enable);

  os->writeU16(x);
  os->writeU16(y);
  os->writeU16(w);
  os->writeU16(h);

  endMsg();
}

void QMsgWriter::writeFence(rdr::U32 flags, unsigned len, const char data[])
{
  qDebug() << "QMsgWriter::writeFence";
  if (!server->supportsFence)
    throw Exception("Server does not support fences");
  if (len > 64)
    throw Exception("Too large fence payload");
  if ((flags & ~fenceFlagsSupported) != 0)
    throw Exception("Unknown fence flags");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeClientFence);
  os->pad(3);

  os->writeU32(flags);

  os->writeU8(len);
  os->writeBytes(data, len);

  endMsg();
}

void QMsgWriter::writeKeyEvent(rdr::U32 keysym, rdr::U32 keycode, bool down)
{
  qDebug() << "QMsgWriter::writeKeyEvent";
  if (!server->supportsQEMUKeyEvent || !keycode) {
    QMutexLocker locker(m_mutex);
    /* This event isn't meaningful without a valid keysym */
    if (!keysym)
      return;

    startMsg(msgTypeKeyEvent);
    os->writeU8(down);
    os->pad(2);
    os->writeU32(keysym);
    endMsg();
  } else {
    startMsg(msgTypeQEMUClientMessage);
    os->writeU8(qemuExtendedKeyEvent);
    os->writeU16(down);
    os->writeU32(keysym);
    os->writeU32(keycode);
    endMsg();
  }
}


void QMsgWriter::writePointerEvent(const Point& pos, int buttonMask)
{
  QMutexLocker locker(m_mutex);
  qDebug() << "QMsgWriter::writePointerEvent";
  Point p(pos);
  if (p.x < 0) p.x = 0;
  if (p.y < 0) p.y = 0;
  if (p.x >= server->width()) p.x = server->width() - 1;
  if (p.y >= server->height()) p.y = server->height() - 1;

  startMsg(msgTypePointerEvent);
  os->writeU8(buttonMask);
  os->writeU16(p.x);
  os->writeU16(p.y);
  endMsg();
}


void QMsgWriter::writeClientCutText(const char* str)
{
  qDebug() << "QMsgWriter::writeClientCutText";
  size_t len;

  if (strchr(str, '\r') != NULL)
    throw Exception("Invalid carriage return in clipboard data");

  QMutexLocker locker(m_mutex);
  len = strlen(str);
  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeU32(len);
  os->writeBytes(str, len);
  endMsg();
}

void QMsgWriter::writeClipboardCaps(rdr::U32 caps,
                                    const rdr::U32* lengths)
{
  qDebug() << "QMsgWriter::writeClipboardCaps";

  if (!(server->clipboardFlags() & clipboardCaps))
    throw Exception("Server does not support clipboard \"caps\" action");

  QMutexLocker locker(m_mutex);
  size_t count = 0;
  for (size_t i = 0;i < 16;i++) {
    if (caps & (1 << i))
      count++;
  }

  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeS32(-(4 + 4 * count));

  os->writeU32(caps | clipboardCaps);

  count = 0;
  for (size_t i = 0;i < 16;i++) {
    if (caps & (1 << i))
      os->writeU32(lengths[count++]);
  }

  endMsg();
}

void QMsgWriter::writeClipboardRequest(rdr::U32 flags)
{
  qDebug() << "QMsgWriter::writeClipboardRequest";
  if (!(server->clipboardFlags() & clipboardRequest))
    throw Exception("Server does not support clipboard \"request\" action");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeS32(-4);
  os->writeU32(flags | clipboardRequest);
  endMsg();
}

void QMsgWriter::writeClipboardPeek(rdr::U32 flags)
{
  qDebug() << "QMsgWriter::writeClipboardPeek";
  if (!(server->clipboardFlags() & clipboardPeek))
    throw Exception("Server does not support clipboard \"peek\" action");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeS32(-4);
  os->writeU32(flags | clipboardPeek);
  endMsg();
}

void QMsgWriter::writeClipboardNotify(rdr::U32 flags)
{
  qDebug() << "QMsgWriter::writeClipboardNotify";
  if (!(server->clipboardFlags() & clipboardNotify))
    throw Exception("Server does not support clipboard \"notify\" action");

  QMutexLocker locker(m_mutex);
  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeS32(-4);
  os->writeU32(flags | clipboardNotify);
  endMsg();
}

void QMsgWriter::writeClipboardProvide(rdr::U32 flags,
                                      const size_t* lengths,
                                      const rdr::U8* const* data)
{
  qDebug() << "QMsgWriter::writeClipboardProvide";
  rdr::MemOutStream mos;
  rdr::ZlibOutStream zos;

  int i, count;

  if (!(server->clipboardFlags() & clipboardProvide))
    throw Exception("Server does not support clipboard \"provide\" action");

  QMutexLocker locker(m_mutex);
  zos.setUnderlying(&mos);

  count = 0;
  for (i = 0;i < 16;i++) {
    if (!(flags & (1 << i)))
      continue;
    zos.writeU32(lengths[count]);
    zos.writeBytes(data[count], lengths[count]);
    count++;
  }

  zos.flush();

  startMsg(msgTypeClientCutText);
  os->pad(3);
  os->writeS32(-(4 + mos.length()));
  os->writeU32(flags | clipboardProvide);
  os->writeBytes(mos.data(), mos.length());
  endMsg();
}

void QMsgWriter::startMsg(int type)
{
  os->writeU8(type);
}

void QMsgWriter::endMsg()
{
  os->flush();
}
