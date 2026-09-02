/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2025 Pierre Ossman for Cendio AB
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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

#include <core/LogWriter.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/time.h>

#include <rfb/CMsgWriter.h>
#include <rfb/Cursor.h>
#include <rfb/KeysymStr.h>
#include <rfb/ledStates.h>

// FLTK can pull in the X11 headers on some systems
#ifndef XK_VoidSymbol
#define XK_LATIN1
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif

#ifndef NoSymbol
#define NoSymbol 0
#endif

#include "fltk/layout.h"
#include "fltk/util.h"
#include "Viewport.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "mainloop.h"
#include "parameters.h"
#include "vncviewer.h"

#include "PlatformPixelBuffer.h"

#include <FL/fl_draw.H>

#include <FL/Fl.H>
#include <FL/x.H>

#if defined(WIN32)
#include "KeyboardWin32.h"
#include "Win32TouchHandler.h"
#elif defined(__APPLE__)
#include "KeyboardMacOS.h"
#include "BaseTouchHandler.h"
#else
#include "KeyboardX11.h"
#include "XInputTouchHandler.h"
#endif

#ifdef __APPLE__
#include "cocoa.h"
#endif

static core::LogWriter vlog("Viewport");

// Used for fake key presses from the menu
static const int FAKE_CTRL_KEY_CODE = 0x10001;
static const int FAKE_ALT_KEY_CODE = 0x10002;
static const int FAKE_DEL_KEY_CODE = 0x10003;

// Used for fake key presses for lock key sync
static const int FAKE_KEY_CODE = 0xffff;

// Used for fake key presses for gestures
static const int FAKE_GESTURE_KEY_CODE = 0x20001;

// Sensitivity threshold for gestures
static const int ZOOMSENS = 30;
static const int SCRLSENS = 50;

static const unsigned DOUBLE_TAP_TIMEOUT   = 1000;
static const unsigned DOUBLE_TAP_THRESHOLD = 50;

Viewport::Viewport(int w, int h, CConn* cc_)
  : Fl_Widget(0, 0, w, h), cc(cc_), frameBuffer(nullptr),
    lastPointerPos(0, 0), lastButtonMask(0),
    keyboard(nullptr), shortcutBypass(false), shortcutActive(false),
    firstLEDState(true), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(nullptr),
    cursorIsBlank(false)
{
#if defined(WIN32)
  keyboard = new KeyboardWin32(this);
#elif defined(__APPLE__)
  keyboard = new KeyboardMacOS(this);
#else
  keyboard = new KeyboardX11(this);
#endif

  // We need a window handle before we can create these
  touch = nullptr;

  gettimeofday(&lastTapTime, nullptr);

  connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
          &Viewport::handleClipboardChange);

  // We need to intercept keyboard events early
  Fl::add_system_handler(handleSystemEvent, this);

  // FIXME: We should only disable this whilst we have keyboard focus,
  //        but we also need to keep it disabled when we lose focus to
  //        any layout selector so it can properly filter out the
  //        layouts we don't support
  Fl::disable_im();

  frameBuffer = new PlatformPixelBuffer(w, h);
  assert(frameBuffer);
  cc->setFramebuffer(frameBuffer);

  contextMenu = new QMenu();

  // Set the default mouse pointer whilst the context menu is open, as
  // it is annoying if the pointer disappears when you move it outside
  // the menu
  connect(contextMenu, &QMenu::aboutToShow, this,
          [this]() {
            if (Fl::belowmouse() == this)
              window()->cursor(FL_CURSOR_DEFAULT);
          });
  connect(contextMenu, &QMenu::aboutToHide, this,
          [this]() {
            if (Fl::belowmouse())
              showCursor();
          });

  unsigned modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  shortcutHandler.setModifiers(modifierMask);

  OptionsDialog::addCallback(handleOptions, this);

  // Make sure we have an initial blank cursor set
  setCursor();
}


