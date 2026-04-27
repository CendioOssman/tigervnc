/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <rdr/Exception.h>

#include <rfb/LogWriter.h>
#include <rfb/CMsgWriter.h>

#include <QApplication>
#include <QMoveEvent>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include "DesktopWindow.h"
#include "OptionsDialog.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"
#include "CConn.h"
#include "Toast.h"
#include "Viewport.h"

#if defined(WIN32)
#include "win32.h"
#elif defined(__APPLE__)
#include "cocoa.h"
#else
#include "x11.h"
#endif

static rfb::LogWriter vlog("DesktopWindow");

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

DesktopWindow::DesktopWindow(int w, int h, const char *name,
                             CConn* cc_, QWidget* parent)
  : QWidget(parent)
  , cc(cc_)
  , firstUpdate(true)
  , keyboardGrabbed(false)
  , mouseGrabbed(false)
  , resizeTimer(new QTimer(this))
{
  setContentsMargins(0, 0, 0, 0);
  resize(w, h);

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

  viewport = new Viewport(w, h, cc, scrollArea);

  scrollArea->setWidget(viewport);
  setName(name);

  OptionsDialog::addCallback(handleOptions, this);

  // Hack. See below...
  windowHandle()->installEventFilter(this);

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

  // FIXME: Not sure why this is needed
  viewport->setFocus();

#ifdef __APPLE__
  cocoa_prevent_native_fullscreen(this);
#endif

  connect(qApp, &QGuiApplication::screenAdded, this, &DesktopWindow::updateMonitorsFullscreen);
  connect(qApp, &QGuiApplication::screenRemoved, this, &DesktopWindow::updateMonitorsFullscreen);

  connect(qApp, &QGuiApplication::focusWindowChanged, this,
          &DesktopWindow::handleFocusWindowChanged);

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  bool force_position = false;
  int geom_x = 0, geom_y = 0;
  if (strcmp(::geometry, "") != 0) {
    int matched;
    matched = sscanf((const char*)::geometry, "+%d+%d", &geom_x, &geom_y);
    if (matched == 2) {
      force_position = true;
    } else {
      int geom_w, geom_h;
      matched = sscanf((const char*)::geometry, "%dx%d+%d+%d", &geom_w, &geom_h, &geom_x, &geom_y);
      switch (matched) {
      case 4:
        force_position = true;
        /* fall through */
      case 2:
        w = geom_w;
        h = geom_h;
        break;
      default:
        geom_x = geom_y = 0;
        vlog.error(_("Invalid geometry specified!"));
      }
    }
  }

  // Many window managers don't properly resize overly large windows,
  // so we'll have to do some sanity checks ourselves here
  QScreen* screen = nullptr;
  QRect screenGeometry;

  if (force_position) {
    screen = qApp->screenAt({geom_x, geom_y});
  } else {
    // If we don't explicitly request a position then we don't know which
    // monitor the window manager might place us on. Assume the popular
    // behaviour of following the cursor.
    screen = qApp->screenAt(QCursor::pos());
  }
  if (screen == nullptr)
    screen = qApp->primaryScreen();

  screenGeometry = screen->availableGeometry();

  if ((w > screenGeometry.width()) || (h > screenGeometry.height())) {
    vlog.info(_("Reducing window size to fit on current monitor"));
    if (w > screenGeometry.width())
      w = screenGeometry.width();
    if (h > screenGeometry.height())
      h = screenGeometry.height();
  }

  // FIXME: This doesn't adjust for title, like FLTK does
  if (force_position)
    move(geom_x, geom_y);
  resize(w, h);

  if (::fullScreen) {
#ifdef __APPLE__
    QTimer::singleShot(100, [=](){
#endif
      fullscreen(true);
#ifdef __APPLE__
    });
#endif
  } else {
    vlog.debug("SHOW");
    show();
  }

  // Show hint about menu key
  QTimer::singleShot(500, this, &DesktopWindow::menuOverlay);

  // By default we get a slight delay when we warp the pointer, something
  // we don't want or we'll get jerky movement
#ifdef __APPLE__
  cocoa_event_delay(0);
#endif
}


DesktopWindow::~DesktopWindow()
{
  // Don't leave any dangling grabs as they are not automatically
  // cleaned up on all platforms
  ungrabPointer();
  ungrabKeyboard();

  OptionsDialog::removeCallback(handleOptions);
}


