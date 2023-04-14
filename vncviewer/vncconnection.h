#ifndef VNCCONNECTION_H
#define VNCCONNECTION_H

#include <QThread>
#include "rdr/types.h"
#include "rfb/Rect.h"

class QTimer;
class QIODevice;
class QMutex;
class QVNCStream;
class QSocketNotifier;
class QMsgReader;
class QMsgWriter;
class QVNCPacketHandler;
struct VeNCryptStatus;
class DecodeManager;

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
  class ScreenSet;
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
  rfb::ServerParams *server() const { return m_serverParams; }
  void setState(int state);
  void serverInit(int width, int height, const rfb::PixelFormat& pf, const char* name);
  rdr::InStream *istream() { return m_istream; }
  rdr::OutStream *ostream() { return m_ostream; }
  QMsgReader* reader() { return m_reader; }
  QMsgWriter* writer() { return m_writer; }
  QString *user() { return m_user; }
  rfb::PlainPasswd *password() { return m_password; }
  void autoSelectFormatAndEncoding();
  void setQualityLevel(int level);
  rfb::ModifiablePixelBuffer *framebuffer();
  void setCompressLevel(int level);
  QCursor *cursor() const { return m_cursor; }

  // CMsgHandler.h
  void supportsQEMUKeyEvent();
    
  // CConn.h
  void resizeFramebuffer();
  void setDesktopSize(int w, int h);
  void setExtendedDesktopSize(unsigned reason, unsigned result, int w, int h, const rfb::ScreenSet& layout);
  void setName(const char* name);
  void setColourMapEntries(int firstColour, int nColours, rdr::U16* rgbs);
  void bell();
  void framebufferUpdateStart();
  void framebufferUpdateEnd();
  bool dataRect(const rfb::Rect& r, int encoding);
  void setCursor(int width, int height, const rfb::Point& hotspot, const unsigned char* data);
  void setCursorPos(const rfb::Point& pos);
  void fence(rdr::U32 flags, unsigned len, const char data[]);
  void setLEDState(unsigned int state);
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);
  void updatePixelFormat();


  // CConnection.h
  void setFramebuffer(rfb::ModifiablePixelBuffer* fb);
  void endOfContinuousUpdates();
  bool readAndDecodeRect(const rfb::Rect& r, int encoding, rfb::ModifiablePixelBuffer* pb);
  void serverCutText(const char* str);
  void handleClipboardCaps(rdr::U32 flags, const rdr::U32* lengths);\
  void handleClipboardRequest();
  void handleClipboardRequest(rdr::U32 flags);
  void handleClipboardPeek(rdr::U32 flags);
  void handleClipboardNotify(rdr::U32 flags);
  void handleClipboardProvide(rdr::U32 flags, const size_t* lengths, const rdr::U8* const* data);
  void setPreferredEncoding(int encoding);


signals:
  void socketNotified();
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void newVncWindowRequested(int width, int height, QString name);
  void cursorChanged(const QCursor &cursor);
  void cursorPositionChanged(int x, int y);
  void ledStateChanged(unsigned int state);
  void clipboardAnnounced(bool available);
  void clipboardChanged(const char *data);

public slots:
  void connectToServer(const QString addressport);
  bool authenticate(QString user, QString password);
  void resetConnection();
  void startProcessing();
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
  QMsgReader *m_reader;
  QMsgWriter *m_writer;
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
  DecodeManager *m_decoder;
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
