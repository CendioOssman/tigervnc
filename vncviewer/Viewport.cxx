/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2026 Pierre Ossman for Cendio AB
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

#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <rfb/CMsgWriter.h>
#include <rfb/LogWriter.h>
#include <rfb/Exception.h>
#include <rfb/ledStates.h>
#include <rfb/util.h>

#define XK_MISCELLANY
#include <rfb/keysymdef.h>

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

Viewport::Viewport(int w, int h, CConn* cc_, QWidget* parent)
  : QWidget(parent), cc(cc_), frameBuffer(nullptr),
    lastPointerPos(0, 0), lastButtonMask(0),
    keyboard(nullptr),
    firstLEDState(true), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(nullptr)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  setContentsMargins(0, 0, 0, 0);
  resize(w, h);

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
  QAbstractEventDispatcher::instance()->installNativeEventFilter(this);

  frameBuffer = new PlatformPixelBuffer(w, h);
  assert(frameBuffer);
  cc->setFramebuffer(frameBuffer);

  contextMenu = new QMenu(this);

  // Set the default mouse pointer whilst the context menu is open, as
  // it is annoying if the pointer disappears when you move it outside
  // the menu
  connect(contextMenu, &QMenu::aboutToShow, this,
          [this]() { QWidget::setCursor(Qt::ArrowCursor); });
  connect(contextMenu, &QMenu::aboutToHide, this,
          [this]() { QWidget::setCursor(*cursor); });

  setMenuKey();

  OptionsDialog::addCallback(handleOptions, this);

  // Make sure we have an initial blank cursor set
  setCursor(0, 0, rfb::Point(0, 0), nullptr);

  mousePointerTimer = new QTimer(this);
  mousePointerTimer->setInterval(::pointerEventInterval);
  mousePointerTimer->setSingleShot(true);
  connect(mousePointerTimer, &QTimer::timeout, this,
          &Viewport::handlePointerTimeout);
}

Viewport::~Viewport()
{
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);

  OptionsDialog::removeCallback(handleOptions);

  delete cursor;

  delete keyboard;
  delete touch;
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
  update(r.tl.x, r.tl.y, r.width(), r.height());
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

  delete cursor;

  for (i = 0; i < width*height; i++)
    if (pixels[i*4 + 3] != 0) break;

  if ((i == width*height) && dotWhenNoCursor) {
    vlog.debug("cursor is empty - using dot");
    cursor = new QCursor(QPixmap(dotcursor_xpm), 2, 2);
  } else {
    if ((width == 0) || (height == 0)) {
      cursor = new QCursor(Qt::BlankCursor);
    } else {
      QImage image(pixels, width, height, QImage::Format_ARGB32);
      cursor = new QCursor(QPixmap::fromImage(image),
                          hotspot.x, hotspot.y);
    }
  }

  QWidget::setCursor(*cursor);
}

