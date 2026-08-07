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

#include "DesktopWindow.h"
#include "OptionsDialog.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"
#include "CConn.h"
#include "Surface.h"
#include "Toast.h"
#include "Viewport.h"

// Conflicts with Qt
#define QPoint _FLTK_QPoint
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/fl_draw.H>
#include <FL/x.H>
#undef QPoint

#if defined(WIN32)
#include "win32.h"
#elif defined(__APPLE__)
#include "cocoa.h"
#else
#include "x11.h"
#endif

static rfb::LogWriter vlog("DesktopWindow");

// Global due to http://www.fltk.org/str.php?L2177 and the similar
// issue for Fl::event_dispatch.
static std::set<DesktopWindow *> instances;

DesktopWindow::DesktopWindow(int w, int h, const char *name,
                             CConn* cc_)
  : Fl_Window(w, h), cc(cc_), offscreen(nullptr),
    firstUpdate(true),
    delayedFullscreen(false), sentDesktopSize(false),
    pendingRemoteResize(false), lastResize({0, 0}),
    keyboardGrabbed(false), mouseGrabbed(false)
{
  Fl_Group* group;

  // Dummy group to prevent FLTK from moving our widgets around
  group = new Fl_Group(0, 0, w, h);
  group->resizable(nullptr);
  resizable(group);

  viewport = new Viewport(w, h, cc);

  // Position will be adjusted later
  hscroll = new Fl_Scrollbar(0, 0, 0, 0);
  vscroll = new Fl_Scrollbar(0, 0, 0, 0);
  hscroll->type(FL_HORIZONTAL);
  Fl_Callback* handleScroll = [](Fl_Widget* /*widget*/, void *data) {
    DesktopWindow *self = (DesktopWindow *)data;
    self->scrollTo(self->hscroll->value(), self->vscroll->value());
  };
  hscroll->callback(handleScroll, this);
  vscroll->callback(handleScroll, this);

  group->end();

  toast = new Toast(this);

  callback([](Fl_Widget*, void*) { disconnect(); });

  setName(name);

  OptionsDialog::addCallback(handleOptions, this);

  // Some events need to be caught globally
  if (instances.size() == 0)
    Fl::add_handler(fltkHandle);
  instances.insert(this);

  // Hack. See below...
  Fl::event_dispatch(fltkDispatch);

  // Support for -geometry option. Note that although we do support
  // negative coordinates, we do not support -XOFF-YOFF (ie
  // coordinates relative to the right edge / bottom edge) at this
  // time.
  int geom_x = 0, geom_y = 0;
  if (strcmp(geometry, "") != 0) {
    int matched;
    matched = sscanf((const char*)geometry, "+%d+%d", &geom_x, &geom_y);
    if (matched == 2) {
      force_position(1);
    } else {
      int geom_w, geom_h;
      matched = sscanf((const char*)geometry, "%dx%d+%d+%d", &geom_w, &geom_h, &geom_x, &geom_y);
      switch (matched) {
      case 4:
        force_position(1);
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
  int sx, sy, sw, sh;
  if (force_position()) {
    Fl::screen_work_area(sx, sy, sw, sh, geom_x, geom_y);
  } else {
    int mx, my;

    // If we don't explicitly request a position then we don't know which
    // monitor the window manager might place us on. Assume the popular
    // behaviour of following the cursor.

    Fl::get_mouse(mx, my);
    Fl::screen_work_area(sx, sy, sw, sh, mx, my);
  }
  if ((w > sw) || (h > sh)) {
    vlog.info(_("Reducing window size to fit on current monitor"));
    if (w > sw)
      w = sw;
    if (h > sh)
      h = sh;
  }

#ifdef __APPLE__
  // On OS X we can do the maximize thing properly before the
  // window is showned. Other platforms handled further down...
  if (maximize) {
    int dummy;
    Fl::screen_work_area(dummy, dummy, w, h, geom_x, geom_y);
  }
#endif

  if (force_position()) {
    resize(geom_x, geom_y, w, h);
  } else {
    size(w, h);
  }

  if (fullScreen) {
    // Hack: Window managers seem to be rather crappy at respecting
    // fullscreen hints on initial windows. So on X11 we'll have to
    // wait until after we've been mapped.
#if defined(WIN32) || defined(__APPLE__)
    fullscreen_on();
#else
    delayedFullscreen = true;
#endif
  }

  show();

  // Full screen events are not sent out for a hidden window,
  // so send a fake one here to set up things properly.
  if (fullscreen_active())
    handle(FL_FULLSCREEN);

  // Unfortunately, current FLTK does not allow us to set the
  // maximized property on Windows and X11 before showing the window.
  // See STR #2083 and STR #2178
#ifndef __APPLE__
  if (maximize) {
    maximizeWindow();
  }
#endif

  // Adjust layout now that we're visible and know our final size
  repositionWidgets();

  // Show hint about menu key
  Fl::add_timeout(0.5, menuToast, this);

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

  // Unregister all timeouts in case they get a change tro trigger
  // again later when this object is already gone.
  Fl::remove_timeout(handleResizeTimeout, this);
  Fl::remove_timeout(handleFullscreenTimeout, this);
  Fl::remove_timeout(menuToast, this);

  OptionsDialog::removeCallback(handleOptions);

  delete toast;
  delete offscreen;

  instances.erase(this);

  if (instances.size() == 0)
    Fl::remove_handler(fltkHandle);

  Fl::event_dispatch(Fl::handle_);

  // FLTK automatically deletes all child widgets, so we shouldn't touch
  // them ourselves here
}


const rfb::PixelFormat &DesktopWindow::getPreferredPF()
{
  return viewport->getPreferredPF();
}


void DesktopWindow::setName(const char *name)
{
  char windowNameStr[256];

  snprintf(windowNameStr, 256, "%.240s - TigerVNC", name);

  copy_label(windowNameStr);
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
  bool maximized;

  if ((new_w == viewport->w()) && (new_h == viewport->h()))
    return;

  maximized = false;

#ifdef WIN32
  WINDOWPLACEMENT wndpl;
  memset(&wndpl, 0, sizeof(WINDOWPLACEMENT));
  wndpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(fl_xid(this), &wndpl);
  if (wndpl.showCmd == SW_SHOWMAXIMIZED)
    maximized = true;
#elif defined(__APPLE__)
  if (cocoa_win_is_zoomed(this))
    maximized = true;
#else
  if (x11_win_is_maximized(this))
    maximized = true;
#endif

  // If we're letting the viewport match the window perfectly, then
  // keep things that way for the new size, otherwise just keep things
  // like they are.
  if (!fullscreen_active() && !maximized && !pendingRemoteResize &&
      !Fl::has_timeout(handleResizeTimeout, this)) {
    if ((w() == viewport->w()) && (h() == viewport->h()))
      size(new_w, new_h);
  }

  viewport->size(new_w, new_h);

  repositionWidgets();
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
                              const uint8_t* data)
{
  viewport->setCursor(width, height, hotspot, data);
}


void DesktopWindow::setCursorPos(const rfb::Point& pos)
{
  if (!mouseGrabbed) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
#if defined(WIN32)
  SetCursorPos(pos.x + x_root() + viewport->x(),
               pos.y + y_root() + viewport->y());
#elif defined(__APPLE__)
  cocoa_set_cursor_pos(pos.x + x_root() + viewport->x(),
                       pos.y + y_root() + viewport->y());
#else // Assume this is Xlib
  x11_warp_pointer(pos.x + x_root() + viewport->x(),
                   pos.y + y_root() + viewport->y());
#endif
}


void DesktopWindow::draw()
{
  bool redraw;

  int X, Y, W, H;

  // X11 needs an off screen buffer for compositing to avoid flicker,
  // and alpha blending doesn't work for windows on Win32
#if !defined(__APPLE__)

  // Adjust offscreen surface dimensions
  if ((offscreen == nullptr) ||
      (offscreen->width() != w()) || (offscreen->height() != h())) {
    delete offscreen;
    offscreen = new Surface(w(), h());
  }

#endif

  // Active area inside scrollbars
  W = w() - (vscroll->visible() ? vscroll->w() : 0);
  H = h() - (hscroll->visible() ? hscroll->h() : 0);

  // Full redraw?
  redraw = (damage() & ~FL_DAMAGE_CHILD);

  // Simplify the clip region to a simple rectangle in order to
  // properly draw all the layers even if they only partially overlap
  if (redraw)
    X = Y = 0;
  else
    fl_clip_box(0, 0, W, H, X, Y, W, H);
  fl_push_no_clip();
  fl_push_clip(X, Y, W, H);

  // Redraw background only on full redraws
  if (redraw) {
    if (offscreen)
      offscreen->clear(40, 40, 40);
    else
      fl_rectf(0, 0, W, H, 40, 40, 40);
  }

  if (offscreen) {
    viewport->draw(offscreen);
    viewport->clear_damage();
  } else {
    if (redraw)
      draw_child(*viewport);
    else
      update_child(*viewport);
  }

  // Toast (if active)
  if (toast->shown()) {
    int ox, oy, ow, oh;
    int sx, sy, sw, sh;

    // Make sure it's properly seen by adjusting it relative to the
    // primary screen rather than the entire window
    if (fullscreen_active()) {
      assert(Fl::screen_count() >= 1);

      rfb::Rect windowRect, screenRect;
      windowRect.setXYWH(x(), y(), w(), h());

      bool foundEnclosedScreen = false;
      for (int idx = 0; idx < Fl::screen_count(); idx++) {
        Fl::screen_xywh(sx, sy, sw, sh, idx);

        // The screen with the smallest index that are enclosed by
        // the viewport will be used for showing the toast.
        screenRect.setXYWH(sx, sy, sw, sh);
        if (screenRect.enclosed_by(windowRect)) {
          foundEnclosedScreen = true;
          break;
        }
      }

      // If no monitor inside the viewport was found,
      // use the one primary instead.
      if (!foundEnclosedScreen)
        Fl::screen_xywh(sx, sy, sw, sh, 0);

      // Adjust the coordinates so they are relative to the viewport.
      sx -= x();
      sy -= y();

    } else {
      sx = 0;
      sy = 0;
      sw = w();
    }

    ox = X = sx + (sw - toast->width()) / 2;
    oy = Y = sy + 50;
    ow = toast->width();
    oh = toast->height();

    fl_clip_box(ox, oy, ow, oh, ox, oy, ow, oh);

    if ((ow != 0) && (oh != 0)) {
      if (offscreen)
        toast->draw(offscreen, ox - X, oy - Y, ox, oy, ow, oh);
      else
        toast->draw(ox - X, oy - Y, ox, oy, ow, oh);
    }
  }

  // Flush offscreen surface to screen
  if (offscreen) {
    fl_clip_box(0, 0, w(), h(), X, Y, W, H);
    offscreen->draw(X, Y, X, Y, W, H);
  }

  fl_pop_clip();
  fl_pop_clip();

  // Finally the scrollbars

  if (redraw) {
    draw_child(*hscroll);
    draw_child(*vscroll);
  } else {
    update_child(*hscroll);
    update_child(*vscroll);
  }
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

void DesktopWindow::handleClipboardData(const char* data)
{
  viewport->handleClipboardData(data);
}


void DesktopWindow::resize(int x, int y, int w, int h)
{
  bool moving, resizing;

#if ! (defined(WIN32) || defined(__APPLE__))
  // X11 window managers will treat a resize to cover the entire
  // monitor as a request to go full screen. Make sure we avoid this.
  if (!fullscreen_active()) {
    bool resize_req;

    // If there is no X11 window, then this must be a resize request,
    // not a notification from the X server.
    if (!shown())
      resize_req = true;
    else {
      // Otherwise we need to get the real window coordinates to tell
      // the difference
      int wx, wy, ww, wh;

      x11_win_get_coords(this, &wx, &wy, &ww, &wh);

      // Actual resize request?
      if ((wx != x) || (wy != y) || (ww != w) || (wh != h))
        resize_req = true;
      else
        resize_req = false;
    }

    if (resize_req) {
      for (int idx = 0;idx < Fl::screen_count();idx++) {
        int sx, sy, sw, sh;

        Fl::screen_xywh(sx, sy, sw, sh, idx);

        // We can't trust x and y if the window isn't mapped as the
        // window manager might adjust those numbers
        if (shown() && ((sx != x) || (sy != y)))
            continue;

        if ((sw != w) || (sh != h))
            continue;

        vlog.info(_("Adjusting window size to avoid accidental full-screen request"));
        // Assume a panel of some form and adjust the height
        h -= 40;
      }
    }
  }
#endif

  moving = resizing = false;
  if ((this->x() != x) || (this->y() != y))
    moving = true;
  if ((this->w() != w) || (this->h() != h))
    resizing = true;

  Fl_Window::resize(x, y, w, h);

  if (moving)
    moveEvent();
  if (resizing)
    resizeEvent();
}


void DesktopWindow::menuToast(void* data)
{
  DesktopWindow *self;

  self = (DesktopWindow*)data;

  if (strcmp((const char*)menuKey, "") != 0) {
    self->setToast(_("Press %s to open the context menu"),
                   (const char*)menuKey);
  }
}

void DesktopWindow::setToast(const char* text, ...)
{
  va_list ap;
  char textbuf[1024];

  va_start(ap, text);
  vsnprintf(textbuf, sizeof(textbuf), text, ap);
  textbuf[sizeof(textbuf)-1] = '\0';
  va_end(ap);

  toast->setText(textbuf);
}


void DesktopWindow::moveEvent()
{
  // We might be overlapping a different set of monitors now, even if
  // our size is the same
  remoteResize();

  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();
}

void DesktopWindow::resizeEvent()
{
  remoteResize();

  repositionWidgets();

  // Some systems require a grab after the window size has been changed.
  // Otherwise they might hold on to displays, resulting in them being unusable.
  maybeGrabKeyboard();
}

void DesktopWindow::fullScreenEvent()
{
  fullScreen.setParam(fullscreen_active());

  // Update scroll bars
  repositionWidgets();

  if (fullscreen_active())
    maybeGrabKeyboard();
  else
    ungrabKeyboard();

  // The window manager respected our full screen request, but we
  // still need to wait a bit long for it to finish resizing us
  if (delayedFullscreen && fullscreen_active()) {
    Fl::remove_timeout(handleFullscreenTimeout, this);
    Fl::add_timeout(0.1, handleFullscreenTimeout, this);
  }
}

void DesktopWindow::enterEvent()
{
  if (keyboardGrabbed)
    grabPointer();
}

void DesktopWindow::leaveEvent()
{
  // This probably never happens as we don't get called with a grab
  // active. But let's be cautious in case we overlooked something.
  if (mouseGrabbed)
    ungrabPointer();
}

void DesktopWindow::mouseMoveEvent()
{
  if (mouseGrabbed) {
    // We don't get FL_LEAVE with a grabbed pointer, so check manually
    if ((Fl::event_x() < 0) || (Fl::event_x() >= w()) ||
        (Fl::event_y() < 0) || (Fl::event_y() >= h())) {
      ungrabPointer();
    }
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

void DesktopWindow::showEvent()
{
#if !defined(WIN32) && !defined(__APPLE__)
  // Request ability to grab keyboard under Xwayland
  x11_win_may_grab(this);
#endif

  // This is a response to MapNotify, which means we can continue
  // enabling initial fullscreen.
  if (delayedFullscreen) {
    // Hack: Fullscreen requests may be ignored, so we need a
    // timeout for when we should stop waiting. We also need to wait
    // for the resize, which can come after the fullscreen event.
    Fl::add_timeout(0.5, handleFullscreenTimeout, this);
    fullscreen_on();
  }
}

int DesktopWindow::handle(int event)
{
  switch (event) {
  case FL_FULLSCREEN:
    fullScreenEvent();
    break;

  case FL_ENTER:
    enterEvent();
    break;
  case FL_LEAVE:
    leaveEvent();
    break;
  case FL_DRAG:
  case FL_MOVE:
    mouseMoveEvent();
    // Continue processing so that the viewport also gets mouse events
    break;
  }

  return Fl_Window::handle(event);
}


int DesktopWindow::fltkDispatch(int event, Fl_Window *win)
{
  int ret;

  // FLTK keeps spamming bogus FL_MOVE events if _any_ X event is
  // received with the mouse pointer outside our windows
  // https://github.com/fltk/fltk/issues/76
  if ((event == FL_MOVE) && (win == nullptr))
    return 0;

#if !defined(WIN32) && !defined(__APPLE__)
  // FLTK passes through the fake grab focus events that can cause us
  // to end up in an infinite loop
  // https://github.com/fltk/fltk/issues/295
  if ((event == FL_FOCUS) || (event == FL_UNFOCUS)) {
    const XFocusChangeEvent* xfocus = &fl_xevent->xfocus;
    if ((xfocus->mode == NotifyGrab) || (xfocus->mode == NotifyUngrab))
      return 0;
  }
#endif

  ret = Fl::handle_(event, win);

  // This is hackish and the result of the dodgy focus handling in FLTK.
  // The basic problem is that FLTK's view of focus and the system's tend
  // to differ, and as a result we do not see all the FL_FOCUS events we
  // need. Fortunately we can grab them here...

  DesktopWindow *dw = dynamic_cast<DesktopWindow*>(win);

  if (dw) {
    switch (event) {
    case FL_FOCUS:
      dw->handleFocusedChanged(true);
      break;
    case FL_UNFOCUS:
      dw->handleFocusedChanged(false);
      break;

    case FL_SHOW:
      // In this particular place, FL_SHOW means an actual MapNotify
      dw->showEvent();
      break;

    case FL_RELEASE:
      // We do this here rather than handle() because a window does not
      // see FL_RELEASE events if a child widget grabs it first
      dw->mouseReleaseEvent();
      break;
    }
  }

  return ret;
}

int DesktopWindow::fltkHandle(int event)
{
  switch (event) {
  case FL_SCREEN_CONFIGURATION_CHANGED:
    // Screens removed or added. Recreate fullscreen window if
    // necessary. On Windows, adding a second screen only works
    // reliable if we are using a timer. Otherwise, the window will
    // not be resized to cover the new screen. A timer makes sense
    // also on other systems, to make sure that whatever desktop
    // environment has a chance to deal with things before we do.
    // Please note that when using FullscreenSystemKeys on macOS, the
    // display configuration cannot be changed: macOS will not detect
    // added or removed screens and there will be no
    // FL_SCREEN_CONFIGURATION_CHANGED event. This is by design:
    // "When you capture a display, you have exclusive use of the
    // display. Other applications and system services are not allowed
    // to use the display or change its configuration. In addition,
    // they are not notified of display changes"
    Fl::remove_timeout(reconfigureFullscreen);
    Fl::add_timeout(0.5, reconfigureFullscreen);
  }

  return 0;
}

void DesktopWindow::fullscreen_on()
{
  bool allMonitors = !strcasecmp(fullScreenMode, "all");
  bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");
  int top, bottom, left, right;

#ifdef __APPLE__
  // Avoid surprises if we cannot do proper multiheaded full screen
  if (cocoa_screens_have_separate_spaces()) {
    allMonitors = false;
    selectedMonitors = false;
  }
#endif

  if (not selectedMonitors and not allMonitors) {
    top = bottom = left = right = Fl::screen_num(x(), y(), w(), h());
  } else {
    int top_y, bottom_y, left_x, right_x;

    int sx, sy, sw, sh;

    std::set<int> monitors;

    if (selectedMonitors and not allMonitors) {
      std::set<int> selected = fullScreenSelectedMonitors.getParam();
      monitors.insert(selected.begin(), selected.end());
    } else {
      for (int idx = 0; idx < Fl::screen_count(); idx++)
        monitors.insert(idx);
    }

    // If no monitors were found in the selected monitors case, we want
    // to explicitly use the window's current monitor.
    if (monitors.size() == 0) {
      monitors.insert(Fl::screen_num(x(), y(), w(), h()));
    }

    // If there are monitors selected, calculate the dimensions
    // of the frame buffer, expressed in the monitor indices that
    // limits it.
    std::set<int>::iterator it = monitors.begin();

    // Get first monitor dimensions.
    Fl::screen_xywh(sx, sy, sw, sh, *it);
    top = bottom = left = right = *it;
    top_y = sy;
    bottom_y = sy + sh;
    left_x = sx;
    right_x = sx + sw;

    // Keep going through the rest of the monitors.
    for (; it != monitors.end(); it++) {
      Fl::screen_xywh(sx, sy, sw, sh, *it);

      if (sy < top_y) {
        top = *it;
        top_y = sy;
      }

      if ((sy + sh) > bottom_y) {
        bottom = *it;
        bottom_y = sy + sh;
      }

      if (sx < left_x) {
        left = *it;
        left_x = sx;
      }

      if ((sx + sw) > right_x) {
        right = *it;
        right_x = sx + sw;
      }
    }

  }
#ifdef __APPLE__
  // This is a workaround for a bug in FLTK, see: https://github.com/fltk/fltk/pull/277
  int savedLevel;
  savedLevel = cocoa_get_level(this);
#endif
  fullscreen_screens(top, bottom, left, right);
#ifdef __APPLE__
  // This is a workaround for a bug in FLTK, see: https://github.com/fltk/fltk/pull/277
  if (cocoa_get_level(this) != savedLevel)
    cocoa_set_level(this, savedLevel);
#endif

  if (!fullscreen_active())
    fullscreen();
}

bool DesktopWindow::hasFocus()
{
  Fl_Widget* focus;

  focus = Fl::grab();
  if (!focus)
    focus = Fl::focus();

  if (!focus)
    return false;

  return focus->window() == this;
}

void DesktopWindow::maybeGrabKeyboard()
{
  if (fullscreenSystemKeys && fullscreen_active() && hasFocus())
    grabKeyboard();
}

void DesktopWindow::grabKeyboard()
{
  // Grabbing the keyboard is fairly safe as FLTK reroutes events to the
  // correct widget regardless of which low level window got the system
  // event.

  // FIXME: Push this stuff into FLTK.

#if defined(WIN32)
  int ret;
  
  ret = win32_enable_lowlevel_keyboard(fl_xid(this));
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

  if (contains(Fl::belowmouse()))
    grabPointer();
}


void DesktopWindow::ungrabKeyboard()
{
  keyboardGrabbed = false;

  ungrabPointer();

#if defined(WIN32)
  win32_disable_lowlevel_keyboard(fl_xid(this));
#elif defined(__APPLE__)
  cocoa_release_displays(this);
#else
  // FLTK has a grab so lets not mess with it
  if (Fl::grab())
    return;

  x11_ungrab_keyboard();
#endif
}


void DesktopWindow::grabPointer()
{
#if !defined(WIN32) && !defined(__APPLE__)
  // We also need to grab the pointer as some WMs like to grab buttons
  // combined with modifies (e.g. Alt+Button0 in metacity).

  // Having a button pressed prevents us from grabbing, we make
  // a new attempt in mouseReleaseEvent()
  if (!x11_grab_pointer(this))
    return;
#endif

  mouseGrabbed = true;
}


void DesktopWindow::ungrabPointer()
{
  mouseGrabbed = false;

#if !defined(WIN32) && !defined(__APPLE__)
  x11_ungrab_pointer(this);
#endif
}

void DesktopWindow::handleFocusedChanged(bool focused)
{
  // Focus might not stay with us just because we have grabbed the
  // keyboard. E.g. we might have sub windows, or we're not using
  // all monitors and the user clicked on another application.
  // Make sure we update our grabs with the focus changes.
  if (focused) {
    maybeGrabKeyboard();
  } else {
    if (fullscreenSystemKeys)
      ungrabKeyboard();
  }
}

void DesktopWindow::maximizeWindow()
{
#if defined(WIN32)
  // We cannot use ShowWindow() in full screen mode as it will
  // resize things implicitly. Fortunately modifying the style
  // directly results in a maximized state once we leave full screen.
  if (fullscreen_active()) {
    WINDOWINFO wi;
    wi.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo(fl_xid(this), &wi);
    SetWindowLongPtr(fl_xid(this), GWL_STYLE, wi.dwStyle | WS_MAXIMIZE);
  } else
    ShowWindow(fl_xid(this), SW_MAXIMIZE);
#elif defined(__APPLE__)
  if (fullscreen_active())
    return;
  cocoa_win_zoom(this);
#else
  fl_open_display();
  x11_win_maximize(this);
#endif
}


void DesktopWindow::handleResizeTimeout(void *data)
{
  DesktopWindow *self = (DesktopWindow *)data;

  assert(self);

  self->remoteResize();
}


void DesktopWindow::reconfigureFullscreen(void* /*data*/)
{
  std::set<DesktopWindow *>::iterator iter;

  for (iter = instances.begin(); iter != instances.end(); ++iter) {
    if ((*iter)->fullscreen_active())
      (*iter)->fullscreen_on();
  }
}


void DesktopWindow::remoteResize()
{
  int width, height;
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
    Fl::remove_timeout(handleResizeTimeout, this);
    Fl::add_timeout((100.0 - rfb::msSince(&lastResize)) / 1000.0,
                    handleResizeTimeout, this);
    return;
  }

  width = w();
  height = h();

  if (!sentDesktopSize && (strcmp(desktopSize, "") != 0)) {
    // An explicit size has been requested

    if (sscanf(desktopSize, "%dx%d", &width, &height) != 2)
      return;

    sentDesktopSize = true;
  }

  if (!fullscreen_active() || (width > w()) || (height > h())) {
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
    layout.begin()->dimensions.br.x = width;
    layout.begin()->dimensions.br.y = height;
  } else {
    uint32_t id;
    int sx, sy, sw, sh;
    rfb::Rect viewport_rect, screen_rect;

    // In full screen we report all screens that are fully covered.

    viewport_rect.setXYWH(x() + (w() - width)/2, y() + (h() - height)/2,
                          width, height);

    // If we can find a matching screen in the existing set, we use
    // that, otherwise we create a brand new screen.
    //
    // FIXME: We should really track screens better so we can handle
    //        a resized one.
    //
    for (int idx = 0;idx < Fl::screen_count();idx++) {
      Fl::screen_xywh(sx, sy, sw, sh, idx);

      // Check that the screen is fully inside the framebuffer
      screen_rect.setXYWH(sx, sy, sw, sh);
      if (!screen_rect.enclosed_by(viewport_rect))
        continue;

      // Adjust the coordinates so they are relative to our viewport
      sx -= viewport_rect.tl.x;
      sy -= viewport_rect.tl.y;

      // Look for perfectly matching existing screen that is not yet present in
      // in the screen layout...
      for (iter = cc->server.screenLayout().begin();
           iter != cc->server.screenLayout().end(); ++iter) {
        if ((iter->dimensions.tl.x == sx) &&
            (iter->dimensions.tl.y == sy) &&
            (iter->dimensions.width() == sw) &&
            (iter->dimensions.height() == sh) &&
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

      layout.add_screen(rfb::Screen(id, sx, sy, sw, sh, 0));
    }

    // If the viewport doesn't match a physical screen, then we might
    // end up with no screens in the layout. Add a fake one...
    if (layout.num_screens() == 0)
      layout.add_screen(rfb::Screen(0, 0, 0, width, height, 0));
  }

  // Do we actually change anything?
  if ((width == cc->server.width()) &&
      (height == cc->server.height()) &&
      (layout == cc->server.screenLayout()))
    return;

  vlog.debug("Requesting framebuffer resize from %dx%d to %dx%d",
             cc->server.width(), cc->server.height(), width, height);

  char buffer[2048];
  layout.print(buffer, sizeof(buffer));
  if (!layout.validate(width, height)) {
    vlog.error(_("Invalid screen layout computed for resize request!"));
    vlog.error("%s", buffer);
    return;
  } else {
    vlog.debug("%s", buffer);
  }

  pendingRemoteResize = true;
  gettimeofday(&lastResize, nullptr);

  try {
    cc->writer()->writeSetDesktopSize(width, height, layout);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
}


void DesktopWindow::repositionWidgets()
{
  int new_x, new_y;

  // Viewport position

  new_x = viewport->x();
  new_y = viewport->y();

  if (w() > viewport->w())
    new_x = (w() - viewport->w()) / 2;
  else {
    if (viewport->x() > 0)
      new_x = 0;
    else if (w() > (viewport->x() + viewport->w()))
      new_x = w() - viewport->w();
  }

  // Same thing for y axis
  if (h() > viewport->h())
    new_y = (h() - viewport->h()) / 2;
  else {
    if (viewport->y() > 0)
      new_y = 0;
    else if (h() > (viewport->y() + viewport->h()))
      new_y = h() - viewport->h();
  }

  if ((new_x != viewport->x()) || (new_y != viewport->y())) {
    viewport->position(new_x, new_y);
    damage(FL_DAMAGE_SCROLL);
  }

  // Scrollbars visbility

  // Decide whether to show a scrollbar by checking if the window
  // size (possibly minus scrollbar_size) is less than the viewport
  // (remote framebuffer) size.
  //
  // We decide whether to subtract scrollbar_size on an axis by
  // checking if the other axis *definitely* needs a scrollbar.  You
  // might be tempted to think that this becomes a weird recursive
  // problem, but it isn't: If the window size is less than the
  // viewport size (without subtracting the scrollbar_size), then
  // that axis *definitely* needs a scrollbar; if the check changes
  // when we subtract scrollbar_size, then that axis only *maybe*
  // needs a scrollbar.  If both axes only "maybe" need a scrollbar,
  // then neither does; so we don't need to recurse on the "maybe"
  // cases.

  if (w() - (h() < viewport->h() ? Fl::scrollbar_size() : 0) < viewport->w())
    hscroll->show();
  else
    hscroll->hide();

  if (h() - (w() < viewport->w() ? Fl::scrollbar_size() : 0) < viewport->h())
    vscroll->show();
  else
    vscroll->hide();

  // Scrollbars positions

  hscroll->resize(0, h() - Fl::scrollbar_size(),
                  w() - (vscroll->visible() ? Fl::scrollbar_size() : 0),
                  Fl::scrollbar_size());
  vscroll->resize(w() - Fl::scrollbar_size(), 0,
                  Fl::scrollbar_size(),
                  h() - (hscroll->visible() ? Fl::scrollbar_size() : 0));

  // Scrollbars range

  hscroll->value(-viewport->x(),
                 w() - (vscroll->visible() ? vscroll->w() : 0),
                 0, viewport->w());
  vscroll->value(-viewport->y(),
                 h() - (hscroll->visible() ? hscroll->h() : 0),
                 0, viewport->h());
  hscroll->value(hscroll->clamp(hscroll->value()));
  vscroll->value(vscroll->clamp(vscroll->value()));
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
    self->fullscreen_on();
  else if (!fullScreen && self->fullscreen_active())
    self->fullscreen_off();
}

void DesktopWindow::handleFullscreenTimeout(void *data)
{
  DesktopWindow *self = (DesktopWindow *)data;

  assert(self);

  // We are here because we got tired of waiting for the window manager
  // to finish switching to fullscreen mode, or because we are waiting
  // for all resize events so we get our final position

  self->delayedFullscreen = false;
  self->remoteResize();
}

void DesktopWindow::scrollTo(int x, int y)
{
  x = hscroll->clamp(x);
  y = vscroll->clamp(y);

  hscroll->value(x);
  vscroll->value(y);

  // Scrollbar position results in inverse movement of
  // the viewport widget
  x = -x;
  y = -y;

  if ((viewport->x() == x) && (viewport->y() == y))
    return;

  viewport->position(x, y);
  damage(FL_DAMAGE_SCROLL);
}
