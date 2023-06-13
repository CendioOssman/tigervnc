#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

#include "rdr/types.h"
#include "rfb/Rect.h"
#include "CConn.h"

class QTimer;
class QCursor;
class QSocketNotifier;
class TunnelFactory;

namespace rdr {
  class InStream;
  class OutStream;
}
namespace rfb {
  class ServerParams;
  class SecurityClient;
  class PixelFormat;
  class ModifiablePixelBuffer;
  class DecodeManager;
  class CMsgReader;
  class CMsgWriter;
  struct ScreenSet;
  struct Point;
}
Q_DECLARE_METATYPE(rfb::ScreenSet)
Q_DECLARE_METATYPE(rfb::Point)
namespace network {
  class Socket;
}

class QVNCConnection : public QObject
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();
  rfb::ServerParams *server() { return &rfbcon_->server; }
  void setState(int state);
  rdr::InStream *istream() { return rfbcon_->getInStream(); }
  rdr::OutStream *ostream() { return rfbcon_->getOutStream(); }
  rfb::CMsgReader* reader() { return rfbcon_->reader(); }
  rfb::CMsgWriter* writer() { return rfbcon_->writer(); }
  void setQualityLevel(int level) { rfbcon_->setQualityLevel(level); }
  rfb::ModifiablePixelBuffer *framebuffer() { return rfbcon_->framebuffer(); }
  void setCompressLevel(int level) { rfbcon_->setCompressLevel(level); }
  QTimer *updateTimer() const { return updateTimer_; }
  void updatePixelFormat() { rfbcon_->updatePixelFormat(); }
  void announceClipboard(bool available);
  void setPreferredEncoding(int encoding) { rfbcon_->setPreferredEncoding(encoding); }

signals:
  void socketNotified();
  void newVncWindowRequested(int width, int height, QString name);
  void cursorChanged(const QCursor &cursor);
  void cursorPositionChanged(int x, int y);
  void ledStateChanged(unsigned int state);
  void clipboardAnnounced(bool available);
  void clipboardDataReceived(const char *data);
  void framebufferResized(int width, int height);
  void refreshFramebufferStarted();
  void refreshFramebufferEnded();
  void bellRequested();

  void writePointerEvent(const rfb::Point &pos, int buttonMask);
  void writeSetDesktopSize(int width, int height, const rfb::ScreenSet &layout);
  void writeKeyEvent(rdr::U32 keysym, rdr::U32 keycode, bool down);

public slots:
  void listen();
  void connectToServer(const QString addressport);
  void resetConnection();
  void startProcessing();
  void refreshFramebuffer();
  QString infoText() { return rfbcon_->connectionInfo(); }
  QString host() { return rfbcon_->host(); }

private:
  CConn *rfbcon_;
  network::Socket *socket_;
  QSocketNotifier *socketNotifier_;
  QSocketNotifier *socketErrorNotifier_;
  QTimer *updateTimer_;
  TunnelFactory *tunnelFactory_;
  bool closing_;

  void bind(int fd);
  void setHost(QString host) { rfbcon_->setHost(host); }
  void setPort(int port) { rfbcon_->setPort(port); }
};

#endif // VNCCONNECTION_H
