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

  QRect getExtendedFrameProperties() override;
  void handleKeyPress(int keyCode, quint32 keySym, bool menuShortCutMode = false) override;
  void handleKeyRelease(int keyCode) override;

public slots:
  void setCursorPos(int x, int y) override;
  void pushLEDState() override;
  void setLEDState(unsigned int state) override;
  void grabKeyboard() override;
  void ungrabKeyboard() override;
  void bell() override;
  void moveView(int x, int y) override;

protected:
  bool event(QEvent *e) override;
  void showEvent(QShowEvent *) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  bool bypassWMHintingEnabled() const override { return true; }

private:
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
