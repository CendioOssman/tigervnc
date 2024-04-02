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

static rfb::LogWriter vlog("VNCWindow");

QVNCWindow::QVNCWindow(QWidget* parent)
  : QScrollArea(parent)
  , resizeTimer(new QTimer(this))
  , devicePixelRatio(devicePixelRatioF())
{
  setFocusPolicy(Qt::StrongFocus);

  setContentsMargins(0, 0, 0, 0);
  setViewportMargins(0, 0, 0, 0);
  setFrameStyle(QFrame::NoFrame);
  setLineWidth(0);

  QPalette p(palette());
  p.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
  setPalette(p);
  setBackgroundRole(QPalette::Window);
  horizontalScrollBar()->setStyle(QStyleFactory::create("Fusion"));
  verticalScrollBar()->setStyle(QStyleFactory::create("Fusion"));

  setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QScrollBar* hScrollBar = horizontalScrollBar();
  hScrollBar->setParent(this);
  hScrollBar->setFixedHeight(14);
  QScrollBar* vScrollBar = verticalScrollBar();
  vScrollBar->setParent(this);
  vScrollBar->setFixedWidth(14);

  QGridLayout* scrollAreaLayout = new QGridLayout(this);
  scrollAreaLayout->setContentsMargins(0, 0, 0, 0);
  scrollAreaLayout->setSpacing(0);
  scrollAreaLayout->setRowStretch(0, 1);
  scrollAreaLayout->setColumnStretch(0, 1);
  scrollAreaLayout->addWidget(vScrollBar, 0, 1);
  scrollAreaLayout->addWidget(hScrollBar, 1, 0);

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  int geom_x = 0, geom_y = 0;
  if (!ViewerConfig::config()->geometry().isEmpty()) {
    int nfields =
        sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(), "+%d+%d", &geom_x, &geom_y);
    if (nfields != 2) {
      int geom_w, geom_h;
      nfields = sscanf((const char*)ViewerConfig::config()->geometry().toStdString().c_str(),
                       "%dx%d+%d+%d",
                       &geom_w,
                       &geom_h,
                       &geom_x,
                       &geom_y);
      if (nfields != 4) {
        vlog.error(_("Invalid geometry specified!"));
      }
    }
    if (nfields == 2 || nfields == 4) {
      move(geom_x, geom_y);
    }
  }

  resizeTimer->setInterval(100); // <-- DesktopWindow::resize(int x, int y, int w, int h)
  resizeTimer->setSingleShot(true);
  connect(resizeTimer, &QTimer::timeout, this, &QVNCWindow::handleDesktopSize);
}

QVNCWindow::~QVNCWindow() {}

void QVNCWindow::updateScrollbars()
{
  QAbstractVNCView* view = AppManager::instance()->getView();
  qDebug() << "QVNCWindow::updateScrollbars" << view->pixmapSize() << size();

  if (view->pixmapSize().width() > width()) {
    horizontalScrollBar()->show();
  } else {
    horizontalScrollBar()->hide();
  }

  if (view->pixmapSize().height() > height()) {
    verticalScrollBar()->show();
  } else {
    verticalScrollBar()->hide();
  }

  qDebug() << "QVNCWindow::updateScrollbars"
           << "END";
}

QList<int> QVNCWindow::fullscreenScreens() const
{
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  QList<int> applicableScreens;
  if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSAll) {
    for (int i = 0; i < screens.length(); i++) {
      applicableScreens << i;
    }
  } else if (ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSSelected) {
    for (int& id : ViewerConfig::config()->selectedScreens()) {
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
  QList<QScreen*> screens = qApp->screens();
  for (QScreen*& screen : screens) {
    if (screen->geometry().contains(mapToGlobal(cursor().pos()))) {
      return screen;
    }
  }
  return screens[0];
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
  qDebug() << "QVNCWindow::fullscreen: enabled=" << enabled;
  //  TODO: Flag fullscreenEnabled seems have to be disabled before executing fullscreen().
  bool fullscreenEnabled0 = fullscreenEnabled;
  fullscreenEnabled = false;
  pendingFullscreen = enabled;
  resizeTimer->stop();
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  if (enabled) {
    // cf. DesktopWindow::fullscreen_on()
    if (!fullscreenEnabled) {
      previousGeometry = saveGeometry();
      previousScreen = getCurrentScreen();
    }

    auto mode = ViewerConfig::config()->fullScreenMode();
    QList<int> selectedScreens = fullscreenScreens();
    if (mode != ViewerConfig::FSCurrent && selectedScreens.length() > 0) {
      QScreen* selectedPrimaryScreen = screens[selectedScreens[0]];
      int xmin = INT_MAX;
      int ymin = INT_MAX;
      int xmax = INT_MIN;
      int ymax = INT_MIN;
      for (int& screenIndex : selectedScreens) {
        QScreen* screen = screens[screenIndex];
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
      // qDebug() << "Fullsize Geometry=" << QRect(xmin, ymin, w, h);
      //  Capture the fullscreen geometry.
      fullscreenX = xmin;
      fullscreenY = ymin;
      fullscreenWidth = w;
      fullscreenHeight = h;

      if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
        fullscreenOnSelectedDisplay(selectedPrimaryScreen, xmin, ymin, w, h);
      } else { // Fullscreen on multiple displays.
        fullscreenOnSelectedDisplays(xmin, ymin, w, h);
      }
    } else { // Fullscreen on the current single display.
      fullscreenOnCurrentDisplay();
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
    ViewerConfig::config()->setFullScreen(false);
  }
  if (fullscreenEnabled != fullscreenEnabled0) {
    emit fullscreenChanged(fullscreenEnabled);
  }
}

void QVNCWindow::fullscreenOnCurrentDisplay()
{
  QScreen* screen = getCurrentScreen();
  qDebug() << "QVNCWindow::fullscreenOnCurrentDisplay" << screen->geometry();
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
}

void QVNCWindow::fullscreenOnSelectedDisplay(QScreen* screen, int vx, int vy, int, int)
{
  qDebug() << "QVNCWindow::fullscreenOnSelectedDisplay" << screen->geometry();
  windowHandle()->setScreen(screen);
  move(vx, vy);
  showFullScreen();
  QAbstractVNCView* view = AppManager::instance()->getView();
  view->grabKeyboard();
}

void QVNCWindow::fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight)
{
  qDebug() << "QVNCWindow::fullscreenOnSelectedDisplays" << vx << vy << vwidth << vheight;
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  setWindowFlag(Qt::FramelessWindowHint, true);
#ifdef Q_OS_LINUX
  setWindowFlag(Qt::BypassWindowManagerHint, true);
#endif

  move(vx, vy);
  resize(vwidth, vheight);
  showNormal();
  QAbstractVNCView* view = AppManager::instance()->getView();
  view->grabKeyboard();

#ifdef Q_OS_LINUX
  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    activateWindow();
  });
