#include <QGuiApplication>
#include <QQmlEngine>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QMutexLocker>
#include <QTimer>
#include <QCursor>
#include <time.h>
#include "rfb/Hostname.h"
#include "rfb/CConnection.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/Security.h"
#include "rfb/PixelFormat.h"
#include "rfb/PixelBuffer.h"
#include "rfb/Password.h"
#include "rfb/d3des.h"
#include "rfb/encodings.h"
#include "rfb/Decoder.h"
#include "rfb/fenceTypes.h"
#include "rfb/screenTypes.h"
#include "rfb/clipboardTypes.h"
#include "rfb/DecodeManager.h"
#include "rfb/encodings.h"
#include "vncstream.h"
#include "viewerconfig.h"
#include "vncconnection.h"
#include "parameters.h"
#include "msgreader.h"
#include "msgwriter.h"
#include "vncpackethandler.h"
#include <QSocketNotifier>
#include "network/TcpSocket.h"
#include "DecodeManager.h"
#include "PlatformPixelBuffer.h"
#include "i18n.h"
#include "abstractvncview.h"
#include "appmanager.h"

#if !defined(Q_OS_WIN)
  #include "network/UnixSocket.h"
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    #include <QX11Info>
  #endif
#endif

static rfb::LogWriter vlog("CConnection");

// 8 colours (1 bit per component)
static const rfb::PixelFormat verylowColourPF(8, 3,false, true, 1, 1, 1, 2, 1, 0);
// 64 colours (2 bits per component)
static const rfb::PixelFormat lowColourPF(8, 6, false, true, 3, 3, 3, 4, 2, 0);
// 256 colours (2-3 bits per component)
static const rfb::PixelFormat mediumColourPF(8, 8, false, true, 7, 7, 3, 5, 2, 0);

// Time new bandwidth estimates are weighted against (in ms)
static const unsigned bpsEstimateWindow = 1000;

class QVncAuthHandler : public QVNCPacketHandler
{
public:
  QVncAuthHandler(QVNCConnection *cc = nullptr)
    : QVNCPacketHandler(cc)
  {
  }
  bool processMsg(int state) override
  {
    const int vncAuthChallengeSize = 16;
    if (state != rfb::CConnection::RFBSTATE_SECURITY) {
      m_cc->resetConnection();
      throw rdr::Exception("QVncAuthHandler: state error.");
    }
    rfb::PlainPasswd *passwd = m_cc->password();
    if (!passwd) {
      return false;
    }
    if (!m_cc->istream()->hasData(vncAuthChallengeSize)) {
      return false;
    }

    // Read the challenge & obtain the user's password
    rdr::U8 challenge[vncAuthChallengeSize];
    m_cc->istream()->readBytes(challenge, vncAuthChallengeSize);

    // Calculate the correct response
    rdr::U8 key[8];
    int pwdLen = strlen(passwd->buf);
    for (int i=0; i<8; i++)
      key[i] = i<pwdLen ? passwd->buf[i] : 0;
    deskey(key, EN0);
    for (int j = 0; j < vncAuthChallengeSize; j += 8)
      des(challenge+j, challenge+j);

    // Return the response to the server
    m_cc->ostream()->writeBytes(challenge, vncAuthChallengeSize);
    m_cc->ostream()->flush();

    QVNCPacketHandler *handler0 = m_cc->setPacketHandler(nullptr);
    m_cc->setState(rfb::CConnection::RFBSTATE_SECURITY_RESULT);
    if (handler0) {
      handler0->deleteLater();
    }
    return true;
  }
};

class QPlainAuthHandler : public QVNCPacketHandler
{
public:
  QPlainAuthHandler(QVNCConnection *cc = nullptr)
    : QVNCPacketHandler(cc)
  {
  }
  bool processMsg(int state) override
  {
    if (state != rfb::CConnection::RFBSTATE_SECURITY) {
      m_cc->resetConnection();
      throw rdr::Exception("QVncAuthHandler: state error.");
    }
    QString *user = m_cc->user();
    if (!user) {
      return false;
    }
    rfb::PlainPasswd *password = m_cc->password();
    if (!password) {
      return false;
    }

    // Return the response to the server
    std::string username = user->toStdString();
    m_cc->ostream()->writeU32(username.size());
    m_cc->ostream()->writeU32(strlen(password->buf));
    m_cc->ostream()->writeBytes(username.c_str(),username.size());
    m_cc->ostream()->writeBytes(password->buf,strlen(password->buf));
    m_cc->ostream()->flush();

    QVNCPacketHandler *handler0 = m_cc->setPacketHandler(nullptr);
    m_cc->setState(rfb::CConnection::RFBSTATE_SECURITY_RESULT);
    if (handler0) {
      handler0->deleteLater();
    }
    return true;
  }
};

struct VeNCryptStatus {
  VeNCryptStatus()
    : haveRecvdMajorVersion(0)
    , haveRecvdMinorVersion(0)
    , haveSentVersion(0)
    , haveAgreedVersion(0)
    , haveListOfTypes(0)
    , haveNumberOfTypes(0)
    , majorVersion(0)
    , minorVersion(0)
    , chosenType(rfb::secTypeVeNCrypt)
    , nAvailableTypes(0)
    , availableTypes(nullptr)
  {
  }

  ~VeNCryptStatus()
  {
    delete availableTypes;
  }

  bool haveRecvdMajorVersion;
  bool haveRecvdMinorVersion;
  bool haveSentVersion;
  bool haveAgreedVersion;
  bool haveListOfTypes;
  bool haveNumberOfTypes;
  rdr::U8 majorVersion;
  rdr::U8 minorVersion;
  rdr::U32 chosenType;
  rdr::U8 nAvailableTypes;
  rdr::U32 *availableTypes;
};

