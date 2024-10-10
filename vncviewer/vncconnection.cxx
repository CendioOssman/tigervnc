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

  rfbcon = new CConn(this, vncserver, sock);
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