Viewport::~Viewport()
{
  // Unregister all timeouts in case they get a change tro trigger
  // again later when this object is already gone.
  Fl::remove_timeout(handlePointerTimeout, this);

  Fl::remove_system_handler(handleSystemEvent);

  OptionsDialog::removeCallback(handleOptions);

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  delete keyboard;
  delete touch;
  Fl::enable_im();

  delete contextMenu;

  // FLTK automatically deletes all child widgets, so we shouldn't touch
  // them ourselves here
}


const rfb::PixelFormat &Viewport::getPreferredPF()
{
  return frameBuffer->getPF();
}


// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.

void Viewport::updateWindow()
{
  core::Rect r;

  r = frameBuffer->getDamage();
  damage(FL_DAMAGE_USER1, r.tl.x + x(), r.tl.y + y(), r.width(), r.height());
}

static const char * dotcursor_xpm[] = {
  "5 5 2 1",
  ".	c #000000",
  " 	c #FFFFFF",
  "     ",
  " ... ",
  " ... ",
  " ... ",
  "     "};

void Viewport::setCursor()
{
  int width, height;
  core::Point hotspot;
  const uint8_t* data;

  int i;

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  width = cc->server.cursor().width();
  height = cc->server.cursor().height();
  hotspot = cc->server.cursor().hotspot();
  data = cc->server.cursor().getBuffer();

  for (i = 0; i < width*height; i++)
    if (data[i*4 + 3] != 0) break;

  cursorIsBlank = i == width*height;

  if (cursorIsBlank && alwaysCursor) {
    // This is the default in case the local cursor should be displayed yet cursorType is invalid.
    // Since the cursor variable isn't used if the cursorType is system, we can do this without checking the current
    // type which helps handle changing the type while the viewer is running.
    vlog.debug("Cursor is empty, using dot");

    Fl_Pixmap pxm(dotcursor_xpm);
    cursor = new Fl_RGB_Image(&pxm);
    cursorHotspot.x = cursorHotspot.y = 2;
  } else {
    if ((width == 0) || (height == 0)) {
      uint8_t *buffer = new uint8_t[4];
      memset(buffer, 0, 4);
      cursor = new Fl_RGB_Image(buffer, 1, 1, 4);
      cursorHotspot.x = cursorHotspot.y = 0;
    } else {
      uint8_t *buffer = new uint8_t[width * height * 4];
      memcpy(buffer, data, width * height * 4);
      cursor = new Fl_RGB_Image(buffer, width, height, 4);
      cursorHotspot = hotspot;
    }
  }

  if (Fl::belowmouse() == this)
    showCursor();
}

void Viewport::showCursor()
{
  if (viewOnly) {
    window()->cursor(FL_CURSOR_DEFAULT);
    return;
  }

  if (cursorIsBlank && alwaysCursor && (cursorType == "system")) {
    window()->cursor(FL_CURSOR_DEFAULT);
  } else {
    window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
  }
}

void Viewport::handleClipboardRequest()
{
  std::string text, filtered;

  if (viewOnly)
    return;

  text = QGuiApplication::clipboard()->text(clipboardMode).toStdString();

  if (!core::isValidUTF8(text.c_str())) {
    vlog.error(_("Invalid UTF-8 sequence in clipboard"));
    // Reset the state as if we don't have any clipboard data at all
    this->pendingClientClipboard = false;
    try {
      this->cc->announceClipboard(false);
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection_unexpected(e);
    }
    return;
  }

  filtered = core::convertLF(text.c_str());

  vlog.debug("Sending clipboard data (%d bytes)", (int)filtered.size());

  try {
    cc->sendClipboardData(filtered.c_str());
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }
}

void Viewport::handleClipboardAnnounce(bool available)
{
  if (viewOnly)
    return;

  if (!acceptClipboard)
    return;

  if (!available) {
    vlog.debug("Clipboard is no longer available on server");
    return;
  }

  if (!hasFocus()) {
    vlog.debug("Got notification of new clipboard on server whilst not focused, ignoring");
    return;
  }

  pendingClientClipboard = false;

  vlog.debug("Got notification of new clipboard on server, requesting data");
  cc->requestClipboard();
}

