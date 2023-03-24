#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

#include <QThread>
#include "vncpackethandler.h"

class QTimer;
class QIODevice;
class QMutex;
class QVNCStream;
class QSocketNotifier;
class QMsgReader;
class QMsgWriter;
struct VeNCryptStatus;

namespace rdr {
  class FdInStream;
  class FdOutStream;
}
namespace rfb {
  class ServerParams;
  class SecurityClient;
  class PixelFormat;
  class ModifiablePixelBuffer;
  class PlainPasswd;
}
namespace network {
  class Socket;
}

class QVNCConnection : public QThread
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();
  QVNCPacketHandler *setPacketHandler(QVNCPacketHandler *handler);
  bool isSecure() const { return m_secured; }
  QString host() const { return m_host; }
  int port() const { return m_port; }
  void setState(int state);
  void serverInit(int width, int height, const rfb::PixelFormat& pf, const char* name);
  rdr::FdInStream *istream() { return m_istream; }
  rdr::FdOutStream *ostream() { return m_ostream; }
  QString *user() { return m_user; }
  rfb::PlainPasswd *password() { return m_password; }

signals:
  void socketNotified();
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void newVncWindowRequested(int width, int height, QString name);

public slots:
  void connectToServer(const QString addressport);
  bool authenticate(QString user, QString password);
  void startProcessing();

private:
  bool m_inProcessing;
  bool m_blocking;
  QMutex *m_mutex;
  network::Socket *m_socket;
  bool m_alive;
  bool m_secured;
  QString m_host;
  int m_port;
  int m_shared;
  int m_state;
  rfb::ServerParams *m_serverParams;
  rfb::SecurityClient *m_security;
  int m_securityType;
  QSocketNotifier *m_socketNotifier;
  QSocketNotifier *m_socketErrorNotifier;
  QVNCPacketHandler *m_packetHandler;
  VeNCryptStatus *m_encStatus;
  rdr::FdInStream *m_istream;
  rdr::FdOutStream *m_ostream;
  QMsgReader *m_reader;
  QMsgWriter *m_writer;
  bool m_pendingPFChange;
  rfb::PixelFormat *m_pendingPF;
  rfb::PixelFormat *m_serverPF;
  int m_preferredEncoding;
  bool m_encodingChange;
  rfb::ModifiablePixelBuffer* m_framebuffer;
  QTimer *m_timer;
  bool m_pendingSocketEvent;
  QString *m_user;
  rfb::PlainPasswd *m_password;

  bool processMsg(int state);
  void bind(int fd);
  void setStreams(rdr::FdInStream *in, rdr::FdOutStream *out);
  bool processVersionMsg();
  bool processSecurityTypesMsg();
  bool processSecurityMsg();
  bool processSecurityResultMsg();
  bool processSecurityReasonMsg();
  bool processInitMsg();
  void securityCompleted();
  void initDone();
  void setPreferredEncoding(int encoding);
  void requestNewUpdate();
  void updatePixelFormat();
  void authSuccess();
  bool getCredentialProperties(bool &userNeeded, bool &passwordNeeded);
  bool getVeNCryptCredentialProperties(bool &userNeeded, bool &passwordNeeded);
  bool establishSecurityLayer(int securitySubType);
  void setBlocking(bool blocking);
  bool blocking();
};

#endif // VNCCONNECTION_H
