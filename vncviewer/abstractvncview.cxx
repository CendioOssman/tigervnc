#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QCheckBox>
#include <QAction>
#include <QTimer>
#include <QScreen>
#include <QWindow>
#include <QDebug>
#include <QUrl>
#include <QPainter>
#include <QClipboard>
#include <QMoveEvent>
#include <QScrollBar>
#include "rfb/ScreenSet.h"
#include "rfb/LogWriter.h"
#include "rfb/ServerParams.h"
#include "rfb/PixelBuffer.h"
#include "EmulateMB.h"
#include "PlatformPixelBuffer.h"
#include "appmanager.h"
#include "menukey.h"
#include "vncconnection.h"
#include "locale.h"
#include "i18n.h"
#undef asprintf
#include "parameters.h"
#include "vncwindow.h"
#include "abstractvncview.h"
#undef asprintf

#if defined(WIN32) || defined(__APPLE__)
#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include "rfb/keysymdef.h"
#endif

#undef KeyPress

#if defined(__APPLE__)
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("VNCView");

QClipboard *QAbstractVNCView::clipboard_ = nullptr;

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
      QVNCWindow *window = AppManager::instance()->window();
      QAbstractVNCView *view = AppManager::instance()->view();
      window->normalizedResize(view->width() + window->horizontalScrollBar()->width(), view->height() + window->verticalScrollBar()->height()); // Needed to hide scrollbars.
      window->normalizedResize(view->width(), view->height());
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
      if (checked) {
        view->handleKeyPress(keyCode_, keySym_);
      }
      else {
        view->handleKeyRelease(keyCode_);
      }
      view->setMenuKeyStatus(keySym_, checked);
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