void Viewport::handleClipboardData(const char* cbdata)
{
  size_t len;

  if (!hasFocus())
    return;

  len = strlen(cbdata);

  vlog.debug("Got clipboard data (%d bytes)", (int)len);

  // RFB doesn't have separate selection and clipboard concepts, so we
  // dump the data into both variants.
#if !defined(WIN32) && !defined(__APPLE__)
  if (setPrimary)
    QGuiApplication::clipboard()->setText(cbdata,
                                          QClipboard::Mode::Selection);
#endif
  QGuiApplication::clipboard()->setText(cbdata,
                                        QClipboard::Mode::Clipboard);
}

void Viewport::setLEDState(unsigned int ledState)
{
  vlog.debug("Got server LED state: 0x%08x", ledState);

  // The first message is just considered to be the server announcing
  // support for this extension. We will push our state to sync up the
  // server when we get focus. If we already have focus we need to push
  // it here though.
  if (firstLEDState) {
    firstLEDState = false;
    if (hasFocus())
      pushLEDState();
    return;
  }

  if (viewOnly)
    return;

  if (!hasFocus())
    return;

  keyboard->setLEDState(ledState);
}

void Viewport::pushLEDState()
{
  unsigned int ledState;

  if (viewOnly)
    return;

  // Server support?
  if (cc->server.ledState() == rfb::ledUnknown)
    return;

  ledState = keyboard->getLEDState();
  if (ledState == rfb::ledUnknown)
    return;

#if defined(__APPLE__)
  // No support for Scroll Lock //
  ledState |= (cc->server.ledState() & rfb::ledScrollLock);
#endif

  if ((ledState & rfb::ledCapsLock) !=
      (cc->server.ledState() & rfb::ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x3a, XK_Caps_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledNumLock) !=
      (cc->server.ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x45, XK_Num_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledScrollLock) !=
      (cc->server.ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x46, XK_Scroll_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
}


void Viewport::draw(Surface* dst)
{
  int X, Y, W, H;

  // Check what actually needs updating
  fl_clip_box(x(), y(), w(), h(), X, Y, W, H);
  if ((W == 0) || (H == 0))
    return;

  frameBuffer->draw(dst, X - x(), Y - y(), X, Y, W, H);
}


void Viewport::draw()
{
  int X, Y, W, H;

  // Check what actually needs updating
  fl_clip_box(x(), y(), w(), h(), X, Y, W, H);
  if ((W == 0) || (H == 0))
    return;

  frameBuffer->draw(X - x(), Y - y(), X, Y, W, H);
}


void Viewport::resize(int x, int y, int w, int h)
{
  if ((w != frameBuffer->width()) || (h != frameBuffer->height())) {
    vlog.debug("Resizing framebuffer from %dx%d to %dx%d",
               frameBuffer->width(), frameBuffer->height(), w, h);

    frameBuffer = new PlatformPixelBuffer(w, h);
    assert(frameBuffer);
    cc->setFramebuffer(frameBuffer);
  }

  Fl_Widget::resize(x, y, w, h);
}


int Viewport::enterEvent()
{
  showCursor();
  // Yes, we would like some pointer events please!
  return 1;
}

int Viewport::leaveEvent()
{
  window()->cursor(FL_CURSOR_DEFAULT);
  // We want a last move event to help trigger edge stuff
  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()}, 0);
  return 1;
}

int Viewport::mouseEvent()
{
  int buttonMask;

  buttonMask = 0;
  if (Fl::event_button1())
    buttonMask |= 1 << 0;
  if (Fl::event_button2())
    buttonMask |= 1 << 1;
  if (Fl::event_button3())
    buttonMask |= 1 << 2;

  // The back/forward buttons are not supported by FTLK 1.3 and require
  // a patch which adds these buttons to the FLTK API. These buttons
  // will be part of the upcoming 1.4 API:
  //   * https://github.com/fltk/fltk/pull/1081
  //
  // A backport for branch-1.3 is available here:
  //   * https://github.com/fltk/fltk/pull/1083
#if defined(FL_BUTTON4) && defined(FL_BUTTON5)
  if (Fl::event_button4())
    buttonMask |= 1 << 7;
  if (Fl::event_button5())
    buttonMask |= 1 << 8;
#endif

  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()}, buttonMask);
  return 1;
}

