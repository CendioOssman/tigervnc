#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appmanager.h"
#include "i18n.h"
#include "network/TcpSocket.h"
#include "parameters.h"
#include "viewerconfig.h"
#include "rfb/CMsgWriter.h"
#include "rfb/Exception.h"
#include "rfb/Hostname.h"
#include "rfb/LogWriter.h"

#include <QApplication>
#include <QClipboard>
#include <QLocalSocket>
#include <QProcess>
#include <QSocketNotifier>
#include <QTcpSocket>
#include <QTimer>
#undef asprintf
#include "CConn.h"
#include "abstractvncview.h"
#include "tunnelfactory.h"
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
  , rfbcon(nullptr)
  , socket(nullptr)
  , socketReadNotifier(nullptr)
  , socketWriteNotifier(nullptr)
  , socketErrorNotifier(nullptr)
  , updateTimer(nullptr)
  , tunnelFactory(nullptr)
  , closing(false)
{
  connect(this, &QVNCConnection::socketReadNotified, this, &QVNCConnection::startProcessing);
  connect(this, &QVNCConnection::socketWriteNotified, this, &QVNCConnection::flushSocket);

  connect(this, &QVNCConnection::writePointerEvent, this, [this](const rfb::Point& pos, int buttonMask) {
    try {
      rfbcon->writer()->writePointerEvent(pos, buttonMask);
    } catch (rdr::Exception& e) {
      AppManager::instance()->publishError(e.str());
    } catch (int& e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });
  connect(this,
          &QVNCConnection::writeSetDesktopSize,
          this,
          [this](int width, int height, const rfb::ScreenSet& layout) {
            try {
              rfbcon->writer()->writeSetDesktopSize(width, height, layout);
            } catch (rdr::Exception& e) {
              AppManager::instance()->publishError(e.str());
            } catch (int& e) {
              AppManager::instance()->publishError(strerror(e));
            }
          });
  connect(this, &QVNCConnection::writeKeyEvent, this, [this](uint32_t keysym, uint32_t keycode, bool down) {
    try {
      rfbcon->writer()->writeKeyEvent(keysym, keycode, down);
    } catch (rdr::Exception& e) {
      AppManager::instance()->publishError(e.str());
    } catch (int& e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });

  if (::listenMode) {
    listen();
  }

  updateTimer = new QTimer;
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, [this]() {
    try {
      rfbcon->framebufferUpdateEnd();
    } catch (rdr::Exception& e) {
      AppManager::instance()->publishError(e.str());
    } catch (int& e) {
      AppManager::instance()->publishError(strerror(e));
    }
  });

  QString gatewayHost = ViewerConfig::instance()->gatewayHost();
  QString remoteHost = ViewerConfig::instance()->getServerHost();
  if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
    tunnelFactory = new TunnelFactory;
    tunnelFactory->start();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    tunnelFactory_->wait(20000);
#else
    tunnelFactory->wait(QDeadlineTimer(20000));
#endif
  }
}

QVNCConnection::~QVNCConnection()
{
  closing = true;
  if (tunnelFactory) {
    tunnelFactory->close();
  }
  resetConnection();
  updateTimer->stop();
  delete updateTimer;
  delete tunnelFactory;
}

