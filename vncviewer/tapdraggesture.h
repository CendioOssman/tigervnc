#ifndef TAPDRAGGESTURE_H
#define TAPDRAGGESTURE_H

#include <QGesture>

class QTapDragGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type {
    Undefined,
    Tap,
    Drag,
    TapAndHold
  };

  QTapDragGesture();

  Type getType() const;

  QPointF getPosition() const;

  QPointF getStartPosition() const;

private:
  Type type = Undefined;

  QPointF position;
  QPointF startPosition;

  int timerId;

  friend class QTapDragGestureRecognizer;
};

#endif // ZOOMGESTURE_H
