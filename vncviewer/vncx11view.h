#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include "abstractvncview.h"

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT

public:
  QVNCX11View(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Widget);

public slots:
  void bell() override;

signals:
  void message(const QString& msg, int timeout);
};

#endif // VNCX11VIEW_H
