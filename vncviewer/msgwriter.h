#ifndef MSGWRITER_H
#define MSGWRITER_H

#include <list>
#include <rdr/types.h>

namespace rdr { class OutStream; }

namespace rfb {

  class PixelFormat;
  class ServerParams;
  struct ScreenSet;
  struct Point;
  struct Rect;
}

class QMsgWriter {
public:
  QMsgWriter(rfb::ServerParams* server, rdr::OutStream* os);
  virtual ~QMsgWriter();

  void writeClientInit(bool shared);

  void writeSetPixelFormat(const rfb::PixelFormat& pf);
  void writeSetEncodings(const std::list<rdr::U32> encodings);
  void writeSetDesktopSize(int width, int height, const rfb::ScreenSet& layout);

  void writeFramebufferUpdateRequest(const rfb::Rect& r,bool incremental);
  void writeEnableContinuousUpdates(bool enable, int x, int y, int w, int h);

  void writeFence(rdr::U32 flags, unsigned len, const char data[]);

  void writeKeyEvent(rdr::U32 keysym, rdr::U32 keycode, bool down);
  void writePointerEvent(const rfb::Point& pos, int buttonMask);

  void writeClientCutText(const char* str);

  void writeClipboardCaps(rdr::U32 caps, const rdr::U32* lengths);
  void writeClipboardRequest(rdr::U32 flags);
  void writeClipboardPeek(rdr::U32 flags);
  void writeClipboardNotify(rdr::U32 flags);
  void writeClipboardProvide(rdr::U32 flags, const size_t* lengths,
                             const rdr::U8* const* data);

protected:
  void startMsg(int type);
  void endMsg();

  rfb::ServerParams* server;
  rdr::OutStream* os;
};

#endif