void Viewport::handleClipboardRequest()
{
  std::string text, filtered;

  text = QGuiApplication::clipboard()->text(clipboardMode).toStdString();

  if (!rfb::isValidUTF8(text.c_str())) {
    vlog.error("Invalid UTF-8 sequence in system clipboard");
    return;
  }

  filtered = rfb::convertLF(text.c_str());

  vlog.debug("Sending clipboard data (%d bytes)", (int)filtered.size());

  try {
    cc->sendClipboardData(filtered.c_str());
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
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

void Viewport::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  QRect rect;
  int x, y, w, h;
  rfb::Rect rfbrect;

  const uint8_t* fbdata;
  int stride;

  // Check what actually needs updating
  rect = event->rect();
  x = rect.x();
  y = rect.y();
  w = rect.width();
  h = rect.height();
  rfbrect.setXYWH(x, y, w, h);

  fbdata = frameBuffer->getBuffer(rfbrect, &stride);
  QImage image(fbdata, w, h, stride * 4, QImage::Format_RGB32);

  painter.drawImage(rect, image);
}

void Viewport::resizeEvent(QResizeEvent* e)
{
  if ((width() != frameBuffer->width()) ||
      (height() != frameBuffer->height())) {
    vlog.debug("Resizing framebuffer from %dx%d to %dx%d",
               frameBuffer->width(), frameBuffer->height(),
               width(), height());

    frameBuffer = new PlatformPixelBuffer(width(), height());
    assert(frameBuffer);
    cc->setFramebuffer(frameBuffer);
  }

  QWidget::resizeEvent(e);
}

void Viewport::leaveEvent(QEvent*)
{
  QPoint pos = mapFromGlobal(QCursor::pos());
  // We want a last move event to help trigger edge stuff
  handlePointerEvent({pos.x(), pos.y()}, 0);
}

void Viewport::mouseEvent(QMouseEvent* event)
{
  int buttonMask;

  event->accept();

  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton)
    buttonMask |= 1;
  if (event->buttons() & Qt::MiddleButton)
    buttonMask |= 2;
  if (event->buttons() & Qt::RightButton)
    buttonMask |= 4;

  handlePointerEvent({event->x(), event->y()}, buttonMask);
}

void Viewport::mouseMoveEvent(QMouseEvent* event)
{
  mouseEvent(event);
}

void Viewport::mousePressEvent(QMouseEvent* event)
{
  mouseEvent(event);
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
  mouseEvent(event);
}

void Viewport::wheelEvent(QWheelEvent* event)
{
  int buttonMask, wheelMask;

  event->accept();

  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton)
    buttonMask |= 1;
  if (event->buttons() & Qt::MiddleButton)
    buttonMask |= 2;
  if (event->buttons() & Qt::RightButton)
    buttonMask |= 4;

  wheelMask = 0;
  if (event->angleDelta().y() > 0)
    wheelMask |= 8;
  if (event->angleDelta().y() < 0)
    wheelMask |= 16;
  if (event->angleDelta().x() > 0)
    wheelMask |= 32;
  if (event->angleDelta().x() < 0)
    wheelMask |= 64;

  // A quick press of the wheel "button", followed by a immediate
  // release below
  handlePointerEvent({(int)event->position().x(), (int)event->position().y()},
                     buttonMask | wheelMask);

  handlePointerEvent({(int)event->position().x(), (int)event->position().y()},
                     buttonMask);
}

void Viewport::focusInEvent(QFocusEvent* event)
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

  QWidget::focusInEvent(event);
}

void Viewport::focusOutEvent(QFocusEvent* event)
{
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();

  QWidget::focusOutEvent(event);
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
    if (!mousePointerTimer->isActive())
      mousePointerTimer->start();
  }
  lastPointerPos = pos;
  lastButtonMask = buttonMask;
}

