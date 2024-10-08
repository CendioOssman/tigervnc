#include "appmanager.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QTimer>
#if defined(__APPLE__)
#include "cocoa.h"
#include <QMenuBar>
#endif
#include "i18n.h"
#include "rdr/Exception.h"
#include "rfb/Timer.h"
#include "rfb/LogWriter.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#undef asprintf
#include "Viewport.h"
#include "authdialog.h"
#include "ServerDialog.h"
#include "parameters.h"
#include "viewerconfig.h"
#include "vncconnection.h"
#include "DesktopWindow.h"
#undef asprintf

#if defined(WIN32)
#include "vncwinview.h"
#elif defined(__APPLE__)
#include "vncmacview.h"
#elif defined(Q_OS_UNIX)
#include "vncx11view.h"
#endif

#ifdef __APPLE__
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("AppManager");

AppManager::AppManager()
  : QObject(nullptr)
  , view(nullptr)
{

}

void AppManager::initialize()
{
  rfbTimerProxy = new QTimer;
  connection = new QVNCConnection;
  connect(this, &AppManager::connectToServerRequested, connection, &QVNCConnection::connectToServer);
  connect(connection, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, connection, &QVNCConnection::resetConnection);
  connect(rfbTimerProxy, &QTimer::timeout, this, []() {
    rfb::Timer::checkTimeouts();
  });
  connect(QApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, [this]() {
    int next = rfb::Timer::checkTimeouts();
    if (next != -1)
      rfbTimerProxy->start(next);
  });
  rfbTimerProxy->setSingleShot(true);

  connect(this, &AppManager::credentialRequested, this, [=](bool secured, bool userNeeded, bool passwordNeeded) {
    AuthDialog d(secured, userNeeded, passwordNeeded, topWindow());
    d.exec();
  });

#ifdef __APPLE__
  QMenuBar* menuBar = new QMenuBar(nullptr); // global menu bar for mac
  QMenu* appMenu = new QMenu(nullptr);
  QAction* aboutAction = new QAction(nullptr);
  connect(aboutAction, &QAction::triggered, this, []() { ViewerConfig::aboutDialog(nullptr); });
  aboutAction->setText(_("About"));
  aboutAction->setMenuRole(QAction::AboutRole);
  appMenu->addAction(aboutAction);
  menuBar->addMenu(appMenu);
  QMenu *file = new QMenu(p_("SysMenu|", "&File"));
  file->addAction(p_("SysMenu|File|", "&New Connection"), [=](){
    QProcess process;
    if (process.startDetached(qApp->arguments()[0], QStringList())) {
      vlog.error(_("Error starting new TigerVNC Viewer: %s"), QVariant::fromValue(process.error()).toString().toStdString().c_str());
    }
  }, QKeySequence("Ctrl+N"));
  menuBar->addMenu(file);
#endif

  connection->initialize();
}

int AppManager::exec()
{
  int ret = 0;

  while (true) {
    ret = QApplication::exec();

    if (fatalError) {
      if (alertOnFatalError) {
        QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                        _("Connection error"),
                                        exitError.c_str(),
                                        QMessageBox::NoButton);
        d->addButton(QMessageBox::Close);
        openDialog(d);
      }
      break;
    }

    if (exitError.empty())
      break;

    if (reconnectOnError) {
      QString text;
      text = QString::asprintf(_("%s\n\nAttempt to reconnect?"), exitError.c_str());

      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                        _("Connection error"), text,
                                        QMessageBox::NoButton);
      d->addButton(_("Reconnect"), QMessageBox::AcceptRole);
      d->addButton(QMessageBox::Close);

      openDialog(d);
      if (d->buttonRole(d->clickedButton()) == QMessageBox::AcceptRole) {
        exitError.clear();
        connectToServer();
        continue;
      } else {
        break;
      }
    }

    break;
  }

  return ret;
}

