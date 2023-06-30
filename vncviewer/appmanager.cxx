#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QQmlEngine>
#include <QTcpSocket>
#include <QScreen>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#if defined(__APPLE__)
#include <QtQuickWidgets/QtQuickWidgets>
#include <QQuickWindow>
#include "cocoa.h"
#endif
#if defined(Q_OS_UNIX)
#include <QApplication>
#endif

#include "rdr/Exception.h"
#include "rfb/Timer.h"
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
 , rfbTimerProxy_(new QTimer)
 , visibleInfo_(false)
#if defined(__APPLE__)
 , overlay_(new QQuickWidget(scroll_))
#endif
{
  connect(this, &AppManager::connectToServerRequested, facade_, &QVNCConnection::connectToServer);
  connect(facade_, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, facade_, &QVNCConnection::resetConnection);
  connect(rfbTimerProxy_, &QTimer::timeout, this, [this]() {
    rfbTimerProxy_->setInterval(rfb::Timer::checkTimeouts());
  });
  rfbTimerProxy_->setSingleShot(false);
  rfbTimerProxy_->setInterval(0);
  rfbTimerProxy_->start();

#if defined(__APPLE__)
  overlay_->setAttribute(Qt::WA_NativeWindow);
  overlay_->setResizeMode(QQuickWidget::SizeViewToRootObject);
  overlay_->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
#endif
}

AppManager::~AppManager()
{
  facade_->deleteLater();
  scroll_->takeWidget();
  scroll_->deleteLater();
  if (view_) {
    view_->deleteLater();
  }
  rfbTimerProxy_->deleteLater();
#if defined(__APPLE__)
  if (overlay_) {
    overlay_->deleteLater();
  }
#endif
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
  QString text(message);
  if (!quit) {
    text = QString::asprintf(_("%s\n\nAttempt to reconnect?"), message.toStdString().c_str());
  }
#if defined(__APPLE__)
  openOverlay("qrc:/qml/AlertDialogContent.qml", _("TigerVNC Viewer"), text.toStdString().c_str());
  if (quit) {
    QGuiApplication::exit(0);
  }
#else
  emit errorOcurred(error_++, text, quit);
#endif
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
  if (!ViewerConfig::config()->remoteResize()) {
    view_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    view_->setMinimumSize(QSize(width, height));
    view_->setMaximumSize(QSize(width, height));
  }
  scroll_->setWidget(view_);
  scroll_->normalizedResize(width, height);
  scroll_->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  scroll_->show();

  if (ViewerConfig::config()->fullScreen()) {
    view_->fullscreen(true);
  }

  emit vncWindowOpened();
}

void AppManager::closeVNCWindow()
{
  QWidget *w = scroll_->takeWidget();
  if (w) {
    scroll_->setVisible(false);
    w->setVisible(false);
    w->deleteLater();
    view_ = nullptr;
    emit vncWindowClosed();
  }
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
  view_->dim(true);
  visibleInfo_ = true;
  emit visibleInfoChanged();
#if defined(__APPLE__)
  openOverlay("qrc:/qml/InfoDialogContent.qml", _("VNC connection info"));
#else
  emit infoDialogRequested();
#endif
}

void AppManager::openOptionDialog()
{
  view_->dim(true);
#if defined(__APPLE__)
  openOverlay("qrc:/qml/OptionDialogContent.qml", _("TigerVNC Options"));
#else
  emit optionDialogRequested();
#endif
}

void AppManager::openAboutDialog()
{
  view_->dim(true);
#if defined(__APPLE__)
  openOverlay("qrc:/qml/AboutDialogContent.qml", _("About TigerVNC Viewer"));
#else
  emit aboutDialogRequested();
#endif
}

void AppManager::respondToMessage(int response)
{
  emit messageResponded(response);
}

#if defined(__APPLE__)
void AppManager::openOverlay(QString qml, const char *title, const char *message)
{
  overlay_->setWindowTitle(title);
  overlay_->setSource(QUrl(qml));
  overlay_->show();
  WId winid = overlay_->winId();
  cocoa_set_overlay_property(winid);
  if (message) {
    QQuickItem *item = overlay_->rootObject()->findChild<QQuickItem*>("AlertDialogMessageText");
    if (item) {
      qDebug() << "AppManager::openOverlay: message=" << message;
      item->setProperty("text", message);
    }
  }
  connect(this, &AppManager::closeOverlayRequested, overlay_, &QQuickWidget::hide);
}
#endif

void AppManager::closeOverlay()
{
  if (view_) {
    view_->dim(false);
  }
  visibleInfo_ = false;
  emit visibleInfoChanged();
#if defined(__APPLE__)
  emit closeOverlayRequested();
#endif
}
