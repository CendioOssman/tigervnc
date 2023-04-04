#include <QDialog>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QAction>
#include <QDebug>
#include "appmanager.h"
#include "qdesktopwindow.h"
#include "parameters.h"
#include "menukey.h"
#include "vncconnection.h"
#include "abstractvncview.h"

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"

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
      QDesktopWindow *window = AppManager::instance()->window();
      if (checked) {
        // TODO: DesktopWindow::fullscreen_on() must be ported.
        window->showFullScreen();
      }
      else {
        window->showNormal();
      }
    });
  }
};

class QRevertSizeAction : public QAction
{
public:
  QRevertSizeAction(const QString &text, QWidget *parent = nullptr)
    : QAction(text, parent)
  {
    connect(this, &QAction::triggered, this, []() {
      QDesktopWindow *window = AppManager::instance()->window();
      window->showNormal();
      // TODO:
    });
  }
};

class QKeyToggleAction : public QCheckableAction
{
public:
  QKeyToggleAction(const QString &text, int keyCode, quint32 keySym, QWidget *parent = nullptr)
    : QCheckableAction(text, parent)
    , m_keyCode(keyCode)
    , m_keySym(keySym)
  {
    connect(this, &QAction::toggled, this, [this](bool checked) {
      QAbstractVNCView *view = AppManager::instance()->window()->view();
      if (checked) {
        view->handleKeyPress(m_keyCode, m_keySym);
      }
      else {
        view->handleKeyRelease(m_keyCode);
      }
    });
  }

private:
  int m_keyCode;
  quint32 m_keySym;
};

class QMenuKeyAction : public QAction
{
public:
  QMenuKeyAction(const QString &text, QWidget *parent = nullptr)
    : QAction(text, parent)
  {
    connect(this, &QAction::triggered, this, []() {
      int dummy;
      int keyCode;
      quint32 keySym;
      ::getMenuKey(&dummy, &keyCode, &keySym);
      QAbstractVNCView *view = AppManager::instance()->window()->view();
      view->handleKeyPress(keyCode, keySym);
      view->handleKeyRelease(keyCode);
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
      QAbstractVNCView *view = AppManager::instance()->window()->view();
      view->handleKeyPress(0x1d, XK_Control_L);
      view->handleKeyPress(0x38, XK_Alt_L);
      view->handleKeyPress(0xd3, XK_Delete);
      view->handleKeyRelease(0xd3);
      view->handleKeyRelease(0x38);
      view->handleKeyRelease(0x1d);
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
      QDesktopWindow *window = AppManager::instance()->window();
      window->showMinimized();
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

QAbstractVNCView::QAbstractVNCView(QWidget *parent, Qt::WindowFlags f)
  : QWidget(parent, f)
  , m_contextMenu(nullptr)
  , m_firstLEDState(false)
  , m_pendingServerClipboard(false)
  , m_pendingClientClipboard(false)
  , m_clipboardSource(0)
  , m_keyboardGrabbed(false)
  , m_mouseGrabbed(false)
{
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction *&action: m_actions) {
    delete action;
  }
  delete m_contextMenu;
}

void QAbstractVNCView::popupContextMenu()
{
  createContextMenu();
  m_contextMenu->exec(QCursor::pos());
}

void QAbstractVNCView::createContextMenu()
{
  if (!m_contextMenu) {
    m_actions << new QDisconnectAction("Dis&connect");
    m_actions << new QMenuSeparator();
    m_actions << new QFullScreenAction("&Full screen");
    m_actions << new QMinimizeAction("Minimi&ze");
    m_actions << new QRevertSizeAction("Resize &window to session");
    m_actions << new QMenuSeparator();
    m_actions << new QKeyToggleAction("&Ctrl", 0x1d, XK_Control_L);
    m_actions << new QKeyToggleAction("&Alt", 0x38, XK_Alt_L);
    m_actions << new QAction(QString("Send ") + ::menuKey);
    m_actions << new QCtrlAltDelAction("Send Ctrl-Alt-&Del");
    m_actions << new QMenuSeparator();
    m_actions << new QRefreshAction("&Refresh screen");
    m_actions << new QMenuSeparator();
    m_actions << new QOptionDialogAction("&Options...");
    m_actions << new QInfoDialogAction("Connection &info...");
    m_actions << new QAboutDialogAction("About &TigerVNC viewer...");
    m_contextMenu = new QMenu();
    for (QAction *&action: m_actions) {
      m_contextMenu->addAction(action);
    }
  }
}

void QAbstractVNCView::handleKeyPress(int, quint32)
{
}

void QAbstractVNCView::handleKeyRelease(int)
{
}

void QAbstractVNCView::setCursor(int, int, int, int, const unsigned char *)
{
}

void QAbstractVNCView::setCursorPos(int, int)
{
}

void QAbstractVNCView::pushLEDState()
{
}

void QAbstractVNCView::setLEDState(unsigned int)
{
  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  if (m_firstLEDState) {
    m_firstLEDState = false;
    if (hasFocus()) {
      pushLEDState();
    }
    return;
  }
}

void QAbstractVNCView::handleClipboardAnnounce(bool available)
{
  if (!::acceptClipboard) {
    return;
  }

  if (!available) {
    m_pendingServerClipboard = false;
    return;
  }

  m_pendingClientClipboard = false;

  if (!hasFocus()) {
    m_pendingServerClipboard = true;
  }
}

void QAbstractVNCView::handleClipboardData(const char*)
{
}

void QAbstractVNCView::maybeGrabKeyboard()
{
}

void QAbstractVNCView::grabKeyboard()
{
}

void QAbstractVNCView::ungrabKeyboard()
{
}

void QAbstractVNCView::grabPointer()
{
}

void QAbstractVNCView::ungrabPointer()
{
}
bool QAbstractVNCView::isFullscreen()
{
  return false;
}

void QAbstractVNCView::bell()
{
}
