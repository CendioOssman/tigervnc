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

// clang-format off
// QEvent must be included before X headers to avoid symbol comflicts.
#include <QEvent>
// QAction must be included before X headers to avoid symbol comflicts.
#include <QAction>
// clang-format on

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QMenu>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QMimeData>
#include <QGestureRecognizer>
#include <QGesture>

#include <rfb/CMsgWriter.h>
#include <rfb/LogWriter.h>
#include <rfb/Exception.h>
#include <rfb/ServerParams.h>
#include <rfb/ledStates.h>
#include <rfb/util.h>

#define XK_LATIN1
#define XK_MISCELLANY
#define XK_XKB_KEYS
#include <rfb/keysymdef.h>

#include "Viewport.h"
#include "CConn.h"
#include "OptionsDialog.h"
#include "DesktopWindow.h"
#include "Keyboard.h"
#ifdef __APPLE__
#include "KeyboardMacOS.h"
#endif
#include "EmulateMB.h"
#include "i18n.h"
#include "locale.h"
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

#undef KeyPress

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

Viewport::Viewport(CConn* cc_, QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f)
  , cc(cc_)
  , firstUpdate(true)
  , delayedInitializeTimer(new QTimer(this))
  , lastButtonMask(0)
  , mousePointerTimer(new QTimer(this))
  , keyboard(nullptr)
  , firstLEDState(true)
  , pendingClientClipboard(false)
  , contextMenu(nullptr)
  , menuCtrlKey(false)
  , menuAltKey(false)
  , cursor(nullptr)
#ifdef QT_DEBUG
  , fpsTimer(this)
