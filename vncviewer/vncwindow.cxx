#include "vncwindow.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "abstractvncview.h"
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/LogWriter.h"
#include "rfb/ScreenSet.h"
#include "toast.h"
#include "vncconnection.h"

#include <QApplication>
#include <QDebug>
#include <QGestureEvent>
#include <QGridLayout>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTimer>
#include <QWindow>
#undef asprintf
#if defined(WIN32)
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif
#endif

static rfb::LogWriter vlog("VNCWindow");

class ScrollArea : public QScrollArea
{
public:
  ScrollArea(QWidget* parent = nullptr)
    : QScrollArea(parent)
  {
    setViewportMargins(0, 0, 0, 0);
    setFrameStyle(QFrame::NoFrame);
    setLineWidth(0);
    setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  }
};

QVNCWindow::QVNCWindow(QWidget* parent)
  : QWidget(parent)
  , resizeTimer(new QTimer(this))
  , devicePixelRatio(devicePixelRatioF())
{
  setFocusPolicy(Qt::StrongFocus);

  setContentsMargins(0, 0, 0, 0);

  scrollArea = new ScrollArea;

  QPalette p(palette());
  p.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
  setPalette(p);
  setBackgroundRole(QPalette::Window);

  resizeTimer->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  resizeTimer->setSingleShot(true);
  connect(resizeTimer, &QTimer::timeout, this, &QVNCWindow::handleDesktopSize);

  toast = new Toast(this);

  QVBoxLayout* l = new QVBoxLayout;
  l->setSpacing(0);
  l->setContentsMargins(0,0,0,0);
  l->addWidget(scrollArea);
  setLayout(l);
}

QVNCWindow::~QVNCWindow() {}

void QVNCWindow::updateScrollbars()
{
}

QList<int> QVNCWindow::fullscreenScreens() const
{
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  QList<int> applicableScreens;
  if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "all")) {
    for (int i = 0; i < screens.length(); i++) {
      applicableScreens << i;
    }
  } else if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "selected")) {
    for (int const& id : ::fullScreenSelectedMonitors.getParam()) {
      int i = id - 1; // Screen ID in config is 1-origin.
      if (i < screens.length()) {
        applicableScreens << i;
      }
    }
  } else {
    QScreen* cscreen = getCurrentScreen();
    for (int i = 0; i < screens.length(); i++) {
      if (screens[i] == cscreen) {
        applicableScreens << i;
        break;
      }
    }
  }

  return applicableScreens;
}

QScreen* QVNCWindow::getCurrentScreen() const
{
  return windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
}

double QVNCWindow::effectiveDevicePixelRatio(QScreen* screen) const
{
#ifdef Q_OS_DARWIN
  return 1.0;
#endif

  if (screen) {
    return screen->devicePixelRatio();
  }
  return devicePixelRatio;
}

