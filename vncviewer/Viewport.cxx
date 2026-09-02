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

#include <stdexcept>

#include <QAbstractEventDispatcher>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGesture>
#include <QGestureRecognizer>
#include <QEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <core/LogWriter.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/time.h>

#include <rfb/CMsgWriter.h>
#include <rfb/Cursor.h>
#include <rfb/KeysymStr.h>
#include <rfb/ledStates.h>

#define XK_LATIN1
#define XK_MISCELLANY
#include <rfb/keysymdef.h>

#ifndef NoSymbol
#define NoSymbol 0
#endif

#include "Viewport.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "mainloop.h"
#include "parameters.h"
#include "vncviewer.h"

#include "clicksalternativegesture.h"
#include "clicksalternativegesturerecognizer.h"
#include "panzoomgesture.h"
#include "panzoomgesturerecognizer.h"
#include "tapdraggesture.h"
#include "tapdraggesturerecognizer.h"

#include "PlatformPixelBuffer.h"

#if defined(WIN32)
#include "KeyboardWin32.h"
#elif defined(__APPLE__)
#include "KeyboardMacOS.h"
#else
#include "KeyboardX11.h"
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

static Qt::GestureType ClicksAlternativeGesture;
static Qt::GestureType PanZoomGesture;
static Qt::GestureType TapDragGesture;

Viewport::Viewport(int w, int h, CConn* cc_, QWidget* parent)
  : QWidget(parent), cc(cc_), frameBuffer(nullptr),
    lastPointerPos(0, 0), lastButtonMask(0),
    keyboard(nullptr), shortcutBypass(false), shortcutActive(false),
    firstLEDState(true), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(nullptr),
    cursorIsBlank(false)
#ifdef QT_DEBUG
    , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_AcceptTouchEvents);
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

  ClicksAlternativeGesture = QGestureRecognizer::registerRecognizer(new QClicksAlternativeGestureRecognizer);
  PanZoomGesture = QGestureRecognizer::registerRecognizer(new QPanZoomGestureRecognizer);
  TapDragGesture = QGestureRecognizer::registerRecognizer(new QTapDragGestureRecognizer);

  grabGesture(ClicksAlternativeGesture);
  grabGesture(PanZoomGesture);
  grabGesture(TapDragGesture);

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
          [this]() { showCursor(); });

  unsigned modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  shortcutHandler.setModifiers(modifierMask);

  OptionsDialog::addCallback(handleOptions, this);

  // Make sure we have an initial blank cursor set
  setCursor();

  mousePointerTimer = new QTimer(this);
  mousePointerTimer->setInterval(::pointerEventInterval);
  mousePointerTimer->setSingleShot(true);
  connect(mousePointerTimer, &QTimer::timeout, this,
          &Viewport::handlePointerTimeout);

#ifdef QT_DEBUG
  gettimeofday(&fpsLast, nullptr);
  fpsTimer.start(5000);
#endif
}

Viewport::~Viewport()
{
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);

  OptionsDialog::removeCallback(handleOptions);

  delete cursor;

  delete keyboard;

  QGestureRecognizer::unregisterRecognizer(ClicksAlternativeGesture);
  ClicksAlternativeGesture = (Qt::GestureType)0;
  QGestureRecognizer::unregisterRecognizer(PanZoomGesture);
  PanZoomGesture = (Qt::GestureType)0;
  QGestureRecognizer::unregisterRecognizer(TapDragGesture);
  TapDragGesture = (Qt::GestureType)0;
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
  damage += QRect(r.tl.x, r.tl.y, r.width(), r.height());
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

