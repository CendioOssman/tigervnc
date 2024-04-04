#include "tapdraggesture.h"

TapDragGesture::TapDragGesture()
{

}

TapDragGesture::Type TapDragGesture::getType() const
{

  return type;
}
QPointF TapDragGesture::getPosition() const
{
  return position;
}

QPointF TapDragGesture::getStartPosition() const
{
  return startPosition;
}