void QVNCWindow::fullscreen(bool enabled)
{
  vlog.debug("QVNCWindow::fullscreen enabled=%d", enabled);
  bool fullscreenEnabled0 = fullscreenEnabled;
  fullscreenEnabled = false;
  pendingFullscreen = enabled;
  resizeTimer->stop();
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  if (enabled) {
    // cf. DesktopWindow::fullscreen_on()
    if (!fullscreenEnabled0) {
      previousGeometry = saveGeometry();
      previousScreen = getCurrentScreen();
    }

    QList<int> selectedScreens = fullscreenScreens();
    int top, bottom, left, right;
    QScreen* selectedPrimaryScreen = screens[selectedScreens[0]];
    top = bottom = left = right = selectedScreens[0];
    if (strcasecmp(::fullScreenMode.getValueStr().c_str(), "current") && selectedScreens.length() > 0) {
      int xmin = INT_MAX;
      int ymin = INT_MAX;
      int xmax = INT_MIN;
      int ymax = INT_MIN;
      for (int& screenIndex : selectedScreens) {
        QScreen* screen = screens[screenIndex];
        QRect rect = screen->geometry();
        double dpr = effectiveDevicePixelRatio(screen);
        if (xmin > rect.x()) {
          left = screenIndex;
          xmin = rect.x();
        }
        if (xmax < rect.x() + rect.width() * dpr) {
          right = screenIndex;
          xmax = rect.x() + rect.width() * dpr;
        }
        if (ymin > rect.y()) {
          top = screenIndex;
          ymin = rect.y();
        }
        if (ymax < rect.y() + rect.height() * dpr) {
          bottom = screenIndex;
          ymax = rect.y() + rect.height() * dpr;
        }
      }
      int w = xmax - xmin;
      int h = ymax - ymin;
      vlog.debug("QVNCWindow::fullscreen geometry=(%d, %d, %d, %d)", xmin, ymin, w, h);
      //  Capture the fullscreen geometry.
      fullscreenX = xmin;
      fullscreenY = ymin;
      fullscreenWidth = w;
      fullscreenHeight = h;

      if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
#ifdef Q_OS_LINUX
        fullscreenOnSelectedDisplays(top, top, top, top);
#else
        fullscreenOnSelectedDisplay(selectedPrimaryScreen);
#endif
      } else { // Fullscreen on multiple displays.
#ifdef Q_OS_LINUX
        fullscreenOnSelectedDisplays(top, bottom, left, right);
#else
        fullscreenOnSelectedDisplays(xmin, ymin, w, h);
#endif
      }
    } else { // Fullscreen on the current single display.
#ifdef Q_OS_LINUX
      fullscreenOnSelectedDisplays(top, top, top, top);
#else
      fullscreenOnSelectedDisplay(selectedPrimaryScreen);
#endif
    }
  } else { // Exit fullscreen mode.
    exitFullscreen();
  }
  fullscreenEnabled = enabled;
  pendingFullscreen = false;
  setFocus();
  activateWindow();
  raise();

  if (!enabled) {
    ::fullScreen.setParam(false);
  }
  if (fullscreenEnabled != fullscreenEnabled0) {
    emit fullscreenChanged(fullscreenEnabled);
  }
}

void QVNCWindow::fullscreenOnCurrentDisplay()
{
#ifdef Q_OS_LINUX

#else
  QScreen* screen = getCurrentScreen();
  vlog.debug("QVNCWindow::fullscreenOnCurrentDisplay geometry=(%d, %d, %d, %d)",
             screen->geometry().x(),
             screen->geometry().y(),
             screen->geometry().width(),
             screen->geometry().height());
  show();
  QApplication::sync();
  windowHandle()->setScreen(screen);
  showFullScreen();

  // Capture the fullscreen geometry.
  double dpr = effectiveDevicePixelRatio(screen);
  QRect vg = screen->geometry();
  fullscreenX = vg.x();
  fullscreenY = vg.y();
  fullscreenWidth = vg.width() * dpr;
  fullscreenHeight = vg.height() * dpr;

  QAbstractVNCView* view = AppManager::instance()->getView();
  view->grabKeyboard();
#endif
}

void QVNCWindow::fullscreenOnSelectedDisplay(QScreen* screen)
{
  vlog.debug("QVNCWindow::fullscreenOnSelectedDisplay geometry=(%d, %d, %d, %d)",
             screen->geometry().x(),
             screen->geometry().y(),
             screen->geometry().width(),
             screen->geometry().height());
  show();
  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    windowHandle()->setScreen(screen);
    move(screen->geometry().x(), screen->geometry().y());
    resize(screen->geometry().width(), screen->geometry().height());
    showFullScreen();
    QAbstractVNCView* view = AppManager::instance()->getView();
    view->grabKeyboard();
  });
}