void Viewport::setCursor()
{
  int width, height;
  core::Point hotspot;
  const uint8_t* pixels;

  int i;

  delete cursor;

  width = cc->server.cursor().width();
  height = cc->server.cursor().height();
  hotspot = cc->server.cursor().hotspot();
  pixels = cc->server.cursor().getBuffer();

  for (i = 0; i < width*height; i++)
    if (pixels[i*4 + 3] != 0) break;

  if (cursorIsBlank && alwaysCursor) {
    // This is the default in case the local cursor should be displayed yet cursorType is invalid.
    // Since the cursor variable isn't used if the cursorType is system, we can do this without checking the current
    // type which helps handle changing the type while the viewer is running.
    vlog.debug("Cursor is empty, using dot");

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

  showCursor();
}

void Viewport::showCursor()
{
  if (viewOnly) {
    QWidget::setCursor(Qt::ArrowCursor);
    return;
  }

  if (cursorIsBlank && alwaysCursor && (cursorType == "system")) {
    QWidget::setCursor(Qt::ArrowCursor);
  } else {
    QWidget::setCursor(*cursor);
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
#ifdef __APPLE__
  serverReceivedData = cbdata;
#endif
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

void Viewport::paintEvent(QPaintEvent* event)
{
  // Shadow pixmap size out of sync with framebuffer?
  if ((frameBuffer->width() != pixmap.width()) ||
      (frameBuffer->height() != pixmap.height())) {
    pixmap = QPixmap(frameBuffer->width(), frameBuffer->height());
    damage = QRegion(0, 0, frameBuffer->width(), frameBuffer->height());
  }

  // Shadow pixmap contents out of sync with framebuffer?
  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);

    QRect bounds;
    int x, y, w, h;
    core::Rect fbrect;

    const uint8_t* fbdata;
    int stride;

    bounds = damage.boundingRect();
    damage = QRegion();
    x = bounds.x();
    y = bounds.y();
    w = bounds.width();
    h = bounds.height();
    fbrect.setXYWH(x, y, w, h);

    fbdata = frameBuffer->getBuffer(fbrect, &stride);
    QImage image(fbdata, w, h, stride * 4, QImage::Format_RGB32);

#ifdef __APPLE__
    // Qt assumes that the alpha channel is always 0xff, which we cannot
    // guarantee
    pixmapPainter.fillRect(bounds, QColor("#ff000000"));
    pixmapPainter.setCompositionMode(QPainter::CompositionMode_Plus);
#endif

    pixmapPainter.drawImage(bounds, image);
  }

  QPainter painter(this);

  QRect rect;

  // Check what actually needs updating
  rect = event->rect();

  painter.drawPixmap(rect, pixmap, rect);

#ifdef QT_DEBUG
  fpsCounter++;
  QFont f;
  f.setBold(true);
  f.setPixelSize(14);
  painter.setFont(f);
  painter.setPen(Qt::NoPen);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor("#96101010"));
  painter.drawRect(fpsRect);
  QPen p;
  p.setColor("#e0ffffff");
  painter.setPen(p);
  QString text = QString("%1 fps").arg(fpsValue);
  painter.drawText(fpsRect, text, QTextOption(Qt::AlignCenter));
#endif
}

#ifdef QT_DEBUG
void Viewport::handleTimeout(core::Timer* t)
{
  struct timeval now;
  int count;

  EmulateMB::handleTimeout(t);

  if (t != &fpsTimer)
    return;

  gettimeofday(&now, nullptr);
  count = fpsCounter;

  fpsValue = int(count * 1000.0 / core::msSince(&fpsLast));

  vlog.info("%d frames in %g seconds = %d FPS", count, core::msSince(&fpsLast) / 1000.0, fpsValue);

  fpsCounter -= count;
  fpsLast = now;

  damage += fpsRect;
  update(damage);

  t->repeat();
}
#endif

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

  // FIXME: Is this really needed?
  if(event->source() != Qt::MouseEventNotSynthesized)
    return;

  event->accept();

  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton)
    buttonMask |= 1 << 0;
  if (event->buttons() & Qt::MiddleButton)
    buttonMask |= 1 << 1;
  if (event->buttons() & Qt::RightButton)
    buttonMask |= 1 << 2;
  if (event->buttons() & Qt::BackButton)
    buttonMask |= 1 << 7;
  if (event->buttons() & Qt::ForwardButton)
    buttonMask |= 1 << 8;

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
    buttonMask |= 1 << 0;
  if (event->buttons() & Qt::MiddleButton)
    buttonMask |= 1 << 1;
  if (event->buttons() & Qt::RightButton)
    buttonMask |= 1 << 2;
  if (event->buttons() & Qt::BackButton)
    buttonMask |= 1 << 7;
  if (event->buttons() & Qt::ForwardButton)
    buttonMask |= 1 << 8;

  wheelMask = 0;
  if (event->angleDelta().y() > 0)
    wheelMask |= 1 << 3;
  if (event->angleDelta().y() < 0)
    wheelMask |= 1 << 4;
  if (event->angleDelta().x() > 0)
    wheelMask |= 1 << 5;
  if (event->angleDelta().x() < 0)
    wheelMask |= 1 << 6;

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

bool Viewport::gestureEvent(QGestureEvent* event)
{
  QGesture* gesture;

  for(QGesture* g: event->gestures()) {
    vlog.debug("Viewport::gestureEvent: %d %s",
               g->gestureType(),
               QVariant::fromValue(g->state()).toString().toStdString().c_str());
  }

  gesture = event->gesture(ClicksAlternativeGesture);
  if (gesture)
    clicksAlternativeGesture((QClicksAlternativeGesture*)gesture);
  gesture = event->gesture(PanZoomGesture);
  if (gesture)
    panZoomGesture((QPanZoomGesture*)gesture);
  gesture = event->gesture(TapDragGesture);
  if (gesture)
    tapDragGesture((QTapDragGesture*)gesture);

  return true;
}

