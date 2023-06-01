#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QAction>
#include <QTimer>
#include <QScreen>
#include <QWindow>
#include <QBitmap>
#include <QPainter>
#include <QDebug>
#include <QUrl>
#include <QClipboard>
#include <QMoveEvent>
#include "rfb/ScreenSet.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/PixelBuffer.h"
#include "EmulateMB.h"
#include "appmanager.h"
#include "parameters.h"
#include "menukey.h"
#include "vncconnection.h"
#include "i18n.h"
#include "viewerconfig.h"
#include "abstractvncview.h"
#undef asprintf

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"

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

QMBEmu::QMBEmu(QTimer *qtimer)
 : EmulateMB(qtimer)
{
}

QMBEmu::~QMBEmu()
{
}

void QMBEmu::sendPointerEvent(const rfb::Point& pos, int buttonMask)
{
  emit AppManager::instance()->connection()->writePointerEvent(pos, buttonMask);
}

QVNCToast::QVNCToast(QWidget *parent)
 : QLabel(QString::asprintf(_("Press %s to open the context menu"), (const char*)::menuKey), parent, Qt::SplashScreen | Qt::WindowStaysOnTopHint)
 , m_closeTimer(new QTimer)
{
  int radius = 5;
  hide();
  setWindowModality(Qt::NonModal);
  setGeometry(0, 0, 300, 40);
  setStyleSheet(QString("QLabel {"
                        "border-radius: %1px;"
                        "background-color: #50505050;"
                        "color: #e0ffffff;"
                        "font-size: 14px;"
                        "font-weight: bold;"
                        "}").arg(radius));
  setWindowOpacity(0.8);
  const QRect rect(QPoint(0,0), geometry().size());
  QBitmap b(rect.size());
  b.fill(QColor(Qt::color0));
  QPainter painter(&b);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(Qt::color1);
  painter.drawRoundedRect(rect, radius, radius, Qt::AbsoluteSize);
  painter.end();
  setMask(b);
  setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  m_closeTimer->setInterval(5000);
  m_closeTimer->setSingleShot(true);
  connect(m_closeTimer, &QTimer::timeout, this, &QWidget::hide);
}

QVNCToast::~QVNCToast()
{
  delete m_closeTimer;
}

void QVNCToast::show()
{
  m_closeTimer->start();
  QLabel::show();
}

QVNCWindow::QVNCWindow(QWidget *parent)
 : QScrollArea(parent)
 , m_toast(new QVNCToast(this))
{
  setWidgetResizable(::remoteResize);
  setContentsMargins(0, 0, 0, 0);
  setFrameStyle(QFrame::NoFrame);

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  int geom_x = 0, geom_y = 0;
  if (strcmp(::geometry, "") != 0) {
    int nfields = sscanf((const char*)::geometry, "+%d+%d", &geom_x, &geom_y);
    if (nfields != 2) {
      int geom_w, geom_h;
      nfields = sscanf((const char*)::geometry, "%dx%d+%d+%d", &geom_w, &geom_h, &geom_x, &geom_y);
      if (nfields != 4) {
        vlog.error(_("Invalid geometry specified!"));
      }
    }
    if (nfields == 2 || nfields == 4) {
      move(geom_x, geom_y);
    }
  }
}

QVNCWindow::~QVNCWindow()
{
  delete m_toast;
}

void QVNCWindow::popupToast()
{
  QPoint point = mapToGlobal(QPoint(0, 0));
  m_toast->move(point.x() + (width() - m_toast->width()) / 2, point.y() + 50);
  m_toast->show();
}

void QVNCWindow::moveEvent(QMoveEvent *e)
{
  QWidget::moveEvent(e);
  if (m_toast->isVisible()) {
    QPoint point = mapToGlobal(QPoint(0, 0));
    m_toast->move(point.x() + (width() - m_toast->width()) / 2, point.y() + 50);
  }
}

