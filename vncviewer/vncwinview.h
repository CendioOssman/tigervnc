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

public slots:
  void setCursorPos(int x, int y) override;
  void ungrabKeyboard() override;
  void bell() override;
  void moveView(int x, int y) override;

protected:
  bool event(QEvent *e) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;
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

  int handleTouchEvent(UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // VNCWINVIEW_H