void QVNCConnection::bind(int fd)
{
  rfbcon->setStreams(&socket->inStream(), &socket->outStream());

  delete socketReadNotifier;
  socketReadNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(socketReadNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketReadNotified();
  });

  delete socketWriteNotifier;
  socketWriteNotifier = new QSocketNotifier(fd, QSocketNotifier::Write);
  socketWriteNotifier->setEnabled(false);
  QObject::connect(socketWriteNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    emit socketWriteNotified();
  });

  delete socketErrorNotifier;
  socketErrorNotifier = new QSocketNotifier(fd, QSocketNotifier::Exception);
  QObject::connect(socketErrorNotifier, &QSocketNotifier::activated, this, [this](int fd) {
    Q_UNUSED(fd)
    if (!closing) {
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
      this->addressport = addressport;
    } else {
      this->addressport = addressport;
    }
    delete rfbcon;
    rfbcon = new CConn(this);
    ViewerConfig::instance()->saveViewerParameters("", addressport);
    if (addressport.contains("/")) {
#ifndef Q_OS_WIN
      delete socket;
      socket = new network::UnixSocket(addressport.toStdString().c_str());
      setHost(socket->getPeerAddress());
      vlog.info("Connected to socket %s", host().toStdString().c_str());
      bind(socket->getFd());
#endif
    } else {
      std::string shost;
      int port;
      rfb::getHostAndPort(addressport.toStdString().c_str(), &shost, &port);
      setHost(shost.c_str());
      setPort(port);
      delete socket;
      socket = new network::TcpSocket(shost.c_str(), port);
      bind(socket->getFd());
    }
  } catch (rdr::Exception& e) {
    resetConnection();
    AppManager::instance()->publishError(e.str(), true);
  } catch (int& e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e), true);
  }
}

void QVNCConnection::listen()
{
  std::list<network::SocketListener*> listeners;
  try {
    bool ok;
    int port = ViewerConfig::instance()->getServerName().toInt(&ok);
    if (!ok) {
      port = 5500;
    }
    network::createTcpListeners(&listeners, 0, port);

    vlog.info(_("Listening on port %d"), port);

    /* Wait for a connection */
    while (socket == nullptr) {
      fd_set rfds;
      FD_ZERO(&rfds);
      for (network::SocketListener* listener : listeners) {
        FD_SET(listener->getFd(), &rfds);
      }

      int n = select(FD_SETSIZE, &rfds, 0, 0, 0);
      if (n < 0) {
        if (errno == EINTR) {
          vlog.debug("Interrupted select() system call");
          continue;
        } else {
          throw rdr::SystemException("select", errno);
        }
      }

      for (network::SocketListener* listener : listeners) {
        if (FD_ISSET(listener->getFd(), &rfds)) {
          socket = listener->accept();
          if (socket) {
            /* Got a connection */
            bind(socket->getFd());
            break;
          }
        }
      }
    }
  } catch (rdr::Exception& e) {
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
  delete socketReadNotifier;
  socketReadNotifier = nullptr;
  delete socketWriteNotifier;
  socketWriteNotifier = nullptr;
  delete socketErrorNotifier;
  socketErrorNotifier = nullptr;
  if (socket) {
    socket->shutdown();
  }
  delete socket;
  socket = nullptr;

  if (rfbcon) {
    rfbcon->resetConnection();
  }
  delete rfbcon;
  rfbcon = nullptr;
}

void QVNCConnection::announceClipboard(bool available)
{
  if (::viewOnly) {
    return;
  }
  try {
    rfbcon->announceClipboard(available);
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::refreshFramebuffer()
{
  try {
    emit refreshFramebufferStarted();
    rfbcon->refreshFramebuffer();
  } catch (rdr::Exception& e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::sendClipboardData(QString data)
{
  try {
    rfbcon->sendClipboardContent(data.toStdString().c_str());
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::requestClipboard()
{
  try {
    rfbcon->requestClipboard();
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::setState(int state)
{
  try {
    rfbcon->setProcessState(state);
  } catch (rdr::Exception& e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::startProcessing()
{
  if (!socket) {
    return;
  }
  try {
    rfbcon->getOutStream()->cork(true);

    while (rfbcon->processMsg()) {
      QApplication::processEvents();
      if (!socket)
        break;
    }

    rfbcon->getOutStream()->cork(false);
  } catch (rdr::Exception& e) {
    resetConnection();
    AppManager::instance()->publishError(e.str());
  } catch (int& e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }

  if (socket)
    socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());
}

void QVNCConnection::flushSocket()
{
  if (!socket) {
    return;
  }

  socket->outStream().flush();

  socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());
}
