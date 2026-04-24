/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __DESKTOPWINDOW_H__
#define __DESKTOPWINDOW_H__

#include <QTimer>
#include <QWidget>

#include <rfb/Rect.h>

namespace rfb {
  class PixelFormat;
}

class CConn;
class ScrollArea;
class Viewport;
class Toast;

class DesktopWindow : public QWidget {
  Q_OBJECT

public:
  DesktopWindow(int w, int h, const char *name,
                CConn* cc, QWidget* parent=nullptr);
  ~DesktopWindow();

  // Most efficient format (from DesktopWindow's point of view)
  const rfb::PixelFormat &getPreferredPF();

  // Flush updates to screen
  void updateWindow();

  // Updated session title
  void setName(const char *name);

  // Resize the current framebuffer, but retain the contents
  void resizeFramebuffer(int new_w, int new_h);

  // A previous call to writeSetDesktopSize() has completed
  void setDesktopSizeDone(unsigned result);

  // New image for the locally rendered cursor
  void setCursor(int width, int height, const rfb::Point& hotspot,
                 const uint8_t* pixels);

  // Server-provided cursor position
  void setCursorPos(const rfb::Point& pos);

  // Change client LED state
  void setLEDState(unsigned int state);

  // Clipboard events
  void handleClipboardRequest();
  void handleClipboardAnnounce(bool available);
  void handleClipboardData(const char* text);

  // QWidget methods (poorly overridden)
  void resize(int w, int h);
  void resize(const QSize& size);

  bool isFullScreen() const;
  void setFullScreen(bool enabled);

protected:
  // Qt event handlers
  void moveEvent(QMoveEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void changeEvent(QEvent* e) override;
  void fullScreenEvent();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEvent* event) override;
#else
  void enterEvent(QEnterEvent* event) override;
#endif
  void leaveEvent(QEvent* event) override;
  void mouseMoveEvent();
  void mouseReleaseEvent();
  void showEvent(QShowEvent* event) override;
  void exposeEvent();
  void closeEvent(QCloseEvent* e) override;

  bool eventFilter(QObject* obj, QEvent* event) override;

private:
  void menuOverlay();

  void setOverlay(const char *text, ...)
    __attribute__((__format__ (__printf__, 2, 3)));

  void maybeGrabKeyboard();
  void grabKeyboard();
  void ungrabKeyboard();
  void grabPointer();
  void ungrabPointer();

  void handleActiveChanged();

  void handleResizeTimeout();
  void reconfigureFullscreen();
  void remoteResize();

  static void handleOptions(void *data);

  void handleFullscreenTimeout();

private:
  CConn* cc;
  ScrollArea* scrollArea;
  Viewport *viewport;
  Toast* toast;

  bool firstUpdate;
  bool delayedFullscreen;
  bool sentDesktopSize;
  QTimer* fullscreenTimer;

  bool pendingRemoteResize;
  struct timeval lastResize;
  QTimer* resizeTimer;

  bool fakeFullScreen;
  QByteArray previousGeometry;

  bool keyboardGrabbed;
  bool mouseGrabbed;
};

#endif
