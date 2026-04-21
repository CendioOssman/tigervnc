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

#include <stdio.h>
#include <string.h>

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

static rfb::LogWriter vlog("Viewport");

// Used for fake key presses from the menu
static const int FAKE_CTRL_KEY_CODE = 0x10001;
static const int FAKE_ALT_KEY_CODE = 0x10002;
static const int FAKE_DEL_KEY_CODE = 0x10003;

// Used for fake key presses for lock key sync
static const int FAKE_KEY_CODE = 0xffff;

// Used for fake key presses for gestures
static const int FAKE_GESTURE_KEY_CODE = 0x20001;

Viewport::Viewport(CConn* cc_, QWidget* parent)
  : QWidget(parent), cc(cc_), firstUpdate(true),
    delayedInitializeTimer(new QTimer(this)), lastButtonMask(0),
    mousePointerTimer(new QTimer(this)), keyboard(nullptr),
    firstLEDState(true), pendingClientClipboard(false),
    menuCtrlKey(false), menuAltKey(false), cursor(nullptr)
#ifdef QT_DEBUG
    , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NativeWindow, true);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
  setContentsMargins(0, 0, 0, 0);

#if defined(WIN32)
  keyboard = new KeyboardWin32(this);
#elif defined(__APPLE__)
  keyboard = new KeyboardMacOS(this);
#else
  keyboard = new KeyboardX11(this);
#endif

  // 257
  registerGesture(new ClicksAlternativeGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    if (ClicksAlternativeGesture *gesture = static_cast<ClicksAlternativeGesture *>(event->gesture(type))) {
      if (gesture->state() == Qt::GestureFinished) {
        QPoint pos = gesture->getPosition();
        sendKeyRelease(FAKE_GESTURE_KEY_CODE); // Prevents non handling of PanZoomGesture Finished
        if (gesture->getType() == ClicksAlternativeGesture::TwoPoints) {
          vlog.debug("Cendio Right click alternative gesture");
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 4 /* RightButton */);
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        } else if (gesture->getType() == ClicksAlternativeGesture::ThreePoints) {
          vlog.debug("Cendio Middle click gesture");
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 2 /* MiddleButton */);
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }
    }
    return false;
  });

  // 258
  registerGesture(new PanZoomGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    static bool panZoomGesture = false;
    if (PanZoomGesture *gesture = static_cast<PanZoomGesture *>(event->gesture(type))) {
      if (gesture->state() == Qt::GestureUpdated) {
        if (gesture->getType() == PanZoomGesture::Pan) {
          QPoint pos = gesture->getPosition().toPoint();
          int wheelMask = 0;
          if (gesture->getOffsetDelta().y() > 0) {
            wheelMask |= 8;
          }
          if (gesture->getOffsetDelta().y() < 0) {
            wheelMask |= 16;
          }
          if (gesture->getOffsetDelta().x() > 0) {
            wheelMask |= 32;
          }
          if (gesture->getOffsetDelta().x() < 0) {
            wheelMask |= 64;
          }
          vlog.debug("Cendio Pan / Scroll gesture x=%f y=%f mask=%d",
                     gesture->getOffsetDelta().x(),
                     gesture->getOffsetDelta().y(),
                     wheelMask);
          if (!panZoomGesture) {
            panZoomGesture = true;
            QTimer::singleShot(100, this, [=](){
              panZoomGesture = false;
              handlePointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
            });
          }
        }

        if (gesture->getType() == PanZoomGesture::Pinch) {
          QPoint pos = gesture->getPosition().toPoint();
          int wheelMask = 0;
          if (gesture->getScaleFactor() > 1.00) {
            wheelMask |= 8;
          }
          if (gesture->getScaleFactor() < 1.00) {
            wheelMask |= 16;
          }
          vlog.debug("Cendio Zoom gesture %d %f", wheelMask, gesture->getScaleFactor());
          sendKeyPress(FAKE_GESTURE_KEY_CODE, 0x1d, XK_Control_L);
          if (!panZoomGesture) {
            panZoomGesture = true;
            QTimer::singleShot(100, this, [=](){
              panZoomGesture = false;
              handlePointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
            });
          }
          event->accept();
          return true;
        }

        if (gesture->getType() == PanZoomGesture::Undefined) {
          vlog.debug("Cendio UNDEFINED gesture");
        }
      } else {
        event->accept();
        sendKeyRelease(FAKE_GESTURE_KEY_CODE);
        return true;
      }
    }
    return false;
  });

  // 259
  registerGesture(new TapDragGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    if (TapDragGesture *gesture = static_cast<TapDragGesture *>(event->gesture(type))) {
      QPoint pos = gesture->getPosition().toPoint();

      if (gesture->getType() == TapDragGesture::Drag) {
        static bool dragGesture = false;
        if (gesture->state() == Qt::GestureUpdated) {
          vlog.debug("Cendio Drag gesture");
          if (!dragGesture) {
            dragGesture = true;
            QPoint startPos = gesture->getStartPosition().toPoint();
            handlePointerEvent(rfb::Point(startPos.x(), startPos.y()), 1 /* LeftButton */);
          }
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 1 /* LeftButton */);
          event->accept();
          return true;
        } else if (gesture->state() == Qt::GestureFinished) {
          dragGesture = false;
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::TapAndHold) {
        if (gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Right click gesture");
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 4 /* RightButton */);
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::Tap) {
        if(gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Click gesture");
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 1 /* LeftButton */);
          handlePointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }
    }
    return false;
  });

  for (auto type : gestureRecognizers.keys()) {
    grabGesture(type);
    vlog.debug("QGestureRecognizer::registerRecognizer type=%d", type);
  }

  connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
          &Viewport::handleClipboardChange);

  // We need to intercept keyboard events early
  QAbstractEventDispatcher::instance()->installNativeEventFilter(this);

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

  delayedInitializeTimer->setInterval(1000);
  delayedInitializeTimer->setSingleShot(true);
  connect(delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    cc->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer->start();

  mousePointerTimer->setInterval(::pointerEventInterval);
  mousePointerTimer->setSingleShot(true);
  connect(mousePointerTimer, &QTimer::timeout, this,
          &Viewport::handlePointerTimeout);

#ifdef QT_DEBUG
  gettimeofday(&fpsLast, nullptr);
  fpsTimer.start(5000);
#endif

  connect(
      this,
      &Viewport::bufferResized,
      this,
      [=]() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        repaint();
      },
      Qt::QueuedConnection);

  setMouseTracking(true);
}

