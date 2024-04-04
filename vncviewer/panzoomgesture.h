#ifndef PANZOOMGESTURE_H
#define PANZOOMGESTURE_H

#include <QGesture>

class PanZoomGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type
  {
    Undefined,
    Pinch,
    Pan
  };

  PanZoomGesture();

  Type getType() const;

  qreal getScaleFactor() const;

  QPointF getOffsetDelta() const;

private:
  Type type = Undefined;

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

  friend class PanZoomGestureRecognizer;
};

#endif // ZOOMGESTURE_H
