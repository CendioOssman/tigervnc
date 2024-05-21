#include "panzoomgesturerecognizer.h"

#include "panzoomgesture.h"

#include <QWidget>

// If the change in scale for a single touch event is out of this range,
// we consider it to be spurious.
static const qreal kSingleStepScaleMax = 2.0;
static const qreal kSingleStepScaleMin = 0.1;
static const qreal kSingleStepOffset = 10;

static QPointF panOffset(const QList<QTouchEvent::TouchPoint> &touchPoints, int maxCount)
{
  QPointF result;
  const int count = qMin(touchPoints.size(), maxCount);
  for (int p = 0; p < count; ++p)
    result += touchPoints.at(p).pos() - touchPoints.at(p).startPos();
  return result / qreal(count);
}

QGesture *PanZoomGestureRecognizer::create(QObject *target)
{
  if (target && target->isWidgetType()) {
    reinterpret_cast<QWidget *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
  }
  return new PanZoomGesture;
}

QGestureRecognizer::Result PanZoomGestureRecognizer::recognize(QGesture *state, QObject *watched, QEvent *event)
{
  PanZoomGesture *q = static_cast<PanZoomGesture *>(state);

  QGestureRecognizer::Result result = QGestureRecognizer::Ignore;

  switch (event->type()) {
  case QEvent::TouchBegin: {
    const QTouchEvent *ev = static_cast<const QTouchEvent *>(event);
    QTouchEvent::TouchPoint p = ev->touchPoints().at(0);
    q->lastOffset = q->offset = QPointF();
    result = QGestureRecognizer::MayBeGesture;
    break;
  }
  case QEvent::TouchEnd: {
    if (q->state() != Qt::NoGesture) {
      result = QGestureRecognizer::FinishGesture;
    } else {
      result = QGestureRecognizer::CancelGesture;
    }
    break;
  }
  case QEvent::TouchUpdate: {
    const QTouchEvent *ev = static_cast<const QTouchEvent *>(event);
    if (ev->touchPoints().size() == 2) {
      if (q->type == PanZoomGesture::Undefined) {
        q->lastOffset = q->offset;
        q->offset = panOffset(ev->touchPoints(), 2);
        if (q->offset.x() > 3*kSingleStepOffset  || q->offset.y() > 3*kSingleStepOffset ||
            q->offset.x() < -3*kSingleStepOffset || q->offset.y() < -3*kSingleStepOffset) {
          q->setHotSpot(ev->touchPoints().first().startScreenPos());
          q->position = ev->touchPoints().first().startPos();
          q->type = PanZoomGesture::Pan;
          result = QGestureRecognizer::MayBeGesture;

          break;
        }

        QTouchEvent::TouchPoint p1 = ev->touchPoints().at(0);
        QTouchEvent::TouchPoint p2 = ev->touchPoints().at(1);
        QPointF centerPoint = (p1.screenPos() + p2.screenPos()) / 2.0;
        if (q->isNewSequence) {
          q->scaleFactor = 1.0;
          q->lastScaleFactor = 1.0;
          q->startPosition[0] = p1.screenPos();
          q->startPosition[1] = p2.screenPos();
          q->lastCenterPoint = centerPoint;
          q->centerPoint = centerPoint;
        } else {
          q->centerPoint = centerPoint;
          q->lastScaleFactor = q->scaleFactor;
          QLineF line(p1.screenPos(), p2.screenPos());
          QLineF startLine(p1.startScreenPos(),  p2.startScreenPos());
          qreal newScaleFactor = line.length() / startLine.length();
          if (qAbs(line.length() - startLine.length()) > 2*kSingleStepOffset) {
            q->type = PanZoomGesture::Pinch;
            result = QGestureRecognizer::MayBeGesture;
          }
          q->scaleFactor = newScaleFactor;
        }
        q->totalScaleFactor = q->totalScaleFactor * q->scaleFactor;
        q->isNewSequence = false;
        if (q->type == PanZoomGesture::Pinch) {
          break;
        }
      } else if (q->type == PanZoomGesture::Pan) {
        q->lastOffset = q->offset;
        q->offset = panOffset(ev->touchPoints(), 2);
        if (q->offset.x() > kSingleStepOffset  || q->offset.y() > kSingleStepOffset ||
            q->offset.x() < -kSingleStepOffset || q->offset.y() < -kSingleStepOffset) {
          q->setHotSpot(ev->touchPoints().first().startScreenPos());
          q->position = ev->touchPoints().first().startPos();
          result = QGestureRecognizer::TriggerGesture;
        } else {
          result = QGestureRecognizer::MayBeGesture;
        }
      } else if (q->type == PanZoomGesture::Pinch) {
        QTouchEvent::TouchPoint p1 = ev->touchPoints().at(0);
        QTouchEvent::TouchPoint p2 = ev->touchPoints().at(1);

        q->setHotSpot(p1.screenPos());
        q->position = p1.pos();

        QPointF centerPoint = (p1.screenPos() + p2.screenPos()) / 2.0;
        q->lastCenterPoint = q->centerPoint;
        q->centerPoint = centerPoint;

        q->lastScaleFactor = q->scaleFactor;
        QLineF line(p1.screenPos(), p2.screenPos());
        QLineF lastLine(p1.lastScreenPos(),  p2.lastScreenPos());
        qreal newScaleFactor = line.length() / lastLine.length();
        if (newScaleFactor > kSingleStepScaleMax || newScaleFactor < kSingleStepScaleMin) {
          return QGestureRecognizer::Ignore;
        }
        q->scaleFactor = newScaleFactor;
        q->totalScaleFactor = q->totalScaleFactor * q->scaleFactor;

        q->isNewSequence = false;
        result = QGestureRecognizer::TriggerGesture;
      }
    } else {
      q->isNewSequence = true;
      if (q->state() == Qt::NoGesture)
        result = QGestureRecognizer::Ignore;
      else
        result = QGestureRecognizer::FinishGesture;
    }
    break;
  }
  default:
    break;
  }
  return result;
}

void PanZoomGestureRecognizer::reset(QGesture *state)
{
  PanZoomGesture *q = static_cast<PanZoomGesture *>(state);

  q->type = PanZoomGesture::Undefined;

  q->startCenterPoint = q->lastCenterPoint = q->centerPoint = QPointF();
  q->totalScaleFactor = q->lastScaleFactor = q->scaleFactor = 1;
  q->isNewSequence = true;
  q->startPosition[0] = q->startPosition[1] = QPointF();

  q->lastOffset = q->offset = q->delta = QPointF();
  q->position = QPointF();

  QGestureRecognizer::reset(state);
}
