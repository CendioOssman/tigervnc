#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include <windows.h>
#include <map>
#include "abstractvncview.h"

class QTimer;
class Win32TouchHandler;

class QVNCWinView : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCWinView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Window);
  virtual ~QVNCWinView();

  void setWindow(HWND);
  bool hasViewFocus() const override;
  qulonglong nativeWindowHandle() const override;
  QRect getExtendedFrameProperties() override;
  void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  void handleKeyRelease(int keyCode) override;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void setCursorPos(int x, int y) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void maybeGrabKeyboard() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void bell() override;
  void moveView(int x, int y) override;
  void updateWindow() override;

protected:
  static LRESULT CALLBACK eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void getMouseProperties(QVNCWinView *window, UINT message, WPARAM wParam, LPARAM lParam, int &x, int &y, int &buttonMask, int &wheelMask);
  bool event(QEvent *e) override;
  HWND createWindow(HWND parent, HINSTANCE instance);
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool bypassWMHintingEnabled() const override { return true; }
#if 0
  void fullscreenOnCurrentDisplay() override;
  void fullscreenOnSelectedDisplay(QScreen *screen) override;
  void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight) override;
#endif

private:
  void *wndproc_;
  bool hwndowner_;
  HWND hwnd_;

  bool altGrArmed_;
  unsigned int altGrCtrlTime_;
  QTimer *altGrCtrlTimer_;

  HCURSOR cursor_;
  bool mouseTracking_;
  HCURSOR defaultCursor_;

  Win32TouchHandler *touchHandler_;

  void fixParent();
  friend void *getWindowProc(QVNCWinView *host);
  void resolveAltGrDetection(bool isAltGrSequence);
  int handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  int handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
  int handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam);
  void startMouseTracking();
  void stopMouseTracking();
  void draw();
};

#endif // VNCWINVIEW_H
