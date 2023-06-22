#ifndef VNCGESTURE_H
#define VNCGESTURE_H

#include <QGesture>

class QVNCGesture : public QGesture
{
  Q_OBJECT
public:
  QVNCGesture();
  virtual ~QVNCGesture();

  Qt::GestureType gestureType;
  Qt::GestureState state;
  QPointF hotSpot;
  QPointF sceneHotSpot;
  uint isHotSpotSet : 1;
  uint gestureCancelPolicy : 2;
  QPinchGesture::ChangeFlags totalChangeFlags;
  QPinchGesture::ChangeFlags changeFlags;
  QPointF startCenterPoint;
  QPointF lastCenterPoint;
  QPointF centerPoint;
  bool isNewSequence;
  QPointF startPosition[3];
};

#endif // VNCGESTURE_H
