#include "appmanager.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#include "rfb/LogWriter.h"

#include <QApplication>
#include "Viewport.h"
#include "DesktopWindow.h"
#include "parameters.h"
#include "vncviewer.h"
#include "viewerconfig.h"

#ifdef __APPLE__
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("AppManager");

AppManager::AppManager()
  : QObject(nullptr)
{

}

AppManager::~AppManager()
{
}

AppManager *AppManager::instance()
{
  static AppManager manager;
  return &manager;
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