QVNCConnection::QVNCConnection()
  : QThread(nullptr)
  , m_inProcessing(false)
  , m_blocking(false)
  , m_mutex(new QMutex)
  , m_socket(nullptr)
  , m_alive(false)
  , m_secured(false)
  , m_host("")
  , m_port(5900)
  , m_shared(::shared)
  , m_state(rfb::CConnection::RFBSTATE_PROTOCOL_VERSION)
  , m_serverParams(new rfb::ServerParams())
  , m_security(new rfb::SecurityClient())
  , m_securityType(rfb::secTypeInvalid)
  , m_socketNotifier(nullptr)
  , m_socketErrorNotifier(nullptr)
  , m_packetHandler(nullptr)
  , m_encStatus(new VeNCryptStatus)
  , m_istream(nullptr)
  , m_ostream(nullptr)
  , m_reader(nullptr)
  , m_writer(nullptr)
  , m_pendingPFChange(false)
  , m_updateCount(0)
  , m_pixelCount(0)
  , m_pendingPF(new rfb::PixelFormat)
  , m_serverPF(new rfb::PixelFormat)
  , m_fullColourPF(new rfb::PixelFormat(32, 24, false, true, 255, 255, 255, 16, 8, 0))
  , m_nextPF(new rfb::PixelFormat)
  , m_preferredEncoding(rfb::encodingTight)
  , m_compressLevel(2)
  , m_qualityLevel(-1)
  , m_encodingChange(false)
  , m_firstUpdate(true)
  , m_pendingUpdate(false)
  , m_continuousUpdates(false)
  , m_forceNonincremental(true)
  , m_framebuffer(nullptr)
  , m_decoder(new DecodeManager(this))
  , m_hasLocalClipboard(false)
  , m_unsolicitedClipboardAttempt(false)
  , m_pendingSocketEvent(false)
  , m_user(nullptr)
  , m_password(nullptr)
  , m_formatChange(false)
  , m_supportsLocalCursor(true)
  , m_supportsCursorPosition(true)
  , m_supportsDesktopResize(true)
  , m_supportsLEDState(true)
  , m_lastServerEncoding((unsigned int)-1)
  , m_updateStartPos(0)
  , m_bpsEstimate(20000000)
  , m_updateTimer(nullptr)
  , m_cursor(nullptr)
{
  moveToThread(this);
  connect(this, &QVNCConnection::socketNotified, this, &QVNCConnection::startProcessing);

  if (customCompressLevel) {
    setCompressLevel(::compressLevel);
  }

  if (!::noJpeg) {
    setQualityLevel(::qualityLevel);
  }
}

QVNCConnection::~QVNCConnection()
{
  resetConnection();
  m_updateTimer->stop();
  delete m_updateTimer;
  delete m_mutex;
  delete m_serverParams;
  delete m_security;
  delete m_encStatus;
  delete m_pendingPF;
  delete m_serverPF;
  delete m_fullColourPF;
  delete m_nextPF;
  delete m_cursor;
}

void QVNCConnection::run()
{
  m_updateTimer = new QTimer;
  m_updateTimer->setSingleShot(true);
  connect(m_updateTimer, &QTimer::timeout, this, []() {
    AppManager::instance()->view()->handleDesktopSize();
  });
  m_updateTimer->moveToThread(this);

  exec();
}

void QVNCConnection::bind(int fd)
{
  setStreams(&m_socket->inStream(), &m_socket->outStream());

  delete m_socketNotifier;
  m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(m_socketNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketNotified();
  });

  delete m_socketErrorNotifier;
  m_socketErrorNotifier = new QSocketNotifier(fd, QSocketNotifier::Exception);
  QObject::connect(m_socketErrorNotifier, &QSocketNotifier::activated, this, [](int fd) {
    Q_UNUSED(fd)
    throw rdr::Exception("CConnection::bind: socket error.");
  });
}

void QVNCConnection::setStreams(rdr::InStream *in, rdr::OutStream *out)
{
  m_istream = in;
  m_ostream = out;
}

void QVNCConnection::setFramebuffer(rfb::ModifiablePixelBuffer* fb)
{
  m_decoder->flush();

  if (fb) {
    assert(fb->width() == server()->width());
    assert(fb->height() == server()->height());
  }

  if ((m_framebuffer != NULL) && (fb != NULL)) {
    rfb::Rect rect;

    const rdr::U8* data;
    int stride;

    const rdr::U8 black[4] = { 0, 0, 0, 0 };

    // Copy still valid area

    rect.setXYWH(0, 0, __rfbmin(fb->width(), m_framebuffer->width()), __rfbmin(fb->height(), m_framebuffer->height()));
    data = m_framebuffer->getBuffer(m_framebuffer->getRect(), &stride);
    fb->imageRect(rect, data, stride);

    // Black out any new areas

    if (fb->width() > m_framebuffer->width()) {
      rect.setXYWH(m_framebuffer->width(), 0,
                   fb->width() - m_framebuffer->width(),
                   fb->height());
      fb->fillRect(rect, black);
    }

    if (fb->height() > m_framebuffer->height()) {
      rect.setXYWH(0, m_framebuffer->height(),
                   fb->width(),
                   fb->height() - m_framebuffer->height());
      fb->fillRect(rect, black);
    }
  }

  delete m_framebuffer;
  m_framebuffer = fb;
}

QVNCPacketHandler *QVNCConnection::setPacketHandler(QVNCPacketHandler *handler)
{
  QVNCPacketHandler *handler0 = m_packetHandler;
  m_packetHandler = handler;
  return handler0;
}

void QVNCConnection::connectToServer(const QString addressport)
{
  ViewerConfig::config()->saveViewerParameters("", addressport);
  if (addressport.contains("/")) {
#ifndef Q_OS_WIN
    delete m_socket;
    m_socket = new network::UnixSocket(addressport.toStdString().c_str());
    m_host = m_socket->getPeerAddress();
    vlog.info("Connected to socket %s", m_host.toStdString().c_str());
    bind(m_socket->getFd());
    delete m_encStatus;
    m_encStatus = new VeNCryptStatus;
#endif
  }
  else {
    std::string shost;
    int port;
    rfb::getHostAndPort(addressport.toStdString().c_str(), &shost, &port);
    m_host = shost.c_str();
    m_port = port;
    delete m_socket;
    m_socket = new network::TcpSocket(shost.c_str(), port);
    bind(m_socket->getFd());
    delete m_encStatus;
    m_encStatus = new VeNCryptStatus;
  }
}

bool QVNCConnection::authenticate(QString user, QString password)
{
  delete m_user;
  if (user.length() > 0) {
    m_user = new QString(user);
  }
  delete m_password;
  if (password.length() > 0) {
    m_password = new rfb::PlainPasswd(strdup(password.toStdString().c_str()));
  }
  setBlocking(false);
  return true;
}

void QVNCConnection::resetConnection()
{
  QMutexLocker locker(m_mutex);

  if (m_socket) {
    m_socket->shutdown();
  }
  delete m_socket;
  m_socket = nullptr;
  delete m_socketNotifier;
  m_socketNotifier = nullptr;
  delete m_socketErrorNotifier;
  m_socketErrorNotifier = nullptr;
  delete m_reader;
  m_reader = nullptr;
  delete m_writer;
  m_writer = nullptr;
  delete m_user;
  m_user = nullptr;
  delete m_password;
  m_password = nullptr;
  delete m_framebuffer;
  m_framebuffer = nullptr;
  delete m_packetHandler;
  m_packetHandler = nullptr;
  delete m_cursor;
  m_cursor = nullptr;

  m_state = rfb::CConnection::RFBSTATE_PROTOCOL_VERSION;
  m_inProcessing = false;
  m_blocking = false;
}

void QVNCConnection::refreshFramebuffer()
{
  qDebug() << "QVNCConnection::refreshFramebuffer: m_continuousUpdates=" << m_continuousUpdates;
  m_forceNonincremental = true;

  // Without continuous updates we have to make sure we only have a
  // single update in flight, so we'll have to wait to do the refresh
  if (m_continuousUpdates)
    requestNewUpdate();
}

