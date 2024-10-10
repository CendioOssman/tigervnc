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
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>
#undef asprintf
#include "CConn.h"
#include "Viewport.h"
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

QVNCConnection::QVNCConnection(const char* vncserver, network::Socket* sock)
  : QObject(nullptr)
  , rfbcon(nullptr)
  , socket(nullptr)
  , socketReadNotifier(nullptr)
  , socketWriteNotifier(nullptr)
  , updateTimer(nullptr)
{
  connect(this, &QVNCConnection::socketReadNotified, this, &QVNCConnection::startProcessing);
  connect(this, &QVNCConnection::socketWriteNotified, this, &QVNCConnection::flushSocket);

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

  if (sock) {
    listen(sock);
  } else {
    connectToServer(vncserver);
  }
}

QVNCConnection::~QVNCConnection()
{
  if (rfbcon) {
    delete rfbcon;
  }
  rfbcon = nullptr;

  delete socketReadNotifier;
  socketReadNotifier = nullptr;
  delete socketWriteNotifier;
  socketWriteNotifier = nullptr;
  if (socket) {
    socket->shutdown();
  }
  delete socket;
  socket = nullptr;
  updateTimer->stop();
  delete updateTimer;
}

void QVNCConnection::bind(int fd)
{
  rfbcon->setStreams(&socket->inStream(), &socket->outStream());

  delete socketReadNotifier;
  socketReadNotifier = new QSocketNotifier(fd, QSocketNotifier::Read);
  QObject::connect(socketReadNotifier, &QSocketNotifier::activated, this, [this](int) {
    emit socketReadNotified();
  });

  delete socketWriteNotifier;
  socketWriteNotifier = new QSocketNotifier(fd, QSocketNotifier::Write);
  socketWriteNotifier->setEnabled(false);
  QObject::connect(socketWriteNotifier, &QSocketNotifier::activated, this, [this](int) {
    emit socketWriteNotified();
  });
}

void QVNCConnection::connectToServer(const char* vncserver)
{
  if (::listenMode) {
    return;
  }

  QString address = vncserver;
  try {
    delete rfbcon;
    rfbcon = new CConn(this);
    if (address.contains("/")) {
#ifndef Q_OS_WIN
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
      socket = new network::TcpSocket(shost.c_str(), port);
      vlog.info(_("Connected to host %s port %d"),
                shost.c_str(), port);

      bind(socket->getFd());
    }
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    AppManager::instance()->publishError(QString::asprintf(_("Failed to connect to \"%s\":\n\n%s"),
                                                           address.toStdString().c_str(), e.str()));
  } catch (int& e) {
    AppManager::instance()->publishError(strerror(e));
  }
}

void QVNCConnection::listen(network::Socket* sock)
{
  rfbcon = new CConn(this);
  socket = sock;
  bind(socket->getFd());
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
    // FIXME
    if (!rfbcon || !rfbcon->desktop) {
      vlog.error(_("The connection was dropped by the server before "
                   "the session could be established."));
      QString message = _("The connection was dropped by the server "
                        "before the session could be established.");
      AppManager::instance()->publishError(message);
    } else {
      qApp->quit();
    }
  } catch (rdr::Exception& e) {
    recursing = false;
    AppManager::instance()->publishUnexpectedError(e.str());
  } catch (int& e) {
    recursing = false;
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
