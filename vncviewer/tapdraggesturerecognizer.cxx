#include "tapdraggesturerecognizer.h"

#include "tapdraggesture.h"

#include <QWidget>

static int kHoldTimeout = 700; // ms
static int kTapRadius = 40;

QGesture *QTapDragGestureRecognizer::create(QObject *target)
{
  if (target && target->isWidgetType()) {
    reinterpret_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
  }
  return new QTapDragGesture;
}

QGestureRecognizer::Result QTapDragGestureRecognizer::recognize(QGesture *state, QObject *watched, QEvent *event)
{
  QTapDragGesture *q = static_cast<QTapDragGesture *>(state);

  if (watched == state && event->type() == QEvent::Timer) {
    if (q->type != QTapDragGesture::Drag) {
      q->killTimer(q->timerId);
      q->timerId = 0;
      q->type = QTapDragGesture::TapAndHold;
      return QGestureRecognizer::FinishGesture | QGestureRecognizer::ConsumeEventHint;
    } else {
      return QGestureRecognizer::Ignore;
    }
  }

  const QTouchEvent *ev = static_cast<const QTouchEvent *>(event);

  switch (event->type()) {
  case QEvent::TouchBegin: {
    q->position = ev->touchPoints().at(0).pos();
    q->startPosition = ev->touchPoints().at(0).pos();
    q->setHotSpot(ev->touchPoints().at(0).screenPos());
    if (q->timerId)
      q->killTimer(q->timerId);
    q->timerId = q->startTimer(kHoldTimeout);
    return QGestureRecognizer::MayBeGesture;
  }
  case QEvent::TouchEnd: {
    if (ev->touchPoints().size() == 1) {
      if (!q->timerId)
        return QGestureRecognizer::Ignore;

      if (q->type == QTapDragGesture::Undefined) {
        q->type = QTapDragGesture::Tap;
        return QGestureRecognizer::FinishGesture;
      }

      if (q->type == QTapDragGesture::TapAndHold) {
        return QGestureRecognizer::CancelGesture; // get out of the MayBeGesture state
      }

      q->position = ev->touchPoints().at(0).pos();
      q->setHotSpot(ev->touchPoints().at(0).screenPos());
      return QGestureRecognizer::FinishGesture;
    } else {
      return QGestureRecognizer::CancelGesture;
    }
  }
  case QEvent::TouchUpdate: {
    if (q->timerId && ev->touchPoints().size() == 1) {
      QTouchEvent::TouchPoint p = ev->touchPoints().at(0);
      q->position = ev->touchPoints().at(0).pos();
      q->setHotSpot(ev->touchPoints().at(0).screenPos());
      QPoint delta = p.pos().toPoint() - p.startPos().toPoint();
      if (delta.manhattanLength() > kTapRadius) {
        q->type = QTapDragGesture::Drag;
        return QGestureRecognizer::TriggerGesture;
      } else {
        q->type = QTapDragGesture::Tap;
        return QGestureRecognizer::MayBeGesture;
      }
    } else {
      return QGestureRecognizer::CancelGesture;
    }
  }
  case QEvent::MouseButtonPress:
  case QEvent::MouseMove:
  case QEvent::MouseButtonRelease:
  default:
    break;
  }

  return QGestureRecognizer::Ignore;
}

void QTapDragGestureRecognizer::reset(QGesture *state)
{
  QTapDragGesture *q = static_cast<QTapDragGesture *>(state);
  q->type = QTapDragGesture::Undefined;
  q->position = q->startPosition = QPointF();
  if (q->timerId)
    q->killTimer(q->timerId);
  q->timerId = 0;
  QGestureRecognizer::reset(state);
}
