#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

#include <QThread>
#include <QProcess>
#include "rdr/types.h"
#include "rfb/Rect.h"
#include "rfb/CMsgHandler.h"

class QTimer;
class QIODevice;
class QMutex;
class QSocketNotifier;
class QVNCPacketHandler;
struct VeNCryptStatus;

namespace rdr {
  class InStream;
  class OutStream;
}
namespace rfb {
  class ServerParams;
  class SecurityClient;
  class PixelFormat;
  class ModifiablePixelBuffer;
  class PlainPasswd;
  struct ScreenSet;
  class DecodeManager;
  class CMsgReader;
  class CMsgWriter;
}
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
};

class QVNCConnection : public QThread, public rfb::CMsgHandler
{
  Q_OBJECT

public:
  QVNCConnection();
  virtual ~QVNCConnection();
  QVNCPacketHandler *setPacketHandler(QVNCPacketHandler *handler);
  bool isSecure() const { return m_secured; }
  QString host() const { return m_host; }
  int port() const { return m_port; }
  rfb::ServerParams *server() override { return m_serverParams; }
  void setState(int state);
  void serverInit(int width, int height, const rfb::PixelFormat& pf, const char* name) override;
  rdr::InStream *istream() { return m_istream; }
  rdr::OutStream *ostream() { return m_ostream; }
  rfb::CMsgReader* reader() { return m_reader; }
  rfb::CMsgWriter* writer() { return m_writer; }
  QString *user() { return m_user; }
  rfb::PlainPasswd *password() { return m_password; }
  void autoSelectFormatAndEncoding();
  void setQualityLevel(int level);
  rfb::ModifiablePixelBuffer *framebuffer();
  void setCompressLevel(int level);
  QCursor *cursor() const { return m_cursor; }
  void exit(int errorCode = 0);

  // missing methods for CMsgHandler.
  void setPixelFormat(const rfb::PixelFormat& pf) {}

  // CMsgHandler.h
  void supportsQEMUKeyEvent() override;
    
  // CConn.h
  void resizeFramebuffer();
  void setDesktopSize(int w, int h) override;
  void setExtendedDesktopSize(unsigned reason, unsigned result, int w, int h, const rfb::ScreenSet& layout) override;
  void setName(const char* name) override;
  void setColourMapEntries(int firstColour, int nColours, rdr::U16* rgbs) override;
  void bell() override;
  void framebufferUpdateStart() override;
  void framebufferUpdateEnd() override;
  bool dataRect(const rfb::Rect& r, int encoding) override;
  void setCursor(int width, int height, const rfb::Point& hotspot, const unsigned char* data) override;
  void setCursorPos(const rfb::Point& pos) override;
  void fence(rdr::U32 flags, unsigned len, const char data[]) override;
  void setLEDState(unsigned int state) override;
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);
  void updatePixelFormat();

  // CConnection.h
  void setFramebuffer(rfb::ModifiablePixelBuffer* fb);
  void endOfContinuousUpdates() override;
  bool readAndDecodeRect(const rfb::Rect& r, int encoding, rfb::ModifiablePixelBuffer* pb) override;
  void serverCutText(const char* str) override;
  void handleClipboardCaps(rdr::U32 flags, const rdr::U32* lengths) override;
  // Server requesting client to send client's clipboard data.
  void handleClipboardRequest(rdr::U32 flags) override;
  void handleClipboardPeek(rdr::U32 flags) override;
  void handleClipboardNotify(rdr::U32 flags) override;
  // Process clipboard data received from the server.
  void handleClipboardProvide(rdr::U32 flags, const size_t* lengths, const rdr::U8* const* data) override;
  void setPreferredEncoding(int encoding);

signals:
  void socketNotified();
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
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

public slots:
  void listen();
  void connectToServer(const QString addressport);
  bool authenticate(QString user, QString password);
  void resetConnection();
  void startProcessing();
  void sendClipboardData(QString text);
  void refreshFramebuffer();
  QString infoText();
  void requestClipboard();
  void updateEncodings();

protected:
  void run() override;

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
  rdr::InStream *m_istream;
  rdr::OutStream *m_ostream;
  rfb::CMsgReader *m_reader;
  rfb::CMsgWriter *m_writer;
  bool m_pendingPFChange;
  unsigned m_updateCount;
  unsigned m_pixelCount;
  rfb::PixelFormat *m_pendingPF;
  rfb::PixelFormat *m_serverPF;
  rfb::PixelFormat *m_fullColourPF;
  rfb::PixelFormat *m_nextPF;
  int m_preferredEncoding;
  int m_compressLevel;
  int m_qualityLevel;
  bool m_encodingChange;
  bool m_firstUpdate;
  bool m_pendingUpdate;
  bool m_continuousUpdates;
  bool m_forceNonincremental;
  rfb::ModifiablePixelBuffer *m_framebuffer;
  rfb::DecodeManager *m_decoder;
  QByteArray m_serverClipboard;
  bool m_hasLocalClipboard;
  bool m_unsolicitedClipboardAttempt;
  bool m_pendingSocketEvent;
  QString *m_user;
  rfb::PlainPasswd *m_password;
  bool m_formatChange;
  // Optional capabilities that a subclass is expected to set to true
  // if supported
  bool m_supportsLocalCursor;
  bool m_supportsCursorPosition;
  bool m_supportsDesktopResize;
  bool m_supportsLEDState;
  int m_lastServerEncoding;
  struct timeval m_updateStartTime;
  size_t m_updateStartPos;
  unsigned long long m_bpsEstimate;
  QTimer *m_updateTimer;
  QCursor *m_cursor;

  TunnelFactory *m_tunnelFactory;
  bool m_closing;

  bool processMsg(int state);
  void bind(int fd);
  void setStreams(rdr::InStream *in, rdr::OutStream *out);
  bool processVersionMsg();
  bool processSecurityTypesMsg();
  bool processSecurityMsg();
  bool processSecurityResultMsg();
  bool processSecurityReasonMsg();
  bool processInitMsg();
  void securityCompleted();
  void initDone();
  void requestNewUpdate();
  void setPF(const rfb::PixelFormat *pf);
  void authSuccess();
  bool getCredentialProperties(bool &userNeeded, bool &passwordNeeded);
  bool getVeNCryptCredentialProperties(bool &userNeeded, bool &passwordNeeded);
  bool establishSecurityLayer(int securitySubType);
  void setBlocking(bool blocking);
  bool blocking();
};

#endif // VNCCONNECTION_H
