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
#include "rfb/LogWriter.h"

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
#include "serverdialog.h"
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

static rfb::LogWriter vlog("AppManager");

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
    AuthDialog d(secured, userNeeded, passwordNeeded, topWindow());
    d.exec();
  });
}

AppManager::~AppManager()
{
  connection->deleteLater();
  window->deleteLater();
  if (view) {
    view->deleteLater();
  }
  rfbTimerProxy->deleteLater();
}

AppManager *AppManager::instance()
{
  static AppManager manager;
  return &manager;
}

bool AppManager::isFullScreen() const
{
  return window && window->isFullscreenEnabled();
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

  qApp->setQuitOnLastWindowClosed(true);
  AlertDialog d(isFullScreen(), message, quit, topWindow());
  d.exec();
  qApp->setQuitOnLastWindowClosed(false);
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
  connect(view, &QAbstractVNCView::bufferResized, window, &QVNCWindow::fromBufferResize, Qt::QueuedConnection);
  connect(view,
          &QAbstractVNCView::remoteResizeRequest,
          window,
          &QVNCWindow::postRemoteResizeRequest,
          Qt::QueuedConnection);
  connect(view, &QAbstractVNCView::delayedInitialized, window, &QVNCWindow::showToast);

  view->resize(width, height);
  window->setWidget(view);
  window->resize(width, height);
  window->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  int geom_x = 0, geom_y = 0;
  if (!QString(::geometry).isEmpty()) {
    int nfields =
        sscanf(::geometry.getValueStr().c_str(), "+%d+%d", &geom_x, &geom_y);
    if (nfields != 2) {
      int geom_w, geom_h;
      nfields = sscanf(::geometry.getValueStr().c_str(),
                       "%dx%d+%d+%d",
                       &geom_w,
                       &geom_h,
                       &geom_x,
                       &geom_y);
      if (nfields != 4) {
        vlog.debug(_("Invalid geometry specified!"));
      }
    }
    if (nfields == 2 || nfields == 4) {
      window->move(geom_x, geom_y);
    }
  }

  if (::fullScreen) {
    window->fullscreen(true);
  } else {
    vlog.debug(_("SHOW"));
    window->show();
  }

  emit vncWindowOpened();
  qApp->setQuitOnLastWindowClosed(true);
}

void AppManager::closeVNCWindow()
{
  qApp->setQuitOnLastWindowClosed(false);
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
  InfoDialog d(topWindow());
  d.exec();
}

void AppManager::openOptionDialog()
{
  OptionsDialog d(isFullScreen(), topWindow());
  d.exec();
}

void AppManager::openAboutDialog()
{
  AboutDialog d(isFullScreen(), topWindow());
  d.exec();
}

void AppManager::openMessageDialog(int flags, QString title, QString text)
{
  MessageDialog d(isFullScreen(), flags, title, text, topWindow());
  int response = d.exec() == QDialog::Accepted ? 1 : 0;
  emit messageResponded(response);
}

void AppManager::handleOptions()
{
  /* CConn::handleOptions() */

  // Checking all the details of the current set of encodings is just
  // a pain. Assume something has changed, as resending the encoding
  // list is cheap. Avoid overriding what the auto logic has selected
  // though.
  QVNCConnection* cc = AppManager::instance()->getConnection();
  if (cc && cc->hasConnection()) {
    if (!::autoSelect) {
      int encNum = encodingNum(::preferredEncoding);

      if (encNum != -1)
        cc->setPreferredEncoding(encNum);
    }

    if (::customCompressLevel)
      cc->setCompressLevel(::compressLevel);
    else
      cc->setCompressLevel(-1);

    if (!::noJpeg && !::autoSelect)
      cc->setQualityLevel(::qualityLevel);
    else
      cc->setQualityLevel(-1);

    cc->updatePixelFormat();
  }

  /* DesktopWindow::handleOptions() */
  auto view = AppManager::instance()->getView();
  if (view) {
    if (::fullscreenSystemKeys)
      view->maybeGrabKeyboard();
    else
      view->ungrabKeyboard();

    auto window = AppManager::instance()->getWindow();
    if (window) {
      // Call fullscreen_on even if active since it handles
      // fullScreenMode
      if (::fullScreen)
        window->fullscreen(true);
      else if (!::fullScreen && window->isFullscreenEnabled())
        window->fullscreen(false);
    }
  }
}

void AppManager::openServerDialog()
{
  serverDialog = new ServerDialog;
  serverDialog->setVisible(!::listenMode);
  QObject::connect(AppManager::instance(), &AppManager::vncWindowOpened, serverDialog, &QWidget::hide);
}

QWidget *AppManager::topWindow() const
{
  return view ? qobject_cast<QWidget*>(view) : qobject_cast<QWidget*>(serverDialog);
}
