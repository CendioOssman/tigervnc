#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QLocalSocket>
#include <QTcpSocket>
#include <QSocketNotifier>
#include <QTimer>
#include <QProcess>
#include <QClipboard>
#include "rfb/Hostname.h"
#include "rfb/Exception.h"
#include "rfb/LogWriter.h"
#include "rfb/CMsgWriter.h"
#include "network/TcpSocket.h"
#include "viewerconfig.h"
#include "appmanager.h"
#include "i18n.h"
#include "abstractvncview.h"
#include "CConn.h"
#include "vncconnection.h"
#undef asprintf

#if !defined(Q_OS_WIN)
  #include "network/UnixSocket.h"
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    #include <QtX11Extras/QX11Info>
  #endif
#endif

static rfb::LogWriter vlog("CConnection");

QVNCConnection::QVNCConnection()
 : QObject(nullptr)
 , m_rfbcon(new CConn(this))
 , m_socket(nullptr)
 , m_socketNotifier(nullptr)
 , m_socketErrorNotifier(nullptr)
 , m_updateTimer(nullptr)
 , m_tunnelFactory(nullptr)
 , m_closing(false)
{
  connect(this, &QVNCConnection::socketNotified, this, &QVNCConnection::startProcessing);

  connect(this, &QVNCConnection::writePointerEvent, this, [this](const rfb::Point &pos, int buttonMask) {
    m_rfbcon->writer()->writePointerEvent(pos, buttonMask);
  });
  connect(this, &QVNCConnection::writeSetDesktopSize, this, [this](int width, int height, const rfb::ScreenSet &layout) {
    m_rfbcon->writer()->writeSetDesktopSize(width, height, layout);
  });
  connect(this, &QVNCConnection::writeKeyEvent, this, [this](rdr::U32 keysym, rdr::U32 keycode, bool down) {
    m_rfbcon->writer()->writeKeyEvent(keysym, keycode, down);
  });
  
  if (ViewerConfig::config()->listenModeEnabled()) {
    listen();
  }

  m_updateTimer = new QTimer;
  m_updateTimer->setSingleShot(true);
  connect(m_updateTimer, &QTimer::timeout, this, [this]() {
    m_rfbcon->framebufferUpdateEnd();
  });

  QString gatewayHost = ViewerConfig::config()->gatewayHost();
  QString remoteHost = ViewerConfig::config()->serverHost();
  if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
    m_tunnelFactory = new TunnelFactory;
    m_tunnelFactory->start();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    m_tunnelFactory->wait(20000);
#else
    m_tunnelFactory->wait(QDeadlineTimer(20000));
#endif
  }
}

QVNCConnection::~QVNCConnection()
{
  m_closing = true;
  if (m_tunnelFactory) {
    m_tunnelFactory->close();
  }
  resetConnection();
  m_updateTimer->stop();
  delete m_updateTimer;
  delete m_tunnelFactory;
}

void QVNCConnection::bind(int fd)
{
  m_rfbcon->setStreams(&m_socket->inStream(), &m_socket->outStream());

  delete m_socketNotifier;
  m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(m_socketNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketNotified();
  });

  delete m_socketErrorNotifier;
  m_socketErrorNotifier = new QSocketNotifier(fd, QSocketNotifier::Exception);
  QObject::connect(m_socketErrorNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    if (!m_closing) {
      resetConnection();
      throw rdr::Exception("CConnection::bind: socket error.");
    }
  });
}

