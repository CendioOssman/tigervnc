#include "DesktopWindow.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Viewport.h"
#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "rfb/LogWriter.h"
#include "rfb/ScreenSet.h"
#include "toast.h"
#include "vncconnection.h"
#include "viewerconfig.h"

#include <QApplication>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QScreen>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTimer>
#include <QWindow>
#include <QProxyStyle>
#undef asprintf
#if defined(WIN32)
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include "x11utils.h"
#include "viewerconfig.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif
#endif
#ifdef WIN32
#include "win32.h"
#endif
#ifdef __APPLE__
#include "cocoa.h"
#endif

static rfb::LogWriter vlog("VNCWindow");

#ifdef __APPLE__
class ScrollBarStyle : public QProxyStyle
{
public:
  int styleHint(StyleHint sh, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *hret) const override
  {
    int ret = 0;

    switch (sh) {
    case SH_ScrollBar_Transient:
      ret = false;
      break;
    default:
      return QProxyStyle::styleHint(sh, opt, widget, hret);
    }

    return ret;
  }

  int pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *widget) const override
  {
    int ret = 0;

    switch (metric) {
    case PM_ScrollBarExtent:
      ret = cocoa_scrollbar_size();
      break;
    case PM_ScrollView_ScrollBarOverlap:
      ret = false;
      break;
    default:
      return QProxyStyle::pixelMetric(metric, opt, widget);
    }

    return ret;
  }
};
#endif

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
#if defined(Q_OS_LINUX)
    setStyleSheet("QScrollArea { background: transparent; }"
                  "QScrollArea > QWidget { background: transparent; }");
#elif defined(__APPLE__)
    horizontalScrollBar()->setStyle(&style);
    verticalScrollBar()->setStyle(&style);
    setStyle(&style);
#endif
  }

private:
#ifdef __APPLE__
  ScrollBarStyle style;
#endif
};

DesktopWindow::DesktopWindow(QVNCConnection* cc_, QWidget* parent)
  : QWidget(parent)
  , cc(cc_)
  , resizeTimer(new QTimer(this))
  , devicePixelRatio(devicePixelRatioF())
  , keyboardGrabberTimer(this, &DesktopWindow::handleGrab)
{
  setAttribute(Qt::WA_NativeWindow, true);
  setFocusPolicy(Qt::StrongFocus);

  setContentsMargins(0, 0, 0, 0);

  scrollArea = new ScrollArea;

  QPalette p(palette());
#ifdef Q_OS_LINUX
  scrollArea->horizontalScrollBar()->setPalette(p);
  scrollArea->horizontalScrollBar()->setAutoFillBackground(true);
  scrollArea->horizontalScrollBar()->setBackgroundRole(QPalette::Window);
  scrollArea->verticalScrollBar()->setPalette(p);
  scrollArea->verticalScrollBar()->setAutoFillBackground(true);
  scrollArea->verticalScrollBar()->setBackgroundRole(QPalette::Window);
#endif
  p.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
  setPalette(p);
  setBackgroundRole(QPalette::Window);

  // FIXME: this is a lot faster than before
  resizeTimer->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  resizeTimer->setSingleShot(true);
  connect(resizeTimer, &QTimer::timeout, this, &DesktopWindow::handleDesktopSize);

  toast = new Toast(this);

  QVBoxLayout* l = new QVBoxLayout;
  l->setSpacing(0);
  l->setContentsMargins(0,0,0,0);
  l->addWidget(scrollArea);
  setLayout(l);

#ifdef __APPLE__
  cocoa_fix_warping();
#endif

  connect(qApp, &QGuiApplication::screenAdded, this, &DesktopWindow::updateMonitorsFullscreen);
  connect(qApp, &QGuiApplication::screenRemoved, this, &DesktopWindow::updateMonitorsFullscreen);
}

DesktopWindow::~DesktopWindow() {}

void DesktopWindow::updateMonitorsFullscreen()
{
  if ((fullscreenEnabled || pendingFullscreen)
      && ViewerConfig::fullscreenType() != ViewerConfig::Current) {
    fullscreen(false);
    fullscreen(true);
  }
}

