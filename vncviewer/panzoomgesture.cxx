#include "panzoomgesture.h"

QPanZoomGesture::QPanZoomGesture()
{

}

QPanZoomGesture::Type QPanZoomGesture::getType() const
{
  return type;
}

qreal QPanZoomGesture::getScaleFactor() const
{
  return scaleFactor;
}

QPointF QPanZoomGesture::getOffsetDelta() const
{
  return offset - lastOffset;
}

QPointF QPanZoomGesture::getPosition() const
{
  return position;
}