void Viewport::clicksAlternativeGesture(QClicksAlternativeGesture* gesture)
{
  core::Point pos;
  int buttonMask;

  if (gesture->state() != Qt::GestureFinished)
    return;

  pos.x = gesture->getPosition().x();
  pos.y = gesture->getPosition().y();

  // If the user quickly taps multiple times we assume they meant to
  // hit the same spot, so slightly adjust coordinates
  if ((core::msSince(&lastTapTime) < DOUBLE_TAP_TIMEOUT) &&
      (firstDoubleTapType == gesture->getType())) {

    double dx = firstDoubleTapPos.x() - gesture->getPosition().x();
    double dy = firstDoubleTapPos.y() - gesture->getPosition().y();
    double distance = hypot(dx, dy);

    if (distance < DOUBLE_TAP_THRESHOLD) {
      pos.x = firstDoubleTapPos.x();
      pos.y = firstDoubleTapPos.y();
    } else {
      firstDoubleTapType = gesture->getType();
      firstDoubleTapPos = gesture->getPosition();
    }
  } else {
    firstDoubleTapType = gesture->getType();
    firstDoubleTapPos = gesture->getPosition();
  }
  gettimeofday(&lastTapTime, nullptr);

  switch (gesture->getType()) {
  case QClicksAlternativeGesture::TwoPoints:
    buttonMask = 1<<2;
    break;
  case QClicksAlternativeGesture::ThreePoints:
    buttonMask = 1<<1;
    break;
  default:
    assert(false);
    return;
  }

  handlePointerEvent(pos, buttonMask);
  handlePointerEvent(pos, 0);
}

void Viewport::tapDragGesture(QTapDragGesture* gesture)
{
  core::Point pos;
  int buttonMask;

  if (gesture->state() != Qt::GestureFinished)
    return;

  pos.x = gesture->getPosition().x();
  pos.y = gesture->getPosition().y();

  if (gesture->getType() == QTapDragGesture::Tap) {
    if (gesture->state() != Qt::GestureFinished)
      return;

    // If the user quickly taps multiple times we assume they meant to
    // hit the same spot, so slightly adjust coordinates
    if ((core::msSince(&lastTapTime) < DOUBLE_TAP_TIMEOUT) &&
        (firstDoubleTapType == gesture->getType())) {

      double dx = firstDoubleTapPos.x() - gesture->getPosition().x();
      double dy = firstDoubleTapPos.y() - gesture->getPosition().y();
      double distance = hypot(dx, dy);

      if (distance < DOUBLE_TAP_THRESHOLD) {
        pos.x = firstDoubleTapPos.x();
        pos.y = firstDoubleTapPos.y();
      } else {
        firstDoubleTapType = gesture->getType();
        firstDoubleTapPos = gesture->getPosition();
      }
    } else {
      firstDoubleTapType = gesture->getType();
      firstDoubleTapPos = gesture->getPosition();
    }
    gettimeofday(&lastTapTime, nullptr);

    buttonMask = 1<<0;
    handlePointerEvent(pos, buttonMask);
    handlePointerEvent(pos, 0);
  }

  if (gesture->getType() == QTapDragGesture::TapAndHold) {
    if (gesture->state() == Qt::GestureFinished)
      handlePointerEvent(pos, 0);
    else
      handlePointerEvent(pos, 1<<2);
  }

  if (gesture->getType() == QTapDragGesture::Drag) {
    if (gesture->state() == Qt::GestureFinished)
      handlePointerEvent(pos, 0);
    else
      handlePointerEvent(pos, 1<<0);
  }
}

