#include "clicksalternativegesturerecognizer.h"

#include "clicksalternativegesture.h"

#include <QWidget>

QGesture *ClicksAlternativeGestureRecognizer::create(QObject *target)
{
  if (target && target->isWidgetType()) {
    reinterpret_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
  }
  return new ClicksAlternativeGesture;
}

QGestureRecognizer::Result ClicksAlternativeGestureRecognizer::recognize(QGesture *state, QObject */*watched*/, QEvent *event)
{
  ClicksAlternativeGesture *q = static_cast<ClicksAlternativeGesture *>(state);
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
      if (q->type == ClicksAlternativeGesture::Undefined) {
        if (ev->touchPoints().size() == 2) {
          q->type = ClicksAlternativeGesture::TwoPoints;
        } else {
          q->type = ClicksAlternativeGesture::ThreePoints;
        }
      } else if (q->type == ClicksAlternativeGesture::TwoPoints) {
        if (ev->touchPoints().size() == 3) {
          q->type = ClicksAlternativeGesture::ThreePoints;
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

void ClicksAlternativeGestureRecognizer::reset(QGesture *state)
{
  ClicksAlternativeGesture *q = static_cast<ClicksAlternativeGesture *>(state);
  q->type = ClicksAlternativeGesture::Undefined;
  q->setPosition(QPoint());
  QGestureRecognizer::reset(state);
}
