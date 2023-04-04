#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include "abstractvncview.h"
#include <windows.h>

namespace rfb {
  class Rect;
}

class QVNCX11view : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11view(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11view();

  void setWindow(HWND);
  HWND window() const;

protected:
  static LRESULT CALLBACK eventHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  HWND createWindow(HWND parent, HINSTANCE instance);
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  void showEvent(QShowEvent *);
  void focusInEvent(QFocusEvent*);
  void resizeEvent(QResizeEvent*);

signals:
  void message(const QString &msg, int timeout);

public slots:
  void returnPressed();
  void refresh(HWND hWnd, bool all = true);

private:
  void fixParent();
  friend void *getWindowProc(QVNCX11view *host);

  void *m_wndproc;
  bool m_hwndowner;
  HWND m_hwnd;
  rfb::Rect *m_rect;
};

#endif // VNCX11VIEW_H
