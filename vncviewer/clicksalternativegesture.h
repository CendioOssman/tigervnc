#ifndef RCAGESTURE_H
#define RCAGESTURE_H

#include <QGesture>

class QClicksAlternativeGesture : public QGesture
{
  Q_OBJECT
public:
  enum Type
  {
    Undefined,
    TwoPoints,
    ThreePoints
  };

  QClicksAlternativeGesture();

  QPoint getPosition() const;
  void setPosition(QPoint newPosition);

  Type getType() const;


private:
  Type type = Undefined;

  QPoint position;

  friend class QClicksAlternativeGestureRecognizer;
};

#endif // VNCGESTURE_H
