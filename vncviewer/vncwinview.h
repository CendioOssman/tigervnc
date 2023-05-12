#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include <windows.h>
#include <map>
#include "abstractvncview.h"

//#define WIN_LEGACY_TOUCH 1
#if WIN_LEGACY_TOUCH // Not necessary in Qt.
class Win32TouchHandler;
#endif
class QTimer;

class QVNCWinView : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCWinView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Window);
  virtual ~QVNCWinView();

  void setWindow(HWND);
  void postMouseMoveEvent(int x, int y, int mask);
  bool hasFocus() const;
  qulonglong nativeWindowHandle() const override;

public slots:
  void setQCursor(const QCursor &cursor) override;
  void setCursorPos(int x, int y) override;
  void setLEDState(unsigned int state) override;
  void pushLEDState() override;
  void handleKeyPress(int keyCode, quint32 keySym) override;
  void handleKeyRelease(int keyCode) override;
  void maybeGrabKeyboard() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void bell() override;
  void moveView(int x, int y) override;
  void updateWindow() override;

protected:
  static LRESULT CALLBACK eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void getMouseProperties(WPARAM wParam, LPARAM lParam, int &x, int &y, int &buttonMask, int &wheelMask);
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  HWND createWindow(HWND parent, HINSTANCE instance);
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;

private:
  void *m_wndproc;
  bool m_hwndowner;
  HWND m_hwnd;

  bool m_altGrArmed;
  unsigned int m_altGrCtrlTime;
  QTimer *m_altGrCtrlTimer;

  HCURSOR m_cursor;
  bool m_mouseTracking;
  HCURSOR m_defaultCursor;

#if WIN_LEGACY_TOUCH // Not necessary in Qt.
  Win32TouchHandler *m_touchHandler;
#endif

  void fixParent();
  friend void *getWindowProc(QVNCWinView *host);
  void resolveAltGrDetection(bool isAltGrSequence);
  int handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  int handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
  void startMouseTracking();
  void stopMouseTracking();
  void refresh(HWND hWnd);
};

#endif // VNCWINVIEW_H
