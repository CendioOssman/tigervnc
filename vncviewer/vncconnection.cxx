#include <QQmlEngine>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QMutexLocker>
#include <QTimer>
#include "rfb/Hostname.h"
#include "rfb/CConnection.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/Security.h"
#include "rfb/PixelFormat.h"
#include "rfb/Password.h"
#include "rfb/d3des.h"
#include "vncstream.h"
#include "vncconnection.h"
#include "parameters.h"
#include "msgreader.h"
#include "msgwriter.h"
#include <QSocketNotifier>
#include "network/TcpSocket.h"
#ifndef Q_OS_WIN
#include "network/UnixSocket.h"
#endif

static rfb::LogWriter vlog("CConnection");

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
  , m_pendingPF(new rfb::PixelFormat)
  , m_serverPF(new rfb::PixelFormat)
  , m_preferredEncoding(0)
  , m_encodingChange(false)
  , m_framebuffer(nullptr)
  , m_timer(new QTimer)
  , m_pendingSocketEvent(false)
  , m_user(nullptr)
  , m_password(nullptr)
{
  moveToThread(this);
  m_timer->moveToThread(this);
  m_timer->setInterval(10);
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, [this]() {
    {
      qDebug() << "QTimer: timeout";
      QMutexLocker locker(m_mutex);
      if (m_inProcessing) {
        qDebug() << "QTimer: start";
        emit socketNotified();
        //m_timer->start();
        return;
      }
    }
    qDebug() << "startProcessing: by timer";
    startProcessing();
  });
  connect(this, &QVNCConnection::socketNotified, this, [this]() {
    {
      QMutexLocker locker(m_mutex);
      if (m_inProcessing) {
        if (!m_timer->isActive()) {
          qDebug() << "socketNotified: timer start.";
          m_timer->start();
        }
        return;
      }
    }
    //qDebug() << "socketNotified: startProcessing.";
    startProcessing();
  });
}

QVNCConnection::~QVNCConnection()
{
  m_timer->deleteLater();
  delete m_mutex;
  delete m_serverParams;
  delete m_security;
  delete m_socketNotifier;
  delete m_socketErrorNotifier;
  delete m_encStatus;
  delete m_reader;
  delete m_writer;
  delete m_socket;
  delete m_user;
  delete m_password;
  delete m_framebuffer;
  delete m_pendingPF;
  delete m_serverPF;
}

void QVNCConnection::bind(int fd)
{
  setStreams(&m_socket->inStream(), &m_socket->outStream());

  m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(m_socketNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketNotified();
  }, Qt::QueuedConnection);

  m_socketErrorNotifier = new QSocketNotifier(fd, QSocketNotifier::Exception);
  QObject::connect(m_socketErrorNotifier, &QSocketNotifier::activated, this, [](int fd) {
    Q_UNUSED(fd)
    throw rdr::Exception("CConnection::processMsg: socket error.");
  });
}

void QVNCConnection::setStreams(rdr::FdInStream *in, rdr::FdOutStream *out)
{
  m_istream = in;
  m_ostream = out;
}

QVNCPacketHandler *QVNCConnection::setPacketHandler(QVNCPacketHandler *handler)
{
  QVNCPacketHandler *handler0 = m_packetHandler;
  m_packetHandler = handler;
  return handler0;
}

void QVNCConnection::connectToServer(const QString addressport)
{
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
//  setPacketHandler(new QVncAuthHandler(this));
  setBlocking(false);
  return true;
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
    //qDebug() << "startProcessing: enter";
    if (m_inProcessing) {
      return;
    }
    m_timer->stop();
    m_inProcessing = true;
  }
  if (!blocking()) {
    processMsg(m_state);
  }
  {
    QMutexLocker locker(m_mutex);
    m_inProcessing = false;
  }
  //qDebug() << "startProcessing: exit";
}

bool QVNCConnection::processMsg(int state)
{
  m_state = state;
  if (m_packetHandler) {
    if (m_packetHandler->processMsg(state)) {
      return true;
    }
  }
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
      throw rfb::Exception("Unknown security result from server");
  }

  if (m_serverParams->beforeVersion(3,8)) {
    m_state = rfb::CConnection::RFBSTATE_INVALID;
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

  m_state = rfb::CConnection::RFBSTATE_INVALID;
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
  if (m_serverParams->beforeVersion(3, 8) && autoSelect)
    fullColour.setParam(true);

  *m_serverPF = m_serverParams->pf();

  emit newVncWindowRequested(m_serverParams->width(), m_serverParams->height(), m_serverParams->name() /*, m_serverPF, this */);
  //  desktop = new DesktopWindow(m_serverParams->width(), m_serverParams->height(),
  //                              m_serverParams->name(), serverPF, this);
  //  fullColourPF = desktop->getPreferredPF();

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

void QVNCConnection::requestNewUpdate()
{
}

void QVNCConnection::updatePixelFormat()
{
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
      throw rfb::AuthFailureException("The server reported an unsupported VeNCrypt version");
    }

    m_encStatus->haveSentVersion = true;
  }

  /* Check that the server is OK */
  if (!m_encStatus->haveAgreedVersion) {
    if (!m_istream->hasData(1))
      return false;

    if (m_istream->readU8())
      throw rfb::AuthFailureException("The server reported it could not support the VeNCrypt version");

    m_encStatus->haveAgreedVersion = true;
  }

  /* get a number of types */
  if (!m_encStatus->haveNumberOfTypes) {
    if (!m_istream->hasData(1))
      return false;

    m_encStatus->nAvailableTypes = m_istream->readU8();

    if (!m_encStatus->nAvailableTypes)
      throw rfb::AuthFailureException("The server reported no VeNCrypt sub-types");

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
    if (m_encStatus->chosenType == rfb::secTypeInvalid || m_encStatus->chosenType == rfb::secTypeVeNCrypt)
      throw rfb::AuthFailureException("No valid VeNCrypt sub-type");

    /* send chosen type to server */
    m_ostream->writeU32(m_encStatus->chosenType);
    m_ostream->flush();

    switch (m_encStatus->chosenType) {
      case rfb::secTypeVncAuth:
        userNeeded = false;
        passwordNeeded = true;
        setPacketHandler(new QVncAuthHandler(this));
        return true;
      case rfb::secTypePlain:
        userNeeded = true;
        passwordNeeded = true;
        setPacketHandler(new QPlainAuthHandler(this));
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
  throw rfb::AuthFailureException("The server reported 0 VeNCrypt sub-types");
}

bool QVNCConnection::establishSecurityLayer(int securitySubType)
{
  // cf. common/rfb/SecurityClient.cxx, line:82-
  return true; // TODO
}
