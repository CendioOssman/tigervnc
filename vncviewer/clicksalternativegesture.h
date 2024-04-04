#ifndef RCAGESTURE_H
#define RCAGESTURE_H

#include <QGesture>

class ClicksAlternativeGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type
  {
    Undefined,
    TwoPoints,
    ThreePoints
  };

  ClicksAlternativeGesture();

  QPoint getPosition() const;
  void setPosition(QPoint newPosition);

  Type getType() const;


private:
  Type type = Undefined;

  QPoint position;

  friend class ClicksAlternativeGestureRecognizer;
};

#endif // VNCGESTURE_H