QList<int> DesktopWindow::fullscreenScreens() const
{
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  QList<int> applicableScreens;
  if (ViewerConfig::fullscreenType() == ViewerConfig::All) {
    for (int i = 0; i < screens.length(); i++) {
      applicableScreens << i;
    }
  } else if (ViewerConfig::fullscreenType() == ViewerConfig::Selected) {
    for (int const& id : ::fullScreenSelectedMonitors.getParam()) {
      int i = id - 1; // Screen ID in config is 1-origin.
      if (i < screens.length()) {
        applicableScreens << i;
        if (!ViewerConfig::canFullScreenOnMultiDisplays()) {
          // To have the same behavior as in screenselectionwidget and taking the last of the list
          applicableScreens = {i};
        }
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
  if (applicableScreens.isEmpty()) {
    applicableScreens << 0;
  }
  return applicableScreens;
}

QScreen* DesktopWindow::getCurrentScreen() const
{
  return windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
}

double DesktopWindow::effectiveDevicePixelRatio(QScreen* screen) const
{
#ifdef Q_OS_DARWIN
  return 1.0;
#endif

  if (screen) {
    return screen->devicePixelRatio();
  }
  return devicePixelRatio;
}

void DesktopWindow::fullscreen(bool enabled)
{
  vlog.debug("DesktopWindow::fullscreen enabled=%d", enabled);
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
    if (ViewerConfig::fullscreenType() != ViewerConfig::Current && selectedScreens.length() > 0) {
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
      vlog.debug("DesktopWindow::fullscreen geometry=(%d, %d, %d, %d)", xmin, ymin, w, h);

      if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
#ifdef Q_OS_LINUX
        if (ViewerConfig::canFullScreenOnMultiDisplays()) {
          if (ViewerConfig::hasWM()) {
            fullscreenOnSelectedDisplaysIndices(top, top, top, top);
          } else {
            fullscreenOnSelectedDisplaysPixels(xmin, ymin, w, h);
          }
        } else {
          fullscreenOnSelectedDisplay(selectedPrimaryScreen);
        }
#else
        fullscreenOnSelectedDisplay(selectedPrimaryScreen);
#endif
      } else { // Fullscreen on multiple displays.
#ifdef Q_OS_LINUX
        if (ViewerConfig::canFullScreenOnMultiDisplays()) {
          if (ViewerConfig::hasWM()) {
            fullscreenOnSelectedDisplaysIndices(top, bottom, left, right);
          } else {
            fullscreenOnSelectedDisplaysPixels(xmin, ymin, w, h);
          }
        } else {
          fullscreenOnSelectedDisplay(selectedPrimaryScreen);
        }
#else
        (void)top;
        (void)bottom;
        (void)left;
        (void)right;
        fullscreenOnSelectedDisplaysPixels(xmin, ymin, w, h);
#endif
      }
    } else { // Fullscreen on the current single display.
#ifdef Q_OS_LINUX
      if (ViewerConfig::canFullScreenOnMultiDisplays()) {
        if (ViewerConfig::hasWM()) {
          fullscreenOnSelectedDisplaysIndices(top, top, top, top);
        } else {
          fullscreenOnSelectedDisplaysPixels(selectedPrimaryScreen->geometry().x(),
                                             selectedPrimaryScreen->geometry().y(),
                                             selectedPrimaryScreen->geometry().width(),
                                             selectedPrimaryScreen->geometry().height());
        }
      } else {
        fullscreenOnSelectedDisplay(selectedPrimaryScreen);
      }
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

  ::fullScreen.setParam(enabled);
  if (fullscreenEnabled != fullscreenEnabled0) {
    emit fullscreenChanged(fullscreenEnabled);
  }
}

void DesktopWindow::fullscreenOnSelectedDisplay(QScreen* screen)
{
  vlog.debug("DesktopWindow::fullscreenOnSelectedDisplay geometry=(%d, %d, %d, %d)",
             screen->geometry().x(),
             screen->geometry().y(),
             screen->geometry().width(),
             screen->geometry().height());
#ifdef __APPLE__
  fullscreenOnSelectedDisplaysPixels(screen->geometry().x(),
                                     screen->geometry().y(),
                                     screen->geometry().width(),
                                     screen->geometry().height());
#else
  show();
  QApplication::sync();
  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    windowHandle()->setScreen(screen);
    move(screen->geometry().x(), screen->geometry().y());
    resize(screen->geometry().width(), screen->geometry().height());
    showFullScreen();
    view->setFocus();
    view->giveKeyboardFocus();
  });
#endif
}

#ifdef Q_OS_LINUX
void DesktopWindow::fullscreenOnSelectedDisplaysIndices(int top, int bottom, int left, int right) // screens indices
{
  vlog.debug("DesktopWindow::fullscreenOnSelectedDisplaysIndices top=%d bottom=%d left=%d right=%d",
             top,
             bottom,
             left,
             right);

  show();

  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto dpy = QX11Info::display();
#else
    auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
    int screen = DefaultScreen(dpy);

    X11Utils::fullscreen_screens(dpy, screen, winId(), top, bottom, left, right);
    X11Utils::fullscreen(dpy, screen, winId(), true);
    QApplication::sync();

    view->setFocus();
    view->giveKeyboardFocus();

    activateWindow();
  });
}
#endif

