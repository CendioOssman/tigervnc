#ifndef TAPDRAGGESTURERECOGNIZER_H
#define TAPDRAGGESTURERECOGNIZER_H

#include <QGestureRecognizer>

class TapDragGestureRecognizer : public QGestureRecognizer
{
public:
  QGesture *create(QObject *target) override;
  QGestureRecognizer::Result recognize(QGesture *gesture, QObject *watched, QEvent *event) override;
  void reset(QGesture *gesture) override;
};

#endif // ZOOMGESTURERECOGNIZER_H
