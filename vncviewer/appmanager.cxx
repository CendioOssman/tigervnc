#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QDebug>
#include <QProcess>
#include <QScreen>
#include <QTcpSocket>
#include <QTimer>
#if defined(__APPLE__)
#include "cocoa.h"
#endif
#include "i18n.h"
#include "rdr/Exception.h"
#include "rfb/Timer.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#undef asprintf
#include "aboutdialog.h"
#include "abstractvncview.h"
#include "alertdialog.h"
#include "appmanager.h"
#include "authdialog.h"
#include "infodialog.h"
#include "messagedialog.h"
#include "optionsdialog.h"
#include "parameters.h"
#include "vncconnection.h"
#include "vncwindow.h"
#undef asprintf

#if defined(WIN32)
#include "vncwinview.h"
#elif defined(__APPLE__)
#include "vncmacview.h"
#elif defined(Q_OS_UNIX)
#include "vncx11view.h"
#endif

AppManager* AppManager::manager_;

AppManager::AppManager()
  : QObject(nullptr)
  , error_(0)
  , facade_(new QVNCConnection)
  , view_(nullptr)
  , scroll_(new QVNCWindow)
  , rfbTimerProxy_(new QTimer)
{
  connect(this, &AppManager::connectToServerRequested, facade_, &QVNCConnection::connectToServer);
  connect(facade_, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, facade_, &QVNCConnection::resetConnection);
  connect(rfbTimerProxy_, &QTimer::timeout, this, []() {
    rfb::Timer::checkTimeouts();
  });
  connect(QApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, [this]() {
    int next = rfb::Timer::checkTimeouts();
    if (next != 0)
      rfbTimerProxy_->start(next);
  });
  rfbTimerProxy_->setSingleShot(true);

  connect(this, &AppManager::credentialRequested, this, [=](bool secured, bool userNeeded, bool passwordNeeded) {
    AuthDialog d(secured, userNeeded, passwordNeeded);
    d.exec();
  });
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
}

int AppManager::initialize()
{
  manager_ = new AppManager();
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
  error_++;

  AlertDialog d(message, quit);
  d.exec();
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
  } else if (platform == "wayland") {
    ;
  }
#endif

  if (!view_) {
    throw rdr::Exception(_("Platform not supported."));
  }
  connect(view_, &QAbstractVNCView::bufferResized, scroll_, &QVNCWindow::updateScrollbars, Qt::QueuedConnection);
  connect(view_,
          &QAbstractVNCView::remoteResizeRequest,
          scroll_,
          &QVNCWindow::postRemoteResizeRequest,
          Qt::QueuedConnection);

  view_->resize(width, height);
  scroll_->setWidget(view_);
  scroll_->resize(width, height);
  scroll_->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  scroll_->show();

  if (ViewerConfig::config()->fullScreen()) {
    scroll_->fullscreen(true);
  }

  emit vncWindowOpened();
}

void AppManager::closeVNCWindow()
{
  QWidget* w = scroll_->takeWidget();
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
  InfoDialog d;
  d.exec();
}

void AppManager::openOptionDialog()
{
  OptionsDialog d;
  d.exec();
}

void AppManager::openAboutDialog()
{
  AboutDialog d;
  d.exec();
}

void AppManager::openMessageDialog(int flags, QString title, QString text)
{
  MessageDialog d(flags, title, text);
  int response = d.exec() == QDialog::Accepted ? 1 : 0;
  emit messageResponded(response);
}