#ifdef Q_OS_LINUX
void QVNCWindow::fullscreenOnSelectedDisplays(int top, int bottom, int left, int right) // screens indices
{
  vlog.debug("QVNCWindow::fullscreenOnSelectedDisplays top=%d bottom=%d left=%d right=%d",
             top,
             bottom,
             left,
             right);

  show();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto display = QX11Info::display();
#else
  auto display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  int screen = DefaultScreen(display);

  XEvent e1;
  e1.xany.type = ClientMessage;
  e1.xany.window = winId();
  e1.xclient.message_type = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", 0);
  e1.xclient.format = 32;
  e1.xclient.data.l[0] = top;
  e1.xclient.data.l[1] = bottom;
  e1.xclient.data.l[2] = left;
  e1.xclient.data.l[3] = right;
  e1.xclient.data.l[4] = 0;
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &e1);
  XEvent e2;
  e2.xany.type = ClientMessage;
  e2.xany.window = winId();
  e2.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
  e2.xclient.format = 32;
  e2.xclient.data.l[0] = 1; // add
  e2.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &e2);
  QApplication::sync();

  QAbstractVNCView* view = AppManager::instance()->getView();
  view->grabKeyboard();

  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    activateWindow();
  });
}
#else
void QVNCWindow::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight) // pixels
{
  vlog.debug("QVNCWindow::fullscreenOnSelectedDisplays geometry=(%d, %d, %d, %d)",
             vx,
             vy,
             vwidth,
             vheight);
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  setWindowFlag(Qt::FramelessWindowHint, true);

  show();
  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    move(vx, vy);
    resize(vwidth, vheight);
    raise();
    activateWindow();
    QAbstractVNCView* view = AppManager::instance()->getView();
    view->grabKeyboard();
  });
}
#endif

void QVNCWindow::exitFullscreen()
{
  vlog.debug("QVNCWindow::exitFullscreen");
#ifdef Q_OS_LINUX
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto display = QX11Info::display();
#else
  auto display = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  int screen = DefaultScreen(display);

  XEvent e2;
  e2.xany.type = ClientMessage;
  e2.xany.window = winId();
  e2.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
  e2.xclient.format = 32;
  e2.xclient.data.l[0] = 2; // toggle
  e2.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
  XSendEvent(display, RootWindow(display, screen),
             0, SubstructureNotifyMask | SubstructureRedirectMask,
             &e2);
  QApplication::sync();
#else
  setWindowFlag(Qt::WindowStaysOnTopHint, false);
  setWindowFlag(Qt::FramelessWindowHint, false);

  showNormal();
  move(0, 0);
  windowHandle()->setScreen(previousScreen);
  restoreGeometry(previousGeometry);
  showNormal();
  QAbstractVNCView* view = AppManager::instance()->getView();
  view->ungrabKeyboard();
#endif
}

bool QVNCWindow::allowKeyboardGrab() const
{
  return fullscreenEnabled || pendingFullscreen;
}

bool QVNCWindow::isFullscreenEnabled() const
{
  return fullscreenEnabled;
}

void QVNCWindow::handleDesktopSize()
{
  vlog.debug("QVNCWindow::handleDesktopSize");
  double f = effectiveDevicePixelRatio();
  if (!QString(::desktopSize).isEmpty()) {
    int w, h;
    // An explicit size has been requested
    if (sscanf(::desktopSize, "%dx%d", &w, &h) != 2) {
      return;
    }
    remoteResize(w * f, h * f);
    vlog.debug("QVNCWindow::handleDesktopSize(explicit) size=(%d, %d)", w, h);
  } else if (::remoteResize) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    vlog.debug("QVNCWindow::handleDesktopSize(implicit) size=(%d, %d)", width(), height());
  }
}

void QVNCWindow::postRemoteResizeRequest()
{
  vlog.debug("QVNCWindow::postRemoteResizeRequest");
  resizeTimer->start();
}

