#include "tapdraggesture.h"

QTapDragGesture::QTapDragGesture()
{

}

QTapDragGesture::Type QTapDragGesture::getType() const
{

  return type;
}
QPointF QTapDragGesture::getPosition() const
{
  return position;
}

QPointF QTapDragGesture::getStartPosition() const
{
  return startPosition;
}

