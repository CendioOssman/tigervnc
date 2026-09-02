#include "tapdraggesture.h"

QTapDragGesture::QTapDragGesture()
{

}

QTapDragGesture::Type QTapDragGesture::getType() const
{

  return type;
}
QPoint QTapDragGesture::getPosition() const
{
  return position;
}

QPoint QTapDragGesture::getStartPosition() const
{
  return startPosition;
}

