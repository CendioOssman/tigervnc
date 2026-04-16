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

#include <rfb/Rect.h>
#include <rfb/Timer.h>

#include <QAbstractNativeEventFilter>
#include <QClipboard>
#include <QLabel>
#include <QMap>
#include <QScrollArea>
#include <QWidget>
#include <functional>

#include "EmulateMB.h"
#include "Keyboard.h"

class QMenu;
class QAction;
class QCursor;
class QLabel;
class QScreen;
class QClipboard;
class QMoveEvent;
class QGestureEvent;
class QVNCToast;
class GestureHandler;
class CConn;
class Keyboard;
class QGestureRecognizer;

namespace rfb
{
struct Point;
}

using DownMap = std::map<int, quint32>;

class Viewport : public QWidget, protected EmulateMB,
                 protected KeyboardHandler,
                 protected QAbstractNativeEventFilter {
  Q_OBJECT

public:
  Viewport(CConn* cc, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  ~Viewport();

  QSize pixmapSize() const { return pixmap.size(); };

  // Flush updates to screen
  void updateWindow();

  // New image for the locally rendered cursor
  void setCursor(int width, int height, const rfb::Point& hotspot,
                 const uint8_t* data);

  virtual void setCursorPos(const rfb::Point& pos);

  // Change client LED state
  void setLEDState(unsigned int state);

  // Clipboard events
  void handleClipboardRequest();
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);

  void resize(int width, int height);
  void resizeFramebuffer(int new_w, int new_h);

  void giveKeyboardFocus();

signals:
  void delayedInitialized();
  void bufferResized(int oldW, int oldH, int w, int h);
  void remoteResizeRequest();

protected:
  QPoint localPointAdjust(QPoint p);
  QRect localRectAdjust(QRect r);
  QRect remoteRectAdjust(QRect r);
  rfb::Point remotePointAdjust(rfb::Point const& pos);

  // Qt event handlers
  void paintEvent(QPaintEvent* event) override;
#ifdef QT_DEBUG
  void handleTimeout(rfb::Timer* t) override;
#endif
  void getMouseProperties(QMouseEvent* event, int& x, int& y, int& buttonMask);
  void getMouseWheelProperties(QWheelEvent* event, int& x, int& y, int& buttonMask, int& wheelMask);
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEvent* event) override;
#else
  void enterEvent(QEnterEvent* event) override;
#endif
  void leaveEvent(QEvent* event) override;
  bool event(QEvent* event) override;

  void sendPointerEvent(const rfb::Point& pos, uint8_t buttonMask) override;

protected:
  bool hasFocus();

  void handleClipboardChange(QClipboard::Mode mode);

  void flushPendingClipboard();

  virtual void maybeGrabPointer();
  virtual void grabPointer();
  virtual void ungrabPointer();

  typedef std::function<bool(QGestureEvent*)> GestureCallback;
  typedef std::function<bool(Qt::GestureType, QGestureEvent*)> GestureCallbackWithType;
  QMap<Qt::GestureType, QPair<QGestureRecognizer*, GestureCallback>> gestureRecognizers;
  bool gestureEvent(QGestureEvent *event);
  void registerGesture(QGestureRecognizer* gr, GestureCallbackWithType cb);

  void initKeyboardHandler();
  void installKeyboardHandler();
  void removeKeyboardHandler();

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
  bool isVisibleContextMenu() const;
  void sendContextMenuKey();
  void sendCtrlAltDel();
  void toggleKey(bool toggle, int systemKeyCode, quint32 keyCode, quint32 keySym);
  void toggleContextMenu();
  // As QMenu eventFilter
  bool eventFilter(QObject* watched, QEvent* event) override;

  void setMenuKey();

  static void handleOptions(void *data);

protected:
  CConn* cc;

  bool firstUpdate;
  QTimer* delayedInitializeTimer;

  QPixmap pixmap;
  QRegion damage;

  bool mouseGrabbed = false;
  rfb::Point lastPointerPos;
  uint8_t lastButtonMask;
  QTimer* mousePointerTimer;

  Keyboard* keyboard;

  bool firstLEDState;

  bool pendingClientClipboard;
  QString pendingClientData;
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
