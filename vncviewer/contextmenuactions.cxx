#include "contextmenuactions.h"

#include "abstractvncview.h"
#include "appmanager.h"
#include "parameters.h"
#include "vncconnection.h"
#include "vncwindow.h"

#include <QApplication>

QMenuSeparator::QMenuSeparator(QWidget* parent)
  : QAction(parent)
{
  setSeparator(true);
}

QCheckableAction::QCheckableAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  setCheckable(true);
}

QFullScreenAction::QFullScreenAction(const QString& text, QWidget* parent)
  : QCheckableAction(text, parent)
{
  connect(this, &QAction::triggered, this, [](bool checked) {
    AppManager::instance()->getWindow()->fullscreen(checked);
  });
  setChecked(::fullScreen);
}

QRevertSizeAction::QRevertSizeAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    QVNCWindow* window = AppManager::instance()->getWindow();
    QAbstractVNCView* view = AppManager::instance()->getView();
    window->resize(view->pixmapSize().width(), view->pixmapSize().height());
  });
}

QKeyToggleAction::QKeyToggleAction(const QString& text, int keyCode, quint32 keySym, QWidget* parent)
  : QCheckableAction(text, parent)
  , keyCode(keyCode)
  , keySym(keySym)
{
  connect(this, &QAction::triggered, this, [this](bool checked) {
    QAbstractVNCView* view = AppManager::instance()->getView();
    view->toggleKey(checked, this->keyCode, this->keySym);
  });
}

QMenuKeyAction::QMenuKeyAction(QWidget* parent)
  : QAction(parent)
{
  connect(this, &QAction::triggered, this, []() {
    QAbstractVNCView* view = AppManager::instance()->getView();
    view->sendContextMenuKey();
  });
}

QCtrlAltDelAction::QCtrlAltDelAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    QAbstractVNCView* view = AppManager::instance()->getView();
    view->sendCtrlAltDel();
  });
}

QMinimizeAction::QMinimizeAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    QVNCWindow* window = AppManager::instance()->getWindow();
    window->showMinimized();
  });
}

QDisconnectAction::QDisconnectAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    QApplication::quit();
  });
}

QOptionDialogAction::QOptionDialogAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    AppManager::instance()->openOptionDialog();
  });
}

QRefreshAction::QRefreshAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    AppManager::instance()->getConnection()->refreshFramebuffer();
  });
}

QInfoDialogAction::QInfoDialogAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    AppManager::instance()->openInfoDialog();
  });
}

QAboutDialogAction::QAboutDialogAction(const QString& text, QWidget* parent)
  : QAction(text, parent)
{
  connect(this, &QAction::triggered, this, []() {
    AppManager::instance()->openAboutDialog();
  });
}
