#include <QMutex>
#include "msgwriter.h"

QMsgWriter::QMsgWriter(rfb::ServerParams *server, rdr::OutStream *os)
 : CMsgWriter(server, os)
 , m_mutex(new QMutex)
{
}

QMsgWriter::~QMsgWriter()
{
  delete m_mutex;
}

void QMsgWriter::writeClientInit(bool shared)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClientInit(shared);
}

void QMsgWriter::writeSetPixelFormat(const rfb::PixelFormat &pf)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeSetPixelFormat(pf);
}

void QMsgWriter::writeSetEncodings(const std::list<rdr::U32> encodings)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeSetEncodings(encodings);
}

void QMsgWriter::writeSetDesktopSize(int width, int height, const rfb::ScreenSet &layout)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeSetDesktopSize(width, height, layout);
}

void QMsgWriter::writeFramebufferUpdateRequest(const rfb::Rect &r, bool incremental)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeFramebufferUpdateRequest(r, incremental);
}

void QMsgWriter::writeEnableContinuousUpdates(bool enable, int x, int y, int w, int h)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeEnableContinuousUpdates(enable, x, y, w, h);
}

void QMsgWriter::writeFence(rdr::U32 flags, unsigned len, const char data[])
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeFence(flags, len, data);
}

void QMsgWriter::writeKeyEvent(rdr::U32 keysym, rdr::U32 keycode, bool down)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeKeyEvent(keysym, keycode, down);
}


void QMsgWriter::writePointerEvent(const rfb::Point &pos, int buttonMask)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writePointerEvent(pos, buttonMask);
}


void QMsgWriter::writeClientCutText(const char *str)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClientCutText(str);
}

void QMsgWriter::writeClipboardCaps(rdr::U32 caps, const rdr::U32 *lengths)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClipboardCaps(caps, lengths);
}

void QMsgWriter::writeClipboardRequest(rdr::U32 flags)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClipboardRequest(flags);
}

void QMsgWriter::writeClipboardPeek(rdr::U32 flags)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClipboardPeek(flags);
}

void QMsgWriter::writeClipboardNotify(rdr::U32 flags)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClipboardNotify(flags);
}

void QMsgWriter::writeClipboardProvide(rdr::U32 flags, const size_t *lengths, const rdr::U8* const *data)
{
  QMutexLocker locker(m_mutex);
  CMsgWriter::writeClipboardProvide(flags, lengths, data);
}
