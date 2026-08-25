/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2021 Pierre Ossman for Cendio AB
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

#include <QAction>
#include <QMenu>
#include <QMessageBox>

#include <rfb/CMsgWriter.h>
#include <rfb/LogWriter.h>
#include <rfb/Exception.h>
#include <rfb/ledStates.h>
#include <rfb/util.h>

// FLTK can pull in the X11 headers on some systems
#ifndef XK_VoidSymbol
#define XK_MISCELLANY
#include <rfb/keysymdef.h>
#endif

#include "fltk/layout.h"
#include "fltk/util.h"
#include "Viewport.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"
#include "menukey.h"
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

static rfb::LogWriter vlog("Viewport");

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
    keyboard(nullptr),
    firstLEDState(true), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(nullptr)
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

  Fl::add_clipboard_notify(handleClipboardChange, this);

  // We need to intercept keyboard events early
  Fl::add_system_handler(handleSystemEvent, this);

  frameBuffer = new PlatformPixelBuffer(w, h);
  assert(frameBuffer);
  cc->setFramebuffer(frameBuffer);

  contextMenu = new QMenu();

  // Set the default mouse pointer whilst the context menu is open, as
  // it is annoying if the pointer disappears when you move it outside
  // the menu
  QObject::connect(contextMenu, &QMenu::aboutToShow,
                   [this]() {
                     if (Fl::belowmouse() == this)
                       window()->cursor(FL_CURSOR_DEFAULT);
                   });
  QObject::connect(contextMenu, &QMenu::aboutToHide,
                   [this]() {
                     if (Fl::belowmouse())
                       window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
                   });

  setMenuKey();

  OptionsDialog::addCallback(handleOptions, this);

  // Make sure we have an initial blank cursor set
  setCursor(0, 0, rfb::Point(0, 0), nullptr);
}


Viewport::~Viewport()
{
  // Unregister all timeouts in case they get a change tro trigger
  // again later when this object is already gone.
  Fl::remove_timeout(handlePointerTimeout, this);

  Fl::remove_system_handler(handleSystemEvent);

  Fl::remove_clipboard_notify(handleClipboardChange);

  OptionsDialog::removeCallback(handleOptions);

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  delete keyboard;
  delete touch;

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
  rfb::Rect r;

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

void Viewport::setCursor(int width, int height,
                         const rfb::Point& hotspot,
                         const uint8_t* pixels)
{
  int i;

  if (cursor) {
    if (!cursor->alloc_array)
      delete [] cursor->array;
    delete cursor;
  }

  for (i = 0; i < width*height; i++)
    if (pixels[i*4 + 3] != 0) break;

  if ((i == width*height) && dotWhenNoCursor) {
    vlog.debug("cursor is empty - using dot");

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
      memcpy(buffer, pixels, width * height * 4);
      cursor = new Fl_RGB_Image(buffer, width, height, 4);
      cursorHotspot = hotspot;
    }
  }

  if (Fl::belowmouse() == this)
    window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
}

void Viewport::handleClipboardRequest()
{
  Fl::paste(*this, clipboardSource);
}

void Viewport::handleClipboardAnnounce(bool available)
{
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
    Fl::copy(cbdata, len, 0);
#endif
  Fl::copy(cbdata, len, 1);
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

  if (!hasFocus())
    return;

  keyboard->setLEDState(ledState);
}

void Viewport::pushLEDState()
{
  unsigned int ledState;

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


int Viewport::pasteEvent()
{
  std::string filtered;

  if (!rfb::isValidUTF8(Fl::event_text(), Fl::event_length())) {
    vlog.error("Invalid UTF-8 sequence in system clipboard");
    return 1;
  }

  filtered = rfb::convertLF(Fl::event_text(), Fl::event_length());

  vlog.debug("Sending clipboard data (%d bytes)", (int)filtered.size());

  try {
    cc->sendClipboardData(filtered.c_str());
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }

  return 1;
}

int Viewport::enterEvent()
{
  window()->cursor(cursor, cursorHotspot.x, cursorHotspot.y);
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
    buttonMask |= 1;
  if (Fl::event_button2())
    buttonMask |= 2;
  if (Fl::event_button3())
    buttonMask |= 4;

  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()}, buttonMask);
  return 1;
}

