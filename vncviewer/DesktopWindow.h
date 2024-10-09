#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>
#include <QScrollBar>

#include <rfb/Rect.h>
#include <rfb/Timer.h>

class QMoveEvent;
class QResizeEvent;
class Toast;
class Viewport;
class ScrollArea;
class QVNCConnection;

class DesktopWindow : public QWidget
{
  Q_OBJECT

public:
  DesktopWindow(int w, int h, const char *name,
                QVNCConnection* cc, QWidget* parent = nullptr);
  virtual ~DesktopWindow();
  void updateMonitorsFullscreen();

  void setName(const char* name);

  // Fullscreen
  QList<int> fullscreenScreens() const;
  QScreen* getCurrentScreen() const;
  double effectiveDevicePixelRatio(QScreen* screen = nullptr) const;
  void fullscreen(bool enabled);
  void fullscreenOnSelectedDisplay(QScreen* screen);
#ifdef Q_OS_LINUX
  void fullscreenOnSelectedDisplaysIndices(int top, int bottom, int left, int right); // screens indices
#endif
  void fullscreenOnSelectedDisplaysPixels(int vx, int vy, int vwidth, int vheight); // pixels
  void exitFullscreen();
  bool allowKeyboardGrab() const;
  bool isFullscreenEnabled() const;

  // Remote resize
  void handleDesktopSize();
  void postRemoteResizeRequest();
  void remoteResize(int width, int height);
  void fromBufferResize(int oldW, int oldH, int width, int height);

  // Flush updates to screen
  void updateWindow();

  // Resize the current framebuffer, but retain the contents
  void resizeFramebuffer(int new_w, int new_h);

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

  void showToast();

  void postDialogClosing();

signals:
  void fullscreenChanged(bool enabled);

protected:
  void moveEvent(QMoveEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void changeEvent(QEvent* e) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void closeEvent(QCloseEvent* e) override;
  bool event(QEvent* event) override;

  static void handleOptions(void *data);

public:
  void maybeGrabKeyboard();
  void grabKeyboard();
  void ungrabKeyboard();
  void grabPointer();
  void ungrabPointer();
  void handleGrab(rfb::Timer*);

private:
  QVNCConnection* cc;
  Viewport* view;

  QTimer* resizeTimer;
  bool fullscreenEnabled = false;
  bool pendingFullscreen = false;
  double devicePixelRatio;

  QScreen* previousScreen;
  QByteArray previousGeometry;

  rfb::MethodTimer<DesktopWindow> keyboardGrabberTimer;

  Toast* toast;
  ScrollArea* scrollArea = nullptr;
};

#endif // VNCWINDOW_H
