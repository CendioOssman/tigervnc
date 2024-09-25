#include "vncconnection.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
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
  , updateTimer(nullptr)
  , tunnelFactory(nullptr)
{
  connect(this, &QVNCConnection::socketReadNotified, this, &QVNCConnection::startProcessing);
  connect(this, &QVNCConnection::socketWriteNotified, this, &QVNCConnection::flushSocket);

  connect(this, &QVNCConnection::writePointerEvent, this, [this](const rfb::Point& pos, int buttonMask) {
    try {
      if (rfbcon) {
        rfbcon->writer()->writePointerEvent(pos, buttonMask);
      }
    } catch (rdr::Exception& e) {
      AppManager::instance()->publishUnexpectedError(e.str());
    } catch (int& e) {
      AppManager::instance()->publishUnexpectedError(strerror(e));
    }
  });
  connect(this,
          &QVNCConnection::writeSetDesktopSize,
          this,
          [this](int width, int height, const rfb::ScreenSet& layout) {
            try {
              if (rfbcon) {
                rfbcon->writer()->writeSetDesktopSize(width, height, layout);
              }
            } catch (rdr::Exception& e) {
              AppManager::instance()->publishError(e.str());
            } catch (int& e) {
              AppManager::instance()->publishError(strerror(e));
            }
          });
  connect(this, &QVNCConnection::writeKeyEvent, this, [this](uint32_t keysym, uint32_t keycode, bool down) {
    try {
      if (rfbcon) {
        rfbcon->writer()->writeKeyEvent(keysym, keycode, down);
      }
    } catch (rdr::Exception& e) {
      AppManager::instance()->publishUnexpectedError(e.str());
    } catch (int& e) {
      AppManager::instance()->publishUnexpectedError(strerror(e));
    }
  });
}

void QVNCConnection::initialize()
{
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

  QString gatewayHost = ViewerConfig::instance()->getGatewayHost();
  QString remoteHost = ViewerConfig::instance()->getServerHost();
  if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
    tunnelFactory = new TunnelFactory;
    tunnelFactory->start();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    tunnelFactory->wait(20000);
#else
    tunnelFactory->wait(QDeadlineTimer(20000));
#endif
  }
}

QVNCConnection::~QVNCConnection()
{
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
}

void QVNCConnection::connectToServer()
{
  if (::listenMode) {
    return;
  }

  QString address;
  try {
    resetConnection();
    address = ViewerConfig::instance()->getFinalAddress();
    delete rfbcon;
    rfbcon = new CConn(this);
    try {
      ViewerConfig::instance()->saveViewerParameters("", address);
    } catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
      AppManager::instance()->publishError(QString::asprintf(_("Unable to save the default configuration:\n\n%s"),
                                                             e.str()));
    }
    if (address.contains("/")) {
#ifndef Q_OS_WIN
      delete socket;
      socket = new network::UnixSocket(address.toStdString().c_str());
      setHost(socket->getPeerAddress());
      vlog.info(_("Connected to socket %s"), host().toStdString().c_str());
      bind(socket->getFd());
#endif
    } else {
      std::string shost;
      int port;
      rfb::getHostAndPort(address.toStdString().c_str(), &shost, &port);
      setHost(shost.c_str());
      setPort(port);
      delete socket;
      socket = new network::TcpSocket(shost.c_str(), port);
      vlog.info(_("Connected to host %s port %d"),
                shost.c_str(), port);

      bind(socket->getFd());
    }
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    resetConnection();
    AppManager::instance()->publishError(QString::asprintf(_("Failed to connect to \"%s\":\n\n%s"),
                                                           address.toStdString().c_str(), e.str()));
  } catch (int& e) {
    resetConnection();
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::listen()
{
  rfbcon = new CConn(this);
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
    AppManager::instance()->publishError(QString::asprintf(_("Failure waiting for incoming VNC connection:\n\n%s"), e.str()));
    QCoreApplication::exit(1);
  }

  while (!listeners.empty()) {
    delete listeners.back();
    listeners.pop_back();
  }
}

void QVNCConnection::resetConnection()
{
  if (rfbcon) {
    delete rfbcon;
  }
  rfbcon = nullptr;

  AppManager::instance()->closeVNCWindow();
  delete socketReadNotifier;
  socketReadNotifier = nullptr;
  delete socketWriteNotifier;
  socketWriteNotifier = nullptr;
  if (socket) {
    socket->shutdown();
  }
  delete socket;
  socket = nullptr;
}

void QVNCConnection::announceClipboard(bool available)
{
  if (::viewOnly) {
    return;
  }
  try {
    if (rfbcon) {
      rfbcon->announceClipboard(available);
    }
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishUnexpectedError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishUnexpectedError(strerror(e));
  }
}

void QVNCConnection::refreshFramebuffer()
{
  try {
    emit refreshFramebufferStarted();
    if (rfbcon) {
      rfbcon->refreshFramebuffer();
    }
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
    if (rfbcon) {
      rfbcon->sendClipboardContent(data.toStdString().c_str());
    }
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishUnexpectedError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishUnexpectedError(strerror(e));
  }
}

void QVNCConnection::requestClipboard()
{
  try {
    if (rfbcon) {
      rfbcon->requestClipboard();
    }
  } catch (rdr::Exception& e) {
    AppManager::instance()->publishUnexpectedError(e.str());
  } catch (int& e) {
    AppManager::instance()->publishUnexpectedError(strerror(e));
  }
}

void QVNCConnection::setState(int state)
{
  try {
    if (rfbcon) {
      rfbcon->setProcessState(state);
    }
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
  static bool recursing = false;

  if (!socket) {
    return;
  }

  // I don't think processMsg() is recursion safe, so add this check
  assert(!recursing);

  recursing = true;
  socketReadNotifier->setEnabled(false);
  socketWriteNotifier->setEnabled(false);

  try {
    if (rfbcon) {
      rfbcon->getOutStream()->cork(true);

      while (rfbcon->processMsg()) {
        QApplication::processEvents();
        if (!socket)
          break;
      }

      rfbcon->getOutStream()->cork(false);
    }
  } catch (rdr::EndOfStream& e) {
    recursing = false;
    vlog.info("%s", e.str());
    if (!AppManager::instance()->getView()) {
      vlog.error(_("The connection was dropped by the server before "
                   "the session could be established."));
      QString message = _("The connection was dropped by the server "
                        "before the session could be established.");
      resetConnection();
      AppManager::instance()->publishError(message);
    } else {
      resetConnection();
      qApp->quit();
    }
  } catch (rdr::Exception& e) {
    recursing = false;
    resetConnection();
    AppManager::instance()->publishUnexpectedError(e.str());
  } catch (int& e) {
    recursing = false;
    resetConnection();
    AppManager::instance()->publishUnexpectedError(strerror(e));
  }

  if (socket) {
    socketReadNotifier->setEnabled(true);
    socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());
  }

  recursing = false;
}

void QVNCConnection::flushSocket()
{
  if (!socket) {
    return;
  }

  socket->outStream().flush();

  socketWriteNotifier->setEnabled(socket->outStream().hasBufferedData());
}