int Viewport::wheelEvent()
{
  int buttonMask, wheelMask;

  buttonMask = 0;
  if (Fl::event_button1())
    buttonMask |= 1 << 0;
  if (Fl::event_button2())
    buttonMask |= 1 << 1;
  if (Fl::event_button3())
    buttonMask |= 1 << 2;
#if defined(FL_BUTTON4) && defined(FL_BUTTON5)
  if (Fl::event_button4())
    buttonMask |= 1 << 7;
  if (Fl::event_button5())
    buttonMask |= 1 << 8;
#endif

  wheelMask = 0;
  if (Fl::event_dy() < 0)
    wheelMask |= 1 << 3;
  if (Fl::event_dy() > 0)
    wheelMask |= 1 << 4;
  if (Fl::event_dx() < 0)
    wheelMask |= 1 << 5;
  if (Fl::event_dx() > 0)
    wheelMask |= 1 << 6;

  // A quick press of the wheel "button", followed by a immediate
  // release below
  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()},
                     buttonMask | wheelMask);

  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()}, buttonMask);
  return 1;
}

int Viewport::focusInEvent()
{
  flushPendingClipboard();

  // We may have gotten our lock keys out of sync with the server
  // whilst we didn't have focus. Try to sort this out.
  pushLEDState();

  // Resend Ctrl/Alt if needed
  if (menuCtrlKey)
    sendKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
  if (menuAltKey)
    sendKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);

  // Yes, we would like some focus please!
  return 1;
}

int Viewport::focusOutEvent()
{
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();
  return 1;
}

int Viewport::handle(int event)
{
  switch (event) {
  case FL_ENTER:
    return enterEvent();
  case FL_LEAVE:
    return leaveEvent();
  case FL_PUSH:
  case FL_RELEASE:
  case FL_DRAG:
  case FL_MOVE:
    return mouseEvent();
  case FL_MOUSEWHEEL:
    return wheelEvent();
  case FL_FOCUS:
    return focusInEvent();
  case FL_UNFOCUS:
    return focusOutEvent();
  case FL_KEYDOWN:
  case FL_KEYUP:
    // Just ignore these as keys were handled in the event handler
    return 1;
  }

  return Fl_Widget::handle(event);
}

void Viewport::handleGestureEvent(const GestureEvent& ev)
{
  switch (ev.gesture) {
  case GestureOneTap:
  case GestureTwoTap:
  case GestureThreeTap:
    handleTapGesture(ev);
    break;
  case GestureLongPress:
    handleLongPressGesture(ev);
    break;
  case GestureDrag:
  case GestureTwoDrag:
    handleDragGesture(ev);
    break;
  case GesturePinch:
    handlePinchGesture(ev);
    break;
  }
}

void Viewport::handleTapGesture(const GestureEvent& ev)
{
  GestureEvent newEv = ev;
  core::Point pos;
  int buttonMask;

  if (ev.type != GestureBegin)
    return;

  // If the user quickly taps multiple times we assume they meant to
  // hit the same spot, so slightly adjust coordinates
  if ((core::msSince(&lastTapTime) < DOUBLE_TAP_TIMEOUT) &&
      (firstDoubleTapEvent.type == ev.type)) {

    double dx = firstDoubleTapEvent.eventX - ev.eventX;
    double dy = firstDoubleTapEvent.eventY - ev.eventY;
    double distance = hypot(dx, dy);

    if (distance < DOUBLE_TAP_THRESHOLD) {
     newEv.eventX = firstDoubleTapEvent.eventX;
     newEv.eventY = firstDoubleTapEvent.eventY;
    } else {
      firstDoubleTapEvent = ev;
    }
  } else {
    firstDoubleTapEvent = ev;
  }
  gettimeofday(&lastTapTime, nullptr);

  pos.x = newEv.eventX - x();
  pos.y = newEv.eventY - y();

  switch (ev.gesture) {
  case GestureOneTap:
    buttonMask = 1<<0;
    break;
  case GestureTwoTap:
    buttonMask = 1<<2;
    break;
  case GestureThreeTap:
    buttonMask = 1<<1;
    break;
  default:
    assert(false);
    return;
  }

  handlePointerEvent(pos, buttonMask);
  handlePointerEvent(pos, 0);
}