const rfb::PixelFormat &DesktopWindow::getPreferredPF()
{
  return viewport->getPreferredPF();
}


void DesktopWindow::updateMonitorsFullscreen()
{
  bool allMonitors = !strcasecmp(fullScreenMode, "all");
  bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");

  if ((fullscreenEnabled || pendingFullscreen)
      && (allMonitors || selectedMonitors)) {
    fullscreen(false);
    fullscreen(true);
  }
}

void DesktopWindow::setName(const char *name)
{
  char windowNameStr[256];

  snprintf(windowNameStr, 256, "%.240s - TigerVNC", name);

  setWindowTitle(windowNameStr);
}


// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.

void DesktopWindow::updateWindow()
{
  if (firstUpdate) {
    if (cc->server.supportsSetDesktopSize) {
      resizeTimer->start();
    }
    firstUpdate = false;
  }

  viewport->updateWindow();
}


void DesktopWindow::resizeFramebuffer(int new_w, int new_h)
{
  if ((new_w == viewport->width()) && (new_h == viewport->height()))
    return;

  // If we're letting the viewport match the window perfectly, then
  // keep things that way for the new size, otherwise just keep things
  // like they are.
  if (!isFullscreenEnabled() && !isMaximized()) {
    if (size() == viewport->size())
      resize(new_w, new_h);
  }

  viewport->resize(new_w, new_h);
}

void DesktopWindow::setCursor(int width, int height,
                              const rfb::Point& hotspot,
                              const uint8_t* pixels)
{
  viewport->setCursor(width, height, hotspot, pixels);
}


void DesktopWindow::setCursorPos(const rfb::Point& pos)
{
  if (!mouseGrabbed) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  QPoint gp = mapToGlobal(QPoint(pos.x, pos.y) + viewport->pos());
#if defined(__APPLE__)
  cocoa_set_cursor_pos(gp.x(), gp.y());
#else
  QCursor::setPos(gp.x(), gp.y());
#endif
}


void DesktopWindow::setLEDState(unsigned int state)
{
  viewport->setLEDState(state);
}


void DesktopWindow::handleClipboardRequest()
{
  viewport->handleClipboardRequest();
}

void DesktopWindow::handleClipboardAnnounce(bool available)
{
  viewport->handleClipboardAnnounce(available);
}

void DesktopWindow::handleClipboardData(const char* text)
{
  viewport->handleClipboardData(text);
}


void DesktopWindow::resize(int w, int h)
{
#if ! (defined(WIN32) || defined(__APPLE__))
  // X11 window managers will treat a resize to cover the entire
  // monitor as a request to go full screen. Make sure we avoid this.
  if (!isFullscreenEnabled()) {
    QList<QScreen*> screens = qApp->screens();
    for (QScreen* screen : screens) {
      // We can't trust x and y if the window isn't mapped as the
      // window manager might adjust those numbers
      if (isVisible() && ((screen->geometry().x() != x()) ||
                          (screen->geometry().y() != y())))
        continue;

      if ((screen->geometry().width() != w) ||
          (screen->geometry().height() != h))
        continue;

      vlog.info(_("Adjusting window size to avoid accidental full-screen request"));
      // Assume a panel of some form and adjust the height
      h -= 40;
    }
  }
#endif

  QWidget::resize(w, h);
}

void DesktopWindow::resize(const QSize& size)
{
  resize(size.width(), size.height());
}


void DesktopWindow::menuOverlay()
{
  if (strcmp((const char*)menuKey, "") != 0) {
    setOverlay(_("Press %s to open the context menu"),
               (const char*)menuKey);
  }
}

void DesktopWindow::setOverlay(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  toast->showToast(textbuf);
  toast->setGeometry(rect());
}


void DesktopWindow::moveEvent(QMoveEvent* e)
{
  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();

  QWidget::moveEvent(e);
}

