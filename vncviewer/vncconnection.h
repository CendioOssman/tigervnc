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

  void setQualityLevel(int level) { rfbcon->setQualityLevel(level); }

  void setCompressLevel(int level) { rfbcon->setCompressLevel(level); }

  QTimer* getUpdateTimer() const { return updateTimer; }

  void updatePixelFormat() { rfbcon->updatePixelFormat(); }

  void setPreferredEncoding(int encoding) { rfbcon->setPreferredEncoding(encoding); }

  bool hasConnection() const { return rfbcon; }

signals:
  void socketReadNotified();
  void socketWriteNotified();

public:
  void initialize();
  void listen();
  void connectToServer();
  void resetConnection();
  void startProcessing();
  void flushSocket();

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