QAbstractVNCView::QAbstractVNCView(QWidget *parent, Qt::WindowFlags f)
 : QWidget(parent, f)
 , m_toast(new QVNCToast(this))
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
 , m_fullscreenEnabled(false)
 , m_pendingFullscreen(false)
 , m_mouseButtonEmulationTimer(new QTimer)
 , m_mbemu(new QMBEmu(m_mouseButtonEmulationTimer))
{
  if (!m_clipboard) {
    m_clipboard = QGuiApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, []() {
      if (!::sendClipboard) {
        return;
      }
      //qDebug() << "QClipboard::dataChanged: owns=" << m_clipboard->ownsClipboard() << ", text=" << m_clipboard->text();
      if (!m_clipboard->ownsClipboard()) {
        AppManager::instance()->connection()->announceClipboard(true);
      }
    });
  }
  setContentsMargins(0, 0, 0, 0);

  m_resizeTimer->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  m_resizeTimer->setSingleShot(true);
  connect(m_resizeTimer, &QTimer::timeout, this, &QAbstractVNCView::handleDesktopSize);

  m_delayedInitializeTimer->setInterval(1000);
  m_delayedInitializeTimer->setSingleShot(true);
  connect(m_delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    AppManager::instance()->connection()->refreshFramebuffer();
    emit delayedInitialized();
  });
  m_delayedInitializeTimer->start();

  m_mouseButtonEmulationTimer->setInterval(50);
  m_mouseButtonEmulationTimer->setSingleShot(true);
  connect(m_mouseButtonEmulationTimer, &QTimer::timeout, this, &QAbstractVNCView::handleMouseButtonEmulationTimeout);

  connect(AppManager::instance()->connection(), &QVNCConnection::cursorChanged, this, &QAbstractVNCView::setQCursor, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::cursorPositionChanged, this, &QAbstractVNCView::setCursorPos, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::ledStateChanged, this, &QAbstractVNCView::setLEDState, Qt::QueuedConnection);
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
  delete m_toast;
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
  //qDebug() << "QWidget::resize: width=" << width << ", height=" << height;
}

void QAbstractVNCView::popupContextMenu()
{
  createContextMenu();
  m_contextMenu->exec(QCursor::pos());
}