void Viewport::handleLongPressGesture(const GestureEvent& ev)
{
  core::Point pos;

  pos.x = ev.eventX - x();
  pos.y = ev.eventY - y();

  if (ev.type == GestureEnd)
    handlePointerEvent(pos, 0);
  else
    handlePointerEvent(pos, 1<<2);
}

void Viewport::handleDragGesture(const GestureEvent& ev)
{
  core::Point pos;

  pos.x = ev.eventX - x();
  pos.y = ev.eventY - y();

  if (ev.gesture == GestureDrag) {
    if (ev.type == GestureEnd)
      handlePointerEvent(pos, 0);
    else
      handlePointerEvent(pos, 1<<0);
  } else {
    switch (ev.type) {
    case GestureBegin:
      lastMagnitudeX = ev.magnitudeX;
      lastMagnitudeY = ev.magnitudeY;
      handlePointerEvent(pos, 0);
      break;
    case GestureUpdate:
      while ((ev.magnitudeY - lastMagnitudeY) > SCRLSENS) {
        handlePointerEvent(pos, 1<<3);
        handlePointerEvent(pos, 0);
        lastMagnitudeY += SCRLSENS;
      }
      while ((ev.magnitudeY - lastMagnitudeY) < -SCRLSENS) {
        handlePointerEvent(pos, 1<<4);
        handlePointerEvent(pos, 0);
        lastMagnitudeY -= SCRLSENS;
      }
      while ((ev.magnitudeX - lastMagnitudeX) > SCRLSENS) {
        handlePointerEvent(pos, 1<<5);
        handlePointerEvent(pos, 0);
        lastMagnitudeX += SCRLSENS;
      }
      while ((ev.magnitudeX - lastMagnitudeX) < -SCRLSENS) {
        handlePointerEvent(pos, 1<<6);
        handlePointerEvent(pos, 0);
        lastMagnitudeX -= SCRLSENS;
      }
      break;
    case GestureEnd:
      break;
    }
  }
}

void Viewport::handlePinchGesture(const GestureEvent& ev)
{
  core::Point pos;
  double magnitude;

  pos.x = ev.eventX - x();
  pos.y = ev.eventY - y();

  switch (ev.type) {
  case GestureBegin:
    lastMagnitudeX = hypot(ev.magnitudeX, ev.magnitudeY);
    handlePointerEvent(pos, 0);
    break;
  case GestureUpdate:
    magnitude = hypot(ev.magnitudeX, ev.magnitudeY);
    if (abs(magnitude - lastMagnitudeX) > ZOOMSENS) {
      sendKeyPress(FAKE_GESTURE_KEY_CODE, 0x1d, XK_Control_L);

      while ((magnitude - lastMagnitudeX) > ZOOMSENS) {
        handlePointerEvent(pos, 1<<3);
        handlePointerEvent(pos, 0);
        lastMagnitudeX += ZOOMSENS;
      }
      while ((magnitude - lastMagnitudeX) < -ZOOMSENS) {
        handlePointerEvent(pos, 1<<4);
        handlePointerEvent(pos, 0);
        lastMagnitudeX -= ZOOMSENS;
      }

      sendKeyRelease(FAKE_GESTURE_KEY_CODE);
    }
    break;
  case GestureEnd:
    break;
  }
}

void Viewport::sendPointerEvent(const core::Point& pos,
                                uint16_t buttonMask)
{
  if (viewOnly)
      return;

  if ((pointerEventInterval == 0) || (buttonMask != lastButtonMask)) {
    try {
      cc->writer()->writePointerEvent(pos, buttonMask);
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection_unexpected(e);
    }
  } else {
    if (!Fl::has_timeout(handlePointerTimeout, this))
      Fl::add_timeout((double)pointerEventInterval/1000.0,
                      handlePointerTimeout, this);
  }
  lastPointerPos = pos;
  lastButtonMask = buttonMask;
}

bool Viewport::hasFocus()
{
  Fl_Widget* focus;

  focus = Fl::grab();
  if (!focus)
    focus = Fl::focus();

  return focus == this;
}