void QVNCConnection::connectToServer(const QString addressport)
{
  try {
    ViewerConfig::config()->saveViewerParameters("", addressport);
    if (addressport.contains("/")) {
#ifndef Q_OS_WIN
      delete m_socket;
      m_socket = new network::UnixSocket(addressport.toStdString().c_str());
      setHost(m_socket->getPeerAddress());
      vlog.info("Connected to socket %s", host().toStdString().c_str());
      bind(m_socket->getFd());
#endif
    }
    else {
      std::string shost;
      int port;
      rfb::getHostAndPort(addressport.toStdString().c_str(), &shost, &port);
      setHost(shost.c_str());
      setPort(port);
      delete m_socket;
      m_socket = new network::TcpSocket(shost.c_str(), port);
      bind(m_socket->getFd());
    }
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::listen()
{
  std::list<network::SocketListener*> listeners;
  try {
    bool ok;
    int port = ViewerConfig::config()->serverName().toInt(&ok);
    if (!ok) {
      port = 5500;
    }
    network::createTcpListeners(&listeners, 0, port);

    vlog.info(_("Listening on port %d"), port);

    /* Wait for a connection */
    while (m_socket == nullptr) {
      fd_set rfds;
      FD_ZERO(&rfds);
      for (network::SocketListener *listener : listeners) {
        FD_SET(listener->getFd(), &rfds);
      }

      int n = select(FD_SETSIZE, &rfds, 0, 0, 0);
      if (n < 0) {
        if (errno == EINTR) {
          vlog.debug("Interrupted select() system call");
          continue;
        }
        else {
          throw rdr::SystemException("select", errno);
        }
      }

      for (network::SocketListener *listener : listeners) {
        if (FD_ISSET(listener->getFd(), &rfds)) {
          m_socket = listener->accept();
          if (m_socket) {
            /* Got a connection */
            bind(m_socket->getFd());
            break;
          }
        }
      }
    }
  }
  catch (rdr::Exception &e) {
    vlog.error("%s", e.str());
    QCoreApplication::exit(1);
  }

  while (!listeners.empty()) {
    delete listeners.back();
    listeners.pop_back();
  }
}

void QVNCConnection::resetConnection()
{
  delete m_socketNotifier;
  m_socketNotifier = nullptr;
  delete m_socketErrorNotifier;
  m_socketErrorNotifier = nullptr;
  if (m_socket) {
    m_socket->shutdown();
  }
  delete m_socket;
  m_socket = nullptr;

  m_rfbcon->resetConnection();
}

void QVNCConnection::announceClipboard(bool available)
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  try {
    m_rfbcon->announceClipboard(available);
  }
  catch (rdr::Exception &e) {
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::refreshFramebuffer()
{
  try {
    //qDebug() << "QVNCConnection::refreshFramebuffer: m_continuousUpdates=" << m_continuousUpdates;
    emit refreshFramebufferStarted();
    m_rfbcon->refreshFramebuffer();
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::setState(int state)
{
  try {
    m_rfbcon->setProcessState(state);
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::startProcessing()
{
  if (!m_socket) {
    return;
  }
  try {
    size_t navailables0;
    size_t navailables = m_socket->inStream().avail();
    do {
      navailables0 = navailables;

      m_rfbcon->processMsg();

      //qDebug() << "pre-avail()  navailables=" << navailables;
      navailables = m_socket->inStream().avail();
      //qDebug() << "post-avail() navailables=" << navailables;
    } while (navailables > 0 && navailables != navailables0 && m_socket);
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

TunnelFactory::TunnelFactory()
 : QThread(nullptr)
 , m_errorOccurrrd(false)
 , m_error(QProcess::FailedToStart)
#if defined(WIN32)
 , m_command(QString(qgetenv("SYSTEMROOT")) + "\\System32\\OpenSSH\\ssh.exe")
#else
 , m_command("/usr/bin/ssh")
 , m_operationSocketName("vncviewer-tun-" + QString::number(QCoreApplication::applicationPid()))
#endif
 , m_process(nullptr)
{
}

void TunnelFactory::run()
{
  QString gatewayHost = ViewerConfig::config()->gatewayHost();
  if (gatewayHost.isEmpty()) {
    return;
  }
  QString remoteHost = ViewerConfig::config()->serverHost();
  if (remoteHost.isEmpty()) {
    return;
  }
  int remotePort = ViewerConfig::config()->serverPort();
  int localPort = ViewerConfig::config()->gatewayLocalPort();

  QString viacmd(qgetenv("VNC_VIA_CMD"));
  qputenv("G", gatewayHost.toUtf8());
  qputenv("H", remoteHost.toUtf8());
  qputenv("R", QString::number(remotePort).toUtf8());
  qputenv("L", QString::number(localPort).toUtf8());

  QStringList args;
  if (viacmd.isEmpty()) {
    args = QStringList({
#if !defined(WIN32)
                         "-fnNTM",
                         "-S",
                         m_operationSocketName,
#endif
                         "-L",
                         QString::number(localPort) + ":" + remoteHost + ":" + QString::number(remotePort),
                         gatewayHost,
                       });
  }
  else {
#if !defined(WIN32)
    /* Compatibility with TigerVNC's method. */
    viacmd.replace('%', '$');
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    args = splitCommand(viacmd);
#else
    args = QProcess::splitCommand(viacmd);
#endif
    m_command = args.length() > 0 ? args[0] : "";
    args.removeFirst();
  }
  delete m_process;
  m_process = new QProcess;

#if !defined(WIN32)
  if (!m_process->execute(m_command, args)) {
    QString serverName = "localhost::" + QString::number(ViewerConfig::config()->gatewayLocalPort());
    ViewerConfig::config()->setAccessPoint(serverName);
  }
  else {
    m_errorOccurrrd = true;
  }
#else
  connect(m_process, &QProcess::started, this, []() {
    QString serverName = "localhost::" + QString::number(ViewerConfig::config()->gatewayLocalPort());
    ViewerConfig::config()->setAccessPoint(serverName);
  });
  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
    m_errorOccurrrd = true;
    m_error = e;
  });
  m_process->start(m_command, args);
  while (true) {
    //qDebug() << "state=" << m_process->state();
    if (m_process->state() == QProcess::Running || m_errorOccurrrd) {
      break;
    }
    QThread::usleep(10);
  }
#endif
}

TunnelFactory::~TunnelFactory()
{
  close();
  delete m_process;
}

void TunnelFactory::close()
{
#if !defined(WIN32)
  if (m_process) {
    QString gatewayHost = ViewerConfig::config()->gatewayHost();
    QStringList args({ "-S",
                       m_operationSocketName,
                       "-O",
                       "exit",
                       gatewayHost,
                     });
    QProcess process;
    process.start(m_command, args);
    QThread::msleep(500);
  }
#endif
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
QStringList TunnelFactory::splitCommand(QStringView command)
{
    QStringList args;
    QString tmp;
    int quoteCount = 0;
    bool inQuote = false;
    // handle quoting. tokens can be surrounded by double quotes
    // "hello world". three consecutive double quotes represent
    // the quote character itself.
    for (int i = 0; i < command.size(); ++i) {
        if (command.at(i) == QLatin1Char('"')) {
            ++quoteCount;
            if (quoteCount == 3) {
                // third consecutive quote
                quoteCount = 0;
                tmp += command.at(i);
            }
            continue;
        }
        if (quoteCount) {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && command.at(i).isSpace()) {
            if (!tmp.isEmpty()) {
                args += tmp;
                tmp.clear();
            }
        } else {
            tmp += command.at(i);
        }
    }
    if (!tmp.isEmpty())
        args += tmp;
    return args;
}
#endif