void QVNCConnection::setState(int state)
{
  QMutexLocker locker(m_mutex);
  m_state = state;
}

void QVNCConnection::setBlocking(bool blocking)
{
  QMutexLocker locker(m_mutex);
  m_blocking = blocking;
}

bool QVNCConnection::blocking()
{
  QMutexLocker locker(m_mutex);
  return m_blocking;
}

void QVNCConnection::startProcessing()
{
  {
    QMutexLocker locker(m_mutex);
    if (m_inProcessing || m_blocking) {
      return;
    }
    m_inProcessing = true;
  }
  size_t navailables0;
  size_t navailables = m_socket->inStream().avail();
  do {
    navailables0 = navailables;

    processMsg(m_state);

    //qDebug() << "pre-avail()  navailables=" << navailables;
    navailables = m_socket->inStream().avail();
    //qDebug() << "post-avail() navailables=" << navailables;
  } while (!m_blocking && navailables > 0 && navailables != navailables0);
  {
    QMutexLocker locker(m_mutex);
    m_inProcessing = false;
  }
}

bool QVNCConnection::processMsg(int state)
{
  m_state = state;
  if (m_packetHandler) {
    if (m_packetHandler->processMsg(state)) {
      return true;
    }
  }
  qDebug() << "QVNCConnection::processMsg: state=" << state;
  switch (state) {
    case rfb::CConnection::RFBSTATE_INVALID:          return true;                       // Stand-by state.
    case rfb::CConnection::RFBSTATE_PROTOCOL_VERSION: return processVersionMsg();        // #1
    case rfb::CConnection::RFBSTATE_SECURITY_TYPES:   return processSecurityTypesMsg();  // #2
    case rfb::CConnection::RFBSTATE_SECURITY:         return processSecurityMsg();       // #3
    case rfb::CConnection::RFBSTATE_SECURITY_RESULT:  return processSecurityResultMsg(); // #4
    case rfb::CConnection::RFBSTATE_SECURITY_REASON:  return processSecurityReasonMsg(); // #3 if error occurred.
    case rfb::CConnection::RFBSTATE_INITIALISATION:   return processInitMsg();
    case rfb::CConnection::RFBSTATE_NORMAL:           return m_reader->readMsg();
    case rfb::CConnection::RFBSTATE_CLOSING:
      throw rdr::Exception("CConnection::processMsg: called while closing");
    case rfb::CConnection::RFBSTATE_UNINITIALISED:
      throw rdr::Exception("CConnection::processMsg: not initialised yet?");
    default:
      throw rdr::Exception("CConnection::processMsg: invalid state");
  }
}

bool QVNCConnection::processVersionMsg()
{
  char verStr[27]; // FIXME: gcc has some bug in format-overflow
  int majorVersion;
  int minorVersion;

  vlog.debug("reading protocol version");

  if (!m_socket->inStream().hasData(12))
    return false;

  m_socket->inStream().readBytes(verStr, 12);
  verStr[12] = '\0';

  if (sscanf(verStr, "RFB %03d.%03d\n",
             &majorVersion, &minorVersion) != 2) {
    resetConnection();
    m_state = rfb::CConnection::RFBSTATE_INVALID;
    throw rdr::Exception("reading version failed: not an RFB server?");
  }

  m_serverParams->setVersion(majorVersion, minorVersion);

  vlog.info("Server supports RFB protocol version %d.%d",
            m_serverParams->majorVersion, m_serverParams->minorVersion);

  // The only official RFB protocol versions are currently 3.3, 3.7 and 3.8
  if (m_serverParams->beforeVersion(3,3)) {
    vlog.error("Server gave unsupported RFB protocol version %d.%d",
               m_serverParams->majorVersion, m_serverParams->minorVersion);
    resetConnection();
    m_state = rfb::CConnection::RFBSTATE_INVALID;
    throw rdr::Exception("Server gave unsupported RFB protocol version %d.%d",
                         m_serverParams->majorVersion, m_serverParams->minorVersion);
  } else if (m_serverParams->beforeVersion(3,7)) {
    m_serverParams->setVersion(3,3);
  } else if (m_serverParams->afterVersion(3,8)) {
    m_serverParams->setVersion(3,8);
  }

  sprintf(verStr, "RFB %03d.%03d\n",
          m_serverParams->majorVersion, m_serverParams->minorVersion);
  m_socket->outStream().writeBytes(verStr, 12);
  m_socket->outStream().flush();

  m_state = rfb::CConnection::RFBSTATE_SECURITY_TYPES;

  vlog.info("Using RFB protocol version %d.%d",
            m_serverParams->majorVersion, m_serverParams->minorVersion);

  return true;
}

bool QVNCConnection::processSecurityTypesMsg()
{
  vlog.debug("processing security types message");

  int secType = rfb::secTypeInvalid;

  std::list<rdr::U8> secTypes;
  secTypes = m_security->GetEnabledSecTypes();

  if (m_serverParams->isVersion(3,3)) {

    // legacy 3.3 server may only offer "vnc authentication" or "none"

    if (!m_socket->inStream().hasData(4))
      return false;

    secType = m_socket->inStream().readU32();
    if (secType == rfb::secTypeInvalid) {
      m_state = rfb::CConnection::RFBSTATE_SECURITY_REASON;
      return true;
    } else if (secType == rfb::secTypeNone || secType == rfb::secTypeVncAuth) {
      std::list<rdr::U8>::iterator i;
      for (i = secTypes.begin(); i != secTypes.end(); i++)
        if (*i == secType) {
          secType = *i;
          break;
        }

      if (i == secTypes.end())
        secType = rfb::secTypeInvalid;
    } else {
      resetConnection();
      vlog.error("Unknown 3.3 security type %d", secType);
      throw rdr::Exception("Unknown 3.3 security type");
    }

  } else {

    // >=3.7 server will offer us a list

    if (!m_socket->inStream().hasData(1))
      return false;

    m_socket->inStream().setRestorePoint();

    int nServerSecTypes = m_socket->inStream().readU8();

    if (!m_socket->inStream().hasDataOrRestore(nServerSecTypes))
      return false;
    m_socket->inStream().clearRestorePoint();

    if (nServerSecTypes == 0) {
      m_state = rfb::CConnection::RFBSTATE_SECURITY_REASON;
      return true;
    }

    std::list<rdr::U8>::iterator j;

    for (int i = 0; i < nServerSecTypes; i++) {
      rdr::U8 serverSecType = m_socket->inStream().readU8();
      vlog.debug("Server offers security type %s(%d)",
                 rfb::secTypeName(serverSecType), serverSecType);

      /*
       * Use the first type sent by server which matches client's type.
       * It means server's order specifies priority.
       */
      if (secType == rfb::secTypeInvalid) {
        for (j = secTypes.begin(); j != secTypes.end(); j++)
          if (*j == serverSecType) {
            secType = *j;
            break;
          }
      }
    }

    // Inform the server of our decision
    if (secType != rfb::secTypeInvalid) {
      m_socket->outStream().writeU8(secType);
      m_socket->outStream().flush();
      vlog.info("Choosing security type %s(%d)",rfb::secTypeName(secType),secType);
    }
  }

  if (secType == rfb::secTypeInvalid) {
    resetConnection();
    m_state = rfb::CConnection::RFBSTATE_INVALID;
    vlog.error("No matching security types");
    throw rdr::Exception("No matching security types");
  }

  m_state = rfb::CConnection::RFBSTATE_SECURITY;
  m_securityType = secType;

  return true;
}