Viewport::~Viewport()
{
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);

  OptionsDialog::removeCallback(handleOptions);

  delete cursor;

  delete keyboard;

  for (auto gr : gestureRecognizers.keys()) {
    QGestureRecognizer::unregisterRecognizer(gr);
  }
}

// Copy the areas of the framebuffer that have been changed (damaged)
// to the displayed window.
void Viewport::updateWindow()
{
  // copied from DesktopWindow.cxx.
  if (firstUpdate) {
    if (cc->server.supportsSetDesktopSize) {
      emit remoteResizeRequest();
    }
    firstUpdate = false;
  }

  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());
  rfb::Rect rect = framebuffer->getDamage();
  int x = rect.tl.x;
  int y = rect.tl.y;
  int w = rect.br.x - x;
  int h = rect.br.y - y;
  if (!rect.is_empty()) {
    damage += QRect(x, y, w, h);
    update(QRect(x, y, w, h));
  }
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

  if ((ledState & rfb::ledCapsLock) != (cc->server.ledState() & rfb::ledCapsLock)) {
    vlog.debug("Inserting fake CapsLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x3a, XK_Caps_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledNumLock) != (cc->server.ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x45, XK_Num_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledScrollLock) != (cc->server.ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    sendKeyPress(FAKE_KEY_CODE, 0x46, XK_Scroll_Lock);
    sendKeyRelease(FAKE_KEY_CODE);
  }
}

void Viewport::resize(int width, int height)
{
  vlog.debug("Viewport::resize size=(%d, %d)", width, height);
  if (this->width() == width && this->height() == height) {
    vlog.debug("Viewport::resize ignored");
    return;
  }
  QWidget::resize(width, height);
}

void Viewport::resizeFramebuffer(int new_w, int new_h)
{
  pixmap = QPixmap(new_w, new_h);
  damage = QRegion(0, 0, pixmap.width(), pixmap.height());
  vlog.debug("Viewport::bufferResized pixmapSize=(%d, %d) size=(%d, %d)",
              pixmap.size().width(), pixmap.size().height(), width(), height());
  emit bufferResized(width(), height(), new_w, new_h);
  resize(new_w, new_h);
}

void Viewport::paintEvent(QPaintEvent* event)
{
  PlatformPixelBuffer* framebuffer = static_cast<PlatformPixelBuffer*>(cc->framebuffer());

  if ((framebuffer->width() != pixmap.width()) || (framebuffer->height() != pixmap.height())) {
    update();
    return;
  }

  if (!damage.isEmpty()) {
    QPainter pixmapPainter(&pixmap);
    const uint8_t* fbdata;
    int stride;
    QRect bounds = damage.boundingRect();
    int x = bounds.x();
    int y = bounds.y();
    int w = bounds.width();
    int h = bounds.height();
    rfb::Rect rfbrect(x, y, x + w, y + h);

    if (rfbrect.enclosed_by(framebuffer->getRect())) {
      fbdata = framebuffer->getBuffer(rfbrect, &stride);
      QImage image(fbdata, w, h, stride * 4, QImage::Format_RGB32);
#ifdef __APPLE__
      pixmapPainter.fillRect(bounds, QColor("#ff000000"));
      pixmapPainter.setCompositionMode(QPainter::CompositionMode_Plus);
#endif
      pixmapPainter.drawImage(bounds, image);
    }
    damage = QRegion();
  }

  QPainter painter(this);
  QRect r = event->rect();

  painter.drawPixmap(r, pixmap, r);

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

  painter.setBrush(Qt::NoBrush);
  painter.setPen(Qt::red);
  painter.drawRect(rect());
#endif

  setAttribute(Qt::WA_OpaquePaintEvent, true);
}

