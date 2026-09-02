#include "clicksalternativegesture.h"

QClicksAlternativeGesture::QClicksAlternativeGesture()
{

}

QClicksAlternativeGesture::Type QClicksAlternativeGesture::getType() const
{
  return type;
}

QPoint QClicksAlternativeGesture::getPosition() const
{
  return position;
}

void QClicksAlternativeGesture::setPosition(QPoint newPosition)
{
  position = newPosition;
}
