#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include "Viewport.h"

#include <windows.h>

class QTimer;

class QVNCWinView : public Viewport
{
  Q_OBJECT

public:
  QVNCWinView(CConn* cc, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);
  virtual ~QVNCWinView();

protected:
  bool event(QEvent* e) override;
};

#endif // VNCWINVIEW_H