bool QVNCConnection::processSecurityMsg()
{
  vlog.debug("processing security message");
  bool userNeeded = false;
  bool passwordNeeded = false;
  if (!getCredentialProperties(userNeeded, passwordNeeded)) {
    return false;
  }
  if (userNeeded || passwordNeeded) {
    setBlocking(true);
    emit credentialRequested(m_secured, userNeeded, passwordNeeded);
  }
  return true;
}

bool QVNCConnection::processSecurityResultMsg()
{
  vlog.debug("processing security result message");
  int result;

  if (m_serverParams->beforeVersion(3,8) && m_securityType == rfb::secTypeNone) {
    result = rfb::secResultOK;
  } else {
    if (!m_socket->inStream().hasData(4))
      return false;
    result = m_socket->inStream().readU32();
  }

  switch (result) {
    case rfb::secResultOK:
      securityCompleted();
      return true;
    case rfb::secResultFailed:
      vlog.debug("auth failed");
      break;
    case rfb::secResultTooMany:
      vlog.debug("auth failed - too many tries");
      break;
    default:
      resetConnection();
      throw rfb::Exception("Unknown security result from server");
  }

  if (m_serverParams->beforeVersion(3,8)) {
    resetConnection();
    throw rfb::AuthFailureException();
  }

  m_state = rfb::CConnection::RFBSTATE_SECURITY_REASON;
  return true;
}

bool QVNCConnection::processSecurityReasonMsg()
{
  vlog.debug("processing security reason message");

  if (!m_socket->inStream().hasData(4))
    return false;

  m_socket->inStream().setRestorePoint();

  rdr::U32 len = m_socket->inStream().readU32();
  if (!m_socket->inStream().hasDataOrRestore(len))
    return false;
  m_socket->inStream().clearRestorePoint();

  rfb::CharArray reason(len + 1);
  m_socket->inStream().readBytes(reason.buf, len);
  reason.buf[len] = '\0';

  resetConnection();
  throw rfb::AuthFailureException(reason.buf);
}

bool QVNCConnection::processInitMsg()
{
  vlog.debug("reading server initialisation");
  return m_reader->readServerInit();
}

void QVNCConnection::securityCompleted()
{
  m_state = rfb::CConnection::RFBSTATE_INITIALISATION;
  m_reader = new QMsgReader(this, m_istream);
  m_writer = new QMsgWriter(m_serverParams, m_ostream);
  vlog.debug("Authentication success!");
  authSuccess();
  m_writer->writeClientInit(m_shared);
}

void QVNCConnection::serverInit(int width, int height,
                                const rfb::PixelFormat& pf,
                                const char* name)
{
  //CMsgHandler::serverInit(width, height, pf, name);
  m_serverParams->setDimensions(width, height);
  m_serverParams->setPF(pf);
  m_serverParams->setName(name);

  m_state = rfb::CConnection::RFBSTATE_NORMAL;
  vlog.debug("initialisation done");

  initDone();
//  assert(m_framebuffer != NULL);
//  assert(m_framebuffer->width() == m_serverParams->width());
//  assert(m_framebuffer->height() == m_serverParams->height());

  // We want to make sure we call SetEncodings at least once
  m_encodingChange = true;

  requestNewUpdate();

  // This initial update request is a bit of a corner case, so we need
  // to help out setting the correct format here.
  if (m_pendingPFChange) {
    m_serverParams->setPF(*m_pendingPF);
    m_pendingPFChange = false;
  }
}

// initDone() is called when the serverInit message has been received.  At
// this point we create the desktop window and display it.  We also tell the
// server the pixel format and encodings to use and request the first update.
void QVNCConnection::initDone()
{
  // If using AutoSelect with old servers, start in FullColor
  // mode. See comment in autoSelectFormatAndEncoding.
  if (m_serverParams->beforeVersion(3, 8) && ::autoSelect)
    fullColour.setParam(true);

  *m_serverPF = m_serverParams->pf();

  delete m_framebuffer;
  m_framebuffer = new PlatformPixelBuffer(m_serverParams->width(), m_serverParams->height());
  emit newVncWindowRequested(m_serverParams->width(), m_serverParams->height(), m_serverParams->name() /*, m_serverPF, this */);
  *m_fullColourPF = m_framebuffer->getPF();

  // Force a switch to the format and encoding we'd like
  updatePixelFormat();
  int encNum = encodingNum(::preferredEncoding);
  if (encNum != -1)
    setPreferredEncoding(encNum);
}

void QVNCConnection::setPreferredEncoding(int encoding)
{
  if (m_preferredEncoding == encoding)
    return;

  m_preferredEncoding = encoding;
  m_encodingChange = true;
}

// requestNewUpdate() requests an update from the server, having set the
// format and encoding appropriately.
void QVNCConnection::requestNewUpdate()
{
  if (m_formatChange && !m_pendingPFChange) {
    /* Catch incorrect requestNewUpdate calls */
    assert(!m_pendingUpdate || m_continuousUpdates);

    // We have to make sure we switch the internal format at a safe
    // time. For continuous updates we temporarily disable updates and
    // look for a EndOfContinuousUpdates message to see when to switch.
    // For classical updates we just got a new update right before this
    // function was called, so we need to make sure we finish that
    // update before we can switch.

    m_pendingPFChange = true;
    *m_pendingPF = *m_nextPF;

    if (m_continuousUpdates)
      writer()->writeEnableContinuousUpdates(false, 0, 0, 0, 0);

    writer()->writeSetPixelFormat(*m_pendingPF);

    if (m_continuousUpdates)
      writer()->writeEnableContinuousUpdates(true, 0, 0,
                                             m_serverParams->width(),
                                             m_serverParams->height());

    m_formatChange = false;
  }

  if (m_encodingChange) {
    updateEncodings();
    m_encodingChange = false;
  }

  if (m_forceNonincremental || !m_continuousUpdates) {
    m_pendingUpdate = true;
    writer()->writeFramebufferUpdateRequest(rfb::Rect(0, 0,
                                                 m_serverParams->width(),
                                                 m_serverParams->height()),
                                            !m_forceNonincremental);
    qDebug() << "QVNCConnection::requestNewUpdate: w=" << m_serverParams->width() << ", h=" << m_serverParams->height();
  }

  m_forceNonincremental = false;
}

