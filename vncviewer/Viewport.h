/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2023 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <core/Rect.h>

#include <FL/Fl_Widget.H>

#include "EmulateMB.h"
#include "Keyboard.h"

class Fl_Menu_Button;
class Fl_RGB_Image;

class CConn;
class Keyboard;
class PlatformPixelBuffer;
class Surface;

class Viewport : public Fl_Widget, public core::Object,
                 protected EmulateMBHandler,
                 protected KeyboardHandler {
public:

  Viewport(int w, int h, CConn* cc_);
  ~Viewport();

  // Most efficient format (from Viewport's point of view)
  const rfb::PixelFormat &getPreferredPF();

  // New image for the locally rendered cursor
  void setCursor();

  void draw(Surface* dst);

  // Fl_Widget callback methods

  void draw() override;

  void resize(int x, int y, int w, int h) override;

  int handle(int event) override;

protected:
  void sendPointerEvent(const core::Point& pos,
                        uint16_t buttonMask) override;

private:
  bool hasFocus();

  // Show the currently set (or system) cursor
  void showCursor();

  void updateWindow();

  static void handleClipboardChange(int source, void *data);
  void handleClipboardRequest();
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* data);

  void flushPendingClipboard();

  void handlePointerEvent(const core::Point& pos, uint16_t buttonMask);
  static void handlePointerTimeout(void *data);

  void resetKeyboard();

  void handleKeyPress(int systemKeyCode,
                      uint32_t keyCode, uint32_t keySym) override;
  void handleKeyRelease(int systemKeyCode) override;

  static int handleSystemEvent(void *event, void *data);

  void handleLEDState();
  void pushLEDState();

  void initContextMenu();
  void popupContextMenu();

  void setMenuKey();

private:
  CConn* cc;

  PlatformPixelBuffer* frameBuffer;

  core::Timer updateTimer;

  core::Point lastPointerPos;
  uint16_t lastButtonMask;

  Keyboard* keyboard;

  EmulateMB emulateMB;

  bool firstLEDState;

  bool pendingClientClipboard;

  int clipboardSource;

  uint32_t menuKeySym;
  int menuKeyCode, menuKeyFLTK;
  Fl_Menu_Button *contextMenu;

  bool menuCtrlKey;
  bool menuAltKey;

  Fl_RGB_Image *cursor;
  core::Point cursorHotspot;
  bool cursorIsBlank;
};

#endif
