#ifndef PANZOOMGESTURERECOGNIZER_H
#define PANZOOMGESTURERECOGNIZER_H

#include <QGestureRecognizer>

class PanZoomGestureRecognizer : public QGestureRecognizer
{
public:
  QGesture *create(QObject *target) override;
  QGestureRecognizer::Result recognize(QGesture *gesture, QObject *watched, QEvent *event) override;
  void reset(QGesture *gesture) override;
};

#endif // ZOOMGESTURERECOGNIZER_H
