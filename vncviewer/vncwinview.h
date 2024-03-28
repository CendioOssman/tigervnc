#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include "abstractvncview.h"

#include <map>
#include <windows.h>

class QTimer;
class Win32TouchHandler;

class QVNCWinView : public QAbstractVNCView
{
  Q_OBJECT

public:
  QVNCWinView(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);
  virtual ~QVNCWinView();

public slots:
  void setCursorPos(int x, int y) override;
  void ungrabKeyboard() override;
  void bell() override;

protected:
  bool event(QEvent* e) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;

private:
  bool altGrArmed_;
  unsigned int altGrCtrlTime_;
  QTimer* altGrCtrlTimer_;

  HCURSOR cursor_;
  bool mouseTracking_;
  HCURSOR defaultCursor_;

  Win32TouchHandler* touchHandler_;

  int handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // VNCWINVIEW_H
