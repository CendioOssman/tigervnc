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

AppManager* AppManager::manager;

AppManager::AppManager()
  : QObject(nullptr)
  , errorCount(0)
  , connection(new QVNCConnection)
  , view(nullptr)
  , window(new QVNCWindow)
  , rfbTimerProxy(new QTimer)
{
  connect(this, &AppManager::connectToServerRequested, connection, &QVNCConnection::connectToServer);
  connect(connection, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, connection, &QVNCConnection::resetConnection);
  connect(rfbTimerProxy, &QTimer::timeout, this, []() {
    rfb::Timer::checkTimeouts();
  });
  connect(QApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, [this]() {
    int next = rfb::Timer::checkTimeouts();
    if (next != 0)
      rfbTimerProxy->start(next);
  });
  rfbTimerProxy->setSingleShot(true);

  connect(this, &AppManager::credentialRequested, this, [=](bool secured, bool userNeeded, bool passwordNeeded) {
    AuthDialog d(secured, userNeeded, passwordNeeded);
    d.exec();
  });
}

AppManager::~AppManager()
{
  connection->deleteLater();
  window->takeWidget();
  window->deleteLater();
  if (view) {
    view->deleteLater();
  }
  rfbTimerProxy->deleteLater();
}

int AppManager::initialize()
{
  manager = new AppManager();
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
  errorCount++;

  AlertDialog d(message, quit);
  d.exec();
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  window->takeWidget();
  delete view;
#if defined(WIN32)
  view = new QVNCWinView(window);
#elif defined(__APPLE__)
  view = new QVNCMacView(window);
#elif defined(Q_OS_UNIX)
  QString platform = QApplication::platformName();
  if (platform == "xcb") {
    view = new QVNCX11View(window);
  } else if (platform == "wayland") {
    ;
  }
#endif

  if (!view) {
    throw rdr::Exception(_("Platform not supported."));
  }
  connect(view, &QAbstractVNCView::bufferResized, window, &QVNCWindow::updateScrollbars, Qt::QueuedConnection);
  connect(view,
          &QAbstractVNCView::remoteResizeRequest,
          window,
          &QVNCWindow::postRemoteResizeRequest,
          Qt::QueuedConnection);

  view->resize(width, height);
  window->setWidget(view);
  window->resize(width, height);
  window->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  window->show();

  if (ViewerConfig::config()->fullScreen()) {
    window->fullscreen(true);
  }

  emit vncWindowOpened();
}

void AppManager::closeVNCWindow()
{
  QWidget* w = window->takeWidget();
  if (w) {
    window->setVisible(false);
    w->setVisible(false);
    w->deleteLater();
    view = nullptr;
    emit vncWindowClosed();
  }
}

void AppManager::setWindowName(QString name)
{
  window->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
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
