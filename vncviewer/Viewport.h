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

#include <QObject>

#include <rfb/Rect.h>

#include <FL/Fl_Widget.H>

#include "EmulateMB.h"
#include "Keyboard.h"
#include "GestureHandler.h"

class QMenu;

class Fl_RGB_Image;

namespace rfb {
  class PixelFormat;
}

class BaseTouchHandler;
class CConn;
class Keyboard;
class PlatformPixelBuffer;
class Surface;

class Viewport : public QObject, public Fl_Widget, protected EmulateMB,
                 protected KeyboardHandler,
                 protected GestureCallback {
  Q_OBJECT

public:

  Viewport(int w, int h, CConn* cc_);
  ~Viewport();

  // Most efficient format (from Viewport's point of view)
  const rfb::PixelFormat &getPreferredPF();

  // Flush updates to screen
  void updateWindow();

  // New image for the locally rendered cursor
  void setCursor(int width, int height, const rfb::Point& hotspot,
                 const uint8_t* data);

  // Change client LED state
  void setLEDState(unsigned int state);

  void draw(Surface* dst);

  // Clipboard events
  void handleClipboardRequest();
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);

protected:
  // Fl_Widget callback methods

  void draw() override;

  void resize(int x, int y, int w, int h) override;

  int pasteEvent();
  int enterEvent();
  int leaveEvent();
  int mouseEvent();
  int wheelEvent();
  int focusInEvent();
  int focusOutEvent();
  int handle(int event) override;

  void handleGestureEvent(const GestureEvent& event) override;
  void handleTapGesture(const GestureEvent& ev);
  void handleLongPressGesture(const GestureEvent& ev);
  void handleDragGesture(const GestureEvent& ev);
  void handlePinchGesture(const GestureEvent& ev);

  void sendPointerEvent(const rfb::Point& pos, uint8_t buttonMask) override;

private:
  bool hasFocus();

  static void handleClipboardChange(int source, void *data);

  void flushPendingClipboard();

  void handlePointerEvent(const rfb::Point& pos, uint8_t buttonMask);
  static void handlePointerTimeout(void *data);

  void resetKeyboard();

  void handleKeyPress(int systemKeyCode,
                      uint32_t keyCode, uint32_t keySym) override;
  void sendKeyPress(int systemKeyCode,
                    uint32_t keyCode, uint32_t keySym);
  void handleKeyRelease(int systemKeyCode) override;
  void sendKeyRelease(int systemKeyCode);

  static int handleSystemEvent(void *event, void *data);

  void pushLEDState();

  void initContextMenu();
  void popupContextMenu();

  void setMenuKey();

  static void handleOptions(void *data);

private:
  CConn* cc;

  PlatformPixelBuffer* frameBuffer;

  rfb::Point lastPointerPos;
  uint8_t lastButtonMask;

  double lastMagnitudeX;
  double lastMagnitudeY;

  GestureEvent firstDoubleTapEvent;
  struct timeval lastTapTime;

  Keyboard* keyboard;
  BaseTouchHandler* touch;

  bool firstLEDState;

  bool pendingClientClipboard;

  int clipboardSource;

  uint32_t menuKeySym;
  int menuKeyCode, menuKeyQt;
  QMenu* contextMenu;

  bool menuCtrlKey;
  bool menuAltKey;

  Fl_RGB_Image *cursor;
  rfb::Point cursorHotspot;
};

#endif