int Viewport::wheelEvent()
{
  int buttonMask, wheelMask;

  buttonMask = 0;
  if (Fl::event_button1())
    buttonMask |= 1;
  if (Fl::event_button2())
    buttonMask |= 2;
  if (Fl::event_button3())
    buttonMask |= 4;

  wheelMask = 0;
  if (Fl::event_dy() < 0)
    wheelMask |= 8;
  if (Fl::event_dy() > 0)
    wheelMask |= 16;
  if (Fl::event_dx() < 0)
    wheelMask |= 32;
  if (Fl::event_dx() > 0)
    wheelMask |= 64;

  // A quick press of the wheel "button", followed by a immediate
  // release below
  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()},
                      buttonMask | wheelMask);

  handlePointerEvent({Fl::event_x() - x(), Fl::event_y() - y()}, buttonMask);
  return 1;
}

int Viewport::focusInEvent()
{
  Fl::disable_im();

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

  Fl::enable_im();
  return 1;
}

int Viewport::handle(int event)
{
  switch (event) {
  case FL_PASTE:
    return pasteEvent();
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
  rfb::Point pos;
  int buttonMask;

  if (ev.type != GestureBegin)
    return;

  // If the user quickly taps multiple times we assume they meant to
  // hit the same spot, so slightly adjust coordinates
  if ((rfb::msSince(&lastTapTime) < DOUBLE_TAP_TIMEOUT) &&
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
  }

  handlePointerEvent(pos, buttonMask);
  handlePointerEvent(pos, 0);
}

void Viewport::handleLongPressGesture(const GestureEvent& ev)
{
  rfb::Point pos;

  pos.x = ev.eventX - x();
  pos.y = ev.eventY - y();

  if (ev.type == GestureEnd)
    handlePointerEvent(pos, 0);
  else
    handlePointerEvent(pos, 1<<2);
}

void Viewport::handleDragGesture(const GestureEvent& ev)
{
  rfb::Point pos;

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
  rfb::Point pos;
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

void Viewport::sendPointerEvent(const rfb::Point& pos, uint8_t buttonMask)
{
  if (viewOnly)
      return;

  if ((pointerEventInterval == 0) || (buttonMask != lastButtonMask)) {
    try {
      cc->writer()->writePointerEvent(pos, buttonMask);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
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

void Viewport::handleClipboardChange(int source, void *data)
{
  Viewport *self = (Viewport *)data;

  assert(self);

  if (!sendClipboard)
    return;

#if !defined(WIN32) && !defined(__APPLE__)
  if (!sendPrimary && (source == 0))
    return;
#endif

  self->clipboardSource = source;

  if (!self->hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    self->pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    self->cc->announceClipboard(false);
    return;
  }

  vlog.debug("Local clipboard changed, notifying server");
  try {
    self->cc->announceClipboard(true);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
}


void Viewport::flushPendingClipboard()
{
  if (pendingClientClipboard) {
    vlog.debug("Focus regained after local clipboard change, notifying server");
    try {
      cc->announceClipboard(true);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_unexpected(e);
    }
  }

  pendingClientClipboard = false;
}


void Viewport::handlePointerEvent(const rfb::Point& pos, uint8_t buttonMask)
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
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
}


void Viewport::resetKeyboard()
{
  try {
    cc->releaseAllKeys();
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
  keyboard->reset();
}


void Viewport::handleKeyPress(int systemKeyCode,
                              uint32_t keyCode, uint32_t keySym)
{
  if (menuKeySym && (keySym == menuKeySym)) {
    popupContextMenu();
    return;
  }

  sendKeyPress(systemKeyCode, keyCode, keySym);
}


void Viewport::sendKeyPress(int systemKeyCode,
                            uint32_t keyCode, uint32_t keySym)
{
  if (viewOnly)
    return;

  try {
    cc->sendKeyPress(systemKeyCode, keyCode, keySym);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
}


void Viewport::handleKeyRelease(int systemKeyCode)
{
  sendKeyRelease(systemKeyCode);
}


void Viewport::sendKeyRelease(int systemKeyCode)
{
  if (viewOnly)
    return;

  try {
    cc->sendKeyRelease(systemKeyCode);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
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

#ifdef __APPLE__
  // Special event that means we temporarily lost some input
  if (KeyboardMacOS::isKeyboardSync(event)) {
    self->resetKeyboard();
    return 1;
  }
#endif

  consumed = self->keyboard->handleEvent(event);
  if (consumed)
    return 1;

  return 0;
}

void Viewport::initContextMenu()
{
  QAction* action;

  contextMenu->clear();

  action = new QAction(p_("ContextMenu|", "Dis&connect"), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   []() {
                     disconnect();
                   });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Full screen"), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this](bool checked) {
                     if (!checked)
                       window()->fullscreen_off();
                     else
                       ((DesktopWindow*)window())->fullscreen_on();
                   });
  action->setCheckable(true);
  action->setChecked(window()->fullscreen_active());
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Minimi&ze"), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this]() {
#ifdef __APPLE__
                     // FIXME: Workaround for not being able to minimize in fullscreen
                     // https://github.com/TigerVNC/tigervnc/pull/1813
                     if (window()->fullscreen_active())
                       window()->fullscreen_off();
#endif
                     window()->iconize();
                   });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Resize &window to session"),
                       contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this]() {
                     if (window()->fullscreen_active())
                       return;
                     window()->size(w(), h());
                   });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Ctrl"), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this](bool checked) {
                     if (checked)
                       sendKeyPress(FAKE_CTRL_KEY_CODE,
                                    0x1d, XK_Control_L);
                     else
                       sendKeyRelease(FAKE_CTRL_KEY_CODE);
                     menuCtrlKey = checked;
                   });
  action->setCheckable(true);
  action->setChecked(menuCtrlKey);
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "&Alt"), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this](bool checked) {
                     if (checked)
                       sendKeyPress(FAKE_ALT_KEY_CODE,
                                    0x38, XK_Alt_L);
                     else
                       sendKeyRelease(FAKE_ALT_KEY_CODE);
                     menuAltKey = checked;
                   });
  action->setCheckable(true);
  action->setChecked(menuAltKey);
  contextMenu->addAction(action);

  if (menuKeySym) {
    char sendMenuKey[64];
    snprintf(sendMenuKey, 64, p_("ContextMenu|", "Send %s"), (const char *)menuKey);
    action = new QAction(sendMenuKey, contextMenu);
    QObject::connect(action, &QAction::triggered,
                     [this]() {
                       sendKeyPress(FAKE_KEY_CODE,
                                    menuKeyCode, menuKeySym);
                       sendKeyRelease(FAKE_KEY_CODE);
                     });
    action->setShortcut(QKeySequence(menuKeyQt));
    action->setShortcutVisibleInContextMenu(false);
    contextMenu->addAction(action);
  }

  action = new QAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"),
                       contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this]() {
                     sendKeyPress(FAKE_CTRL_KEY_CODE,
                                  0x1d, XK_Control_L);
                     sendKeyPress(FAKE_ALT_KEY_CODE,
                                  0x38, XK_Alt_L);
                     sendKeyPress(FAKE_DEL_KEY_CODE,
                                  0xd3, XK_Delete);
                     sendKeyRelease(FAKE_DEL_KEY_CODE);
                     sendKeyRelease(FAKE_ALT_KEY_CODE);
                     sendKeyRelease(FAKE_CTRL_KEY_CODE);
                   });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Refresh screen"),
                       contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this]() {
                     cc->refreshFramebuffer();
                   });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Options..."), contextMenu);
  QObject::connect(action, &QAction::triggered,
                   []() {
                     OptionsDialog* dlg;

                     dlg = new OptionsDialog();
                     dlg->setAttribute(Qt::WA_DeleteOnClose);
                     dlg->open();
                   });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Connection &info..."),
                       contextMenu);
  QObject::connect(action, &QAction::triggered,
                   [this]() {
                     QMessageBox* dlg;
                     dlg = new QMessageBox;
                     dlg->setIcon(QMessageBox::Information);
                     dlg->setWindowTitle(_("VNC connection info"));
                     dlg->setText(cc->connectionInfo());
                     dlg->addButton(QMessageBox::Close);
                     dlg->setAttribute(Qt::WA_DeleteOnClose);
                     dlg->open();
                   });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "About &TigerVNC viewer..."),
                       contextMenu);
  QObject::connect(action, &QAction::triggered,
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


void Viewport::setMenuKey()
{
  getMenuKey(&menuKeyQt, &menuKeyCode, &menuKeySym);
}


void Viewport::handleOptions(void *data)
{
  Viewport *self = (Viewport*)data;

  self->setMenuKey();
  // FIXME: Need to recheck cursor for dotWhenNoCursor
}
