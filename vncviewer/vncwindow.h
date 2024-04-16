#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>
#include <QScrollBar>

class QMoveEvent;
class QResizeEvent;
class Toast;
class ScrollArea;

class QVNCWindow : public QWidget
{
  Q_OBJECT

public:
  QVNCWindow(QWidget* parent = nullptr);
  virtual ~QVNCWindow();
  void updateScrollbars();

  // Fullscreen
  QList<int> fullscreenScreens() const;
  QScreen* getCurrentScreen() const;
  double effectiveDevicePixelRatio(QScreen* screen = nullptr) const;
  void fullscreen(bool enabled);
  void fullscreenOnSelectedDisplay(QScreen* screen);
#ifdef Q_OS_LINUX
  void fullscreenOnSelectedDisplays(int top, int bottom, int left, int right); // screens indices
#else
  void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight); // pixels
#endif
  void exitFullscreen();
  bool allowKeyboardGrab() const;
  bool isFullscreenEnabled() const;

  // Remote resize
  void handleDesktopSize();
  void postRemoteResizeRequest();
  void remoteResize(int width, int height);
  void fromBufferResize(int oldW, int oldH, int width, int height);

  void showToast();
  void setWidget(QWidget* w);
  QWidget* takeWidget();

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

private:
  QTimer* resizeTimer;
  bool fullscreenEnabled = false;
  bool pendingFullscreen = false;
  double devicePixelRatio;

  QScreen* previousScreen;
  QByteArray previousGeometry;

  Toast* toast;
  ScrollArea* scrollArea = nullptr;
};

#endif // VNCWINDOW_H