QAbstractVNCView::QAbstractVNCView(QWidget *parent, Qt::WindowFlags f)
 : QWidget(parent, f)
 , devicePixelRatio_(devicePixelRatioF())
 , menuKeySym_(XK_F8)
 , contextMenu_(nullptr)
 , firstLEDState_(false)
 , pendingServerClipboard_(false)
 , pendingClientClipboard_(false)
 , clipboardSource_(0)
 , firstUpdate_(true)
 , delayedFullscreen_(false)
 , delayedDesktopSize_(false)
 , keyboardGrabbed_(false)
 , mouseGrabbed_(false)
 , resizeTimer_(new QTimer)
 , delayedInitializeTimer_(new QTimer)
 , fullscreenEnabled_(false)
 , pendingFullscreen_(false)
 , mouseButtonEmulationTimer_(new QTimer)
 , mbemu_(new EmulateMB(mouseButtonEmulationTimer_))
 , lastPointerPos_(new rfb::Point)
 , lastButtonMask_(0)
 , mousePointerTimer_(new QTimer)
 , menuCtrlKey_(false)
 , menuAltKey_(false)
{
  setAttribute(Qt::WA_OpaquePaintEvent);

  if (!clipboard_) {
    clipboard_ = QGuiApplication::clipboard();
    connect(clipboard_, &QClipboard::dataChanged, this, []() {
      if (!ViewerConfig::config()->sendClipboard()) {
        return;
      }
      //qDebug() << "QClipboard::dataChanged: owns=" << clipboard_->ownsClipboard() << ", text=" << clipboard_->text();
      if (!clipboard_->ownsClipboard()) {
        AppManager::instance()->connection()->announceClipboard(true);
      }
    });
  }
  setContentsMargins(0, 0, 0, 0);

  resizeTimer_->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  resizeTimer_->setSingleShot(true);
  connect(resizeTimer_, &QTimer::timeout, this, &QAbstractVNCView::handleDesktopSize);

  delayedInitializeTimer_->setInterval(1000);
  delayedInitializeTimer_->setSingleShot(true);
  connect(delayedInitializeTimer_, &QTimer::timeout, this, [this]() {
    AppManager::instance()->connection()->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer_->start();

  mouseButtonEmulationTimer_->setInterval(50);
  mouseButtonEmulationTimer_->setSingleShot(true);
  connect(mouseButtonEmulationTimer_, &QTimer::timeout, this, &QAbstractVNCView::handleMouseButtonEmulationTimeout);

  mousePointerTimer_->setInterval(ViewerConfig::config()->pointerEventInterval());
  mousePointerTimer_->setSingleShot(true);
  connect(mousePointerTimer_, &QTimer::timeout, this, [this]() {
    mbemu_->filterPointerEvent(*lastPointerPos_, lastButtonMask_);
  });

  connect(AppManager::instance()->connection(), &QVNCConnection::cursorChanged, this, &QAbstractVNCView::setCursor, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::cursorPositionChanged, this, &QAbstractVNCView::setCursorPos, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::ledStateChanged, this, &QAbstractVNCView::setLEDState, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::clipboardDataReceived, this, &QAbstractVNCView::handleClipboardData, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::bellRequested, this, &QAbstractVNCView::bell, Qt::QueuedConnection);
  connect(AppManager::instance()->connection(), &QVNCConnection::refreshFramebufferEnded, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);
  connect(AppManager::instance(), &AppManager::refreshRequested, this, &QAbstractVNCView::updateWindow, Qt::QueuedConnection);
}

QAbstractVNCView::~QAbstractVNCView()
{
  for (QAction *&action: actions_) {
    delete action;
  }
  delete contextMenu_;
  delete resizeTimer_;
  delete delayedInitializeTimer_;
  delete mouseButtonEmulationTimer_;
  delete lastPointerPos_;
  delete mousePointerTimer_;
}

void QAbstractVNCView::postRemoteResizeRequest()
{
  resizeTimer_->start();
}

void QAbstractVNCView::resize(int width, int height)
{
  qDebug() << "QAbstractVNCView::resize: w=" << width << ", h=" << height;
  resizeTimer_->stop();
  width /= effectiveDevicePixelRatio();
  height /= effectiveDevicePixelRatio();
  QWidget::resize(width, height);
  QVNCConnection *cc = AppManager::instance()->connection();
  if (cc->server()->supportsSetDesktopSize) {
    handleDesktopSize();
  }
  ungrabPointer();
  grabPointer();
  maybeGrabKeyboard();
  //qDebug() << "QWidget::resize: width=" << width << ", height=" << height;
}

void QAbstractVNCView::popupContextMenu()
{
#if 0
#if defined(__APPLE__)
  ungrabKeyboard();
#endif
#endif
  createContextMenu();
#if 0
  cocoa_set_overlay_property(contextMenu_);
  contextMenu_->raise();
#endif
  contextMenu_->exec(QCursor::pos());
}

void QAbstractVNCView::createContextMenu()
{
  if (!contextMenu_) {
    actions_ << new QDisconnectAction(p_("ContextMenu|", "Dis&connect"));
    actions_ << new QMenuSeparator();
    actions_ << new QFullScreenAction(p_("ContextMenu|", "&Full screen"));
    actions_ << new QMinimizeAction(p_("ContextMenu|", "Minimi&ze"));
    actions_ << new QRevertSizeAction(p_("ContextMenu|", "Resize &window to session"));
    actions_ << new QMenuSeparator();
    actions_ << new QKeyToggleAction(p_("ContextMenu|", "&Ctrl"), 0x1d, XK_Control_L);
    actions_ << new QKeyToggleAction(p_("ContextMenu|", "&Alt"), 0x38, XK_Alt_L);
    actions_ << new QMenuKeyAction(QString::asprintf(p_("ContextMenu|", "Send %s"), ViewerConfig::config()->menuKey().toStdString().c_str()));
    actions_ << new QCtrlAltDelAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"));
    actions_ << new QMenuSeparator();
    actions_ << new QRefreshAction(p_("ContextMenu|", "&Refresh screen"));
    actions_ << new QMenuSeparator();
    actions_ << new QOptionDialogAction(p_("ContextMenu|", "&Options..."));
    actions_ << new QInfoDialogAction(p_("ContextMenu|", "Connection &info..."));
    actions_ << new QAboutDialogAction(p_("ContextMenu|", "About &TigerVNC viewer..."));
    QVNCWindow *window = AppManager::instance()->window();
    contextMenu_ = new QMenu(window);
#if defined(__APPLE__)
    contextMenu_->setAttribute(Qt::WA_NativeWindow);
    cocoa_set_overlay_property(contextMenu_->winId());
#endif
    for (QAction *&action: actions_) {
      contextMenu_->addAction(action);
    }
    contextMenu_->installEventFilter(this);
  }
}

bool QAbstractVNCView::isVisibleContextMenu() const
{
  return contextMenu_ && contextMenu_->isVisible();
}

void QAbstractVNCView::sendContextMenuKey()
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  qDebug() << "QAbstractVNCView::sendContextMenuKey";
  int dummy;
  int keyCode;
  quint32 keySym;
  ::getMenuKey(&dummy, &keyCode, &keySym);
  QAbstractVNCView *view = AppManager::instance()->view();
  view->handleKeyPress(keyCode, keySym, true);
  view->handleKeyRelease(keyCode);
  contextMenu_->hide();
}

bool QAbstractVNCView::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *e = static_cast<QKeyEvent *>(event);
    if (isVisibleContextMenu()) {
      if (QKeySequence(e->key()).toString() == ViewerConfig::config()->menuKey()) {
        sendContextMenuKey();
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QScreen *QAbstractVNCView::screen() const
{
  // FIXME: check best overlap
  return qApp->screens()[0];
}
#endif

void QAbstractVNCView::setMenuKeyStatus(quint32 keysym, bool checked)
{
  if (keysym == XK_Control_L) {
    menuCtrlKey_ = checked;
  }
  else if (keysym == XK_Alt_L) {
    menuAltKey_ = checked;
  }
}

void QAbstractVNCView::disableIM()
{
}

void QAbstractVNCView::enableIM()
{
}

void QAbstractVNCView::resetKeyboard()
{
  while (!downKeySym_.empty()) {
    handleKeyRelease(downKeySym_.begin()->first);
  }
}

void QAbstractVNCView::handleKeyPress(int, quint32, bool)
{
}

void QAbstractVNCView::handleKeyRelease(int)
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
  clipboard_->setText(data);
}

void QAbstractVNCView::maybeGrabKeyboard()
{
  if (ViewerConfig::config()->fullscreenSystemKeys() && (isFullscreenEnabled() || pendingFullscreen_) && hasFocus()) {
    grabKeyboard();
  }
}

void QAbstractVNCView::grabKeyboard()
{
  keyboardGrabbed_ = true;

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void QAbstractVNCView::ungrabKeyboard()
{
  keyboardGrabbed_ = true;
}

void QAbstractVNCView::grabPointer()
{
  setMouseTracking(true);
  mouseGrabbed_ = true;
}

void QAbstractVNCView::ungrabPointer()
{
  setMouseTracking(false);
  mouseGrabbed_ = false;
}

bool QAbstractVNCView::isFullscreenEnabled()
{
  return fullscreenEnabled_;
}

void QAbstractVNCView::bell()
{
}

void QAbstractVNCView::remoteResize(int w, int h)
{
  QVNCConnection *cc = AppManager::instance()->connection();
  rfb::ScreenSet layout;
  rfb::ScreenSet::const_iterator iter;
  double f = effectiveDevicePixelRatio();
  QVNCWindow *window = AppManager::instance()->window();
  if ((!fullscreenEnabled_ && !pendingFullscreen_) || (w > window->width() * f) || (h > window->height() * f)) {
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
    uint32_t id;

    // In full screen we report all screens that are fully covered.
    rfb::Rect viewport_rect;
    viewport_rect.setXYWH(fxmin_, fymin_, fw_, fh_);

    // If we can find a matching screen in the existing set, we use
    // that, otherwise we create a brand new screen.
    //
    // FIXME: We should really track screens better so we can handle
    //        a resized one.
    //
    QApplication *app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
//    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) {
//                return a->geometry().x() == b->geometry().x() ? (a->geometry().y() < b->geometry().y()) : (a->geometry().x() < b->geometry().x());
//              });
    for (QScreen *&screen : screens) {
      double dpr = effectiveDevicePixelRatio(screen);
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
  emit AppManager::instance()->connection()->writeSetDesktopSize(w, h, layout);
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void QAbstractVNCView::updateWindow()
{
  // copied from DesktopWindow.cxx.
  QVNCConnection *cc = AppManager::instance()->connection();
  if (firstUpdate_) {
    if (cc->server()->supportsSetDesktopSize) {
      // Hack: Wait until we're in the proper mode and position until
      // resizing things, otherwise we might send the wrong thing.
      if (delayedFullscreen_)
        delayedDesktopSize_ = true;
      else
        handleDesktopSize();
    }
    firstUpdate_ = false;
  }

  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    damage += QRect(x, y, w, h);
    update(x, y, w, h);
  }
}

void QAbstractVNCView::paintEvent(QPaintEvent *event)
{
  QVNCConnection *cc = AppManager::instance()->connection();
  PlatformPixelBuffer *framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());

  if ((framebuffer->width() != pixmap.width()) ||
      (framebuffer->height() != pixmap.height())) {
    pixmap = QPixmap(framebuffer->width(), framebuffer->height());
    damage = QRegion(0, 0, pixmap.width(), pixmap.height());
  }

  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);
    const uint8_t *data;
    int stride;
    QRect bounds = damage.boundingRect();
    int x = bounds.x();
    int y = bounds.y();
    int w = bounds.width();
    int h = bounds.height();
    rfb::Rect rfbrect(x, y, x+w, y+h);

    data = framebuffer->getBuffer(rfbrect, &stride);
    QImage image(data, w, h, stride*4, QImage::Format_RGB32);

    pixmapPainter.drawImage(bounds, image);
    damage = QRegion();
  }

  QPainter painter(this);
  QRect rect = event->rect();

  painter.drawPixmap(rect, pixmap, rect);
}

void QAbstractVNCView::handleDesktopSize()
{
  double f = effectiveDevicePixelRatio();
  if (!ViewerConfig::config()->desktopSize().isEmpty()) {
    int w, h;
    // An explicit size has been requested
    if (sscanf(ViewerConfig::config()->desktopSize().toStdString().c_str(), "%dx%d", &w, &h) != 2) {
      return;
    }
    remoteResize(w * f, h * f);
    //qDebug() << "QAbstractVNCView::handleDesktopSize(explicit): width=" << w << ", height=" << h;
  }
  else if (ViewerConfig::config()->remoteResize()) {
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
  else if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSSelected) {
    for (int &id : ViewerConfig::config()->selectedScreens()) {
      int i = id - 1; // Screen ID in config is 1-origin.
      if (i < screens.length()) {
        applicableScreens << i;
      }
    }
  }
  else {
    QScreen *cscreen = getCurrentScreen();
    for (int i = 0; i < screens.length(); i++) {
      if (screens[i] == cscreen) {
        applicableScreens << i;
        break;
      }
    }
  }

  return applicableScreens;
}

void QAbstractVNCView::fullscreen(bool enabled)
{
  QVNCWindow *window = AppManager::instance()->window();
  //qDebug() << "QAbstractVNCView::fullscreen: enabled=" << enabled;
  // TODO: Flag fullscreenEnabled_ seems have to be disabled before executing fullscreen().
  bool fullscreenEnabled0 = fullscreenEnabled_;
  fullscreenEnabled_ = false;
  pendingFullscreen_ = enabled;
  resizeTimer_->stop();
  QApplication *app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  setWindowManager();
  if (enabled) {
    // cf. DesktopWindow::fullscreen_on()
    if (!isFullscreenEnabled()) {
      geometry_ = window->saveGeometry();
      fscreen_ = getCurrentScreen();
    }

    auto mode = ViewerConfig::config()->fullScreenMode();
    QList<int> selectedScreens = fullscreenScreens();
    if (mode != ViewerConfig::FSCurrent && selectedScreens.length() > 0) {
      QScreen *selectedPrimaryScreen = screens[selectedScreens[0]];
      int xmin = INT_MAX;
      int ymin = INT_MAX;
      int xmax = INT_MIN;
      int ymax = INT_MIN;
      for (int &screenIndex : selectedScreens) {
        QScreen *screen = screens[screenIndex];
        QRect rect = screen->geometry();
        double dpr = effectiveDevicePixelRatio(screen);
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
      // Capture the fullscreen geometry.
      fxmin_ = xmin;
      fymin_ = ymin;
      fw_ = w;
      fh_ = h;

      if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
        fullscreenOnSelectedDisplay(selectedPrimaryScreen, xmin, ymin, w, h);
      }
      else { // Fullscreen on multiple displays.
        fullscreenOnSelectedDisplays(xmin, ymin, w, h);
      }
    }
    else { // Fullscreen on the current single display.
      fullscreenOnCurrentDisplay();
    }
  }
  else { // Exit fullscreen mode.
    exitFullscreen();
  }
  fullscreenEnabled_ = enabled;
  pendingFullscreen_ = false;
  setFocus();
  window->activateWindow();
  window->raise();

  if (!enabled) {
    ViewerConfig::config()->setFullScreen(false);
  }
  if (fullscreenEnabled_ != fullscreenEnabled0) {
    emit fullscreenChanged(fullscreenEnabled_);
  }
}

void QAbstractVNCView::fullscreenOnCurrentDisplay()
{
  QVNCWindow *window = AppManager::instance()->window();
  if (bypassWMHintingEnabled()) {
    window->setWindowFlag(Qt::BypassWindowManagerHint, true);
  }
  QScreen *screen = getCurrentScreen();
  window->windowHandle()->setScreen(screen);
  window->showFullScreen();

  // Capture the fullscreen geometry.
  double dpr = effectiveDevicePixelRatio(screen);
  QRect vg = screen->geometry();
  fxmin_ = vg.x();
  fymin_ = vg.y();
  fw_ = vg.width() * dpr;
  fh_ = vg.height() * dpr;

  grabKeyboard();
}

void QAbstractVNCView::fullscreenOnSelectedDisplay(QScreen *screen, int vx, int vy, int, int)
{
  QVNCWindow *window = AppManager::instance()->window();
  if (bypassWMHintingEnabled()) {
    window->setWindowFlag(Qt::BypassWindowManagerHint, true);
  }
  window->windowHandle()->setScreen(screen);
  window->move(vx, vy);
  window->showFullScreen();
  grabKeyboard();
}

void QAbstractVNCView::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight)
{
  QVNCWindow *window = AppManager::instance()->window();
  if (bypassWMHintingEnabled()) {
    window->setWindowFlag(Qt::BypassWindowManagerHint, true);
  }

  window->setWindowFlag(Qt::FramelessWindowHint, true);

  QRect r = getExtendedFrameProperties();
  window->move(vx + r.x(), vy);
  window->resize(vwidth, vheight);
  resize(vwidth, vheight);
  window->showNormal();
  grabKeyboard();
}

void QAbstractVNCView::exitFullscreen()
{
  QVNCWindow *window = AppManager::instance()->window();
  if (bypassWMHintingEnabled()) {
    window->setWindowFlag(Qt::BypassWindowManagerHint, false);
  }
  window->setWindowFlag(Qt::FramelessWindowHint, false);
  window->setWindowFlag(Qt::Window);
  window->showNormal();
  window->windowHandle()->setScreen(fscreen_);
  window->restoreGeometry(geometry_);
  ungrabKeyboard();
}

void QAbstractVNCView::moveView(int x, int y)
{
  move(x, y);
}

QScreen *QAbstractVNCView::getCurrentScreen()
{
  int centerX = x() + width() / 2;
  int centerY = y() + height() / 2;
  QPoint globalCursorPos = mapToGlobal(QPoint(centerX, centerY));
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

double QAbstractVNCView::effectiveDevicePixelRatio(QScreen *screen) const
{
  if (screen) {
    return screen->devicePixelRatio();
  }
  return devicePixelRatio_;
}

void QAbstractVNCView::filterPointerEvent(const rfb::Point& pos, int mask)
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  bool instantPosting = ViewerConfig::config()->pointerEventInterval() == 0 || (mask != lastButtonMask_);
  *lastPointerPos_ = pos;
  lastButtonMask_ = mask;
  if (instantPosting) {
    mbemu_->filterPointerEvent(*lastPointerPos_, lastButtonMask_);
  }
  else {
    if (!mousePointerTimer_->isActive())
      mousePointerTimer_->start();
  }
}

void QAbstractVNCView::handleMouseButtonEmulationTimeout()
{
  if (ViewerConfig::config()->viewOnly()) {
    return;
  }
  mbemu_->handleTimeout();
}

QRect QAbstractVNCView::getExtendedFrameProperties()
{
  return QRect();
}