// Ask for encodings based on which decoders are supported.  Assumes higher
// encoding numbers are more desirable.
void QVNCConnection::updateEncodings()
{
  std::list<rdr::U32> encodings;

  if (m_supportsLocalCursor) {
    encodings.push_back(rfb::pseudoEncodingCursorWithAlpha);
    encodings.push_back(rfb::pseudoEncodingVMwareCursor);
    encodings.push_back(rfb::pseudoEncodingCursor);
    encodings.push_back(rfb::pseudoEncodingXCursor);
  }
  if (m_supportsCursorPosition) {
    encodings.push_back(rfb::pseudoEncodingVMwareCursorPosition);
  }
  if (m_supportsDesktopResize) {
    encodings.push_back(rfb::pseudoEncodingDesktopSize);
    encodings.push_back(rfb::pseudoEncodingExtendedDesktopSize);
  }
  if (m_supportsLEDState) {
    encodings.push_back(rfb::pseudoEncodingLEDState);
    encodings.push_back(rfb::pseudoEncodingVMwareLEDState);
  }

  encodings.push_back(rfb::pseudoEncodingDesktopName);
  encodings.push_back(rfb::pseudoEncodingLastRect);
  encodings.push_back(rfb::pseudoEncodingExtendedClipboard);
  encodings.push_back(rfb::pseudoEncodingContinuousUpdates);
  encodings.push_back(rfb::pseudoEncodingFence);
  encodings.push_back(rfb::pseudoEncodingQEMUKeyEvent);

  if (rfb::Decoder::supported(m_preferredEncoding)) {
    encodings.push_back(m_preferredEncoding);
  }

  encodings.push_back(rfb::encodingCopyRect);

  for (int i = rfb::encodingMax; i >= 0; i--) {
    if ((i != m_preferredEncoding) && rfb::Decoder::supported(i))
      encodings.push_back(i);
  }

  if (m_compressLevel >= 0 && m_compressLevel <= 9)
      encodings.push_back(rfb::pseudoEncodingCompressLevel0 + m_compressLevel);
  if (qualityLevel >= 0 && qualityLevel <= 9)
      encodings.push_back(rfb::pseudoEncodingQualityLevel0 + qualityLevel);

  writer()->writeSetEncodings(encodings);
}

// requestNewUpdate() requests an update from the server, having set the
// format and encoding appropriately.
void QVNCConnection::updatePixelFormat()
{
  rfb::PixelFormat pf;

  if (fullColour) {
    pf = *m_fullColourPF;
  } else {
    if (lowColourLevel == 0)
      pf = verylowColourPF;
    else if (lowColourLevel == 1)
      pf = lowColourPF;
    else
      pf = mediumColourPF;
  }

  char str[256];
  pf.print(str, 256);
  vlog.info("Using pixel format %s" ,str);
  setPF(&pf);
}

void QVNCConnection::setPF(const rfb::PixelFormat* pf)
{
  if (m_serverParams->pf().equal(*pf) && !m_formatChange)
    return;

  *m_nextPF = *pf;
  m_formatChange = true;
}

void QVNCConnection::authSuccess()
{
}

bool QVNCConnection::getCredentialProperties(bool &userNeeded, bool &passwordNeeded)
{
  // This method is based on common/rfb/SecurityClient.cxx
  switch (m_securityType) {
    case rfb::secTypeNone:
    default:
      return false;
    case rfb::secTypeVncAuth:
      userNeeded = false;
      passwordNeeded = true;
      setPacketHandler(new QVncAuthHandler(this));
      return true;
    case rfb::secTypeVeNCrypt:
      return getVeNCryptCredentialProperties(userNeeded, passwordNeeded);
    case rfb::secTypePlain:
      userNeeded = true;
      passwordNeeded = true;
      return true;

#if 0
    case rfb::secTypeTLSNone:
    case rfb::secTypeTLSVnc:
    case rfb::secTypeTLSPlain:
    case rfb::secTypeX509None:
    case rfb::secTypeX509Vnc:
    case rfb::secTypeX509Plain:

    case rfb::secTypeRA2:
    case rfb::secTypeRA2ne:
    case rfb::secTypeRA256:
    case rfb::secTypeRAne256:
#endif
      return true;
  }
}