void Viewport::handleClipboardChange(QClipboard::Mode mode)
{
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

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText()) {
    vlog.debug("Got non-plain text in local clipboard, ignoring.");
    // Reset the state as if we don't have any clipboard data at all
    pendingClientClipboard = false;
    try {
      cc->announceClipboard(false);
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_connection_unexpected(e);
    }
    return;
  }

  clipboardMode = mode;

  if (!hasFocus()) {
    vlog.debug("Local clipboard changed whilst not focused, will notify server later");
    pendingClientClipboard = true;
    // Clear any older client clipboard from the server
    cc->announceClipboard(false);
    return;
  }

  vlog.debug("Local clipboard changed, notifying server");
  try {
    cc->announceClipboard(true);
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


void Viewport::handlePointerTimeout()
{
  try {
    cc->writer()->writePointerEvent(lastPointerPos, lastButtonMask);
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


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool Viewport::nativeEventFilter(const QByteArray& eventType, void* message, long*)
#else
bool Viewport::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*)
#endif
{
  bool consumed;

#ifndef __APPLE__
  // We need a window handle before we can create these
  if (touch == nullptr) {
#if defined(WIN32)
    touch = new Win32TouchHandler((HWND)window()->winId(), this);
#else
    touch = new XInputTouchHandler(window()->winId(), this);
#endif
  }

  consumed = touch->handleEvent(eventType, message);
  if (consumed)
    return 1;
#endif

  if (!hasFocus())
    return false;

#ifdef __APPLE__
  // Special event that means we temporarily lost some input
  if (KeyboardMacOS::isKeyboardSync(eventType, message)) {
    resetKeyboard();
    return true;
  }
#endif

  consumed = keyboard->handleEvent(eventType, message);
  if (consumed)
    return true;

  return false;
}

void Viewport::initContextMenu()
{
  QAction* action;

  contextMenu->clear();

  action = new QAction(p_("ContextMenu|", "Dis&connect"), contextMenu);
  connect(action, &QAction::triggered, this,
          []() {
            ::disconnect();
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Full screen"), contextMenu);
  connect(action, &QAction::triggered, this,
          [this](bool checked) {
            ((DesktopWindow*)window())->setFullScreen(checked);
          });
  action->setCheckable(true);
  action->setChecked(((DesktopWindow*)window())->isFullScreen());
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Minimi&ze"), contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            window()->showMinimized();
          });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Resize &window to session"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            if (((DesktopWindow*)window())->isFullScreen())
              return;
            window()->resize(width(), height());
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Ctrl"), contextMenu);
  connect(action, &QAction::triggered, this,
          [this](bool checked) {
            if (checked)
              sendKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
            else
              sendKeyRelease(FAKE_CTRL_KEY_CODE);
            menuCtrlKey = checked;
          });
  action->setCheckable(true);
  action->setChecked(menuCtrlKey);
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "&Alt"), contextMenu);
  connect(action, &QAction::triggered, this,
          [this](bool checked) {
            if (checked)
              sendKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
            else
              sendKeyRelease(FAKE_ALT_KEY_CODE);
            menuAltKey = checked;
          });
  action->setCheckable(true);
  action->setChecked(menuAltKey);
  contextMenu->addAction(action);

  if (menuKeySym) {
    QAction* secretAction;
    char sendMenuKey[64];
    snprintf(sendMenuKey, 64, p_("ContextMenu|", "Send %s"), (const char *)menuKey);
    action = new QAction(sendMenuKey, contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              sendKeyPress(FAKE_KEY_CODE, menuKeyCode, menuKeySym);
              sendKeyRelease(FAKE_KEY_CODE);
            });
    action->setShortcut(QKeySequence(menuKeyQt));
    action->setShortcutVisibleInContextMenu(false);
    contextMenu->addAction(action);

    // FIXME: Qt doesn't respect the shortcut set on a menu entry, but
    //        it works if we attach it to ourselves
    secretAction = new QAction(action);
    connect(secretAction, &QAction::triggered, this,
            [this, action]() {
              action->trigger();
              contextMenu->hide();
            });
    secretAction->setShortcut(QKeySequence(menuKeyQt));
    secretAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(contextMenu, &QMenu::aboutToShow, secretAction,
            [this, secretAction]() { addAction(secretAction); });
    connect(contextMenu, &QMenu::aboutToHide, secretAction,
            [this, secretAction]() { removeAction(secretAction); });
  }

  action = new QAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            sendKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
            sendKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
            sendKeyPress(FAKE_DEL_KEY_CODE, 0xd3, XK_Delete);
            sendKeyRelease(FAKE_DEL_KEY_CODE);
            sendKeyRelease(FAKE_ALT_KEY_CODE);
            sendKeyRelease(FAKE_CTRL_KEY_CODE);
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Refresh screen"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            cc->refreshFramebuffer();
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(p_("ContextMenu|", "&Options..."), contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            OptionsDialog* dlg;

            dlg = new OptionsDialog(window());
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Connection &info..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            QMessageBox* dlg;
            dlg = new QMessageBox(window());
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
  connect(action, &QAction::triggered, this,
          [this]() {
            about_vncviewer(window());
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
