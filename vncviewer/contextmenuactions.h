#ifndef CONTEXTMENUACTIONS_H
#define CONTEXTMENUACTIONS_H

#include <QAction>
#include <QApplication>
#include <QScrollBar>
#include "abstractvncview.h"
#include "appmanager.h"
#include "parameters.h"
#include "vncwindow.h"

class QMenuSeparator : public QAction
{
public:
    QMenuSeparator(QWidget *parent = nullptr)
        : QAction(parent)
    {
        setSeparator(true);
    }
};

class QCheckableAction : public QAction
{
public:
    QCheckableAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        setCheckable(true);
    }
};

class QFullScreenAction : public QCheckableAction
{
public:
    QFullScreenAction(const QString &text, QWidget *parent = nullptr)
        : QCheckableAction(text, parent)
    {
        connect(this, &QAction::toggled, this, [](bool checked) {
            AppManager::instance()->window()->fullscreen(checked);
        });
        connect(ViewerConfig::config(), &ViewerConfig::fullScreenChanged, this, [this](bool enabled) {
            setChecked(enabled);
        });

        setChecked(ViewerConfig::config()->fullScreen());
    }
};

class QRevertSizeAction : public QAction
{
public:
    QRevertSizeAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            QVNCWindow *window = AppManager::instance()->window();
            QAbstractVNCView *view = AppManager::instance()->view();
            window->resize(view->pixmapSize().width(), view->pixmapSize().height());
        });
        connect(ViewerConfig::config(), &ViewerConfig::fullScreenChanged, this, [this](bool enabled) {
            setEnabled(!enabled); // cf. Viewport::initContextMenu()
        });
    }
};

class QKeyToggleAction : public QCheckableAction
{
public:
    QKeyToggleAction(const QString &text, int keyCode, quint32 keySym, QWidget *parent = nullptr)
        : QCheckableAction(text, parent)
        , keyCode_(keyCode)
        , keySym_(keySym)
    {
        connect(this, &QAction::toggled, this, [this](bool checked) {
            QAbstractVNCView *view = AppManager::instance()->view();
            view->toggleKey(checked, keyCode_, keySym_);
        });
    }

private:
    int keyCode_;
    quint32 keySym_;
};

class QMenuKeyAction : public QAction
{
public:
    QMenuKeyAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            QAbstractVNCView *view = AppManager::instance()->view();
            view->sendContextMenuKey();
        });
    }
};

class QCtrlAltDelAction : public QAction
{
public:
    QCtrlAltDelAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            QAbstractVNCView *view = AppManager::instance()->view();
            view->sendCtrlAltDel();
        });
    }
};

class QMinimizeAction : public QAction
{
public:
    QMinimizeAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            QAbstractVNCView *view = AppManager::instance()->view();
            view->showMinimized();
        });
    }
};

class QDisconnectAction : public QAction
{
public:
    QDisconnectAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            QApplication::quit();
        });
    }
};

class QOptionDialogAction : public QAction
{
public:
    QOptionDialogAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            AppManager::instance()->openOptionDialog();
        });
    }
};

class QRefreshAction : public QAction
{
public:
    QRefreshAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            AppManager::instance()->connection()->refreshFramebuffer();
        });
    }
};

class QInfoDialogAction : public QAction
{
public:
    QInfoDialogAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            AppManager::instance()->openInfoDialog();
        });
    }
};

class QAboutDialogAction : public QAction
{
public:
    QAboutDialogAction(const QString &text, QWidget *parent = nullptr)
        : QAction(text, parent)
    {
        connect(this, &QAction::triggered, this, []() {
            AppManager::instance()->openAboutDialog();
        });
    }
};

#endif // CONTEXTMENUACTIONS_H
