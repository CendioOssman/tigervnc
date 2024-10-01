#ifndef VNCWINVIEW_H
#define VNCWINVIEW_H

#include "Viewport.h"

#include <windows.h>

class QTimer;

class QVNCWinView : public Viewport
{
  Q_OBJECT

public:
  QVNCWinView(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Window);
  virtual ~QVNCWinView();

public slots:
  void ungrabKeyboard() override;
  void bell() override;

protected:
  bool event(QEvent* e) override;
};

#endif // VNCWINVIEW_H
