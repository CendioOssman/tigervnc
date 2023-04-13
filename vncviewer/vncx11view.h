#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include "abstractvncview.h"

namespace rfb {
  class Rect;
}

class QVNCX11view : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11view(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11view();
  void bell() override;

protected:
  bool event(QEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent*) override;
  void resizeEvent(QResizeEvent*) override;

signals:
  void message(const QString &msg, int timeout);

private:
  rfb::Rect *m_rect;
};

#endif // VNCX11VIEW_H