void DesktopWindow::fullscreenOnSelectedDisplaysPixels(int vx, int vy, int vwidth, int vheight) // pixels
{
  vlog.debug("DesktopWindow::fullscreenOnSelectedDisplaysPixels geometry=(%d, %d, %d, %d)",
             vx,
             vy,
             vwidth,
             vheight);
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  setWindowFlag(Qt::FramelessWindowHint, true);

#ifndef __APPLE__
  show();
#endif
  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    move(vx, vy);
    resize(vwidth, vheight);
#ifdef __APPLE__
    show();
#endif
    raise();
    activateWindow();
    view->setFocus();
    view->giveKeyboardFocus();
  });
}

void DesktopWindow::exitFullscreen()
{
  vlog.debug("DesktopWindow::exitFullscreen");
  ungrabKeyboard();
#ifdef Q_OS_LINUX
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto dpy = QX11Info::display();
#else
  auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  int screen = DefaultScreen(dpy);

  X11Utils::fullscreen(dpy, screen, winId(), false);
  QApplication::sync();
  if (QString(getenv("DESKTOP_SESSION")).isEmpty()) {
    move(0, 0);
    windowHandle()->setScreen(previousScreen);
    restoreGeometry(previousGeometry);
  }
#else
  setWindowFlag(Qt::WindowStaysOnTopHint, false);
  setWindowFlag(Qt::FramelessWindowHint, false);
#ifdef __APPLE__
  cocoa_update_window_level(this, false);
#endif

  showNormal();
  move(0, 0);
  windowHandle()->setScreen(previousScreen);
  restoreGeometry(previousGeometry);
  showNormal();
#endif
}

bool DesktopWindow::allowKeyboardGrab() const
{
  return fullscreenEnabled || pendingFullscreen;
}

bool DesktopWindow::isFullscreenEnabled() const
{
  return fullscreenEnabled;
}

void DesktopWindow::handleDesktopSize()
{
  vlog.debug("DesktopWindow::handleDesktopSize");
  double f = effectiveDevicePixelRatio();
  if (!QString(::desktopSize).isEmpty()) {
    int w, h;
    // An explicit size has been requested
    if (sscanf(::desktopSize, "%dx%d", &w, &h) != 2) {
      return;
    }
    remoteResize(w * f, h * f);
    vlog.debug("DesktopWindow::handleDesktopSize(explicit) size=(%d, %d)", w, h);
  } else if (::remoteResize) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    vlog.debug("DesktopWindow::handleDesktopSize(implicit) size=(%d, %d)", width(), height());
  }
}

void DesktopWindow::postRemoteResizeRequest()
{
  vlog.debug("DesktopWindow::postRemoteResizeRequest");
  resizeTimer->start();
}

void DesktopWindow::remoteResize(int w, int h)
{
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
    viewport_rect.setXYWH(x(), y(), width(), height());
    vlog.debug("viewport_rect(%d, %d, %dx%d)", x(), y(), width(), height());

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
  vlog.debug("DesktopWindow::remoteResize size=(%d, %d) layout=%s", w, h, buffer);
  emit cc->writeSetDesktopSize(w, h, layout);
}

void DesktopWindow::fromBufferResize(int oldW, int oldH, int width, int height)
{
  vlog.debug("DesktopWindow::resize size=(%d, %d)", width, height);

  if (this->width() == width && this->height() == height) {
    vlog.debug("DesktopWindow::resize ignored");
    return;
  }

  if (!view) {
    vlog.debug("DesktopWindow::resize !view");
    resize(width, height);
  } else {
    vlog.debug("DesktopWindow::resize view");
    if (QSize(oldW, oldH) == size()) {
      vlog.debug("DesktopWindow::resize because session and client were in sync");
      resize(width, height);
    }
  }
}

void DesktopWindow::showToast()
{
  toast->showToast();
  toast->setGeometry(rect());
}

void DesktopWindow::setWidget(Viewport* w)
{
  assert(view == nullptr);
  view = w;
  scrollArea->setWidget(w);
}

QWidget *DesktopWindow::takeWidget()
{
  view = nullptr;
  return scrollArea->takeWidget();
}

void DesktopWindow::postDialogClosing()
{
  raise();
  activateWindow();
  if (view) {
    view->setFocus();
    view->giveKeyboardFocus();
  }
}

void DesktopWindow::moveEvent(QMoveEvent* e)
{
  vlog.debug("DesktopWindow::moveEvent pos=(%d, %d) oldPos=(%d, %d)", e->pos().x(), e->pos().y(), e->oldPos().x(), e->oldPos().y());
  QWidget::moveEvent(e);
}

