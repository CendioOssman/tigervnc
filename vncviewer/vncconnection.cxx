#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
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
#include "parameters.h"
#include "appmanager.h"
#include "i18n.h"
#undef asprintf
#include "abstractvncview.h"
#include "tunnelfactory.h"
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
 , rfbcon_(nullptr)
 , socket_(nullptr)
 , socketReadNotifier_(nullptr)
 , socketWriteNotifier_(nullptr)
 , socketErrorNotifier_(nullptr)
 , updateTimer_(nullptr)
 , tunnelFactory_(nullptr)
 , closing_(false)
{
  connect(this, &QVNCConnection::socketReadNotified, this, &QVNCConnection::startProcessing);
  connect(this, &QVNCConnection::socketWriteNotified, this, &QVNCConnection::flushSocket);

  connect(this, &QVNCConnection::writePointerEvent, this, [this](const rfb::Point &pos, int buttonMask) {
    try {
      rfbcon_->writer()->writePointerEvent(pos, buttonMask);
    }
    catch (rdr::Exception &e) {
      AppManager::instance()->publishError(e.str());
    }
    catch (int &e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });
  connect(this, &QVNCConnection::writeSetDesktopSize, this, [this](int width, int height, const rfb::ScreenSet &layout) {
    try {
      rfbcon_->writer()->writeSetDesktopSize(width, height, layout);
    }
    catch (rdr::Exception &e) {
      AppManager::instance()->publishError(e.str());
    }
    catch (int &e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });
  connect(this, &QVNCConnection::writeKeyEvent, this, [this](uint32_t keysym, uint32_t keycode, bool down) {
    try {
      rfbcon_->writer()->writeKeyEvent(keysym, keycode, down);
    }
    catch (rdr::Exception &e) {
      AppManager::instance()->publishError(e.str());
    }
    catch (int &e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });
  
  if (ViewerConfig::config()->listenModeEnabled()) {
    listen();
  }

  updateTimer_ = new QTimer;
  updateTimer_->setSingleShot(true);
  connect(updateTimer_, &QTimer::timeout, this, [this]() {
    try {
      rfbcon_->framebufferUpdateEnd();
    }
    catch (rdr::Exception &e) {
      AppManager::instance()->publishError(e.str());
    }
    catch (int &e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });

  QString gatewayHost = ViewerConfig::config()->gatewayHost();
  QString remoteHost = ViewerConfig::config()->serverHost();
  if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
    tunnelFactory_ = new TunnelFactory;
    tunnelFactory_->start();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    tunnelFactory_->wait(20000);
#else
    tunnelFactory_->wait(QDeadlineTimer(20000));
#endif
  }
}

QVNCConnection::~QVNCConnection()
{
  closing_ = true;
  if (tunnelFactory_) {
    tunnelFactory_->close();
  }
  resetConnection();
  updateTimer_->stop();
  delete updateTimer_;
  delete tunnelFactory_;
}

void QVNCConnection::bind(int fd)
{
  rfbcon_->setStreams(&socket_->inStream(), &socket_->outStream());

  delete socketReadNotifier_;
  socketReadNotifier_ = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(socketReadNotifier_, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketReadNotified();
  });

  delete socketWriteNotifier_;
  socketWriteNotifier_ = new QSocketNotifier(fd, QSocketNotifier::Write);
  socketWriteNotifier_->setEnabled(false);
  QObject::connect(socketWriteNotifier_, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketWriteNotified();
  });

  delete socketErrorNotifier_;
  socketErrorNotifier_ = new QSocketNotifier(fd, QSocketNotifier::Exception);
  QObject::connect(socketErrorNotifier_, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    if (!closing_) {
      resetConnection();
      throw rdr::Exception("CConnection::bind: socket error.");
    }
  });
}

void QVNCConnection::connectToServer(QString addressport)
{
  try {
    if (addressport.isEmpty()) {
      resetConnection();
      addressport = addressport_;
    }
    else {
      addressport_ = addressport;
    }
    delete rfbcon_;
    rfbcon_ = new CConn(this);
    ViewerConfig::config()->saveViewerParameters("", addressport);
    if (addressport.contains("/")) {
#ifndef Q_OS_WIN
      delete socket_;
      socket_ = new network::UnixSocket(addressport.toStdString().c_str());
      setHost(socket_->getPeerAddress());
      vlog.info("Connected to socket %s", host().toStdString().c_str());
      bind(socket_->getFd());
#endif
    }
    else {
      std::string shost;
      int port;
      rfb::getHostAndPort(addressport.toStdString().c_str(), &shost, &port);
      setHost(shost.c_str());
      setPort(port);
      delete socket_;
      socket_ = new network::TcpSocket(shost.c_str(), port);
      bind(socket_->getFd());
    }
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str(), true);
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e), true);
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
    while (socket_ == nullptr) {
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
          socket_ = listener->accept();
          if (socket_) {
            /* Got a connection */
            bind(socket_->getFd());
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
  AppManager::instance()->closeVNCWindow();
  delete socketReadNotifier_;
  socketReadNotifier_ = nullptr;
  delete socketWriteNotifier_;
  socketWriteNotifier_ = nullptr;
  delete socketErrorNotifier_;
  socketErrorNotifier_ = nullptr;
  if (socket_) {
    socket_->shutdown();
  }
  delete socket_;
  socket_ = nullptr;

  if (rfbcon_) {
    rfbcon_->resetConnection();
  }
  delete rfbcon_;
  rfbcon_ = nullptr;
}

void QVNCConnection::announceClipboard(bool available)
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  try {
    rfbcon_->announceClipboard(available);
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
    //qDebug() << "QVNCConnection::refreshFramebuffer: continuousUpdates_=" << continuousUpdates_;
    emit refreshFramebufferStarted();
    rfbcon_->refreshFramebuffer();
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
    rfbcon_->setProcessState(state);
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
  if (!socket_) {
    return;
  }
  try {
    size_t navailables0;
    size_t navailables = socket_->inStream().avail();
    do {
      navailables0 = navailables;

      socket_->outStream().flush();
      rfbcon_->getOutStream()->cork(true);

      rfbcon_->processMsg();

      rfbcon_->getOutStream()->cork(false);

      //qDebug() << "pre-avail()  navailables=" << navailables;
      navailables = socket_->inStream().avail();
      //qDebug() << "post-avail() navailables=" << navailables;
    } while (navailables > 0 && navailables != navailables0 && socket_);
  }
  catch (rdr::Exception &e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  }
  catch (int &e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }

  if (socket_)
    socketWriteNotifier_->setEnabled(socket_->outStream().hasBufferedData());
}

void QVNCConnection::flushSocket()
{
  if (!socket_) {
    return;
  }

  socket_->outStream().flush();

  socketWriteNotifier_->setEnabled(socket_->outStream().hasBufferedData());
}
