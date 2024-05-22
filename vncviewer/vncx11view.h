#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include "abstractvncview.h"

struct _XDisplay;

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT

public:
  QVNCX11View(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);

public slots:
  void grabPointer() override;
  void ungrabPointer() override;
  void bell() override;

signals:
  void message(const QString& msg, int timeout);

private:
  _XDisplay * display;
};

#endif // VNCX11VIEW_H