void DesktopWindow::resizeEvent(QResizeEvent* e)
{
  vlog.debug("DesktopWindow::resizeEvent size=(%d, %d)", e->size().width(), e->size().height());

  vlog.debug("DesktopWindow::resizeEvent supportsSetDesktopSize=%d", cc->server()->supportsSetDesktopSize);
  if (::remoteResize && cc->server()->supportsSetDesktopSize) {
    postRemoteResizeRequest();
  }

  toast->setGeometry(rect());

  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();

  QWidget::resizeEvent(e);
}

void DesktopWindow::changeEvent(QEvent* e)
{
  if (e->type() == QEvent::WindowStateChange) {
    int oldState = (static_cast<QWindowStateChangeEvent*>(e))->oldState();
    vlog.debug("DesktopWindow::changeEvent size=(%d, %d) state=%d oldState=%d",
               width(),
               height(),
               int(windowState()),
               oldState);
    vlog.debug("DesktopWindow::changeEvent fullscreenEnabled=%d pendingFullscreen=%d",
               fullscreenEnabled, pendingFullscreen);
    if (!fullscreenEnabled && !pendingFullscreen) {
      if (!(oldState & Qt::WindowFullScreen) && windowState() & Qt::WindowFullScreen) {
#ifdef __APPLE__
        vlog.debug("DesktopWindow::changeEvent window has gone fullscreen, we do not like it on Mac");
        QTimer::singleShot(std::chrono::milliseconds(1000), [=]() {
          fullscreen(false);
          fullscreen(true);
        });
#else
        vlog.debug("DesktopWindow::changeEvent window has gone fullscreen, checking if it is our doing");
        fullscreen(true);
#endif
      }
    } else if (fullscreenEnabled && !pendingFullscreen) {
      if (oldState & Qt::WindowFullScreen && !(windowState() & Qt::WindowFullScreen)) {
#ifndef __APPLE__
        vlog.debug("DesktopWindow::changeEvent window has left fullscreen, checking if it is our doing");
        fullscreen(false);
#endif
      }
    }
  }
  QWidget::changeEvent(e);
}

void DesktopWindow::focusInEvent(QFocusEvent*)
{
  vlog.debug("DesktopWindow::focusInEvent");
  if (view) {
    view->setFocus();
    view->giveKeyboardFocus();
  }
}

void DesktopWindow::focusOutEvent(QFocusEvent*)
{
  vlog.debug("DesktopWindow::focusOutEvent");
}

void DesktopWindow::closeEvent(QCloseEvent* e)
{
  emit closed();
  QWidget::closeEvent(e);
}

bool DesktopWindow::event(QEvent* event)
{
  switch (event->type()) {
  case QEvent::WindowActivate:
    vlog.debug("DesktopWindow::WindowActivate");
    maybeGrabKeyboard();
    break;
  case QEvent::WindowDeactivate:
    vlog.debug("DesktopWindow::WindowActivate");
    if (::fullscreenSystemKeys) {
      ungrabKeyboard();
    }
    break;
  default:
    break;
  }

  return QWidget::event(event);
}

void DesktopWindow::maybeGrabKeyboard()
{
  if (::fullscreenSystemKeys && allowKeyboardGrab() && isActiveWindow()) {
    grabKeyboard();
  }
}

void DesktopWindow::grabKeyboard()
{
#if defined(WIN32)
  int ret = win32_enable_lowlevel_keyboard((HWND)winId());
  if (ret != 0)
  {
      vlog.error(_("Failure grabbing keyboard"));
      return;
  }
#elif defined(__APPLE__)
  int ret = cocoa_capture_displays(fullscreenScreens());
  if (ret == 1) {
      vlog.error(_("Failure grabbing keyboard"));
      return;
  }
#else
  keyboardGrabberTimer.stop();
  Display* dpy;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  int ret = XGrabKeyboard(dpy, winId(), True, GrabModeAsync, GrabModeAsync, CurrentTime);
  if (ret) {
    if (ret == AlreadyGrabbed) {
      // It seems like we can race with the WM in some cases.
      // Try again in a bit.
      keyboardGrabberTimer.start(500);
    } else {
      vlog.error(_("Failure grabbing keyboard"));
    }
    return;
  }
#endif

  QPoint gpos = QCursor::pos();
  QPoint lpos = mapFromGlobal(gpos);
  QRect r = rect();
  if (r.contains(lpos)) {
    grabPointer();
  }
}

void DesktopWindow::ungrabKeyboard()
{
  ungrabPointer();

#if defined(WIN32)
  win32_disable_lowlevel_keyboard((HWND)winId());
#elif defined(__APPLE__)
  cocoa_release_displays();
#else
  Display* dpy;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  keyboardGrabberTimer.stop();
  XUngrabKeyboard(dpy, CurrentTime);
#endif
}

void DesktopWindow::grabPointer()
{

}

void DesktopWindow::ungrabPointer()
{

}

void DesktopWindow::handleGrab(rfb::Timer*)
{
  maybeGrabKeyboard();
}