bool QVNCConnection::getVeNCryptCredentialProperties(bool &userNeeded, bool &passwordNeeded)
{
  /* get major, minor versions, send what we can support (or 0.0 for can't support it) */
  if (!m_encStatus->haveRecvdMajorVersion) {
    if (!m_istream->hasData(1))
      return false;

    m_encStatus->majorVersion = m_istream->readU8();
    m_encStatus->haveRecvdMajorVersion = true;
  }

  if (!m_encStatus->haveRecvdMinorVersion) {
    if (!m_istream->hasData(1))
      return false;

    m_encStatus->minorVersion = m_istream->readU8();
    m_encStatus->haveRecvdMinorVersion = true;
  }

  /* major version in upper 8 bits and minor version in lower 8 bits */
  rdr::U16 Version = (((rdr::U16) m_encStatus->majorVersion) << 8) | ((rdr::U16) m_encStatus->minorVersion);

  if (!m_encStatus->haveSentVersion) {
    /* Currently we don't support former VeNCrypt 0.1 */
    if (Version >= 0x0002) {
      m_encStatus->majorVersion = 0;
      m_encStatus->minorVersion = 2;
      m_ostream->writeU8(m_encStatus->majorVersion);
      m_ostream->writeU8(m_encStatus->minorVersion);
      m_ostream->flush();
    } else {
      /* Send 0.0 to indicate no support */
      m_encStatus->majorVersion = 0;
      m_encStatus->minorVersion = 0;
      m_ostream->writeU8(0);
      m_ostream->writeU8(0);
      m_ostream->flush();
      resetConnection();
      throw rfb::AuthFailureException("The server reported an unsupported VeNCrypt version");
    }

    m_encStatus->haveSentVersion = true;
  }

  /* Check that the server is OK */
  if (!m_encStatus->haveAgreedVersion) {
    if (!m_istream->hasData(1))
      return false;

    if (m_istream->readU8()) {
      resetConnection();
      throw rfb::AuthFailureException("The server reported it could not support the VeNCrypt version");
    }
    m_encStatus->haveAgreedVersion = true;
  }

  /* get a number of types */
  if (!m_encStatus->haveNumberOfTypes) {
    if (!m_istream->hasData(1))
      return false;

    m_encStatus->nAvailableTypes = m_istream->readU8();

    if (!m_encStatus->nAvailableTypes) {
      resetConnection();
      throw rfb::AuthFailureException("The server reported no VeNCrypt sub-types");
    }
    m_encStatus->availableTypes = new rdr::U32[m_encStatus->nAvailableTypes];
    m_encStatus->haveNumberOfTypes = true;
  }

  if (m_encStatus->nAvailableTypes) {
    /* read in the types possible */
    if (!m_encStatus->haveListOfTypes) {
      if (!m_istream->hasData(4 * m_encStatus->nAvailableTypes))
        return false;

      for (int i = 0;i < m_encStatus->nAvailableTypes;i++) {
        m_encStatus->availableTypes[i] = m_istream->readU32();
        vlog.debug("Server offers security type %s (%d)", rfb::secTypeName(m_encStatus->availableTypes[i]), m_encStatus->availableTypes[i]);
      }

      m_encStatus->haveListOfTypes = true;
    }

    /* make a choice and send it to the server, meanwhile set up the stack */
    m_encStatus->chosenType = rfb::secTypeInvalid;
    rdr::U8 i;
    std::list<rdr::U32>::iterator j;
    std::list<rdr::U32> secTypes;

    secTypes = m_security->GetEnabledExtSecTypes();

    /* Honor server's security type order */
    for (i = 0; i < m_encStatus->nAvailableTypes; i++) {
      for (j = secTypes.begin(); j != secTypes.end(); j++) {
        if (*j == m_encStatus->availableTypes[i]) {
          m_encStatus->chosenType = *j;
          break;
        }
      }

      if (m_encStatus->chosenType != rfb::secTypeInvalid)
        break;
    }

    vlog.info("Choosing security type %s (%d)", rfb::secTypeName(m_encStatus->chosenType), m_encStatus->chosenType);

    /* Set up the stack according to the chosen type: */
    if (m_encStatus->chosenType == rfb::secTypeInvalid || m_encStatus->chosenType == rfb::secTypeVeNCrypt) {
      resetConnection();
      throw rfb::AuthFailureException("No valid VeNCrypt sub-type");
    }
    /* send chosen type to server */
    m_ostream->writeU32(m_encStatus->chosenType);
    m_ostream->flush();

    QVNCPacketHandler *handler0;
    switch (m_encStatus->chosenType) {
      case rfb::secTypeVncAuth:
        userNeeded = false;
        passwordNeeded = true;
        handler0 = setPacketHandler(new QVncAuthHandler(this));
        if (handler0) {
          handler0->deleteLater();
        }
        return true;
      case rfb::secTypePlain:
        userNeeded = true;
        passwordNeeded = true;
        handler0 = setPacketHandler(new QPlainAuthHandler(this));
        if (handler0) {
          handler0->deleteLater();
        }
        return true;

      case rfb::secTypeTLSNone:
      case rfb::secTypeX509None:
        establishSecurityLayer(m_encStatus->chosenType);
        userNeeded = false;
        passwordNeeded = false;
        return false;
      case rfb::secTypeTLSVnc:
      case rfb::secTypeX509Vnc:
        establishSecurityLayer(m_encStatus->chosenType);
        userNeeded = false;
        passwordNeeded = true;
        return false;
      case rfb::secTypeTLSPlain:
      case rfb::secTypeX509Plain:
        establishSecurityLayer(m_encStatus->chosenType);
        userNeeded = true;
        passwordNeeded = true;
        return false;
    }
  }
  /*
     * Server told us that there are 0 types it can support - this should not
     * happen, since if the server supports 0 sub-types, it doesn't support
     * this security type
     */
  resetConnection();
  throw rfb::AuthFailureException("The server reported 0 VeNCrypt sub-types");
}

bool QVNCConnection::establishSecurityLayer(int securitySubType)
{
  // cf. common/rfb/SecurityClient.cxx, line:82-
  return true; // TODO
}

////////////////////////////////////////////////////////////

// autoSelectFormatAndEncoding() chooses the format and encoding appropriate
// to the connection speed:
//
//   First we wait for at least one second of bandwidth measurement.
//
//   Above 16Mbps (i.e. LAN), we choose the second highest JPEG quality,
//   which should be perceptually lossless.
//
//   If the bandwidth is below that, we choose a more lossy JPEG quality.
//
//   If the bandwidth drops below 256 Kbps, we switch to palette mode.
//
//   Note: The system here is fairly arbitrary and should be replaced
//         with something more intelligent at the server end.
//
void QVNCConnection::autoSelectFormatAndEncoding()
{
  qDebug() << "QVNCConnection::autoSelectFormatAndEncoding";
  // Always use Tight
  setPreferredEncoding(rfb::encodingTight);

  // Select appropriate quality level
  if (!noJpeg) {
    int newQualityLevel;
    if (m_bpsEstimate > 16000000)
      newQualityLevel = 8;
    else
      newQualityLevel = 6;

    if (newQualityLevel != ::qualityLevel) {
      vlog.info("Throughput %d kbit/s - changing to quality %d", (int)(m_bpsEstimate/1000), newQualityLevel);
      ::qualityLevel.setParam(newQualityLevel);
      setQualityLevel(newQualityLevel);
    }
  }

  if (server()->beforeVersion(3, 8)) {
    // Xvnc from TightVNC 1.2.9 sends out FramebufferUpdates with
    // cursors "asynchronously". If this happens in the middle of a
    // pixel format change, the server will encode the cursor with
    // the old format, but the client will try to decode it
    // according to the new format. This will lead to a
    // crash. Therefore, we do not allow automatic format change for
    // old servers.
    return;
  }

  // Select best color level
  bool newFullColour = (m_bpsEstimate > 256000);
  if (newFullColour != ::fullColour) {
    if (newFullColour)
      vlog.info("Throughput %d kbit/s - full color is now enabled", (int)(m_bpsEstimate/1000));
    else
      vlog.info("Throughput %d kbit/s - full color is now disabled", (int)(m_bpsEstimate/1000));
    fullColour.setParam(newFullColour);
    updatePixelFormat();
  }
}

void QVNCConnection::setQualityLevel(int level)
{
  if (m_qualityLevel == level)
    return;

  m_qualityLevel = level;
  m_encodingChange = true;
}

// CMsgHandler.h
void QVNCConnection::supportsQEMUKeyEvent()
{
  m_serverParams->supportsQEMUKeyEvent = true;
}

// CConn.h
void QVNCConnection::resizeFramebuffer()
{
  qDebug() << "QVNCConnection::resizeFramebuffer(): width=" << m_serverParams->width() << ",height=" << m_serverParams->height();
  PlatformPixelBuffer *framebuffer = new PlatformPixelBuffer(m_serverParams->width(), m_serverParams->height());
  setFramebuffer(framebuffer);

  // TODO: DesktopWindow::resizeFramebuffer() may have to be ported here.
}

void QVNCConnection::setDesktopSize(int w, int h)
{
  qDebug() << "QVNCConnection::QVNCConnection::setDesktopSize: w=" << w << ", h=" << h;
  m_decoder->flush();

  server()->setDimensions(w, h);

  if (m_continuousUpdates)
    writer()->writeEnableContinuousUpdates(true, 0, 0, server()->width(), server()->height());

  resizeFramebuffer();
  assert(m_framebuffer != nullptr);
  assert(m_framebuffer->width() == server()->width());
  assert(m_framebuffer->height() == server()->height());
}

