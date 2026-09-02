#include "clicksalternativegesturerecognizer.h"

#include "clicksalternativegesture.h"

#include <QWidget>

QGesture *QClicksAlternativeGestureRecognizer::create(QObject *target)
{
  if (target && target->isWidgetType()) {
    reinterpret_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
  }
  return new QClicksAlternativeGesture;
}

QGestureRecognizer::Result QClicksAlternativeGestureRecognizer::recognize(QGesture *state, QObject */*watched*/, QEvent *event)
{
  QClicksAlternativeGesture *q = static_cast<QClicksAlternativeGesture *>(state);
  const QTouchEvent *ev = static_cast<const QTouchEvent *>(event);

  QGestureRecognizer::Result result = QGestureRecognizer::Ignore;

  switch (event->type()) {
  case QEvent::TouchBegin: {
    q->setPosition(ev->touchPoints().at(0).pos().toPoint());
    q->setHotSpot(ev->touchPoints().at(0).screenPos());
    result = QGestureRecognizer::MayBeGesture;
    break;
  }
  case QEvent::TouchEnd: {
    if (q->state() != Qt::NoGesture) {
      QTouchEvent::TouchPoint p = ev->touchPoints().at(0);
      QPoint delta = p.pos().toPoint() - p.startPos().toPoint();
      enum { TapRadius = 40 };
      if (delta.manhattanLength() <= TapRadius) {
        result = QGestureRecognizer::FinishGesture;
      } else {
        result = QGestureRecognizer::CancelGesture;
      }
    } else {
      result = QGestureRecognizer::CancelGesture;
    }
    break;
  }
  case QEvent::TouchUpdate: {
    if (ev->touchPoints().size() > 1) {
      if (q->type == QClicksAlternativeGesture::Undefined) {
        if (ev->touchPoints().size() == 2) {
          q->type = QClicksAlternativeGesture::TwoPoints;
        } else {
          q->type = QClicksAlternativeGesture::ThreePoints;
        }
      } else if (q->type == QClicksAlternativeGesture::TwoPoints) {
        if (ev->touchPoints().size() == 3) {
          q->type = QClicksAlternativeGesture::ThreePoints;
        }
      }

      QTouchEvent::TouchPoint p = ev->touchPoints().at(0);
      QPoint delta = p.pos().toPoint() - p.startPos().toPoint();
      enum { TapRadius = 40 };
      if (delta.manhattanLength() <= TapRadius) {
        result = QGestureRecognizer::TriggerGesture;
      } else {
        result = QGestureRecognizer::CancelGesture;
      }
    } else {
      result = QGestureRecognizer::MayBeGesture;
    }
    break;
  }
  case QEvent::MouseButtonPress:
  case QEvent::MouseMove:
  case QEvent::MouseButtonRelease:
    result = QGestureRecognizer::Ignore;
    break;
  default:
    result = QGestureRecognizer::Ignore;
    break;
  }
  return result;
}

void QClicksAlternativeGestureRecognizer::reset(QGesture *state)
{
  QClicksAlternativeGesture *q = static_cast<QClicksAlternativeGesture *>(state);
  q->type = QClicksAlternativeGesture::Undefined;
  q->setPosition(QPoint());
  QGestureRecognizer::reset(state);
}
