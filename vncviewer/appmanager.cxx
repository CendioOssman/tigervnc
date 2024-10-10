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
#include "rfb/Timer.h"
#include "rfb/LogWriter.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#undef asprintf
#include "Viewport.h"
#include "authdialog.h"
#include "DesktopWindow.h"
#include "parameters.h"
#include "viewerconfig.h"
#include "vncconnection.h"
#undef asprintf

#ifdef __APPLE__
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("AppManager");

AppManager::AppManager()
  : QObject(nullptr)
{

}

void AppManager::initialize()
{
  rfbTimerProxy = new QTimer;
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
    AuthDialog d(secured, userNeeded, passwordNeeded);
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
}

int AppManager::exec(const char* vncserver, network::Socket* sock)
{
  int ret = 0;

  while (true) {
    connection = new QVNCConnection(vncserver, sock);

    if (exitError.empty())
      ret = qApp->exec();

    delete connection;
    connection = nullptr;

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

    if (reconnectOnError && (sock == nullptr)) {
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
  rfbTimerProxy->deleteLater();
}

AppManager *AppManager::instance()
{
  static AppManager manager;
  return &manager;
}

void AppManager::authenticate(QString user, QString password)
{
  emit authenticateRequested(user, password);
}

void AppManager::cancelAuth()
{
  emit cancelAuthRequested();
}

void AppManager::publishError(const QString message, bool quit)
{
  fatalError = quit;
  exitError = message.toStdString();
  qApp->quit();
}

void AppManager::publishUnexpectedError(QString message, bool quit)
{
  message = QString::asprintf(_("An unexpected error occurred when communicating "
                                "with the server:\n\n%s"), message.toStdString().c_str());
  publishError(message, quit);
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

  if (d->parentWidget()) {
    DesktopWindow* window = dynamic_cast<DesktopWindow*>(d->parentWidget());
    if (window)
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
