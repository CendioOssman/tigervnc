#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QQmlEngine>
#include <QTcpSocket>
#include <QScreen>
#include <QProcess>
#if defined(Q_OS_UNIX)
#include <QApplication>
#endif

#include "rdr/Exception.h"
#include "i18n.h"
#include "vncconnection.h"
#include "parameters.h"
#include "abstractvncview.h"
#include "vncwindow.h"
#include "appmanager.h"
#undef asprintf

#if defined(WIN32)
#include "vncwinview.h"
#elif defined(__APPLE__)
#include "vncmacview.h"
#elif defined(Q_OS_UNIX)
#include "vncx11view.h"
#endif

AppManager *AppManager::manager_;

AppManager::AppManager()
 : QObject(nullptr)
 , error_(0)
 , facade_(new QVNCConnection)
 , view_(nullptr)
 , scroll_(new QVNCWindow)
{
  connect(this, &AppManager::connectToServerRequested, facade_, &QVNCConnection::connectToServer);
  connect(facade_, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, facade_, &QVNCConnection::resetConnection);
}

AppManager::~AppManager()
{
  facade_->deleteLater();
  scroll_->takeWidget();
  delete scroll_;
  delete view_;
}

int AppManager::initialize()
{
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
  qmlRegisterType<QVNCConnection>("Qt.TigerVNC", 1, 0, "VNCConnection");
  manager_ = new AppManager();
  qmlRegisterSingletonType<AppManager>("Qt.TigerVNC", 1, 0, "AppManager", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return manager_;
  });
  return 0;
}

void AppManager::connectToServer(const QString addressport)
{
  emit connectToServerRequested(addressport);
}

void AppManager::authenticate(QString user, QString password)
{
  emit authenticateRequested(user, password);
}

void AppManager::cancelAuth()
{
  emit cancelAuthRequested();
}

void AppManager::resetConnection()
{
  emit resetConnectionRequested();
}

void AppManager::publishError(const QString message, bool quit)
{
  emit errorOcurred(error_++, message, quit);
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  scroll_->takeWidget();
  delete view_;
#if defined(WIN32)
  view_ = new QVNCWinView(scroll_);
#elif defined(__APPLE__)
  view_ = new QVNCMacView(scroll_);
#elif defined(Q_OS_UNIX)
  QString platform = QApplication::platformName();
  if (platform == "xcb") {
    view_ = new QVNCX11View(scroll_);
  }
  else if (platform == "wayland") {
    ;
  }
#endif

  if (!view_) {
    throw rdr::Exception(_("Platform not supported."));
  }
  connect(view_, &QAbstractVNCView::fullscreenChanged, this, [this](bool enabled) {
    scroll_->setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
  }, Qt::QueuedConnection);

  connect(view_, &QAbstractVNCView::delayedInitialized, scroll_, &QVNCWindow::popupToast);
  view_->resize(width, height);
  scroll_->setWidget(view_);
  scroll_->normalizedResize(width, height);
  scroll_->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  scroll_->show();

  if (ViewerConfig::config()->fullScreen()) {
    view_->fullscreen(true);
  }

  emit vncWindowOpened();
}

void AppManager::setWindowName(QString name)
{
  scroll_->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
}

void AppManager::invalidate(int x0, int y0, int x1, int y1)
{
  emit invalidateRequested(x0, y0, x1, y1);
}

void AppManager::refresh()
{
  emit refreshRequested();
}

void AppManager::openContextMenu()
{
  emit contextMenuRequested();
}

void AppManager::openInfoDialog()
{
  emit infoDialogRequested();
}

void AppManager::openOptionDialog()
{
  emit optionDialogRequested();
}

void AppManager::openAboutDialog()
{
  emit aboutDialogRequested();
}

void AppManager::respondToMessage(int response)
{
  emit messageResponded(response);
}