#ifdef QT_DEBUG
void Viewport::handleTimeout(rfb::Timer* t)
{
  struct timeval now;
  int count;

  EmulateMB::handleTimeout(t);

  if (t != &fpsTimer)
    return;

  gettimeofday(&now, nullptr);
  count = fpsCounter;

  fpsValue = int(count * 1000.0 / rfb::msSince(&fpsLast));

  vlog.info("%d frames in %g seconds = %d FPS", count, rfb::msSince(&fpsLast) / 1000.0, fpsValue);

  fpsCounter -= count;
  fpsLast = now;

  damage += fpsRect;
  update(damage);

  t->repeat();
}
#endif

void Viewport::mouseEvent(QMouseEvent* event)
{
  int buttonMask;

  // FIXME: Is this really needed?
  if(event->source() != Qt::MouseEventNotSynthesized)
    return;

  event->accept();

  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton)
    buttonMask |= 1;
  if (event->buttons() & Qt::MiddleButton)
    buttonMask |= 2;
  if (event->buttons() & Qt::RightButton)
    buttonMask |= 4;

  handlePointerEvent(rfb::Point(event->x(), event->y()), buttonMask);
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
  handlePointerEvent(rfb::Point(event->position().x(),
                                event->position().y()),
                     buttonMask | wheelMask);

  handlePointerEvent(rfb::Point(event->position().x(),
                                event->position().y()),
                     buttonMask);
}

void Viewport::focusInEvent(QFocusEvent* event)
{
  vlog.debug("Viewport::focusInEvent");

  flushPendingClipboard();

  // We may have gotten our lock keys out of sync with the server
  // whilst we didn't have focus. Try to sort this out.
  vlog.debug("KeyboardHandler::pushLEDState");
  pushLEDState();

  // Resend Ctrl/Alt if needed
  if (menuCtrlKey)
    sendKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
  if (menuAltKey)
    sendKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);

  QWidget::focusInEvent(event);
#ifdef __APPLE__
  vlog.debug("cocoa_update_window_level hasFocus=%d", hasFocus());
  if (hasFocus()) {
    bool shielding = ::fullscreenSystemKeys && ((DesktopWindow*)window())->allowKeyboardGrab();
    cocoa_update_window_level(window(),((DesktopWindow*)window())->isFullscreenEnabled(), shielding);
  }
#endif
}

void Viewport::focusOutEvent(QFocusEvent* event)
{
  vlog.debug("Viewport::focusOutEvent");
  // We won't get more key events, so reset our knowledge about keys
  resetKeyboard();

  QWidget::focusOutEvent(event);
#ifdef __APPLE__
  vlog.debug("cocoa_update_window_level hasFocus=%d", hasFocus());
  if (!hasFocus()) {
    cocoa_update_window_level(window(), false);
  }
#endif
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
    return gestureEvent(reinterpret_cast<QGestureEvent*>(event));
  default:
    break;
  }
  return QWidget::event(event);
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
    if (!mousePointerTimer->isActive()) {
      mousePointerTimer->start();
    }
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

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText())
    return;

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


bool Viewport::gestureEvent(QGestureEvent* event)
{
  vlog.debug("Viewport::gestureEvent");

  if (::viewOnly) {
    return true;
  }

  static QMap<QTapGesture*, bool> tapGestures;
  for(auto g : event->gestures()) {
    vlog.debug("Viewport::gestureEvent: %d %s",
               g->gestureType(),
               QVariant::fromValue(g->state()).toString().toStdString().c_str());
  }

  for (auto gr : gestureRecognizers) {
    if (gr.second(event))
      return true;
  }

  return true;
}


void Viewport::registerGesture(QGestureRecognizer *gr, GestureCallbackWithType cb)
{
  auto type = QGestureRecognizer::registerRecognizer(gr);
  gestureRecognizers.insert(type, QPair<QGestureRecognizer*, GestureCallback>(gr, std::bind(cb, type, std::placeholders::_1)));
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
            ((DesktopWindow*)window())->fullscreen(checked);
          });
  action->setCheckable(true);
  action->setChecked(((DesktopWindow*)window())->isFullscreenEnabled());
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
            if (((DesktopWindow*)window())->isFullscreenEnabled())
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
            OptionsDialog* dlg = new OptionsDialog(isFullScreen(),
                                                   window());
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "Connection &info..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            QMessageBox* dlg;
            dlg = new QMessageBox(QMessageBox::Information,
                                  _("VNC connection info"),
                                  cc->connectionInfo(),
                                  QMessageBox::Close, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
          });
  contextMenu->addAction(action);

  action = new QAction(p_("ContextMenu|", "About &TigerVNC viewer..."),
                       contextMenu);
  connect(action, &QAction::triggered, this,
          [this]() {
            about_vncviewer(this);
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
