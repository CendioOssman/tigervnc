/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2021 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __VIEWPORT_H__
#define __VIEWPORT_H__

#include <functional>

#include <rfb/Rect.h>
#include <rfb/Timer.h>

#include <QAbstractNativeEventFilter>
#include <QClipboard>
#include <QMap>
#include <QWidget>

#include "EmulateMB.h"
#include "Keyboard.h"

class QCursor;
class QGestureEvent;
class QGestureRecognizer;
class QMenu;

class CConn;
class Keyboard;

class Viewport : public QWidget, protected EmulateMB,
                 protected QAbstractNativeEventFilter,
                 protected KeyboardHandler {
  Q_OBJECT

public:
  Viewport(CConn* cc, QWidget* parent=nullptr);
  ~Viewport();

  QSize pixmapSize() const { return pixmap.size(); };

  // Flush updates to screen
  void updateWindow();

  // New image for the locally rendered cursor
  void setCursor(int width, int height, const rfb::Point& hotspot,
                 const uint8_t* data);

  // Change client LED state
  void setLEDState(unsigned int state);

  // Clipboard events
  void handleClipboardRequest();
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);

  void resize(int width, int height);
  void resizeFramebuffer(int new_w, int new_h);

protected:
  // Qt event handlers
  void paintEvent(QPaintEvent* event) override;
#ifdef QT_DEBUG
  void handleTimeout(rfb::Timer* t) override;
#endif
  void mouseEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  bool event(QEvent* event) override;

  void sendPointerEvent(const rfb::Point& pos, uint8_t buttonMask) override;

private:
  void handleClipboardChange(QClipboard::Mode mode);

  void flushPendingClipboard();

  void handlePointerEvent(const rfb::Point& pos, uint8_t buttonMask);
  void handlePointerTimeout();

  typedef std::function<bool(QGestureEvent*)> GestureCallback;
  typedef std::function<bool(Qt::GestureType, QGestureEvent*)> GestureCallbackWithType;
  QMap<Qt::GestureType, QPair<QGestureRecognizer*, GestureCallback>> gestureRecognizers;
  bool gestureEvent(QGestureEvent *event);
  void registerGesture(QGestureRecognizer* gr, GestureCallbackWithType cb);

  void resetKeyboard();

  void handleKeyPress(int systemKeyCode,
                      uint32_t keyCode, uint32_t keySym) override;
  void sendKeyPress(int systemKeyCode,
                    uint32_t keyCode, uint32_t keySym);
  void handleKeyRelease(int systemKeyCode) override;
  void sendKeyRelease(int systemKeyCode);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  bool nativeEventFilter(QByteArray const& eventType, void* message, long*) override;
#else
  bool nativeEventFilter(QByteArray const& eventType, void* message, qintptr*) override;
#endif

  void pushLEDState();

  void initContextMenu();
  void popupContextMenu();

  void setMenuKey();

  static void handleOptions(void *data);

private:
  CConn* cc;

  QPixmap pixmap;
  QRegion damage;

  rfb::Point lastPointerPos;
  uint8_t lastButtonMask;
  QTimer* mousePointerTimer;

  Keyboard* keyboard;

  bool firstLEDState;

  bool pendingClientClipboard;

  QClipboard::Mode clipboardMode;
#ifdef __APPLE__
  QString serverReceivedData;
#endif

  uint32_t menuKeySym;
  int menuKeyCode, menuKeyQt;
  QMenu* contextMenu;

  bool menuCtrlKey;
  bool menuAltKey;

  QCursor* cursor;

#ifdef QT_DEBUG
  QAtomicInt fpsCounter;
  int fpsValue = 0;
  QRect fpsRect = {10, 10, 100, 20};
  struct timeval fpsLast;
  rfb::Timer fpsTimer;
#endif
};

#endif
