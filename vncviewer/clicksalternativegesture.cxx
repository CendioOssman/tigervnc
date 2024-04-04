#include "clicksalternativegesture.h"

ClicksAlternativeGesture::ClicksAlternativeGesture()
{

}

ClicksAlternativeGesture::Type ClicksAlternativeGesture::getType() const
{
  return type;
}

QPoint ClicksAlternativeGesture::getPosition() const
{
  return position;
}

void ClicksAlternativeGesture::setPosition(QPoint newPosition)
{
  position = newPosition;
}
