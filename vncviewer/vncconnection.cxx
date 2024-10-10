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
  , updateTimer(nullptr)
{
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
  updateTimer->stop();
  delete updateTimer;
}

void QVNCConnection::connectToServer(const char* vncserver)
{
  if (::listenMode) {
    return;
  }

  QString address = vncserver;
  try {
    network::Socket* socket;
    QString serverHost;
    int serverPort;
#ifndef Q_OS_WIN
    if (address.contains("/")) {
      socket = new network::UnixSocket(address.toStdString().c_str());
      serverHost = socket->getPeerAddress();
      serverPort = 0;
      vlog.info(_("Connected to socket %s"), serverHost.toStdString().c_str());
    } else
#endif
    {
      std::string shost;
      int port;
      rfb::getHostAndPort(address.toStdString().c_str(), &shost, &port);
      serverHost = shost.c_str();
      serverPort = port;
      socket = new network::TcpSocket(shost.c_str(), port);
      vlog.info(_("Connected to host %s port %d"),
                shost.c_str(), port);
    }
    rfbcon = new CConn(this, address.toStdString().c_str(), socket);
    setHost(serverHost);
    setPort(serverPort);
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
  rfbcon = new CConn(this, "", sock);
}