#endif
}

void QVNCWindow::exitFullscreen()
{
  qDebug() << "QVNCWindow::exitFullscreen";
  setWindowFlag(Qt::WindowStaysOnTopHint, false);
  setWindowFlag(Qt::FramelessWindowHint, false);
#ifdef Q_OS_LINUX
  setWindowFlag(Qt::BypassWindowManagerHint, false);
#endif

  showNormal();
  move(0, 0);
  windowHandle()->setScreen(previousScreen);
  restoreGeometry(previousGeometry);
  showNormal();
  QAbstractVNCView* view = AppManager::instance()->getView();
  view->ungrabKeyboard();
}

bool QVNCWindow::allowKeyboardGrab() const
{
  return fullscreenEnabled || pendingFullscreen;
}

void QVNCWindow::handleDesktopSize()
{
  qDebug() << "QVNCWindow::handleDesktopSize";
  double f = effectiveDevicePixelRatio();
  if (!ViewerConfig::config()->desktopSize().isEmpty()) {
    int w, h;
    // An explicit size has been requested
    if (sscanf(ViewerConfig::config()->desktopSize().toStdString().c_str(), "%dx%d", &w, &h) != 2) {
      return;
    }
    remoteResize(w * f, h * f);
    // qDebug() << "QVNCWindow::handleDesktopSize(explicit): width=" << w << ", height=" << h;
  } else if (ViewerConfig::config()->remoteResize()) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width() * f, height() * f);
    // qDebug() << "QVNCWindow::handleDesktopSize(implicit): width=" << width() << ", height=" << height();
  }
}

void QVNCWindow::postRemoteResizeRequest()
{
  qDebug() << "QVNCWindow::postRemoteResizeRequest";
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
  qDebug() << "QVNCWindow::remoteResize: w=" << w << ", h=" << h << ", layout=" << buffer;
  emit AppManager::instance()->getConnection()->writeSetDesktopSize(w, h, layout);
}

void QVNCWindow::resize(int width, int height)
{
  qDebug() << "QVNCWindow::resize: w=" << width << ", h=" << height << ", widgetResizable=" << widgetResizable();
  QScrollArea::resize(width, height);
}

void QVNCWindow::moveEvent(QMoveEvent* e)
{
  qDebug() << "QVNCWindow::moveEvent: pos=" << e->pos() << "oldPos=" << e->oldPos();
  QScrollArea::moveEvent(e);
}

void QVNCWindow::resizeEvent(QResizeEvent* e)
{
  qDebug() << "QVNCWindow::resizeEvent: w=" << e->size().width() << ", h=" << e->size().height()
           << ", widgetResizable=" << widgetResizable();

  QVNCConnection* cc = AppManager::instance()->getConnection();

  if (ViewerConfig::config()->remoteResize() && cc->server()->supportsSetDesktopSize) {
    postRemoteResizeRequest();
  }

  updateScrollbars();

  QScrollArea::resizeEvent(e);
}

void QVNCWindow::changeEvent(QEvent* e)
{
  if (e->type() == QEvent::WindowStateChange) {
    qDebug() << "QVNCWindow::changeEvent: w=" << width() << ",h=" << height() << ",state=" << windowState()
             << ",oldState=" << (static_cast<QWindowStateChangeEvent*>(e))->oldState();
  }
  QScrollArea::changeEvent(e);
}

void QVNCWindow::focusInEvent(QFocusEvent*)
{
  qDebug() << "QVNCWindow::focusInEvent";
  QAbstractVNCView* view = AppManager::instance()->getView();
  if (view)
    view->maybeGrabKeyboard();
}

void QVNCWindow::focusOutEvent(QFocusEvent*)
{
  qDebug() << "QVNCWindow::focusOutEvent";
  if (ViewerConfig::config()->fullscreenSystemKeys()) {
    QAbstractVNCView* view = AppManager::instance()->getView();
    if (view)
      view->ungrabKeyboard();
  }
}