void Viewport::handleClipboardChange(QClipboard::Mode mode)
{
  if (viewOnly)
    return;

  if (!sendClipboard)
    return;

#if !defined(WIN32) && !defined(__APPLE__)
  if (!sendPrimary && (mode == QClipboard::Mode::Selection))
    return;
#endif

  if ((mode != QClipboard::Mode::Clipboard) &&
      (mode != QClipboard::Mode::Selection))
    return;

  if ((mode == QClipboard::Mode::Clipboard) &&
      QGuiApplication::clipboard()->ownsClipboard())
    return;

  if ((mode == QClipboard::Mode::Selection) &&
      QGuiApplication::clipboard()->ownsSelection())
    return;

  if (!hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    clipboardMode = mode;
    pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    try {
      cc->announceClipboard(false);
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection_unexpected(e);
    }
    return;
  }

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText()) {
    vlog.debug("Got non-plain text in local clipboard, ignoring.");
    // Reset the state as if we don't have any clipboard data at all
    pendingClientClipboard = false;
    try {
      cc->announceClipboard(false);
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection_unexpected(e);
    }
    return;
  }

  clipboardMode = mode;

  vlog.debug("Local clipboard changed, notifying server");
  try {
    cc->announceClipboard(true);
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }
}


void Viewport::flushPendingClipboard()
{
  if (pendingClientClipboard) {
    if (!QGuiApplication::clipboard()->mimeData(clipboardMode)->hasText()) {
      vlog.debug("Pending local clipboard has no plain text, ignoring.");
      pendingClientClipboard = false;
      return;
    }

    vlog.debug("Focus regained after local clipboard change, notifying server");
    try {
      cc->announceClipboard(true);
    } catch (std::exception& e) {
      vlog.error("%s", e.what());
      abort_connection_unexpected(e);
    }
  }

  pendingClientClipboard = false;
}


void Viewport::handlePointerEvent(const core::Point& pos,
                                  uint16_t buttonMask)
{
  filterPointerEvent(pos, buttonMask);
}


void Viewport::handlePointerTimeout(void *data)
{
  Viewport *self = (Viewport *)data;

  assert(self);

  try {
    self->cc->writer()->writePointerEvent(self->lastPointerPos,
                                          self->lastButtonMask);
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }
}


void Viewport::resetKeyboard()
{
  try {
    cc->releaseAllKeys();
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }

  keyboard->reset();

  shortcutHandler.reset();
  shortcutBypass = false;
  shortcutActive = false;
  pressedKeys.clear();
}