void QVNCConnection::setExtendedDesktopSize(unsigned reason, unsigned result, int w, int h, const rfb::ScreenSet& layout)
{
  qDebug() << "QVNCConnection::QVNCConnection::setExtendedDesktopSize: w=" << w << ", h=" << h;
  m_decoder->flush();

  server()->supportsSetDesktopSize = true;

  if ((reason == rfb::reasonClient) && (result != rfb::resultSuccess))
    return;

  server()->setDimensions(w, h, layout);

  if (m_continuousUpdates)
    writer()->writeEnableContinuousUpdates(true, 0, 0,
                                           server()->width(),
                                           server()->height());

  resizeFramebuffer();
  assert(m_framebuffer != nullptr);
  assert(m_framebuffer->width() == server()->width());
  assert(m_framebuffer->height() == server()->height());
}

void QVNCConnection::setName(const char* name)
{
  m_serverParams->setName(name);
}

void QVNCConnection::setColourMapEntries(int firstColour, int nColours, rdr::U16* rgbs)
{
  Q_UNUSED(firstColour)
  Q_UNUSED(nColours)
  Q_UNUSED(rgbs)
  vlog.error("Invalid SetColourMapEntries from server!");
}

void QVNCConnection::bell()
{
  AppManager::instance()->view()->bell();
  // TODO
#if 0
#if defined(WIN32)
  MessageBeep(0xFFFFFFFF); // cf. fltk/src/drivers/WinAPI/Fl_WinAPI_Screen_Driver.cxx:245
#endif
#if defined(__APPLE__)
  NSBeep(); // cf. fltk/src/drivers/Cocoa/Fl_Cocoa_Screen_Driver.cxx:162
#endif
#if !defined(WIN32) && !defined(__APPLE__)
  QString platform = QGuiApplication::platformName();
  if (platform == "xcb") { // cf. fltk/src/drivers/X11/Fl_X11_Screen_Driver.cxx:398
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Display *display = QX11Info::display();
#else
    QNativeInterface::QX11Application *f = (QNativeInterface::QX11Application *)QGuiApplication::nativeInterface<QNativeInterface::QX11Application>();
    Display *display f->display();
#endif
    extern int XBell(Display*, int);
    XBell(display, 0 /* volume */);
  }
  if (platform == "wayland") {
    fprintf(stderr, "\007"); // cf. fltk/src/drivers/Wayland/Fl_Wayland_Screen_Driver.cxx:1272
  }
#endif
#endif
}

// framebufferUpdateStart() is called at the beginning of an update.
// Here we try to send out a new framebuffer update request so that the
// next update can be sent out in parallel with us decoding the current
// one.
void QVNCConnection::framebufferUpdateStart()
{
  assert(m_framebuffer != nullptr);

  // Note: This might not be true if continuous updates are supported
  m_pendingUpdate = false;

  requestNewUpdate();

  // For bandwidth estimate
  gettimeofday(&m_updateStartTime, NULL);
  m_updateStartPos = m_socket->inStream().pos();

  // Update the screen prematurely for very slow updates
  m_updateTimer->setInterval(1000);
  m_updateTimer->start();
}

// framebufferUpdateEnd() is called at the end of an update.
// For each rectangle, the FdInStream will have timed the speed
// of the connection, allowing us to select format and encoding
// appropriately, and then request another incremental update.
void QVNCConnection::framebufferUpdateEnd()
{
  unsigned long long elapsed, bps, weight;
  struct timeval now;

  m_decoder->flush();

  // A format change has been scheduled and we are now past the update
  // with the old format. Time to active the new one.
  if (m_pendingPFChange && !m_continuousUpdates) {
    server()->setPF(*m_pendingPF);
    m_pendingPFChange = false;
  }

  if (m_firstUpdate) {
    if (server()->supportsContinuousUpdates) {
      vlog.info("Enabling continuous updates");
      m_continuousUpdates = true;
      writer()->writeEnableContinuousUpdates(true, 0, 0,
                                             server()->width(),
                                             server()->height());
    }

    m_firstUpdate = false;
  }

  m_updateCount++;

  // Calculate bandwidth everything managed to maintain during this update
  gettimeofday(&now, NULL);
  elapsed = (now.tv_sec - m_updateStartTime.tv_sec) * 1000000;
  elapsed += now.tv_usec - m_updateStartTime.tv_usec;
  if (elapsed == 0)
    elapsed = 1;
  bps = (unsigned long long)(m_socket->inStream().pos() -
                             m_updateStartPos) * 8 *
                            1000000 / elapsed;
  // Allow this update to influence things more the longer it took, to a
  // maximum of 20% of the new value.
  weight = elapsed * 1000 / bpsEstimateWindow;
  if (weight > 200000)
    weight = 200000;
  m_bpsEstimate = ((m_bpsEstimate * (1000000 - weight)) +
                 (bps * weight)) / 1000000;

  m_updateTimer->stop();
  AppManager::instance()->view()->updateWindow();

  // Compute new settings based on updated bandwidth values
  if (::autoSelect)
    autoSelectFormatAndEncoding();
}

bool QVNCConnection::dataRect(const rfb::Rect& r, int encoding)
{
  bool ret;

  if (encoding != rfb::encodingCopyRect)
    m_lastServerEncoding = encoding;

  ret = m_decoder->decodeRect(r, encoding, m_framebuffer);

  if (ret)
    m_pixelCount += r.area();

  return ret;
}

void QVNCConnection::setCursor(int width, int height, const rfb::Point& hotspot, const unsigned char* data)
{
  bool emptyCursor = true;
  for (int i = 0; i < width * height; i++) {
    if (data[i*4 + 3] != 0) {
      emptyCursor = false;
      break;
    }
  }
  if (emptyCursor) {
    if (::dotWhenNoCursor) {
      static const char * dotcursor_xpm[] = {
        "5 5 2 1",
        ".	c #000000",
        " 	c #FFFFFF",
        "     ",
        " ... ",
        " ... ",
        " ... ",
        "     "};
      delete m_cursor;
      m_cursor = new QCursor(QPixmap(dotcursor_xpm), 2, 2);
    }
    else {
      static const char * emptycursor_xpm[] = {
        "2 2 1 1",
        ".	c None",
        "..",
        ".."};
      delete m_cursor;
      m_cursor = new QCursor(QPixmap(emptycursor_xpm), 0, 0);
    }
  }
  else {
    qDebug() << "QVNCConnection::setCursor: w=" << width << ", h=" << height << ", data=" << data;
    QImage image(data, width, height, QImage::Format_RGBA8888);
    delete m_cursor;
    m_cursor = new QCursor(QPixmap::fromImage(image), hotspot.x, hotspot.y);
  }
  emit cursorChanged(*m_cursor);
}

void QVNCConnection::setCursorPos(const rfb::Point& pos)
{
  emit cursorPositionChanged(pos.x, pos.y);
}

