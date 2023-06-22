#ifndef VNCGESTURERECOGNIZER_H
#define VNCGESTURERECOGNIZER_H

#include <QGestureRecognizer>

class QVNCGestureRecognizer : public QGestureRecognizer
{
public:
  QVNCGestureRecognizer();
  virtual ~QVNCGestureRecognizer();
  QGesture *create(QObject *target) override;
  QGestureRecognizer::Result recognize(QGesture *gesture, QObject *watched, QEvent *event) override;
  void reset(QGesture *gesture) override;
};

#endif // VNCGESTURERECOGNIZER_H
