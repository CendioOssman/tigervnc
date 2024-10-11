#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

//#include "rdr/types.h"
#include "rfb/Rect.h"
#include "CConn.h"
#include <QObject>

class QTimer;
class QCursor;
class QSocketNotifier;
class TunnelFactory;

namespace rdr
{
class InStream;
class OutStream;
} // namespace rdr

namespace rfb
{
class ServerParams;
class SecurityClient;
class PixelFormat;
class ModifiablePixelBuffer;
class CMsgReader;
class CMsgWriter;
struct ScreenSet;
struct Point;
} // namespace rfb
Q_DECLARE_METATYPE(rfb::ScreenSet)
Q_DECLARE_METATYPE(rfb::Point)

namespace network
{
class Socket;
}

class QVNCConnection : public QObject
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();

  rfb::ServerParams* server() { return rfbcon ? &rfbcon->server : nullptr; }

  void setState(int state);

  rdr::InStream* istream() { return rfbcon->getInStream(); }

  rdr::OutStream* ostream() { return rfbcon->getOutStream(); }

  rfb::CMsgReader* reader() { return rfbcon->reader(); }

  rfb::CMsgWriter* writer() { return rfbcon->writer(); }

  void setQualityLevel(int level) { rfbcon->setQualityLevel(level); }

  rfb::ModifiablePixelBuffer* framebuffer() { return rfbcon->framebuffer(); }

  void setCompressLevel(int level) { rfbcon->setCompressLevel(level); }

  QTimer* getUpdateTimer() const { return updateTimer; }

  void updatePixelFormat() { rfbcon->updatePixelFormat(); }

  void announceClipboard(bool available);

  void setPreferredEncoding(int encoding) { rfbcon->setPreferredEncoding(encoding); }

  bool hasConnection() const { return rfbcon; }

signals:
  void socketReadNotified();
  void socketWriteNotified();
  void cursorChanged(const QCursor& cursor);
  void cursorPositionChanged(int x, int y);
  void ledStateChanged(unsigned int state);
  void clipboardRequested();
  void clipboardAnnounced(bool available);
  void clipboardDataReceived(const char* data);
  void framebufferResized(int width, int height);
  void refreshFramebufferStarted();
  void refreshFramebufferEnded();
  void bellRequested();

  void writePointerEvent(const rfb::Point& pos, int buttonMask);
  void writeSetDesktopSize(int width, int height, const rfb::ScreenSet& layout);
  void writeKeyEvent(uint32_t keysym, uint32_t keycode, bool down);

public:
  void initialize();
  void listen();
  void connectToServer();
  void resetConnection();
  void startProcessing();
  void flushSocket();
  void refreshFramebuffer();
  void sendClipboardData(QString data);
  void requestClipboard();
  void sendKeyPress(int systemKeyCode, uint32_t keyCode, uint32_t keySym);
  void sendKeyRelease(int systemKeyCode);
  void releaseAllKeys();


  QString infoText() { return rfbcon ? rfbcon->connectionInfo() : ""; }

  QString host() { return rfbcon ? rfbcon->host() : ""; }

private:
  CConn* rfbcon;
  network::Socket* socket;
  QSocketNotifier* socketReadNotifier;
  QSocketNotifier* socketWriteNotifier;
  QTimer* updateTimer;
  TunnelFactory* tunnelFactory;

  void bind(int fd);

  void setHost(QString host)
  {
    rfbcon->setServerName(host.toStdString().c_str());
    rfbcon->setHost(host);
  }

  void setPort(int port) { rfbcon->setPort(port); }
};

#endif // VNCCONNECTION_H