void QVNCWindow::remoteResize(int w, int h)
{
  QVNCConnection* cc = AppManager::instance()->getConnection();
  rfb::ScreenSet layout;
  rfb::ScreenSet::const_iterator iter;
  double f = effectiveDevicePixelRatio();
  if ((!fullscreenEnabled && !pendingFullscreen) || (w > width() * f) || (h > height() * f)) {
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
  } else {
    uint32_t id;

    // In full screen we report all screens that are fully covered.
    rfb::Rect viewport_rect;
    viewport_rect.setXYWH(fullscreenX, fullscreenY, fullscreenWidth, fullscreenHeight);

    // If we can find a matching screen in the existing set, we use
    // that, otherwise we create a brand new screen.
    //
    // FIXME: We should really track screens better so we can handle
    //        a resized one.
    //
    QApplication* app = static_cast<QApplication*>(QApplication::instance());
    QList<QScreen*> screens = app->screens();
    //    std::sort(screens.begin(), screens.end(), [](QScreen *a, QScreen *b) {
    //                return a->geometry().x() == b->geometry().x() ? (a->geometry().y() < b->geometry().y()) :
    //                (a->geometry().x() < b->geometry().x());
    //              });
    for (QScreen*& screen : screens) {
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
        if ((iter->dimensions.tl.x == sx) && (iter->dimensions.tl.y == sy) && (iter->dimensions.width() == sw)
            && (iter->dimensions.height() == sh) && (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
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
        for (iter = cc->server()->screenLayout().begin(); iter != cc->server()->screenLayout().end(); ++iter) {
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
  if ((w == cc->server()->width()) && (h == cc->server()->height()) && (layout == cc->server()->screenLayout()))
    return;

  vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d", cc->server()->width(), cc->server()->height(), w, h);

  char buffer[2048];
  layout.print(buffer, sizeof(buffer));
  if (!layout.validate(w, h)) {
    vlog.error(_("Invalid screen layout computed for resize request!"));
    vlog.error("%s", buffer);
    return;
  } else {
    vlog.debug("%s", buffer);
  }
  vlog.debug("QVNCWindow::remoteResize size=(%d, %d) layout=%s", w, h, buffer);
  emit AppManager::instance()->getConnection()->writeSetDesktopSize(w, h, layout);
}

void QVNCWindow::fromBufferResize(int oldW, int oldH, int width, int height)
{
  vlog.debug("QVNCWindow::resize size=(%d, %d)", width, height);

  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    updateScrollbars();
  });

  if (this->width() == width && this->height() == height) {
    vlog.debug("QVNCWindow::resize ignored");
    return;
  }

  QAbstractVNCView* view = AppManager::instance()->getView();

  if (!view) {
    vlog.debug("QVNCWindow::resize !view");
    resize(width, height);
  } else {
    vlog.debug("QVNCWindow::resize view");
    if (QSize(oldW, oldH) == size()) {
      vlog.debug("QVNCWindow::resize because session and client were in sync");
      resize(width, height);
    }
  }
}

void QVNCWindow::showToast()
{
  toast->showToast();
}

void QVNCWindow::setWidget(QWidget *w)
{
  scrollArea->setWidget(w);
}

QWidget *QVNCWindow::takeWidget()
{
  return scrollArea->takeWidget();
}

void QVNCWindow::moveEvent(QMoveEvent* e)
{
  vlog.debug("QVNCWindow::moveEvent pos=(%d, %d) oldPos=(%d, %d)", e->pos().x(), e->pos().y(), e->oldPos().x(), e->oldPos().y());
  QWidget::moveEvent(e);
}

void QVNCWindow::resizeEvent(QResizeEvent* e)
{
  vlog.debug("QVNCWindow::resizeEvent size=(%d, %d)", e->size().width(), e->size().height());

  QVNCConnection* cc = AppManager::instance()->getConnection();

  vlog.debug("QVNCWindow::resizeEvent supportsSetDesktopSize=%d", cc->server()->supportsSetDesktopSize);
  if (::remoteResize && cc->server()->supportsSetDesktopSize) {
    postRemoteResizeRequest();
  }

  updateScrollbars();

  toast->setGeometry(rect());

  QWidget::resizeEvent(e);
}

void QVNCWindow::changeEvent(QEvent* e)
{
  if (e->type() == QEvent::WindowStateChange) {
    vlog.debug("QVNCWindow::changeEvent size=(%d, %d) state=%s oldState=%s",
               width(),
               height(),
               QVariant::fromValue(windowState()).toString().toStdString().c_str(),
               QVariant::fromValue((static_cast<QWindowStateChangeEvent*>(e))->oldState()).toString().toStdString().c_str());
  }
  QWidget::changeEvent(e);
}

void QVNCWindow::focusInEvent(QFocusEvent*)
{
  vlog.debug("QVNCWindow::focusInEvent");
  QAbstractVNCView* view = AppManager::instance()->getView();
  if (view)
    view->maybeGrabKeyboard();
}

void QVNCWindow::focusOutEvent(QFocusEvent*)
{
  vlog.debug("QVNCWindow::focusOutEvent");
  if (::fullscreenSystemKeys) {
    QAbstractVNCView* view = AppManager::instance()->getView();
    if (view)
      view->ungrabKeyboard();
  }
}