#endif
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NativeWindow, true);
  setAttribute(Qt::WA_AcceptTouchEvents);
  setFocusPolicy(Qt::StrongFocus);
  setContentsMargins(0, 0, 0, 0);

  // 257
  registerGesture(new ClicksAlternativeGestureRecognizer, [=](Qt::GestureType type, QGestureEvent* event){
    if (ClicksAlternativeGesture *gesture = static_cast<ClicksAlternativeGesture *>(event->gesture(type))) {
      if (gesture->state() == Qt::GestureFinished) {
        QPoint pos = gesture->getPosition();
        handleKeyRelease(FAKE_GESTURE_KEY_CODE); // Prevents non handling of PanZoomGesture Finished
        if (gesture->getType() == ClicksAlternativeGesture::TwoPoints) {
          vlog.debug("Cendio Right click alternative gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 4 /* RightButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        } else if (gesture->getType() == ClicksAlternativeGesture::ThreePoints) {
          vlog.debug("Cendio Middle click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 2 /* MiddleButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
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
              filterPointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
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
          handleKeyPress(FAKE_GESTURE_KEY_CODE, 0x1d, XK_Control_L);
          if (!panZoomGesture) {
            panZoomGesture = true;
            QTimer::singleShot(100, this, [=](){
              panZoomGesture = false;
              filterPointerEvent(rfb::Point(pos.x(), pos.y()), wheelMask);
              filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
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
        handleKeyRelease(FAKE_GESTURE_KEY_CODE);
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
            filterPointerEvent(rfb::Point(startPos.x(), startPos.y()), 1 /* LeftButton */);
          }
          filterPointerEvent(rfb::Point(pos.x(), pos.y()), 1 /* LeftButton */);
          event->accept();
          return true;
        } else if (gesture->state() == Qt::GestureFinished) {
          dragGesture = false;
          filterPointerEvent(rfb::Point(pos.x(), pos.y()), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::TapAndHold) {
        if (gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Right click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 4 /* RightButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
          event->accept();
          return true;
        }
      }

      if (gesture->getType() == TapDragGesture::Tap) {
        if(gesture->state() == Qt::GestureFinished) {
          vlog.debug("Cendio Click gesture");
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 1 /* LeftButton */);
          filterPointerEvent(remotePointAdjust(rfb::Point(pos.x(), pos.y())), 0);
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

  connect(QGuiApplication::clipboard(), &QClipboard::changed, this, &Viewport::handleClipboardChange);

  delayedInitializeTimer->setInterval(1000);
  delayedInitializeTimer->setSingleShot(true);
  connect(delayedInitializeTimer, &QTimer::timeout, this, [this]() {
    cc->refreshFramebuffer();
    emit delayedInitialized();
  });
  delayedInitializeTimer->start();

  mousePointerTimer->setInterval(::pointerEventInterval);
  mousePointerTimer->setSingleShot(true);
  connect(mousePointerTimer, &QTimer::timeout, this, [this]() {
    try {
      cc->writer()->writePointerEvent(lastPointerPos, lastButtonMask);
    } catch (rdr::Exception& e) {
      abort_connection_unexpected(e);
    }
  });

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
  delete cursor;

  removeKeyboardHandler();

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
    update(localRectAdjust(QRect(x, y, w, h)));
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
  bool emptyCursor = true;
  for (int i = 0; i < width * height; i++) {
    if (pixels[i*4 + 3] != 0) {
      emptyCursor = false;
      break;
    }
  }
  if (emptyCursor) {
    if (::dotWhenNoCursor) {
      delete cursor;
      cursor = new QCursor(QPixmap(dotcursor_xpm), 2, 2);
    }
    else {
      static const char * emptycursor_xpm[] = {
        "2 2 1 1",
        ".	c None",
        "..",
        ".."};
      delete cursor;
      cursor = new QCursor(QPixmap(emptycursor_xpm), 0, 0);
    }
  }
  else {
    QImage image(pixels, width, height, QImage::Format_RGBA8888);
    delete cursor;
    cursor = new QCursor(QPixmap::fromImage(image), hotspot.x, hotspot.y);
  }
  QWidget::setCursor(*cursor);
}

void Viewport::setCursorPos(const rfb::Point& pos)
{
  vlog.debug("Viewport::setCursorPos mouseGrabbed=%d", mouseGrabbed);
  if (!mouseGrabbed) {
    // Do nothing if we do not have the mouse captured.
    return;
  }
  QPoint gp = mapToGlobal(localPointAdjust(QPoint(pos.x, pos.y)));
  vlog.debug("Viewport::setCursorPos local x=%d y=%d", pos.x, pos.y);
  vlog.debug("Viewport::setCursorPos screen x=%d y=%d", gp.x(), gp.y());
  QCursor::setPos(gp.x(), gp.y());
}

void Viewport::handleClipboardRequest()
{
  vlog.debug("Viewport::handleClipboardRequest: %s", pendingClientData.toStdString().c_str());
  vlog.debug("Sending clipboard data (%d bytes)", (int)pendingClientData.size());
  cc->sendClipboardData(pendingClientData.toStdString().c_str());
  pendingClientData = "";
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
  pendingClientData = "";

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
  QGuiApplication::clipboard()->setText(cbdata);
#if !defined(WIN32) && !defined(__APPLE__)
  if (setPrimary)
    QGuiApplication::clipboard()->setText(cbdata, QClipboard::Mode::Selection);
#endif
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
    handleKeyPress(FAKE_KEY_CODE, 0x3a, XK_Caps_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledNumLock) != (cc->server.ledState() & rfb::ledNumLock)) {
    vlog.debug("Inserting fake NumLock to get in sync with server");
    handleKeyPress(FAKE_KEY_CODE, 0x45, XK_Num_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
  }
  if ((ledState & rfb::ledScrollLock) != (cc->server.ledState() & rfb::ledScrollLock)) {
    vlog.debug("Inserting fake ScrollLock to get in sync with server");
    handleKeyPress(FAKE_KEY_CODE, 0x46, XK_Scroll_Lock);
    handleKeyRelease(FAKE_KEY_CODE);
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

void Viewport::giveKeyboardFocus()
{
  vlog.debug("Viewport::giveKeyboardFocus");
  if (qApp->activeModalWidget()) {
    vlog.debug("Viewport::giveKeyboardFocus activeModalWidget=%s", qApp->activeModalWidget()->metaObject()->className());
  }
  if (keyboard && !qApp->activeModalWidget()) {
    installKeyboardHandler();

    flushPendingClipboard();

    // We may have gotten our lock keys out of sync with the server
    // whilst we didn't have focus. Try to sort this out.
    vlog.debug("KeyboardHandler::pushLEDState");
    pushLEDState();

    // Resend Ctrl/Alt if needed
    if (menuCtrlKey)
      handleKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
    if (menuAltKey)
      handleKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
  }
}

QPoint Viewport::localPointAdjust(QPoint p)
{
  p.rx() += (width() - pixmap.width()) / 2;
  p.ry() += (height() - pixmap.height()) / 2;
  return p;
}

QRect Viewport::localRectAdjust(QRect r)
{
  return r.adjusted((width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2,
                    (width() - pixmap.width()) / 2,
                    (height() - pixmap.height()) / 2);
}

QRect Viewport::remoteRectAdjust(QRect r)
{
  return r.adjusted(-(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2,
                    -(width() - pixmap.width()) / 2,
                    -(height() - pixmap.height()) / 2);
}

rfb::Point Viewport::remotePointAdjust(const rfb::Point& pos)
{
  return rfb::Point(pos.x - (width() - pixmap.width()) / 2, pos.y - (height() - pixmap.height()) / 2);
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

  painter.drawPixmap(r, pixmap, remoteRectAdjust(r));

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

void Viewport::getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask)
{
  buttonMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }

  x = event->x();
  y = event->y();
}

void Viewport::getMouseWheelProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask)
{
  buttonMask = 0;
  wheelMask = 0;
  if (event->buttons() & Qt::LeftButton) {
    buttonMask |= 1;
  }
  if (event->buttons() & Qt::MiddleButton) {
    buttonMask |= 2;
  }
  if (event->buttons() & Qt::RightButton) {
    buttonMask |= 4;
  }
  if (event->angleDelta().y() > 0) {
    wheelMask |= 8;
  }
  if (event->angleDelta().y() < 0) {
    wheelMask |= 16;
  }
  if (event->angleDelta().x() > 0) {
    wheelMask |= 32;
  }
  if (event->angleDelta().x() < 0) {
    wheelMask |= 64;
  }

  x = event->position().x();
  y = event->position().y();
}

void Viewport::mouseMoveEvent(QMouseEvent* event)
{
  if(event->source() != Qt::MouseEventNotSynthesized) {
    return;
  }

  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);
}

void Viewport::mousePressEvent(QMouseEvent* event)
{
  vlog.debug("Viewport::mousePressEvent");

  if(event->source() != Qt::MouseEventNotSynthesized) {
    vlog.debug("!MouseEventNotSynthesized");
    event->accept();
    return;
  }

  if (::viewOnly) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
  vlog.debug("Viewport::mouseReleaseEvent");

  if(event->source() != Qt::MouseEventNotSynthesized) {
    vlog.debug("!MouseEventNotSynthesized");
    event->accept();
    return;
  }

  if (::viewOnly) {
    return;
  }

  setFocus(Qt::FocusReason::MouseFocusReason);

  int x, y, buttonMask;
  getMouseProperties(event, x, y, buttonMask);
  filterPointerEvent(rfb::Point(x, y), buttonMask);
}

void Viewport::wheelEvent(QWheelEvent* event)
{
  vlog.debug("Viewport::wheelEvent");

  int x, y, buttonMask, wheelMask;
  getMouseWheelProperties(event, x, y, buttonMask, wheelMask);
  if (wheelMask) {
    filterPointerEvent(rfb::Point(x, y), buttonMask | wheelMask);
  }
  filterPointerEvent(rfb::Point(x, y), buttonMask);
  event->accept();
}

void Viewport::focusInEvent(QFocusEvent* event)
{
  vlog.debug("Viewport::focusInEvent");
  giveKeyboardFocus();
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
  removeKeyboardHandler();
  QWidget::focusOutEvent(event);
#ifdef __APPLE__
  vlog.debug("cocoa_update_window_level hasFocus=%d", hasFocus());
  if (!hasFocus()) {
    cocoa_update_window_level(window(), false);
  }
#endif
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void Viewport::enterEvent(QEvent *event)
#else
void Viewport::enterEvent(QEnterEvent *event)
#endif
{
  vlog.debug("Viewport::enterEvent");
  maybeGrabPointer();
  QWidget::enterEvent(event);
}

void Viewport::leaveEvent(QEvent *event)
{
  vlog.debug("Viewport::leaveEvent");
  ungrabPointer();
  QWidget::leaveEvent(event);
}

bool Viewport::event(QEvent *event)
{
  switch (event->type()) {
  case QEvent::WindowActivate:
    vlog.debug("Viewport::WindowActivate");
    break;
  case QEvent::WindowDeactivate:
    vlog.debug("Viewport::WindowDeactivate");
    ungrabPointer();
    break;
  case QEvent::CursorChange:
    event->setAccepted(true); // This event must be ignored, otherwise setCursor() may crash.
    return true;
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
  lastPointerPos = remotePointAdjust(pos);
  lastButtonMask = buttonMask;
}

bool Viewport::hasFocus()
{
  return QWidget::hasFocus() || isActiveWindow();
}

void Viewport::handleClipboardChange(QClipboard::Mode mode)
{
  vlog.debug("Viewport::handleClipboardChange: mode=%d", mode);
  vlog.debug("Viewport::handleClipboardChange: text=%s", QGuiApplication::clipboard()->text(mode).toStdString().c_str());
  vlog.debug("Viewport::handleClipboardChange: ownsClipboard=%d", QGuiApplication::clipboard()->ownsClipboard());
  vlog.debug("Viewport::handleClipboardChange: hasText=%d", QGuiApplication::clipboard()->mimeData(mode)->hasText());

  if (!sendClipboard)
    return;

#if !defined(WIN32) && !defined(__APPLE__)
  if (!sendPrimary && (mode == QClipboard::Mode::Selection))
    return;
#endif

  if(mode == QClipboard::Mode::Clipboard && QGuiApplication::clipboard()->ownsClipboard()) {
    return;
  }

  if(mode == QClipboard::Mode::Selection && QGuiApplication::clipboard()->ownsSelection()) {
    return;
  }

  if (!QGuiApplication::clipboard()->mimeData(mode)->hasText()) {
    return;
  }

#ifdef __APPLE__
  if (QGuiApplication::clipboard()->text(mode) == serverReceivedData) {
    serverReceivedData = "";
    return;
  }
#endif

  pendingClientData = QGuiApplication::clipboard()->text(mode);

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

void Viewport::maybeGrabPointer()
{
  vlog.debug("Viewport::maybeGrabPointer");
  if (::fullscreenSystemKeys && ((DesktopWindow*)window())->allowKeyboardGrab() && hasFocus()) {
    grabPointer();
  }
}

void Viewport::grabPointer()
{
  vlog.debug("Viewport::grabPointer");
  activateWindow();
  mouseGrabbed = true;
}

void Viewport::ungrabPointer()
{
  mouseGrabbed = false;
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

void Viewport::initKeyboardHandler()
{
  installKeyboardHandler();
}

void Viewport::installKeyboardHandler()
{
  vlog.debug("Viewport::installKeyboardHandler");
  QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
}

void Viewport::removeKeyboardHandler()
{
  vlog.debug("Viewport::removeNativeEventFilter");
  QAbstractEventDispatcher::instance()->removeNativeEventFilter(this);
}

void Viewport::resetKeyboard()
{
  try {
    cc->releaseAllKeys();
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_connection_unexpected(e);
  }
  if (keyboard)
    keyboard->reset();
}

void Viewport::handleKeyPress(int systemKeyCode,
                              uint32_t keyCode, uint32_t keySym)
{
  static bool menuRecursion = false;
  int menuKeyCode;
  quint32 menuKeySym;
  ::getMenuKey(&menuKeyCode, &menuKeySym);

  // Prevent recursion if the menu wants to send its own
  // activation key.
  if (menuKeySym && (keySym == menuKeySym) && !menuRecursion) {
    menuRecursion = true;
    toggleContextMenu();
    menuRecursion = false;
    return;
  }

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
  if (!contextMenu) {
    QAction* action;

    contextMenu = new QMenu(this);

    action = new QAction(p_("ContextMenu|", "Dis&connect"), contextMenu);
    connect(action, &QAction::triggered, this,
            []() {
              QApplication::quit();
            });
    contextMenu->addAction(action);

    contextMenu->addSeparator();

    action = new QAction(p_("ContextMenu|", "&Full screen"), contextMenu);
    action->setCheckable(true);
    connect(action, &QAction::triggered, this,
            [this](bool checked) {
              ((DesktopWindow*)window())->fullscreen(checked);
            });
    action->setChecked(::fullScreen);
    connect(contextMenu, &QMenu::aboutToShow, this, [=]() {
      action->setChecked(((DesktopWindow*)window())->isFullscreenEnabled());
    });
    contextMenu->addAction(action);

    action = new QAction(p_("ContextMenu|", "Minimi&ze"), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              window()->showMinimized();
            });
    contextMenu->addAction(action);

    action = new QAction(p_("ContextMenu|", "Resize &window to session"), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              window()->resize(pixmapSize().width(), pixmapSize().height());
            });
    contextMenu->addAction(action);

    contextMenu->addSeparator();

    action = new QAction(p_("ContextMenu|", "&Ctrl"), contextMenu);
    action->setCheckable(true);
    connect(action, &QAction::triggered, this,
            [this](bool checked) {
              toggleKey(checked, FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
            });
    contextMenu->addAction(action);

    action = new QAction(p_("ContextMenu|", "&Alt"), contextMenu);
    action->setCheckable(true);
    connect(action, &QAction::triggered, this,
            [this](bool checked) {
              toggleKey(checked, FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
            });
    contextMenu->addAction(action);

    action = new QAction(contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              sendContextMenuKey();
            });
    connect(contextMenu, &QMenu::aboutToShow, this, [=]() {
      std::string text;
      text = rfb::format(p_("ContextMenu|", "Send %s"),
                         ::menuKey.getValueStr().c_str());
      action->setText(text.c_str());
    });
    contextMenu->addAction(action);

    action = new QAction(p_("ContextMenu|", "Send Ctrl-Alt-&Del"), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              sendCtrlAltDel();
            });
    contextMenu->addAction(action);

    contextMenu->addSeparator();

    action = new QAction(p_("ContextMenu|", "&Refresh screen"), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              cc->refreshFramebuffer();
            });
    contextMenu->addAction(action);

    contextMenu->addSeparator();

    action = new QAction(p_("ContextMenu|", "&Options..."), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              OptionsDialog* dlg = new OptionsDialog(isFullScreen(), window());
              dlg->setAttribute(Qt::WA_DeleteOnClose);
              dlg->open();
            });
    contextMenu->addAction(action);

    action = new QAction(p_("ContextMenu|", "Connection &info..."), contextMenu);
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

    action = new QAction(p_("ContextMenu|", "About &TigerVNC viewer..."), contextMenu);
    connect(action, &QAction::triggered, this,
            [this]() {
              about_vncviewer(this);
            });
    contextMenu->addAction(action);

    contextMenu->installEventFilter(this);
  }
}

bool Viewport::isVisibleContextMenu() const
{
  return contextMenu && contextMenu->isVisible();
}

void Viewport::sendContextMenuKey()
{
  vlog.debug("Viewport::sendContextMenuKey");
  if (::viewOnly) {
    return;
  }
  int keyCode;
  quint32 keySym;
  ::getMenuKey(&keyCode, &keySym);
  handleKeyPress(FAKE_KEY_CODE, keyCode, keySym);
  handleKeyRelease(FAKE_KEY_CODE);
  contextMenu->hide();
}

void Viewport::sendCtrlAltDel()
{
  handleKeyPress(FAKE_CTRL_KEY_CODE, 0x1d, XK_Control_L);
  handleKeyPress(FAKE_ALT_KEY_CODE, 0x38, XK_Alt_L);
  handleKeyPress(FAKE_DEL_KEY_CODE, 0xd3, XK_Delete);
  handleKeyRelease(FAKE_DEL_KEY_CODE);
  handleKeyRelease(FAKE_ALT_KEY_CODE);
  handleKeyRelease(FAKE_CTRL_KEY_CODE);
}

void Viewport::toggleKey(bool toggle, int systemKeyCode, quint32 keyCode, quint32 keySym)
{
  if (toggle) {
    handleKeyPress(systemKeyCode, keyCode, keySym);
  } else {
    handleKeyRelease(systemKeyCode);
  }
  if (keySym == XK_Control_L) {
    menuCtrlKey = toggle;
  } else if (keySym == XK_Alt_L) {
    menuAltKey = toggle;
  }
}

void Viewport::toggleContextMenu()
{
  if (isVisibleContextMenu()) {
    contextMenu->hide();
  } else {
    initContextMenu();
    contextMenu->exec(QCursor::pos());
    contextMenu->setFocus();
  }
}

// As QMenu eventFilter
bool Viewport::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* e = static_cast<QKeyEvent*>(event);
    if (isVisibleContextMenu()) {
      QString str = ::getMenuKeyQString();
      if (!str.isEmpty() && QKeySequence(e->key()).toString() == str) {
        sendContextMenuKey();
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}
