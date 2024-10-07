#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>
#include <QScrollBar>

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
  DesktopWindow(QVNCConnection* cc, QWidget* parent = nullptr);
  virtual ~DesktopWindow();
  void updateMonitorsFullscreen();

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

  void showToast();
  void setWidget(Viewport* w);
  QWidget* takeWidget();

  void postDialogClosing();

signals:
  void fullscreenChanged(bool enabled);
  void closed();

protected:
  void moveEvent(QMoveEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void changeEvent(QEvent* e) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void closeEvent(QCloseEvent* e) override;
  bool event(QEvent* event) override;

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
