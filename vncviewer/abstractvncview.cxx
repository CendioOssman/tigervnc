#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QDialog>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QAction>
#include <QTimer>
#include <QScreen>
#include <QWindow>
#include <QLabel>
#include <QBitmap>
#include <QPainter>
#include <QDebug>
#include <QUrl>
#include <QClipboard>
#include <climits>
#include "rfb/Exception.h"
#include "rfb/ScreenSet.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/PixelBuffer.h"
#include "PlatformPixelBuffer.h"
#include "msgwriter.h"
#include "appmanager.h"
#include "parameters.h"
#include "menukey.h"
#include "vncconnection.h"
#include "i18n.h"
#include "viewerconfig.h"
#include "abstractvncview.h"

#if defined(WIN32) || defined(__APPLE__)
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"
#endif

static rfb::LogWriter vlog("VNCView");

QClipboard *QAbstractVNCView::m_clipboard = nullptr;

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
      AppManager::instance()->view()->fullscreen(checked);
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
      QAbstractVNCView *view = AppManager::instance()->view();
      if (!view->isFullscreenEnabled()) {
        view->showNormal();
        view->handleDesktopSize();
      }
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
   , m_keyCode(keyCode)
   , m_keySym(keySym)
  {
    connect(this, &QAction::toggled, this, [this](bool checked) {
      QAbstractVNCView *view = AppManager::instance()->view();
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
      QAbstractVNCView *view = AppManager::instance()->view();
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
      QAbstractVNCView *view = AppManager::instance()->view();
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
#if !defined(__APPLE__)
      // Temporarily commented out, because refreshFramebuffer() causes crash.
      AppManager::instance()->connection()->refreshFramebuffer();
      AppManager::instance()->view()->updateWindow();
#endif
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

/*
 * Lets create a simple finite-state machine for 3 button emulation:
 *
 * We track buttons 1 and 3 (left and right).  There are 11 states:
 *   0 ground           - initial state
 *   1 delayed left     - left pressed, waiting for right
 *   2 delayed right    - right pressed, waiting for left
 *   3 pressed middle   - right and left pressed, emulated middle sent
 *   4 pressed left     - left pressed and sent
 *   5 pressed right    - right pressed and sent
 *   6 released left    - left released after emulated middle
 *   7 released right   - right released after emulated middle
 *   8 repressed left   - left pressed after released left
 *   9 repressed right  - right pressed after released right
 *  10 pressed both     - both pressed, not emulating middle
 *
 * At each state, we need handlers for the following events
 *   0: no buttons down
 *   1: left button down
 *   2: right button down
 *   3: both buttons down
 *   4: emulate3Timeout passed without a button change
 * Note that button events are not deltas, they are the set of buttons being
 * pressed now.  It's possible (ie, mouse hardware does it) to go from (eg)
 * left down to right down without anything in between, so all cases must be
 * handled.
 *
 * a handler consists of three values:
 *   0: action1
 *   1: action2
 *   2: new emulation state
 *
 * action > 0: ButtonPress
 * action = 0: nothing
 * action < 0: ButtonRelease
 *
 * The comment preceeding each section is the current emulation state.
 * The comments to the right are of the form
 *      <button state> (<events>) -> <new emulation state>
 * which should be read as
 *      If the buttons are in <button state>, generate <events> then go to
 *      <new emulation state>.
 */
static const signed char stateTab[11][5][3] = {
/* 0 ground */
  {
    {  0,  0,  0 },   /* nothing -> ground (no change) */
    {  0,  0,  1 },   /* left -> delayed left */
    {  0,  0,  2 },   /* right -> delayed right */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  0,  0, -1 }    /* timeout N/A */
  },
/* 1 delayed left */
  {
    {  1, -1,  0 },   /* nothing (left event) -> ground */
    {  0,  0,  1 },   /* left -> delayed left (no change) */
    {  1, -1,  2 },   /* right (left event) -> delayed right */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  1,  0,  4 },   /* timeout (left press) -> pressed left */
  },
/* 2 delayed right */
  {
    {  3, -3,  0 },   /* nothing (right event) -> ground */
    {  3, -3,  1 },   /* left (right event) -> delayed left (no change) */
    {  0,  0,  2 },   /* right -> delayed right (no change) */
    {  2,  0,  3 },   /* left & right (middle press) -> pressed middle */
    {  3,  0,  5 },   /* timeout (right press) -> pressed right */
  },
/* 3 pressed middle */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    {  0,  0,  7 },   /* left -> released right */
    {  0,  0,  6 },   /* right -> released left */
    {  0,  0,  3 },   /* left & right -> pressed middle (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 4 pressed left */
  {
    { -1,  0,  0 },   /* nothing (left release) -> ground */
    {  0,  0,  4 },   /* left -> pressed left (no change) */
    { -1,  0,  2 },   /* right (left release) -> delayed right */
    {  3,  0, 10 },   /* left & right (right press) -> pressed both */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 5 pressed right */
  {
    { -3,  0,  0 },   /* nothing (right release) -> ground */
    { -3,  0,  1 },   /* left (right release) -> delayed left */
    {  0,  0,  5 },   /* right -> pressed right (no change) */
    {  1,  0, 10 },   /* left & right (left press) -> pressed both */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 6 released left */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    { -2,  0,  1 },   /* left (middle release) -> delayed left */
    {  0,  0,  6 },   /* right -> released left (no change) */
    {  1,  0,  8 },   /* left & right (left press) -> repressed left */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 7 released right */
  {
    { -2,  0,  0 },   /* nothing (middle release) -> ground */
    {  0,  0,  7 },   /* left -> released right (no change) */
    { -2,  0,  2 },   /* right (middle release) -> delayed right */
    {  3,  0,  9 },   /* left & right (right press) -> repressed right */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 8 repressed left */
  {
    { -2, -1,  0 },   /* nothing (middle release, left release) -> ground */
    { -2,  0,  4 },   /* left (middle release) -> pressed left */
    { -1,  0,  6 },   /* right (left release) -> released left */
    {  0,  0,  8 },   /* left & right -> repressed left (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 9 repressed right */
  {
    { -2, -3,  0 },   /* nothing (middle release, right release) -> ground */
    { -3,  0,  7 },   /* left (right release) -> released right */
    { -2,  0,  5 },   /* right (middle release) -> pressed right */
    {  0,  0,  9 },   /* left & right -> repressed right (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
/* 10 pressed both */
  {
    { -1, -3,  0 },   /* nothing (left release, right release) -> ground */
    { -3,  0,  4 },   /* left (right release) -> pressed left */
    { -1,  0,  5 },   /* right (left release) -> pressed right */
    {  0,  0, 10 },   /* left & right -> pressed both (no change) */
    {  0,  0, -1 },   /* timeout N/A */
  },
};

QAbstractVNCView::QAbstractVNCView(QWidget *parent, Qt::WindowFlags f)
 : QWidget(parent, f)
 , m_devicePixelRatio(devicePixelRatioF())
 , m_menuKeySym(XK_F8)
 , m_contextMenu(nullptr)
 , m_firstLEDState(false)
 , m_pendingServerClipboard(false)
 , m_pendingClientClipboard(false)
 , m_clipboardSource(0)
 , m_firstUpdate(true)
 , m_delayedFullscreen(false)
 , m_delayedDesktopSize(false)
 , m_keyboardGrabbed(false)
 , m_mouseGrabbed(false)
 , m_resizeTimer(new QTimer)
 , m_delayedInitializeTimer(new QTimer)
 , m_overlayTipCloseTimer(new QTimer)
 , m_fullscreenEnabled(false)
 , m_mouseButtonEmulationTimer(new QTimer)
 , m_state(0)
 , m_emulatedButtonMask(0)
 , m_lastButtonMask(0)
 , m_lastPos(new rfb::Point)
 , m_origPos(new rfb::Point)
{
  if (!m_clipboard) {
    m_clipboard = QGuiApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, []() {
      QString text = m_clipboard->text();
      //qDebug() << "QClipboard::dataChanged" << text;
      AppManager::instance()->connection()->sendClipboardData(text);
    });
  }
  setContentsMargins(0, 0, 0, 0);
  int radius = 5;
  m_overlayTip = new QLabel(QString(_("Press %1 to open the context menu")).arg((const char*)::menuKey), this, Qt::SplashScreen | Qt::WindowStaysOnTopHint);
  m_overlayTip->hide();
  m_overlayTip->setGeometry(0, 0, 300, 40);
  m_overlayTip->setStyleSheet(QString("QLabel {"
                                      "border-radius: %1px;"
                                      "background-color: #50505050;"
                                      "color: #e0ffffff;"
                                      "font-size: 14px;"
                                      "font-weight: bold;"
                                      "}").arg(radius));
  m_overlayTip->setWindowOpacity(0.8);
  const QRect rect(QPoint(0,0), m_overlayTip->geometry().size());
  QBitmap b(rect.size());
  b.fill(QColor(Qt::color0));
  QPainter painter(&b);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(Qt::color1);
  painter.drawRoundedRect(rect, radius, radius, Qt::AbsoluteSize);
  painter.end();
  m_overlayTip->setMask(b);
  m_overlayTip->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  m_overlayTipCloseTimer->setInterval(5000);
  m_overlayTipCloseTimer->setSingleShot(true);
  connect(m_overlayTipCloseTimer, &QTimer::timeout, this, [this]() {
    m_overlayTip->hide();
  });

  m_resizeTimer->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  m_resizeTimer->setSingleShot(true);
  connect(m_resizeTimer, &QTimer::timeout, this, &QAbstractVNCView::handleDesktopSize);

  m_delayedInitializeTimer->setInterval(1000);
  m_delayedInitializeTimer->setSingleShot(true);
  connect(m_delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    AppManager::instance()->connection()->refreshFramebuffer();
#if !defined(__APPLE__)
    AppManager::instance()->view()->updateWindow();
#endif

    m_overlayTip->move(x() + (width() - m_overlayTip->width()) / 2, y() + 50);
    m_overlayTip->show();
    m_overlayTipCloseTimer->start();
  });
  m_delayedInitializeTimer->start();

  m_mouseButtonEmulationTimer->setInterval(50);
  m_mouseButtonEmulationTimer->setSingleShot(true);
  connect(m_mouseButtonEmulationTimer, &QTimer::timeout, this, &QAbstractVNCView::handleMouseButtonEmulationTimeout);

  connect(AppManager::instance()->connection(), &QVNCConnection::cursorChanged, this, &QAbstractVNCView::setQCursor, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::cursorPositionChanged, this, &QAbstractVNCView::setCursorPos, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::ledStateChanged, this, &QAbstractVNCView::setLEDState, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::clipboardAnnounced, this, &QAbstractVNCView::handleClipboardAnnounce, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::clipboardDataReceived, this, &QAbstractVNCView::handleClipboardData, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::bellRequested, this, &QAbstractVNCView::bell, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::refreshFramebufferEnded, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);
  connect(AppManager::instance(), &AppManager::refreshRequested, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction *&action: m_actions) {
    delete action;
  }
  delete m_contextMenu;
  delete m_resizeTimer;
  delete m_delayedInitializeTimer;
  delete m_mouseButtonEmulationTimer;
  delete m_lastPos;
  delete m_origPos;
}

void QAbstractVNCView::postRemoteResizeRequest()
{
  m_resizeTimer->start();
}

void QAbstractVNCView::resize(int width, int height)
{
  m_resizeTimer->stop();
#if !defined(__APPLE__)
  width /= m_devicePixelRatio;
  height /= m_devicePixelRatio;
#endif
  QWidget::resize(width, height);
  QVNCConnection *cc = AppManager::instance()->connection();
  if (cc->server()->supportsSetDesktopSize) {
    handleDesktopSize();
  }
  qDebug() << "QWidget::resize: width=" << width << ", height=" << height;
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

qulonglong QAbstractVNCView::nativeWindowHandle() const
{
  return 0;
}

void QAbstractVNCView::handleKeyPress(int, quint32)
{
}

void QAbstractVNCView::handleKeyRelease(int)
{
}

void QAbstractVNCView::setQCursor(const QCursor &)
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

void QAbstractVNCView::handleClipboardData(const char *data)
{
  if (!hasFocus()) {
    return;
  }
  size_t len = strlen(data);
  vlog.debug("Got clipboard data (%d bytes)", (int)len);

  m_clipboard->setText(data);
}

void QAbstractVNCView::maybeGrabKeyboard()
{
}

void QAbstractVNCView::grabKeyboard()
{
  m_keyboardGrabbed = true;

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void QAbstractVNCView::ungrabKeyboard()
{
  m_keyboardGrabbed = true;
}

void QAbstractVNCView::grabPointer()
{
  setMouseTracking(true);
  m_mouseGrabbed = true;
}

void QAbstractVNCView::ungrabPointer()
{
  setMouseTracking(false);
  m_mouseGrabbed = false;
}

bool QAbstractVNCView::isFullscreenEnabled()
{
  return m_fullscreenEnabled;
}

void QAbstractVNCView::bell()
{
}

void QAbstractVNCView::remoteResize(int w, int h)
{
  QVNCConnection *cc = AppManager::instance()->connection();
  rfb::ScreenSet layout;
  rfb::ScreenSet::const_iterator iter;
#if defined(__APPLE__)
  double f = 1.0;
#else
  double f = m_devicePixelRatio;
#endif
  if (!m_fullscreenEnabled || (w > width() * f) || (h > height() * f)) {
    // In windowed mode (or the framebuffer is so large that we need
    // to scroll) we just report a single virtual screen that covers
    // the entire framebuffer.

    layout = cc->server()->screenLayout();

    // Not sure why we have no screens, but adding a new one should be
    // safe as there is nothing to conflict with...
    if (layout.num_screens() == 0)
      layout.add_screen(rfb::Screen());
    else if (layout.num_screens() != 1) {
      // More than one screen. Remove all but the first (which we
      // assume is the "primary").

      while (true) {
        iter = layout.begin();
        ++iter;

        if (iter == layout.end())
          break;

        layout.remove_screen(iter->id);
      }
    }

    // Resize the remaining single screen to the complete framebuffer
    layout.begin()->dimensions.tl.x = 0;
    layout.begin()->dimensions.tl.y = 0;
    layout.begin()->dimensions.br.x = w;
    layout.begin()->dimensions.br.y = h;
  }
  else {
    rdr::U32 id;

    // In full screen we report all screens that are fully covered.
    rfb::Rect viewport_rect;
    //viewport_rect.setXYWH(x() + (width() - w)/2, y() + (height() - h)/2, w, h);
    viewport_rect.setXYWH(0, 0, w, h);

    // If we can find a matching screen in the existing set, we use
    // that, otherwise we create a brand new screen.
    //
    // FIXME: We should really track screens better so we can handle
    //        a resized one.
    //
    QApplication *app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    for (QScreen *&screen : screens) {
      double dpr = screen->devicePixelRatio();
      QRect vg = screen->geometry();
      int sx = vg.x();
      int sy = vg.y();
      int sw = vg.width() * dpr;
      int sh = vg.height() * dpr;

      // Check that the screen is fully inside the framebuffer
      rfb::Rect screen_rect;
      screen_rect.setXYWH(sx, sy, sw, sh);
      if (!screen_rect.enclosed_by(viewport_rect))
        continue;

      // Adjust the coordinates so they are relative to our viewport
      sx -= viewport_rect.tl.x;
      sy -= viewport_rect.tl.y;

      // Look for perfectly matching existing screen that is not yet present in
      // in the screen layout...
      for (iter = cc->server()->screenLayout().begin(); iter != cc->server()->screenLayout().end(); ++iter) {
        if ((iter->dimensions.tl.x == sx) &&
            (iter->dimensions.tl.y == sy) &&
            (iter->dimensions.width() == sw) &&
            (iter->dimensions.height() == sh) &&
            (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
          break;
      }

      // Found it?
      if (iter != cc->server()->screenLayout().end()) {
        layout.add_screen(*iter);
        continue;
      }

      // Need to add a new one, which means we need to find an unused id
      while (true) {
        id = rand();
        for (iter = cc->server()->screenLayout().begin();
             iter != cc->server()->screenLayout().end(); ++iter) {
          if (iter->id == id)
            break;
        }

        if (iter == cc->server()->screenLayout().end())
          break;
      }

      layout.add_screen(rfb::Screen(id, sx, sy, sw, sh, 0));
    }

    // If the viewport doesn't match a physical screen, then we might
    // end up with no screens in the layout. Add a fake one...
    if (layout.num_screens() == 0)
      layout.add_screen(rfb::Screen(0, 0, 0, w, h, 0));
  }

  // Do we actually change anything?
  if ((w == cc->server()->width()) &&
      (h == cc->server()->height()) &&
      (layout == cc->server()->screenLayout()))
    return;

  vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d",
             cc->server()->width(), cc->server()->height(), w, h);

  char buffer[2048];
  layout.print(buffer, sizeof(buffer));
  if (!layout.validate(w, h)) {
    vlog.error(_("Invalid screen layout computed for resize request!"));
    vlog.error("%s", buffer);
    return;
  }
  else {
    vlog.debug("%s", buffer);
  }
  qDebug() << "QAbstractVNCView::remoteResize: w=" << w << ", h=" << h << ", layout=" << buffer;
  AppManager::instance()->connection()->writer()->writeSetDesktopSize(w, h, layout);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
  // copied from DesktopWindow.cxx.
  QVNCConnection *cc = AppManager::instance()->connection();
  if (m_firstUpdate) {
    if (cc->server()->supportsSetDesktopSize) {
      // Hack: Wait until we're in the proper mode and position until
      // resizing things, otherwise we might send the wrong thing.
      if (m_delayedFullscreen)
        m_delayedDesktopSize = true;
      else
        handleDesktopSize();
    }
    m_firstUpdate = false;
  }
}

void QAbstractVNCView::handleDesktopSize()
{
#if defined(__APPLE__)
  double f = 1.0;
#else
  double f = m_devicePixelRatio;
#endif
  if (strcmp(::desktopSize, "") != 0) {
    int w, h;
    // An explicit size has been requested
    if (sscanf(::desktopSize, "%dx%d", &w, &h) != 2) {
      return;
    }
    remoteResize(w * f, h * f);
    qDebug() << "QAbstractVNCView::handleDesktopSize(explicit): width=" << w << ", height=" << h;
  }
  else if (::remoteResize) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    qDebug() << "QAbstractVNCView::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
  }
}

QList<int> QAbstractVNCView::fullscreenScreens()
{
  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  QList<int> applicableScreens;
  if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSAll) {
    for (int i = 0; i < screens.length(); i++) {
      applicableScreens << i;
    }
  }
  else {
    for (int &id : ViewerConfig::config()->selectedScreens()) {
      int i = id - 1; // Screen ID in config is 1-origin.
      if (i < screens.length()) {
        applicableScreens << i;
      }
    }
  }
  return applicableScreens;
}

void QAbstractVNCView::fullscreen(bool enabled)
{
  qDebug() << "QAbstractVNCView::fullscreen: enabled=" << enabled;
  // TODO: Flag m_fullscreenEnabled seems have to be disabled before executing fullscreen(). Need clarification.
  m_fullscreenEnabled = false;
  m_resizeTimer->stop();
  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  if (enabled) {
    // cf. DesktopWindow::fullscreen_on()
    if (!isFullscreenEnabled()) {
      m_geometry = saveGeometry();
    }

    auto mode = ViewerConfig::config()->fullScreenMode();
    if (mode != ViewerConfig::FSCurrent) {
      QList<int> selectedScreens = fullscreenScreens();
      QScreen *selectedPrimaryScreen = screens[selectedScreens[0]];
      int xmin = INT_MAX;
      int ymin = INT_MAX;
      int xmax = INT_MIN;
      int ymax = INT_MIN;
      for (int &screenIndex : selectedScreens) {
        QScreen *screen = screens[screenIndex];
        QRect rect = screen->geometry();
        double dpr = screen->devicePixelRatio();
        if (xmin > rect.x()) {
          xmin = rect.x();
        }
        if (xmax < rect.x() + rect.width() * dpr) {
          xmax = rect.x() + rect.width() * dpr;
        }
        if (ymin > rect.y()) {
          ymin = rect.y();
        }
        if (ymax < rect.y() + rect.height() * dpr) {
          ymax = rect.y() + rect.height() * dpr;
        }
      }
      int w = xmax - xmin;
      int h = ymax - ymin;
      qDebug() << "Fullsize Geometry=" << QRect(xmin, ymin, w, h);

      if (selectedScreens.length() == 1) {
        setWindowFlag(Qt::BypassWindowManagerHint, true);
        windowHandle()->setScreen(selectedPrimaryScreen);
        moveView(xmin, ymin);
        showFullScreen();
        handleDesktopSize();
      }
      else {
        setWindowFlag(Qt::BypassWindowManagerHint, true);
        setWindowFlag(Qt::FramelessWindowHint, true);
        setWindowState(Qt::WindowFullScreen);
        moveView(xmin, ymin);
        resize(w, h);
        showNormal();
      }
    }
    else {
      setWindowFlag(Qt::BypassWindowManagerHint, true);
      windowHandle()->setScreen(getCurrentScreen());
      showFullScreen();
      handleDesktopSize();
    }
  }
  else {
    setWindowFlag(Qt::BypassWindowManagerHint, false);
    setWindowFlag(Qt::FramelessWindowHint, false);
    setWindowFlag(Qt::Window, true);
    showNormal();
    restoreGeometry(m_geometry);
    handleDesktopSize();
  }
  m_fullscreenEnabled = enabled;
  setFocus();
  activateWindow();
  raise();
}

void QAbstractVNCView::moveView(int x, int y)
{
  move(x, y);
}

QScreen *QAbstractVNCView::getCurrentScreen()
{
  QPoint globalCursorPos = QCursor::pos();
  qDebug() << "QAbstractVNCView::getCurrentScreen: pos=" << globalCursorPos;
  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  for (QScreen *&screen : screens) {
    if (screen->geometry().contains(globalCursorPos)) {
      qDebug() << "QAbstractVNCView::getCurrentScreen: found screen isPrimary=" << (screen == app->primaryScreen());
      return screen;
    }
  }
  return screens[0];
}


// EmulateMB::filterPointerEvent(const rfb::Point& pos, int buttonMask)
void QAbstractVNCView::filterPointerEvent(const rfb::Point& pos, int mask)
{
  QMsgWriter *writer = AppManager::instance()->connection()->writer();

  // Just pass through events if the emulate setting is disabled
  if (!emulateMiddleButton) {
    writer->writePointerEvent(pos, mask);
    return;
  }

  m_lastButtonMask = mask;
  *m_lastPos = pos;

  int btstate = 0;
  if (mask & 0x1) {
    btstate |= 0x1;
  }
  if (mask & 0x4) {
    btstate |= 0x2;
  }
  if ((m_state > 10) || (m_state < 0)) {
    throw rfb::Exception(_("Invalid state for 3 button emulation"));
  }
  int action1 = stateTab[m_state][btstate][0];
  if (action1 != 0) {
    // Some presses are delayed, that means we have to check if that's
    // the case and send the position corresponding to where the event
    // first was initiated
    if ((stateTab[m_state][4][2] >= 0) && action1 > 0)
      // We have a timeout state and a button press (a delayed press),
      // always use the original position when leaving a timeout state,
      // whether the timeout was triggered or not
      sendAction(*m_origPos, mask, action1);
    else
      // Normal non-delayed event
      sendAction(pos, mask, action1);
  }

  // In our case with the state machine, action2 always occurs during a button
  // release but if this change we need handle action2 accordingly
  int action2 = stateTab[m_state][btstate][1];
  if (action2 != 0) {
    if ((stateTab[m_state][4][2] >= 0) && action2 > 0)
      sendAction(*m_origPos, mask, action2);
    else
      // Normal non-delayed event
      sendAction(pos, mask, action2);
  }

  // Still send a pointer move event even if there are no actions.
  // However if the timer is running then we are supressing _all_
  // events, even movement. The pointer's actual position will be
  // sent once the timer fires or is abandoned.
  if ((action1 == 0) && (action2 == 0) && !m_mouseButtonEmulationTimer->isActive()) {
    mask = createButtonMask(mask);
    QMsgWriter *writer = AppManager::instance()->connection()->writer();
    writer->writePointerEvent(pos, mask);
  }

  int lastState = m_state;
  m_state = stateTab[m_state][btstate][2];

  if (lastState != m_state) {
    m_mouseButtonEmulationTimer->stop();

    if (stateTab[m_state][4][2] >= 0) {
      // We need to save the original position so that
      // drags start from the correct position
      *m_origPos = pos;
      m_mouseButtonEmulationTimer->start();
    }
  }
}

//EmulateMB::sendAction(const rfb::Point& pos, int buttonMask, int action)
void QAbstractVNCView::sendAction(const rfb::Point& pos, int buttonMask, int action)
{
  assert(action != 0);
  if (action < 0) {
    m_emulatedButtonMask &= ~(1 << ((-action) - 1));
  }
  else {
    m_emulatedButtonMask |= (1 << (action - 1));
  }
  buttonMask = createButtonMask(buttonMask);
  QMsgWriter *writer = AppManager::instance()->connection()->writer();
  writer->writePointerEvent(pos, buttonMask);
}

// EmulateMB:createButtonMask(int buttonMask)
int QAbstractVNCView::createButtonMask(int buttonMask)
{
  // Unset left and right buttons in the mask
  buttonMask &= ~0x5;

  // Set the left and right buttons according to the action
  return buttonMask | m_emulatedButtonMask;
}

// EmulateMB::handleTimeout(rfb::Timer *t)
void QAbstractVNCView::handleMouseButtonEmulationTimeout()
{
  if ((m_state > 10) || (m_state < 0)) {
    throw rfb::Exception(_("Invalid state for 3 button emulation"));
  }

  // Timeout shouldn't trigger when there's no timeout action
  assert(stateTab[m_state][4][2] >= 0);

  int action1 = stateTab[m_state][4][0];
  if (action1 != 0) {
    sendAction(*m_origPos, m_lastButtonMask, action1);
  }
  int action2 = stateTab[m_state][4][1];
  if (action2 != 0) {
    sendAction(*m_origPos, m_lastButtonMask, action2);
  }
  int buttonMask = m_lastButtonMask;

  // Pointer move events are not sent when waiting for the timeout.
  // However, we can't let the position get out of sync so when
  // the pointer has moved we have to send the latest position here.
  if (!m_origPos->equals(*m_lastPos)) {
    buttonMask = createButtonMask(buttonMask);
    QMsgWriter *writer = AppManager::instance()->connection()->writer();
    writer->writePointerEvent(*m_lastPos, buttonMask);
  }

  m_state = stateTab[m_state][4][2];
}