void QVNCConnection::fence(rdr::U32 flags, unsigned len, const char data[])
{
  m_serverParams->supportsFence = true;
  if (flags & rfb::fenceFlagRequest) {
    // We handle everything synchronously so we trivially honor these modes
    flags = flags & (rfb::fenceFlagBlockBefore | rfb::fenceFlagBlockAfter);

    writer()->writeFence(flags, len, data);
    return;
  }
}

void QVNCConnection::requestClipboard()
{
  if (m_serverClipboard != nullptr) {
    handleClipboardData(m_serverClipboard);
    return;
  }

  if (m_serverParams->clipboardFlags() & rfb::clipboardRequest)
    writer()->writeClipboardRequest(rfb::clipboardUTF8);
}

void QVNCConnection::setLEDState(unsigned int state)
{
  qDebug() << "QVNCConnection::setLEDState";
  vlog.debug("Got server LED state: 0x%08x", state);
  m_serverParams->setLEDState(state);
  emit ledStateChanged(state);
}

void QVNCConnection::handleClipboardAnnounce(bool available)
{
  emit clipboardAnnounced(available);
  requestClipboard();
}

void QVNCConnection::handleClipboardData(const char* data)
{
  emit clipboardChanged(data);
}


// CConnection.h
void QVNCConnection::endOfContinuousUpdates()
{
  server()->supportsContinuousUpdates = true;

  // We've gotten the marker for a format change, so make the pending
  // one active
  if (m_pendingPFChange) {
    server()->setPF(*m_pendingPF);
    m_pendingPFChange = false;

    // We might have another change pending
    if (m_formatChange)
      requestNewUpdate();
  }
}

bool QVNCConnection::readAndDecodeRect(const rfb::Rect& r, int encoding, rfb::ModifiablePixelBuffer* pb)
{
  if (!m_decoder->decodeRect(r, encoding, pb))
    return false;
  m_decoder->flush();
  return true;
}

void QVNCConnection::serverCutText(const char* str)
{
  m_hasLocalClipboard = false;

  m_serverClipboard.clear();

  m_serverClipboard.append(rfb::latin1ToUTF8(str));

  handleClipboardAnnounce(true);
}

void QVNCConnection::handleClipboardCaps(rdr::U32 flags, const rdr::U32* lengths)
{
  vlog.debug("Got server clipboard capabilities:");
  for (int i = 0;i < 16;i++) {
    if (flags & (1 << i)) {
      const char *type;

      switch (1 << i) {
        case rfb::clipboardUTF8:
          type = "Plain text";
          break;
        case rfb::clipboardRTF:
          type = "Rich text";
          break;
        case rfb::clipboardHTML:
          type = "HTML";
          break;
        case rfb::clipboardDIB:
          type = "Images";
          break;
        case rfb::clipboardFiles:
          type = "Files";
          break;
        default:
          vlog.debug("    Unknown format 0x%x", 1 << i);
          continue;
      }

      if (lengths[i] == 0)
        vlog.debug("    %s (only notify)", type);
      else {
        char bytes[1024];
        rfb::iecPrefix(lengths[i], "B", bytes, sizeof(bytes));
        vlog.debug("    %s (automatically send up to %s)", type, bytes);
      }
    }
  }

  server()->setClipboardCaps(flags, lengths);

  rdr::U32 sizes[] = { 0 };
  writer()->writeClipboardCaps(rfb::clipboardUTF8 |
                               rfb::clipboardRequest |
                               rfb::clipboardPeek |
                               rfb::clipboardNotify |
                               rfb::clipboardProvide,
                               sizes);
}

void QVNCConnection::handleClipboardRequest()
{
  //  desktop->handleClipboardRequest();
}

void QVNCConnection::handleClipboardRequest(rdr::U32 flags)
{
  if (!(flags & rfb::clipboardUTF8)) {
    vlog.debug("Ignoring clipboard request for unsupported formats 0x%x", flags);
    return;
  }
  if (!m_hasLocalClipboard) {
    vlog.debug("Ignoring unexpected clipboard request");
    return;
  }
  handleClipboardRequest();
}

void QVNCConnection::handleClipboardPeek(rdr::U32 flags)
{
  Q_UNUSED(flags)
  if (server()->clipboardFlags() & rfb::clipboardNotify)
    writer()->writeClipboardNotify(m_hasLocalClipboard ? rfb::clipboardUTF8 : 0);
}

void QVNCConnection::handleClipboardNotify(rdr::U32 flags)
{
  m_serverClipboard.clear();
  if (flags & rfb::clipboardUTF8) {
    m_hasLocalClipboard = false;
    handleClipboardAnnounce(true);
  } else {
    handleClipboardAnnounce(false);
  }
}

void QVNCConnection::handleClipboardProvide(rdr::U32 flags, const size_t* lengths, const rdr::U8* const* data)
{
  if (!(flags & rfb::clipboardUTF8)) {
    vlog.debug("Ignoring clipboard provide with unsupported formats 0x%x", flags);
    return;
  }

  m_serverClipboard.clear();
  m_serverClipboard.append(rfb::convertLF((const char*)data[0], lengths[0]));

  // FIXME: Should probably verify that this data was actually requested
  handleClipboardData(m_serverClipboard);
}

QString QVNCConnection::infoText()
{
  QString infoText;
  char pfStr[100];

  infoText += QString::asprintf(_("Desktop name: %.80s\n"), m_serverParams->name());
  infoText += QString::asprintf(_("Host: %.80s port: %d\n"), m_host.toStdString().c_str(), m_port);
  infoText += QString::asprintf(_("Size: %d x %d\n"), m_serverParams->width(), m_serverParams->height());

  // TRANSLATORS: Will be filled in with a string describing the
  // protocol pixel format in a fairly language neutral way
  m_serverParams->pf().print(pfStr, 100);
  infoText += QString::asprintf(_("Pixel format: %s\n"), pfStr);

  // TRANSLATORS: Similar to the earlier "Pixel format" string
  m_serverPF->print(pfStr, 100);
  infoText += QString::asprintf(_("(server default %s)\n"), pfStr);
  infoText += QString::asprintf(_("Requested encoding: %s\n"), rfb::encodingName(m_preferredEncoding));
  infoText += QString::asprintf(_("Last used encoding: %s\n"), rfb::encodingName(m_lastServerEncoding));
  infoText += QString::asprintf(_("Line speed estimate: %d kbit/s\n"), (int)(m_bpsEstimate/1000));
  infoText += QString::asprintf(_("Protocol version: %d.%d\n"), m_serverParams->majorVersion, m_serverParams->minorVersion);
  infoText += QString::asprintf(_("Security method: %s\n"), rfb::secTypeName(m_securityType));

  return infoText;
}

void QVNCConnection::setCompressLevel(int level)
{
  if (m_compressLevel == level)
    return;

  m_compressLevel = level;
  m_encodingChange = true;
}
