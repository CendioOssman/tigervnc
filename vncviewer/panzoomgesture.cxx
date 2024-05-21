#include "panzoomgesture.h"

PanZoomGesture::PanZoomGesture()
{

}

PanZoomGesture::Type PanZoomGesture::getType() const
{
  return type;
}

qreal PanZoomGesture::getScaleFactor() const
{
  return scaleFactor;
}

QPointF PanZoomGesture::getOffsetDelta() const
{
  return offset - lastOffset;
}

QPointF PanZoomGesture::getPosition() const
{
  return position;
}
