#ifndef TAPDRAGGESTURE_H
#define TAPDRAGGESTURE_H

#include <QGesture>

class TapDragGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type {
    Undefined,
    Tap,
    Drag,
    TapAndHold
  };

  TapDragGesture();

  Type getType() const;

  QPointF getPosition() const;

  QPointF getStartPosition() const;

private:
  Type type = Undefined;

  QPointF position;
  QPointF startPosition;

  int timerId;

  friend class TapDragGestureRecognizer;
};

#endif // ZOOMGESTURE_H
