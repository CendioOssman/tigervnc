#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

#include <QThread>
#include <QProcess>
#include "rdr/types.h"
#include "rfb/Rect.h"
#include "CConn.h"

class QTimer;
class QCursor;
class QSocketNotifier;

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

class TunnelFactory : public QThread
{
  Q_OBJECT

public:
  TunnelFactory();
  virtual ~TunnelFactory();
  void close();
  bool errorOccurred() const { return m_errorOccurrrd; }
  QProcess::ProcessError error() const { return m_error; }

protected:
  void run() override;

private:
  bool m_errorOccurrrd;
  QProcess::ProcessError m_error;
  QString m_command;
#if !defined(WIN32)
  QString m_operationSocketName;
#endif
  QProcess *m_process;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  QStringList splitCommand(QStringView command);
#endif
};

class QVNCConnection : public QObject
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();
  rfb::ServerParams *server() { return m_rfbcon->server(); }
  void setState(int state);
  rdr::InStream *istream() { return m_rfbcon->getInStream(); }
  rdr::OutStream *ostream() { return m_rfbcon->getOutStream(); }
  rfb::CMsgReader* reader() { return m_rfbcon->reader(); }
  rfb::CMsgWriter* writer() { return m_rfbcon->writer(); }
  void setQualityLevel(int level) { m_rfbcon->setQualityLevel(level); }
  rfb::ModifiablePixelBuffer *framebuffer() { return m_rfbcon->framebuffer(); }
  void setCompressLevel(int level) { m_rfbcon->setCompressLevel(level); }
  QTimer *updateTimer() const { return m_updateTimer; }
  void updatePixelFormat() { m_rfbcon->updatePixelFormat(); }
  void announceClipboard(bool available);
  void setPreferredEncoding(int encoding) { m_rfbcon->setPreferredEncoding(encoding); }

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
  QString infoText() { return m_rfbcon->connectionInfo(); }
  QString host() { return m_rfbcon->host(); }

private:
  CConn *m_rfbcon;
  network::Socket *m_socket;
  QSocketNotifier *m_socketNotifier;
  QSocketNotifier *m_socketErrorNotifier;
  QTimer *m_updateTimer;
  TunnelFactory *m_tunnelFactory;
  bool m_closing;

  void bind(int fd);
  void setHost(QString host) { m_rfbcon->setHost(host); }
  void setPort(int port) { m_rfbcon->setPort(port); }
};

#endif // VNCCONNECTION_H
