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
#include <rfb/util.h>

#include <QApplication>
#include <QMoveEvent>
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
#include "QStyles.h"
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

DesktopWindow::DesktopWindow(int w, int h, const char *name,
                             CConn* cc_, QWidget* parent)
  : QWidget(parent), cc(cc_),
    firstUpdate(true),
    delayedFullscreen(false), sentDesktopSize(false),
    pendingRemoteResize(false), lastResize({0, 0}),
    fakeFullScreen(false),
    keyboardGrabbed(false), mouseGrabbed(false)
{
  // We need an early and stable QWindow as we do a lot of low level
  // stuff with it
  setAttribute(Qt::WA_NativeWindow);

  setContentsMargins(0, 0, 0, 0);
  resize(w, h);

  viewport = new Viewport(w, h, cc);

  scrollArea = new QScrollArea;
  scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  scrollArea->setFrameStyle(QFrame::NoFrame);
  scrollArea->setWidget(viewport);

  QPalette bg(scrollArea->viewport()->palette());
  bg.setColor(QPalette::Window, QColor::fromRgb(40, 40, 40));
  scrollArea->viewport()->setPalette(bg);

  // We don't have any alternative way of scrolling, so we cannot let
  // scrollbars be transient (hidden) when idle
  QStyle* style = scrollArea->horizontalScrollBar()->style();
  style = new QNonTransientStyle(style);
  scrollArea->horizontalScrollBar()->setStyle(style);
  scrollArea->verticalScrollBar()->setStyle(style);

  toast = new Toast(this);

  setName(name);

  OptionsDialog::addCallback(handleOptions, this);

  // Hack. See below...
  windowHandle()->installEventFilter(this);

  resizeTimer = new QTimer(this);
  resizeTimer->setSingleShot(true);
  connect(resizeTimer, &QTimer::timeout, this,
          &DesktopWindow::handleResizeTimeout);

  QVBoxLayout* l = new QVBoxLayout;
  l->setSpacing(0);
  l->setContentsMargins(0,0,0,0);
  l->addWidget(scrollArea);
  setLayout(l);

  // FIXME: Not sure why this is needed
  viewport->setFocus();

  // Screens removed or added. Recreate fullscreen window if
  // necessary. On Windows, adding a second screen only works
  // reliable if we are using a timer. Otherwise, the window will
  // not be resized to cover the new screen. A timer makes sense
  // also on other systems, to make sure that whatever desktop
  // environment has a chance to deal with things before we do.
  // Please note that when using FullscreenSystemKeys on macOS, the
  // display configuration cannot be changed: macOS will not detect
  // added or removed screens and there will be no event.
  // This is by design:
  // "When you capture a display, you have exclusive use of the
  // display. Other applications and system services are not allowed
  // to use the display or change its configuration. In addition,
  // they are not notified of display changes"
  QTimer* screensTimer = new QTimer(this);
  screensTimer->setInterval(500);
  screensTimer->setSingleShot(true);
  connect(screensTimer, &QTimer::timeout, this,
          &DesktopWindow::reconfigureFullscreen);
  connect(qApp, &QGuiApplication::screenAdded, screensTimer,
          [screensTimer]() { screensTimer->start(); });
  connect(qApp, &QGuiApplication::screenRemoved, screensTimer,
          [screensTimer]() { screensTimer->start(); });

  // Activate events aren't sent for focus grabs, but signals are
  connect(windowHandle(), &QWindow::activeChanged, this,
          &DesktopWindow::handleActiveChanged);

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

#ifdef __APPLE__
  cocoa_prevent_native_full_screen(this);
#endif

  fullscreenTimer = new QTimer(this);
  fullscreenTimer->setSingleShot(true);
  connect(fullscreenTimer, &QTimer::timeout, this,
          &DesktopWindow::handleFullscreenTimeout);

  if (fullScreen) {
    // We cannot enable full screen before the window is visible/mapped
    // for two reasons:
    //
    // a) Window managers seem to be rather crappy at respecting
    // fullscreen hints on initial windows. So on X11 we'll have to
    // wait until after we've been mapped.
    //
    // b) We bypass Qt and do low level manipulation of the window on
    // Windows and macOS, which only works reliably once the window is
    // visible.
    delayedFullscreen = true;
  }

  if (maximize) {
    setWindowState(windowState() ^ Qt::WindowMaximized);
  }

  show();

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
    firstUpdate = false;
    remoteResize();
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
  if (!isFullScreen() && !isMaximized() && !pendingRemoteResize &&
      !resizeTimer->isActive()) {
    if (size() == viewport->size())
      resize(new_w, new_h);
  }

  viewport->resize(new_w, new_h);
}


