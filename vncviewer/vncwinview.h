#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include <windows.h>
#include <map>
#include "abstractvncview.h"

class QTimer;
class QMutex;

namespace rfb {
  class Rect;
}

using DownMap = std::map<int, quint32>;

class QVNCWinView : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCWinView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCWinView();

  void setWindow(HWND);
  HWND window() const;
  void clearPendingMouseMoveEvent();
  void postMouseMoveEvent(int x, int y);

protected:
  static LRESULT CALLBACK eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  HWND createWindow(HWND parent, HINSTANCE instance);
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;

public slots:
  void returnPressed();
  void refresh(HWND hWnd, bool all = true);
  void handleKeyPress(int keyCode, quint32 keySym) override;
  void handleKeyRelease(int keyCode) override;
  void setCursor(int width, int height, int hotX, int hotY, const unsigned char *data) override;
  void setCursorPos(int x, int y) override;
  void pushLEDState() override;
  void setLEDState(unsigned int state) override;
  void handleClipboardData(const char* data) override;
  void maybeGrabKeyboard() override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void grabPointer() override;
  void ungrabPointer() override;
  bool isFullscreen() override;
  void bell() override;

private:
  void fixParent();
  friend void *getWindowProc(QVNCWinView *host);
  void resolveAltGrDetection(bool isAltGrSequence);
  int handleKeyDownEvent(UINT message, WPARAM wParam, LPARAM lParam);
  int handleKeyUpEvent(UINT message, WPARAM wParam, LPARAM lParam);
  void startMouseTracking();
  void stopMouseTracking();

  void *m_wndproc;
  bool m_hwndowner;
  HWND m_hwnd;
  QMutex *m_mutex;
  bool m_pendingMouseMoveEvent;
  int m_mouseX;
  int m_mouseY;
  QTimer *m_mouseMoveEventTimer;

  bool m_altGrArmed;
  unsigned int m_altGrCtrlTime;
  quint32 m_menuKeySym;
  DownMap m_downKeySym;
  QTimer *m_altGrCtrlTimer;

  HCURSOR m_cursor;
  bool m_mouseTracking;
  HCURSOR m_defaultCursor;

  const int m_invisibleCursorWidth = 2;
  const int m_invisibleCursorHeight = 2;
  static const unsigned char *m_invisibleCursor;
};

#endif // VNCWINVIEW_H
