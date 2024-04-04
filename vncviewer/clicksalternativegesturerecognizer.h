#ifndef RCAGESTURERECOGNIZER_H
#define RCAGESTURERECOGNIZER_H

#include <QGestureRecognizer>

class ClicksAlternativeGestureRecognizer : public QGestureRecognizer
{
public:
  QGesture *create(QObject *target) override;
  QGestureRecognizer::Result recognize(QGesture *gesture, QObject *watched, QEvent *event) override;
  void reset(QGesture *gesture) override;
};

#endif // RCAGESTURERECOGNIZER_H