void DesktopWindow::resizeEvent(QResizeEvent* e)
{
  vlog.debug("DesktopWindow::resizeEvent size=(%d, %d)", e->size().width(), e->size().height());

  vlog.debug("DesktopWindow::resizeEvent supportsSetDesktopSize=%d", cc->server.supportsSetDesktopSize);
  if (::remoteResize && cc->server.supportsSetDesktopSize) {
    resizeTimer->start();
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

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void DesktopWindow::enterEvent(QEvent *event)
#else
void DesktopWindow::enterEvent(QEnterEvent *event)
#endif
{
  if (keyboardGrabbed)
    grabPointer();

  QWidget::enterEvent(event);
}

void DesktopWindow::leaveEvent(QEvent *event)
{
  if (mouseGrabbed)
    ungrabPointer();

  QWidget::leaveEvent(event);
}

void DesktopWindow::mouseMoveEvent()
{
  if (mouseGrabbed) {
    // We don't get leaveEvent() with a grabbed pointer, so check
    // manually
    if ((QGuiApplication::mouseButtons() == Qt::NoButton) &&
        !geometry().contains(QCursor::pos()))
      ungrabPointer();

#if !defined(WIN32) && !defined(__APPLE__)
    // We also don't get sensible coordinates on zaphod setups
    if (!x11_is_pointer_on_same_screen(this))
      ungrabPointer();
#endif
  }
}

void DesktopWindow::mouseReleaseEvent()
{
  // We usually fail to grab the mouse if a mouse button was
  // pressed when we gained focus (e.g. clicking on our window),
  // so we may need to try again when the button is released.
  if (keyboardGrabbed && !mouseGrabbed)
    grabPointer();
}

void DesktopWindow::showEvent(QShowEvent* event)
{
#if !defined(WIN32) && !defined(__APPLE__)
  // Request ability to grab keyboard under Xwayland
  x11_win_may_grab(this);
#endif

  QWidget::showEvent(event);
}

void DesktopWindow::closeEvent(QCloseEvent* e)
{
  QWidget::closeEvent(e);
  ::disconnect();
}


bool DesktopWindow::eventFilter(QObject* watched, QEvent* event)
{
  switch (event->type()) {
  // Mouse events are normally only sent to the widget under the mouse,
  // so we need to intercept them here to actually see them
  case QEvent::MouseButtonRelease:
    mouseReleaseEvent();
    break;
  case QEvent::MouseMove:
    mouseMoveEvent();
    break;
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}


QList<QScreen*> DesktopWindow::fullscreenScreens() const
{
  QApplication* app = static_cast<QApplication*>(QApplication::instance());
  QList<QScreen*> screens = app->screens();
  QList<QScreen*> applicableScreens;
  if (!strcasecmp(fullScreenMode, "all")) {
    for (int i = 0; i < screens.length(); i++) {
      applicableScreens << screens[i];
    }
  } else if (!strcasecmp(fullScreenMode, "selected")) {
    for (QScreen* screen : ::fullScreenSelectedMonitors.getParam()) {
      applicableScreens << screen;
    }
  } else {
    QScreen* cscreen = getCurrentScreen();
    for (int i = 0; i < screens.length(); i++) {
      if (screens[i] == cscreen) {
        applicableScreens << screens[i];
        break;
      }
    }
  }
  if (applicableScreens.isEmpty()) {
    applicableScreens << screens[0];
  }
  return applicableScreens;
}

QScreen* DesktopWindow::getCurrentScreen() const
{
  return windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
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

#if defined(Q_OS_LINUX)
    bool hasWM = x11_has_wm();
#endif

    bool allMonitors = !strcasecmp(fullScreenMode, "all");
    bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");
    QList<QScreen*> selectedScreens = fullscreenScreens();
    QScreen* top, * bottom, * left, * right;
    QScreen* selectedPrimaryScreen = selectedScreens[0];
    top = bottom = left = right = selectedScreens[0];
    if ((allMonitors || selectedMonitors) && selectedScreens.length() > 0) {
      int xmin = INT_MAX;
      int ymin = INT_MAX;
      int xmax = INT_MIN;
      int ymax = INT_MIN;
      for (QScreen* screen : selectedScreens) {
        QRect rect = screen->geometry();
        if (xmin > rect.x()) {
          left = screen;
          xmin = rect.x();
        }
        if (xmax < rect.x() + rect.width()) {
          right = screen;
          xmax = rect.x() + rect.width();
        }
        if (ymin > rect.y()) {
          top = screen;
          ymin = rect.y();
        }
        if (ymax < rect.y() + rect.height()) {
          bottom = screen;
          ymax = rect.y() + rect.height();
        }
      }
      int w = xmax - xmin;
      int h = ymax - ymin;
      vlog.debug("DesktopWindow::fullscreen geometry=(%d, %d, %d, %d)", xmin, ymin, w, h);

      if (selectedScreens.length() == 1) { // Fullscreen on the selected single display.
#ifdef Q_OS_LINUX
        if (hasWM) {
          fullscreenOnSelectedDisplaysIndices(top, top, top, top);
        } else {
          fullscreenOnSelectedDisplaysPixels(xmin, ymin, w, h);
        }
#else
        fullscreenOnSelectedDisplay(selectedPrimaryScreen);
#endif
      } else { // Fullscreen on multiple displays.
#ifdef Q_OS_LINUX
        if (hasWM) {
          fullscreenOnSelectedDisplaysIndices(top, bottom, left, right);
        } else {
          fullscreenOnSelectedDisplaysPixels(xmin, ymin, w, h);
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
      if (hasWM) {
        fullscreenOnSelectedDisplaysIndices(top, top, top, top);
      } else {
        fullscreenOnSelectedDisplaysPixels(selectedPrimaryScreen->geometry().x(),
                                            selectedPrimaryScreen->geometry().y(),
                                            selectedPrimaryScreen->geometry().width(),
                                            selectedPrimaryScreen->geometry().height());
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
    viewport->setFocus();
  });
#endif
}

#ifdef Q_OS_LINUX
void DesktopWindow::fullscreenOnSelectedDisplaysIndices(QScreen* top, QScreen* bottom, QScreen* left, QScreen* right) // screens indices
{

  show();

  QTimer::singleShot(std::chrono::milliseconds(100), [=]() {
    x11_fullscreen_screens(this, top, bottom, left, right);
    x11_fullscreen(this, true);
    QApplication::sync();

    viewport->setFocus();

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
    viewport->setFocus();
  });
}

void DesktopWindow::exitFullscreen()
{
  vlog.debug("DesktopWindow::exitFullscreen");
  ungrabKeyboard();
#ifdef Q_OS_LINUX
  x11_fullscreen(this, false);
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
  cocoa_normal_window_level(this);
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

void DesktopWindow::maybeGrabKeyboard()
{
  if (fullscreenSystemKeys && allowKeyboardGrab() && isActiveWindow())
    grabKeyboard();
}

void DesktopWindow::grabKeyboard()
{
  // FIXME: Investigate QWidget::grabKeyboard()
#if defined(WIN32)
  int ret;

  ret = win32_enable_lowlevel_keyboard((HWND)winId());
  if (ret != 0) {
    vlog.error(_("Failure grabbing keyboard"));
    return;
  }
#elif defined(__APPLE__)
  int ret;

  ret = cocoa_capture_displays(this);
  if (ret != 0) {
    vlog.error(_("Failure grabbing keyboard"));
    return;
  }
#else
  bool ret;

  ret = x11_grab_keyboard(this);
  if (!ret) {
    vlog.error(_("Failure grabbing control of the keyboard"));
    return;
  }
#endif

  keyboardGrabbed = true;

  if (underMouse())
    grabPointer();
}


void DesktopWindow::ungrabKeyboard()
{
  keyboardGrabbed = false;

  ungrabPointer();

#if defined(WIN32)
  win32_disable_lowlevel_keyboard((HWND)winId());
#elif defined(__APPLE__)
  cocoa_release_displays(this);
#else
  // Qt has a grab so lets not mess with it
  if (keyboardGrabber() != nullptr)
    return;
  // Popups have an implicit grab that keyboardGrabber() doesn't show
  QWindow* focused = qApp->focusWindow();
  if ((focused != nullptr) &&
      (focused->flags() & Qt::WindowType_Mask) == Qt::Popup)
    return;

  x11_ungrab_keyboard();
#endif
}


void DesktopWindow::grabPointer()
{
  // FIXME: Investigate QWidget::grabMouse()

#if !defined(WIN32) && !defined(__APPLE__)
  // We also need to grab the pointer as some WMs like to grab buttons
  // combined with modifies (e.g. Alt+Button0 in metacity).

  // Having a button pressed prevents us from grabbing, we make
  // a new attempt in fltkHandle()
  if (!x11_grab_pointer(this))
    return;
#endif

  mouseGrabbed = true;
}


void DesktopWindow::ungrabPointer()
{
  mouseGrabbed = false;

#if !defined(WIN32) && !defined(__APPLE__)
  x11_ungrab_pointer();
#endif
}

void DesktopWindow::handleFocusWindowChanged(QWindow* window)
{
  // Focus might not stay with us just because we have grabbed the
  // keyboard. E.g. we might have sub windows, or we're not using
  // all monitors and the user clicked on another application.
  // Make sure we update our grabs with the focus changes.
  if (window == windowHandle()) {
    maybeGrabKeyboard();
  } else {
    if (fullscreenSystemKeys)
      ungrabKeyboard();
  }
}


void DesktopWindow::handleDesktopSize()
{
  if (strcmp(desktopSize, "") != 0) {
    int width, height;

    // An explicit size has been requested

    if (sscanf(desktopSize, "%dx%d", &width, &height) != 2)
      return;

    remoteResize(width, height);
  } else if (::remoteResize) {
    // No explicit size, but remote resizing is on so make sure it
    // matches whatever size the window ended up being
    remoteResize(width(), height());
  }
}

void DesktopWindow::remoteResize(int w, int h)
{
  rfb::ScreenSet layout;
  rfb::ScreenSet::const_iterator iter;
  if ((!fullscreenEnabled && !pendingFullscreen) || (w > width()) || (h > height())) {
    // In windowed mode (or the framebuffer is so large that we need
    // to scroll) we just report a single virtual screen that covers
    // the entire framebuffer.

    layout = cc->server.screenLayout();

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
    QRect sg;
    rfb::Rect viewport_rect, screen_rect;

    // In full screen we report all screens that are fully covered.

    viewport_rect.setXYWH(x() + (width() - w)/2, y() + (height() - h)/2,
                          w, h);

    // If we can find a matching screen in the existing set, we use
    // that, otherwise we create a brand new screen.
    //
    // FIXME: We should really track screens better so we can handle
    //        a resized one.
    //
    QList<QScreen*> screens = qApp->screens();
    for (QScreen*& screen : screens) {
      sg = screen->geometry();

      // Check that the screen is fully inside the framebuffer
      screen_rect.setXYWH(sg.x(), sg.y(), sg.width(), sg.height());
      if (!screen_rect.enclosed_by(viewport_rect))
        continue;

      // Adjust the coordinates so they are relative to our viewport
      sg.translate(-viewport_rect.tl.x, -viewport_rect.tl.y);

      // Look for perfectly matching existing screen that is not yet present in
      // in the screen layout...
      for (iter = cc->server.screenLayout().begin();
           iter != cc->server.screenLayout().end(); ++iter) {
        if ((iter->dimensions.tl.x == sg.x()) &&
            (iter->dimensions.tl.y == sg.y()) &&
            (iter->dimensions.width() == sg.width()) &&
            (iter->dimensions.height() == sg.height()) &&
            (std::find(layout.begin(), layout.end(), *iter) == layout.end()))
          break;
      }

      // Found it?
      if (iter != cc->server.screenLayout().end()) {
        layout.add_screen(*iter);
        continue;
      }

      // Need to add a new one, which means we need to find an unused id
      while (true) {
        id = rand();
        for (iter = cc->server.screenLayout().begin();
             iter != cc->server.screenLayout().end(); ++iter) {
          if (iter->id == id)
            break;
        }

        if (iter == cc->server.screenLayout().end())
          break;
      }

      layout.add_screen(
        rfb::Screen(id, sg.x(), sg.y(), sg.width(), sg.height(), 0));
    }

    // If the viewport doesn't match a physical screen, then we might
    // end up with no screens in the layout. Add a fake one...
    if (layout.num_screens() == 0)
      layout.add_screen(rfb::Screen(0, 0, 0, w, h, 0));
  }

  // Do we actually change anything?
  if ((w == cc->server.width()) &&
      (h == cc->server.height()) &&
      (layout == cc->server.screenLayout()))
    return;

  vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d",
             cc->server.width(), cc->server.height(), w, h);

  char buffer[2048];
  layout.print(buffer, sizeof(buffer));
  if (!layout.validate(w, h)) {
    vlog.error(_("Invalid screen layout computed for resize request!"));
    vlog.error("%s", buffer);
    return;
  } else {
    vlog.debug("%s", buffer);
  }

  try {
    cc->writer()->writeSetDesktopSize(w, h, layout);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
}

void DesktopWindow::handleOptions(void *data)
{
  DesktopWindow *self = (DesktopWindow*)data;

  if (fullscreenSystemKeys)
    self->maybeGrabKeyboard();
  else
    self->ungrabKeyboard();

  // Call fullscreen_on even if active since it handles
  // fullScreenMode
  if (fullScreen)
    self->fullscreen(true);
  else if (!fullScreen && self->isFullscreenEnabled())
    self->fullscreen(false);
}