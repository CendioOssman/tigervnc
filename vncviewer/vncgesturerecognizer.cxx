#include <QWidget>
#include "vncgesture.h"
#include "vncgesturerecognizer.h"

QVNCGestureRecognizer::QVNCGestureRecognizer()
{
}

QVNCGestureRecognizer::~QVNCGestureRecognizer()
{
}

QGesture *QVNCGestureRecognizer::create(QObject *target)
{
  if (target && target->isWidgetType()) {
    reinterpret_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
  }
  return new QVNCGesture;
}

QGestureRecognizer::Result QVNCGestureRecognizer::recognize(QGesture *state, QObject *watched, QEvent *event)
{
  QVNCGesture *q = static_cast<QVNCGesture *>(state);
  QGestureRecognizer::Result result = QGestureRecognizer::Ignore;
  switch (event->type()) {
  case QEvent::TouchBegin: {
    result = QGestureRecognizer::MayBeGesture;
    break;
  }
  case QEvent::TouchEnd: {
    if (q->state != Qt::NoGesture) {
      result = QGestureRecognizer::FinishGesture;
    }
    else {
      result = QGestureRecognizer::CancelGesture;
    }
    break;
  }
  case QEvent::TouchUpdate: {
    const QTouchEvent *ev = static_cast<const QTouchEvent *>(event);
    q->changeFlags = { };
    if (ev->touchPoints().size() == 3) {
      QTouchEvent::TouchPoint p1 = ev->touchPoints().at(0);
      QTouchEvent::TouchPoint p2 = ev->touchPoints().at(1);
      QTouchEvent::TouchPoint p3 = ev->touchPoints().at(2);
      q->hotSpot = p1.screenPos();
      q->isHotSpotSet = true;
      QPointF centerPoint = p2.screenPos();
      if (q->isNewSequence) {
        q->startPosition[0] = p1.screenPos();
        q->startPosition[1] = p2.screenPos();
        q->startPosition[2] = p3.screenPos();
        q->lastCenterPoint = centerPoint;
      }
      else {
        q->lastCenterPoint = centerPoint;
      }
      q->centerPoint = centerPoint;
      q->changeFlags |= QPinchGesture::CenterPointChanged;
      q->totalChangeFlags |= q->changeFlags;
      q->isNewSequence = false;
      result = QGestureRecognizer::TriggerGesture;
    }
    else {
      q->isNewSequence = true;
      if (q->state == Qt::NoGesture) {
	result = QGestureRecognizer::Ignore;
      }
      else {
	result = QGestureRecognizer::FinishGesture;
      }
    }
    break;
  }
  default:
    break;
  }
  return result;
}

void QVNCGestureRecognizer::reset(QGesture *state)
{
  QVNCGesture *q = static_cast<QVNCGesture *>(state);
  q->totalChangeFlags = q->changeFlags = { };
  q->startCenterPoint = q->lastCenterPoint = q->centerPoint = QPointF();
  q->isNewSequence = true;
  q->startPosition[0] = q->startPosition[1] = QPointF();
  QGestureRecognizer::reset(state);
}