void DesktopWindow::setDesktopSizeDone(unsigned result)
{
  pendingRemoteResize = false;

  if (result != 0)
    return;

  // We might have resized again whilst waiting for the previous
  // request, so check if we are in sync
  remoteResize();
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
  if (!isFullScreen()) {
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
  // We might be overlapping a different set of monitors now, even if
  // our size is the same
  remoteResize();

  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();

  QWidget::moveEvent(e);
}

void DesktopWindow::resizeEvent(QResizeEvent* e)
{
  remoteResize();

  toast->setGeometry(rect());

  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();

  QWidget::resizeEvent(e);
}

void DesktopWindow::changeEvent(QEvent* e)
{
  if (e->type() == QEvent::WindowStateChange) {
    QWindowStateChangeEvent* event;

    event = dynamic_cast<QWindowStateChangeEvent*>(e);
    assert(event);

    // We only use Qt's native full screen for X11 with a window manager
#if !defined(WIN32) && !defined(__APPLE__)
    if (x11_has_wm()) {
      if ((event->oldState() & Qt::WindowFullScreen) !=
          (windowState() & Qt::WindowFullScreen)) {
        fullScreenEvent();
      }
    } else
#endif
    {
      if (windowState() & Qt::WindowFullScreen)
        vlog.error("Unexpected system full screen state");
    }
  }

  QWidget::changeEvent(e);
}

void DesktopWindow::fullScreenEvent()
{
  fullScreen.setParam(isFullScreen());

  if (isFullScreen())
    maybeGrabKeyboard();
  else
    ungrabKeyboard();

  // The window manager respected our full screen request, but we
  // still need to wait a bit long for it to finish resizing us
  if (delayedFullscreen && isFullScreen())
    fullscreenTimer->start(100);
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

void DesktopWindow::exposeEvent()
{
  // This is a response to MapNotify, which means we can continue
  // enabling initial fullscreen.
  if (delayedFullscreen && !fullscreenTimer->isActive()) {
    // Hack: Fullscreen requests may be ignored, so we need a
    // timeout for when we should stop waiting. We also need to wait
    // for the resize, which can come after the fullscreen event.
    fullscreenTimer->start(500);
    setFullScreen(true);
  }
}

void DesktopWindow::closeEvent(QCloseEvent* e)
{
  QWidget::closeEvent(e);
  ::disconnect();
}


bool DesktopWindow::eventFilter(QObject* watched, QEvent* event)
{
  switch (event->type()) {
  // This is the only way we can detect a MapNotify event
  case QEvent::Expose:
    exposeEvent();
    break;
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


bool DesktopWindow::isFullScreen() const
{
  // We only use Qt's native full screen for X11 with a window manager
#if !defined(WIN32) && !defined(__APPLE__)
  if (x11_has_wm())
    return QWidget::isFullScreen();
#endif
  return fakeFullScreen;
}

void DesktopWindow::setFullScreen(bool enabled)
{
  bool allMonitors = !strcasecmp(fullScreenMode, "all");
  bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");
  QScreen* top, * bottom, * left, * right;

  // We're messing around with low-level details of the window, which
  // requires it to be visible to not conflict badly with Qt
  assert(isVisible());

#ifdef __APPLE__
  // Avoid surprises if we cannot do proper multiheaded full screen
  if (cocoa_screens_have_separate_spaces()) {
    allMonitors = false;
    selectedMonitors = false;
  }
#endif

  if (not selectedMonitors and not allMonitors) {
    top = bottom = left = right = windowHandle()->screen();
  } else {
    int top_y, bottom_y, left_x, right_x;

    QRect sg;

    std::set<QScreen*> monitors;

    if (selectedMonitors and not allMonitors) {
      std::set<QScreen*> selected = fullScreenSelectedMonitors.getParam();
      monitors.insert(selected.begin(), selected.end());
    } else {
      QList<QScreen*> all = qApp->screens();
      monitors.insert(all.begin(), all.end());
    }

    // If no monitors were found in the selected monitors case, we want
    // to explicitly use the window's current monitor.
    if (monitors.size() == 0) {
      monitors.insert(windowHandle()->screen());
    }

    // If there are monitors selected, calculate the dimensions
    // of the frame buffer, expressed in the monitor indices that
    // limits it.
    std::set<QScreen*>::iterator it = monitors.begin();

    // Get first monitor dimensions.
    sg = (*it)->geometry();
    top = bottom = left = right = *it;
    top_y = sg.y();
    bottom_y = sg.y() + sg.height();
    left_x = sg.x();
    right_x = sg.x() + sg.width();

    // Keep going through the rest of the monitors.
    for (; it != monitors.end(); it++) {
      sg = (*it)->geometry();

      if (sg.y() < top_y) {
        top = *it;
        top_y = sg.y();
      }

      if ((sg.y() + sg.height()) > bottom_y) {
        bottom = *it;
        bottom_y = sg.y() + sg.height();
      }

      if (sg.x() < left_x) {
        left = *it;
        left_x = sg.x();
      }

      if ((sg.x() + sg.width()) > right_x) {
        right = *it;
        right_x = sg.x() + sg.width();
      }
    }

  }

  // Qt doesn't support what we need, so we fake things in most cases

#if !defined(WIN32) && !defined(__APPLE__)
  if (x11_has_wm()) {
    // Best effort, as the window manager might not support it
    x11_fullscreen_screens(this, top, bottom, left, right);

    if (enabled)
      setWindowState(windowState() | Qt::WindowFullScreen);
    else
      setWindowState(windowState() & ~Qt::WindowFullScreen);

    return;
  }
#endif

  bool toggling;

  // Are we switching in/out of full screen? Or merely adjusting it?
  toggling = (isFullScreen() != enabled);

  // We need to set this early so other routines respect the mode we're
  // trying to set up
  fakeFullScreen = enabled;

  if (enabled) {
    int vx, vy, vwidth, vheight;

    vx = left->geometry().x();
    vy = top->geometry().y();
    vwidth = right->geometry().x() + right->geometry().width() - vx;
    vheight = bottom->geometry().y() + bottom->geometry().height() - vy;

    if (toggling) {
      previousGeometry = saveGeometry();

      windowHandle()->setFlags(windowHandle()->flags() |
                               Qt::FramelessWindowHint);

#if defined(__APPLE__)
      cocoa_set_presentation_full_screen();
#endif
    }

    // We use QWindow directly to avoid some weird abstraction effects
    windowHandle()->setFramePosition({vx, vy});
    // These are required to prevent user resizing the window on at
    // least macOS
    windowHandle()->setMinimumSize({vwidth, vheight});
    windowHandle()->setMaximumSize({vwidth, vheight});
    windowHandle()->resize(vwidth, vheight);
    windowHandle()->raise();
  } else {
    if (toggling) {
      // Hidden API constant
      const unsigned QWINDOWSIZE_MAX = ((1<<24)-1);
      windowHandle()->setMinimumSize({0, 0});
      windowHandle()->setMaximumSize({QWINDOWSIZE_MAX,
                                      QWINDOWSIZE_MAX});

#if defined(__APPLE__)
      cocoa_set_presentation_default();
#endif

      windowHandle()->setFlags(windowHandle()->flags() &
                               ~Qt::FramelessWindowHint);

      restoreGeometry(previousGeometry);
    }
  }

  if (toggling)
    fullScreenEvent();
}

void DesktopWindow::maybeGrabKeyboard()
{
  if (fullscreenSystemKeys && isFullScreen() && isActiveWindow())
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

void DesktopWindow::handleActiveChanged()
{
  // Focus might not stay with us just because we have grabbed the
  // keyboard. E.g. we might have sub windows, or we're not using
  // all monitors and the user clicked on another application.
  // Make sure we update our grabs with the focus changes.
  if (qApp->focusWindow() == windowHandle()) {
    maybeGrabKeyboard();
  } else {
    if (fullscreenSystemKeys)
      ungrabKeyboard();
  }
}


void DesktopWindow::handleResizeTimeout()
{
  remoteResize();
}


void DesktopWindow::reconfigureFullscreen()
{
  if (isFullScreen())
    setFullScreen(true);
}


void DesktopWindow::remoteResize()
{
  int w, h;
  rfb::ScreenSet layout;
  rfb::ScreenSet::const_iterator iter;

  if (!::remoteResize)
    return;
  if (!cc->server.supportsSetDesktopSize)
    return;

  // Don't pester the server with a resize until we have our final size
  // FIXME: Some window managers (e.g. mutter) will do multiple resizes
  //        every time we enter or leave full screen, which we'd also
  //        like to avoid
  if (delayedFullscreen)
    return;

  // Rate limit to one pending resize at a time
  if (pendingRemoteResize)
    return;

  // And no more than once every 100ms
  if (rfb::msSince(&lastResize) < 100) {
    resizeTimer->start((100.0 - rfb::msSince(&lastResize)) / 1000);
    return;
  }

  w = width();
  h = height();

  if (!sentDesktopSize && (strcmp(desktopSize, "") != 0)) {
    // An explicit size has been requested

    if (sscanf(desktopSize, "%dx%d", &w, &h) != 2)
      return;

    sentDesktopSize = true;
  }

  if (!isFullScreen() || (w > width()) || (h > height())) {
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

  pendingRemoteResize = true;
  gettimeofday(&lastResize, nullptr);

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

  // Call setFullScreen() even if active since it handles
  // fullScreenMode
  if (fullScreen)
    self->setFullScreen(true);
  else if (!fullScreen && self->isFullScreen())
    self->setFullScreen(false);
}

void DesktopWindow::handleFullscreenTimeout()
{
  // We are here because we got tired of waiting for the window manager
  // to finish switching to fullscreen mode, or because we are waiting
  // for all resize events so we get our final position

  delayedFullscreen = false;
  remoteResize();
}