void QAbstractVNCView::createContextMenu()
{
  if (!m_contextMenu) {
    m_actions << new QDisconnectAction(p_("ContextMenu|", "Dis&connect"));
    m_actions << new QMenuSeparator();
    m_actions << new QFullScreenAction(p_("ContextMenu|", "&Full screen"));
    m_actions << new QMinimizeAction(p_("ContextMenu|", "Minimi&ze"));
    m_actions << new QRevertSizeAction(p_("ContextMenu|", "Resize &window to session"));
    m_actions << new QMenuSeparator();
    m_actions << new QKeyToggleAction(p_("ContextMenu|", "&Ctrl"), 0x1d, XK_Control_L);
    m_actions << new QKeyToggleAction(p_("ContextMenu|", "&Alt"), 0x38, XK_Alt_L);
    m_actions << new QAction(QString::asprintf(p_("ContextMenu|", "Send %s"), (const char*)::menuKey));
    m_actions << new QCtrlAltDelAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"));
    m_actions << new QMenuSeparator();
    m_actions << new QRefreshAction(p_("ContextMenu|", "&Refresh screen"));
    m_actions << new QMenuSeparator();
    m_actions << new QOptionDialogAction(p_("ContextMenu|", "&Options..."));
    m_actions << new QInfoDialogAction(p_("ContextMenu|", "Connection &info..."));
    m_actions << new QAboutDialogAction(p_("ContextMenu|", "About &TigerVNC viewer..."));
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

void QAbstractVNCView::handleClipboardData(const char *data)
{
  vlog.debug("Got clipboard data (%d bytes)", (int)strlen(data));
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
  //qDebug() << "QAbstractVNCView::remoteResize: w=" << w << ", h=" << h << ", layout=" << buffer;
  emit AppManager::instance()->connection()->writeSetDesktopSize(w, h, layout);
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
    //qDebug() << "QAbstractVNCView::handleDesktopSize(explicit): width=" << w << ", height=" << h;
  }
  else if (::remoteResize) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    //qDebug() << "QAbstractVNCView::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
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
  bool bypassWMHintingNeeded = bypassWMHintingEnabled();
  //qDebug() << "QAbstractVNCView::fullscreen: enabled=" << enabled;
  // TODO: Flag m_fullscreenEnabled seems have to be disabled before executing fullscreen().
  bool fullscreenEnabled0 = m_fullscreenEnabled;
  m_fullscreenEnabled = false;
  m_pendingFullscreen = enabled;
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
      //qDebug() << "Fullsize Geometry=" << QRect(xmin, ymin, w, h);

      if (selectedScreens.length() == 1) {
        if (bypassWMHintingNeeded) {
          setWindowFlag(Qt::BypassWindowManagerHint, true);
        }
        windowHandle()->setScreen(selectedPrimaryScreen);
        moveView(xmin, ymin);
        showFullScreen();
        handleDesktopSize();
      }
      else {
        if (bypassWMHintingNeeded) {
          setWindowFlag(Qt::BypassWindowManagerHint, true);
        }
        setWindowFlag(Qt::FramelessWindowHint, true);
        setWindowState(Qt::WindowFullScreen);
        moveView(xmin, ymin);
        resize(w, h);
        showNormal();
      }
    }
    else {
      if (bypassWMHintingNeeded) {
        setWindowFlag(Qt::BypassWindowManagerHint, true);
      }
      windowHandle()->setScreen(getCurrentScreen());
      showFullScreen();
      handleDesktopSize();
    }
  }
  else {
    if (bypassWMHintingNeeded) {
      setWindowFlag(Qt::BypassWindowManagerHint, false);
    }
    setWindowFlag(Qt::FramelessWindowHint, false);
    setWindowFlag(Qt::Window, true);
    showNormal();
    restoreGeometry(m_geometry);
    handleDesktopSize();
  }
  m_fullscreenEnabled = enabled;
  m_pendingFullscreen = false;
  setFocus();
  activateWindow();
  raise();

  if (!enabled) {
    ViewerConfig::config()->setFullScreen(false);
  }
  if (m_fullscreenEnabled != fullscreenEnabled0) {
    emit fullscreenChanged(m_fullscreenEnabled);
  }
}

void QAbstractVNCView::moveView(int x, int y)
{
  move(x, y);
}

QScreen *QAbstractVNCView::getCurrentScreen()
{
  QPoint globalCursorPos = QCursor::pos();
  //qDebug() << "QAbstractVNCView::getCurrentScreen: pos=" << globalCursorPos;
  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  for (QScreen *&screen : screens) {
    if (screen->geometry().contains(globalCursorPos)) {
      //qDebug() << "QAbstractVNCView::getCurrentScreen: found screen isPrimary=" << (screen == app->primaryScreen());
      return screen;
    }
  }
  return screens[0];
}

void QAbstractVNCView::filterPointerEvent(const rfb::Point& pos, int mask)
{
  if (::viewOnly) {
    return;
  }
  m_mbemu->filterPointerEvent(pos, mask);
}

void QAbstractVNCView::handleMouseButtonEmulationTimeout()
{
  if (::viewOnly) {
    return;
  }
  m_mbemu->handleTimeout();
}

void QAbstractVNCView::popupToast()
{
  QPoint point = mapToGlobal(QPoint(0, 0));
  m_toast->move(point.x() + (width() - m_toast->width()) / 2, point.y() + 50);
  m_toast->show();
}

void QAbstractVNCView::moveEvent(QMoveEvent *e)
{
  QWidget::moveEvent(e);
  if (m_toast->isVisible()) {
    QPoint point = mapToGlobal(QPoint(0, 0));
    m_toast->move(point.x() + (width() - m_toast->width()) / 2, point.y() + 50);
  }
}
