#ifndef PANZOOMGESTURE_H
#define PANZOOMGESTURE_H

#include <QGesture>

class QPanZoomGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type
  {
    Undefined,
    Pinch,
    Pan
  };

  QPanZoomGesture();

  Type getType() const;

  qreal getScaleFactor() const;

  QPointF getOffsetDelta() const;

  QPointF getPosition() const;

private:
  Type type = Undefined;

  QPointF position;

  // Pinch
  QPointF startCenterPoint;
  QPointF lastCenterPoint;
  QPointF centerPoint;

  qreal totalScaleFactor;
  qreal lastScaleFactor;
  qreal scaleFactor;

  bool isNewSequence;
  QPointF startPosition[2];

  // Pan
  QPointF lastOffset;
  QPointF offset;
  QPointF delta;

  friend class QPanZoomGestureRecognizer;
};

#endif // ZOOMGESTURE_H