void Viewport::handleKeyPress(int systemKeyCode,
                              uint32_t keyCode, uint32_t keySym)
{
  pressedKeys.insert(systemKeyCode);

  // Possible keyboard shortcut?

  if (!shortcutBypass) {
    ShortcutHandler::KeyAction action;

    action = shortcutHandler.handleKeyPress(systemKeyCode, keySym);

    if (action == ShortcutHandler::KeyIgnore) {
      vlog.debug("Ignoring key press %d => 0x%02x / XK_%s (0x%04x)",
                 systemKeyCode, keyCode, KeySymName(keySym), keySym);
      return;
    }

    if (action == ShortcutHandler::KeyShortcut) {
      std::list<uint32_t> keySyms;
      std::list<uint32_t>::const_iterator iter;

      // Modifiers can change the KeySym that's been resolved, so we
      // need to check all possible KeySyms for this physical key, not
      // just the current one
      keySyms = keyboard->translateToKeySyms(systemKeyCode);

      // Then we pick the one that matches first
      keySym = NoSymbol;
      for (iter = keySyms.begin(); iter != keySyms.end(); iter++) {
        bool found;

        switch (*iter) {
        case XK_space:
        case XK_G:
        case XK_g:
        case XK_M:
        case XK_m:
        case XK_KP_Enter:
        case XK_Return:
          keySym = *iter;
          found = true;
          break;
        default:
          found = false;
          break;
        }

        if (found)
          break;
      }

      if (keySym != NoSymbol) {
        vlog.debug("Detected shortcut %d => 0x%02x / XK_%s (0x%04x)",
                  systemKeyCode, keyCode, KeySymName(keySym), keySym);
      } else {
        std::string names;

        for (iter = keySyms.begin(); iter != keySyms.end(); iter++) {
          if (!names.empty())
            names += ", ";
          names += core::format("XK_%s (0x%04x)",
                                KeySymName(*iter), *iter);
        }

        vlog.debug("Detected unknown shortcut %d => 0x%02x / %s",
                   systemKeyCode, keyCode, names.c_str());
      }

      // Special case which we need to handle first
      if (keySym == XK_space) {
        // If another shortcut has already fired, then we're too late as
        // we've already released the modifier keys
        if (!shortcutActive) {
          shortcutBypass = true;
          shortcutHandler.reset();
        }
        return;
      }

      shortcutActive = true;

      // The remote session won't see any more keys, so release the ones
      // currently down
      try {
        cc->releaseAllKeys();
      } catch (std::exception& e) {
        vlog.error("%s", e.what());
        abort_connection(_("An unexpected error occurred when communicating "
                           "with the server:\n\n%s"), e.what());
      }

      switch (keySym) {
      case XK_G:
      case XK_g:
        ((DesktopWindow*)window())->grabKeyboard();
        break;
      case XK_M:
      case XK_m:
        popupContextMenu();
        break;
      case XK_KP_Enter:
      case XK_Return:
        if (window()->fullscreen_active()) {
          fullScreen.setParam(false);
          window()->fullscreen_off();
        } else {
          fullScreen.setParam(true);
          ((DesktopWindow*)window())->fullscreen_on();
        }
        break;
      default:
        // Unknown/Unused keyboard shortcut
        break;
      }

      return;
    }
  }

  // Normal key, so send to server...

  sendKeyPress(systemKeyCode, keyCode, keySym);
}

void Viewport::sendKeyPress(int systemKeyCode,
                            uint32_t keyCode, uint32_t keySym)
{
  if (viewOnly)
    return;

  try {
    cc->sendKeyPress(systemKeyCode, keyCode, keySym);
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }
}


void Viewport::handleKeyRelease(int systemKeyCode)
{
  pressedKeys.erase(systemKeyCode);

  if (pressedKeys.empty())
    shortcutActive = false;

  // Possible keyboard shortcut?

  if (!shortcutBypass) {
    ShortcutHandler::KeyAction action;

    action = shortcutHandler.handleKeyRelease(systemKeyCode);

    if (action == ShortcutHandler::KeyIgnore) {
      vlog.debug("Ignoring key release %d", systemKeyCode);
      return;
    }

    if (action == ShortcutHandler::KeyShortcut) {
      vlog.debug("Shortcut release %d", systemKeyCode);
      return;
    }

    if (action == ShortcutHandler::KeyUnarm) {
      DesktopWindow *win;

      vlog.debug("Detected shortcut to release grab");

      try {
        cc->releaseAllKeys();
      } catch (std::exception& e) {
        vlog.error("%s", e.what());
        abort_connection(_("An unexpected error occurred when communicating "
                           "with the server:\n\n%s"), e.what());
      }

      win = dynamic_cast<DesktopWindow*>(window());
      assert(win);
      win->ungrabKeyboard();

      return;
    }
  }

  if (pressedKeys.empty())
    shortcutBypass = false;

  // Normal key, so send to server...

  sendKeyRelease(systemKeyCode);
}

void Viewport::sendKeyRelease(int systemKeyCode)
{
  if (viewOnly)
    return;

  try {
    cc->sendKeyRelease(systemKeyCode);
  } catch (std::exception& e) {
    vlog.error("%s", e.what());
    abort_connection_unexpected(e);
  }
}


int Viewport::handleSystemEvent(void *event, void *data)
{
  Viewport *self = (Viewport *)data;
  bool consumed;

  assert(self);

#ifndef __APPLE__
  // We need a window handle before we can create these
  if (self->touch == nullptr) {
#if defined(WIN32)
    self->touch = new Win32TouchHandler(fl_xid(self->window()), self);
#else
    self->touch = new XInputTouchHandler(fl_xid(self->window()), self);
#endif
  }

  consumed = self->touch->handleEvent(event);
  if (consumed)
    return 1;
#endif

  if (!self->hasFocus())
    return 0;

  // Special event that means we temporarily lost some input
  if (self->keyboard->isKeyboardReset(event)) {
    self->resetKeyboard();
    return 1;
  }

  consumed = self->keyboard->handleEvent(event);
  if (consumed)
    return 1;

  return 0;
}

