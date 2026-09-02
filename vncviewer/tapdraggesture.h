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

  QPoint getPosition() const;

  QPoint getStartPosition() const;

private:
  Type type = Undefined;

  QPoint position;
  QPoint startPosition;

  int timerId;

  friend class QTapDragGestureRecognizer;
};

#endif // ZOOMGESTURE_H
