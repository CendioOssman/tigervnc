#ifndef VNCMACVIEW_H
#define VNCMACVIEW_H

#include "Viewport.h"

#include <QAbstractNativeEventFilter>

class QWindow;
class QScreen;
class QLabel;
class NSView;
class NSCursor;

class QVNCMacView : public Viewport
{
  Q_OBJECT

public:
  QVNCMacView(QVNCConnection* cc, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCMacView();

public slots:
  void setCursorPos(int x, int y) override;
  void bell() override;

protected:
  bool event(QEvent* e) override;
};

#endif // VNCMACVIEW_H