AppManager::~AppManager()
{
  connection->deleteLater();
  if (window) {
    window->deleteLater();
  }
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

void AppManager::connectToServer()
{
  if (serverDialog) {
    serverDialog->hide();
  }
  QTimer::singleShot(0, this, &AppManager::connectToServerRequested);
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
  fatalError = quit;
  exitError = message.toStdString();
  resetConnection();
  qApp->quit();
}

void AppManager::publishUnexpectedError(QString message, bool quit)
{
  message = QString::asprintf(_("An unexpected error occurred when communicating "
                                "with the server:\n\n%s"), message.toStdString().c_str());
  publishError(message, quit);
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  connectedOnce = true;

  window = new DesktopWindow(connection);
  connect(window, &DesktopWindow::closed, qApp, &QApplication::quit);

  window->takeWidget();
  delete view;
#if defined(WIN32)
  view = new QVNCWinView(connection, window);
#elif defined(__APPLE__)
  view = new QVNCMacView(connection, window);
#elif defined(Q_OS_UNIX)
  QString platform = QApplication::platformName();
  if (platform == "xcb") {
    view = new QVNCX11View(connection, window);
  } else if (platform == "wayland") {
    ;
  }
#endif

  if (!view) {
    delete window;
    window = nullptr;
    throw rdr::Exception(_("Platform not supported."));
  }
  connect(view, &Viewport::bufferResized, window, &DesktopWindow::fromBufferResize, Qt::QueuedConnection);
  connect(view,
          &Viewport::remoteResizeRequest,
          window,
          &DesktopWindow::postRemoteResizeRequest,
          Qt::QueuedConnection);
  connect(view, &Viewport::delayedInitialized, window, &DesktopWindow::showToast);

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

#ifdef __APPLE__
  cocoa_prevent_native_fullscreen(window);
#endif

  if (::fullScreen) {
#ifdef __APPLE__
    QTimer::singleShot(100, [=](){
#endif
      window->fullscreen(true);
#ifdef __APPLE__
    });
#endif
  } else {
    vlog.debug("SHOW");
    window->show();
  }

  emit vncWindowOpened();
}

void AppManager::closeVNCWindow()
{
  vlog.debug("AppManager::closeVNCWindow");
  if (window) {
    window->takeWidget();
    window->deleteLater();
    window = nullptr;
    view->deleteLater();
    view = nullptr;
    emit vncWindowClosed();
  }
}

void AppManager::setWindowName(QString name)
{
  window->setWindowTitle(QString::asprintf("%s - TigerVNC", name.toStdString().c_str()));
}

void AppManager::refresh()
{
  emit refreshRequested();
}

void AppManager::openContextMenu()
{
  emit contextMenuRequested();
}

void AppManager::openDialog(QDialog* d)
{
  bool reopen = false;
#ifdef __APPLE__
  d->setModal(true);
  d->setWindowModality(Qt::ApplicationModal);
  d->setWindowFlag(Qt::CustomizeWindowHint, true);
  d->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  d->setWindowFlag(Qt::WindowFullscreenButtonHint, false);
  d->setParent(d->parentWidget(), Qt::Dialog);
  d->setWindowFlag(Qt::CustomizeWindowHint, true);
  d->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  d->setWindowFlag(Qt::WindowFullscreenButtonHint, false);
  d->show();
  d->setFocus();
  QEventLoop loop;
  connect(qApp, &QGuiApplication::screenAdded, d, [&](){
    d->hide();
    reopen = true;
    loop.exit();
  });
  connect(qApp, &QGuiApplication::screenRemoved, d, [&](){
    d->hide();
    reopen = true;
    loop.exit();
  });
  connect(d, &QDialog::accepted, this, [&](){ loop.exit(); });
  connect(d, &QDialog::rejected, this, [&](){ loop.exit(); });
  connect(d, &QDialog::destroyed, this, [&](){ loop.exit(); });
  loop.exec();
  disconnect(qApp, &QGuiApplication::screenAdded, d, nullptr);
  disconnect(qApp, &QGuiApplication::screenRemoved, d, nullptr);
  disconnect(d, nullptr, this, nullptr);

  if (window) {
    window->postDialogClosing();
  }
  if (reopen) {
    QTimer::singleShot(100, [this, d](){
      openDialog(d);
    });
  } else {
    d->deleteLater();
  }
#else
  connect(qApp, &QGuiApplication::screenAdded, d, [&](){
    reopen = true;
    d->close();
  });
  connect(qApp, &QGuiApplication::screenRemoved, d, [&](){
    reopen = true;
    d->close();
  });
  d->exec();
  disconnect(qApp, &QGuiApplication::screenAdded, d, nullptr);
  disconnect(qApp, &QGuiApplication::screenRemoved, d, nullptr);
  if (reopen) {
    QTimer::singleShot(100, [this, d](){
      openDialog(d);
    });
  } else {
    d->deleteLater();
  }
#endif
}

void AppManager::handleOptions()
{
  /* CConn::handleOptions() */

  // Checking all the details of the current set of encodings is just
  // a pain. Assume something has changed, as resending the encoding
  // list is cheap. Avoid overriding what the auto logic has selected
  // though.
  QVNCConnection* cc = connection;
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
    if (window) {
    if (::fullscreenSystemKeys)
      window->maybeGrabKeyboard();
    else
      window->ungrabKeyboard();

    // Call fullscreen_on even if active since it handles
    // fullScreenMode
    if (::fullScreen)
      window->fullscreen(true);
    else if (!::fullScreen && window->isFullscreenEnabled())
      window->fullscreen(false);
  }
}

void AppManager::openServerDialog()
{
  serverDialog = new ServerDialog;
  serverDialog->setVisible(!::listenMode);
  connect(AppManager::instance(), &AppManager::vncWindowOpened, serverDialog, &QWidget::hide);
  connect(serverDialog, &ServerDialog::closed, qApp, &QApplication::quit);
}

QWidget *AppManager::topWindow() const
{
  return view ? qobject_cast<QWidget*>(view) : qobject_cast<QWidget*>(serverDialog);
}