void Viewport::initContextMenu()
{
  QAction* action;

  contextMenu->clear();

  action = new QAction(C_("ContextMenu|", "Dis&connect"), contextMenu);
  connect(action, &QAction::triggered, this,
          []() {
            ::disconnect();
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Full screen"), contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
            if (self->window()->fullscreen_active())
              self->window()->fullscreen_off();
            else
              ((DesktopWindow*)self->window())->fullscreen_on();
          });
  action->setCheckable(true);
  action->setChecked(window()->fullscreen_active());
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Minimi&ze"), contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
#ifdef __APPLE__
            // FIXME: Workaround for not being able to minimize in fullscreen
            // https://github.com/TigerVNC/tigervnc/pull/1813
            if (self->window()->fullscreen_active())
              cocoa_enable_minimize(self->window());
#endif
            self->window()->iconize();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Resize &window to session"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
            if (self->window()->fullscreen_active())
              return;
            self->window()->size(self->w(), self->h());
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Ctrl"), contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this](bool checked) {
            if (checked)
              self->sendKeyPress(FAKE_CTRL_KEY_CODE,
                                 0x1d, XK_Control_L);
            else
              self->sendKeyRelease(FAKE_CTRL_KEY_CODE);
            self->menuCtrlKey = !(self->menuCtrlKey);
          });
  action->setCheckable(true);
  action->setChecked(menuCtrlKey);
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "&Alt"), contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this](bool checked) {
            if (checked)
              self->sendKeyPress(FAKE_ALT_KEY_CODE,
                                 0x38, XK_Alt_L);
            else
              self->sendKeyRelease(FAKE_ALT_KEY_CODE);
            self->menuAltKey = !(self->menuAltKey);
          });
  action->setCheckable(true);
  action->setChecked(menuAltKey);
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Send Ctrl-Alt-&Del"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
            self->sendKeyPress(FAKE_CTRL_KEY_CODE,
                               0x1d, XK_Control_L);
            self->sendKeyPress(FAKE_ALT_KEY_CODE,
                               0x38, XK_Alt_L);
            self->sendKeyPress(FAKE_DEL_KEY_CODE,
                               0xd3, XK_Delete);
            self->sendKeyRelease(FAKE_DEL_KEY_CODE);
            self->sendKeyRelease(FAKE_ALT_KEY_CODE);
            self->sendKeyRelease(FAKE_CTRL_KEY_CODE);
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Refresh screen"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
            self->cc->refreshFramebuffer();
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Options..."), contextMenu);
  connect(action, &QAction::triggered, this,
          []() {
            OptionsDialog* dlg;

            dlg = new OptionsDialog();
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Connection &info..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [self=this]() {
            QMessageBox* dlg;
            dlg = new QMessageBox;
            dlg->setIcon(QMessageBox::Information);
            dlg->setWindowTitle(_("VNC connection info"));
            dlg->setText(self->cc->connectionInfo().c_str());
            dlg->addButton(QMessageBox::Close);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "About &TigerVNC..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          []() {
            about_vncviewer();
          });
  contextMenu->addAction(action);
}

void Viewport::popupContextMenu()
{
  // initialize context menu before display
  initContextMenu();

  contextMenu->popup(QCursor::pos());
  // FIXME: We get Qt::PopupFocusReason, but focus still remains with
  // us, so force it again to fully move focus
  // https://qt-project.atlassian.net/browse/QTBUG-145865
  contextMenu->setFocus(Qt::PopupFocusReason);
}


void Viewport::handleOptions(void *data)
{
  Viewport *self = (Viewport*)data;
  unsigned modifierMask;

  modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  self->shortcutHandler.setModifiers(modifierMask);

  if (Fl::belowmouse() == self)
    self->showCursor();
}