void Viewport::panZoomGesture(QPanZoomGesture* gesture)
{
  core::Point pos;

  pos.x = gesture->getPosition().x();
  pos.y = gesture->getPosition().y();

  if (gesture->getType() == QPanZoomGesture::Pan) {
    switch (gesture->state()) {
    case Qt::GestureStarted:
      lastMagnitudeX = gesture->getOffsetDelta().x();
      lastMagnitudeY = gesture->getOffsetDelta().y();
      handlePointerEvent(pos, 0);
      break;
    case Qt::GestureUpdated:
      while ((gesture->getOffsetDelta().y() - lastMagnitudeY) > SCRLSENS) {
        handlePointerEvent(pos, 1<<3);
        handlePointerEvent(pos, 0);
        lastMagnitudeY += SCRLSENS;
      }
      while ((gesture->getOffsetDelta().y() - lastMagnitudeY) < -SCRLSENS) {
        handlePointerEvent(pos, 1<<4);
        handlePointerEvent(pos, 0);
        lastMagnitudeY -= SCRLSENS;
      }
      while ((gesture->getOffsetDelta().x() - lastMagnitudeX) > SCRLSENS) {
        handlePointerEvent(pos, 1<<5);
        handlePointerEvent(pos, 0);
        lastMagnitudeX += SCRLSENS;
      }
      while ((gesture->getOffsetDelta().x() - lastMagnitudeX) < -SCRLSENS) {
        handlePointerEvent(pos, 1<<6);
        handlePointerEvent(pos, 0);
        lastMagnitudeX -= SCRLSENS;
      }
    default:
      break;
    }
  } else {
    switch (gesture->state()) {
    case Qt::GestureStarted:
      lastMagnitudeX = gesture->getScaleFactor();
      handlePointerEvent(pos, 0);
      break;
    case Qt::GestureUpdated:
      if (abs(gesture->getScaleFactor() - lastMagnitudeX) > ZOOMSENS) {
        sendKeyPress(FAKE_GESTURE_KEY_CODE, 0x1d, XK_Control_L);

        while ((gesture->getScaleFactor() - lastMagnitudeX) > ZOOMSENS) {
          handlePointerEvent(pos, 1<<3);
          handlePointerEvent(pos, 0);
          lastMagnitudeX += ZOOMSENS;
        }
        while ((gesture->getScaleFactor() - lastMagnitudeX) < -ZOOMSENS) {
          handlePointerEvent(pos, 1<<4);
          handlePointerEvent(pos, 0);
          lastMagnitudeX -= ZOOMSENS;
        }

        sendKeyRelease(FAKE_GESTURE_KEY_CODE);
      }
    default:
      break;
    }
  }
}

bool Viewport::event(QEvent *event)
{
  switch (event->type()) {
  case QEvent::CursorChange:
    // FIXME: Check this
    // event->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    // return true;
    break;
  case QEvent::Gesture:
    return gestureEvent((QGestureEvent*)event);
  default:
    break;
  }
  return QWidget::event(event);
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
    if (!mousePointerTimer->isActive())
      mousePointerTimer->start();
  }
  lastPointerPos = pos;
  lastButtonMask = buttonMask;
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

#ifdef __APPLE__
  // FIXME: This shouldn't be needed, as Qt checks for
  // kPasteboardModified from PasteboardSynchronize(), but we can
  // use [[NSPasteboard generalPasteboard] changeCount] otherwise
  // https://qt-project.atlassian.net/browse/QTBUG-124214
  if (QGuiApplication::clipboard()->text(mode) == serverReceivedData) {
    serverReceivedData = "";
    return;
  }
#endif

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


void Viewport::handlePointerTimeout()
{
  try {
    cc->writer()->writePointerEvent(lastPointerPos, lastButtonMask);
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
        if (((DesktopWindow*)window())->isFullScreen()) {
          fullScreen.setParam(false);
          ((DesktopWindow*)window())->setFullScreen(false);
        } else {
          fullScreen.setParam(true);
          ((DesktopWindow*)window())->setFullScreen(true);
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


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool Viewport::nativeEventFilter(const QByteArray& eventType, void* message, long*)
#else
bool Viewport::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*)
#endif
{
  bool consumed;

  if (!hasFocus())
    return false;

  // Special event that means we temporarily lost some input
  if (keyboard->isKeyboardReset(eventType, message)) {
    resetKeyboard();
    return true;
  }

  consumed = keyboard->handleEvent(eventType, message);
  if (consumed)
    return true;

  return false;
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
          [this](bool checked) {
            ((DesktopWindow*)window())->setFullScreen(checked);
          });
  action->setCheckable(true);
  action->setChecked(((DesktopWindow*)window())->isFullScreen());
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Minimi&ze"), contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            window()->showMinimized();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Resize &window to session"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            if (((DesktopWindow*)window())->isFullScreen())
              return;
            window()->resize(width(), height());
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Ctrl"), contextMenu);
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

  action = new QAction(C_("ContextMenu|", "&Alt"), contextMenu);
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

  action = new QAction(C_("ContextMenu|", "Send Ctrl-Alt-&Del"),
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

  action = new QAction(C_("ContextMenu|", "&Refresh screen"),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            cc->refreshFramebuffer();
          });
  contextMenu->addAction(action);

  contextMenu->addSeparator();

  action = new QAction(C_("ContextMenu|", "&Options..."), contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            OptionsDialog* dlg;

            dlg = new OptionsDialog(window());
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "Connection &info..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            QMessageBox* dlg;
            dlg = new QMessageBox(window());
            dlg->setIcon(QMessageBox::Information);
            dlg->setWindowTitle(_("VNC connection info"));
            dlg->setText(cc->connectionInfo().c_str());
            dlg->addButton(QMessageBox::Close);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(C_("ContextMenu|", "About &TigerVNC..."),
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


void Viewport::handleOptions(void *data)
{
  Viewport *self = (Viewport*)data;
  unsigned modifierMask;

  modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  self->shortcutHandler.setModifiers(modifierMask);

  self->showCursor();
}
