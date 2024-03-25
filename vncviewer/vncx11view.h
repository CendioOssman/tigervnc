#ifndef VNCX11VIEW_H
#define VNCX11VIEW_H

#include <QBitmap>
#include "abstractvncview.h"

class XInputTouchHandler;
class QVNCGestureRecognizer;
class QTimer;

class QVNCX11View : public QAbstractVNCView
{
  Q_OBJECT
public:
  QVNCX11View(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
  virtual ~QVNCX11View();

public slots:
  void handleClipboardData(const char* data) override;
  void bell() override;

protected:
  bool event(QEvent *e) override;
  void resizeEvent(QResizeEvent*) override;

  void fullscreenOnSelectedDisplays(int vx, int vy, int vwidth, int vheight) override;

signals:
  void message(const QString &msg, int timeout);

private:
  GestureHandler *gestureHandler_;
  int eventNumber_;
  static QVNCGestureRecognizer *vncGestureRecognizer_;

  bool gestureEvent(QGestureEvent *event);
};

#endif // VNCX11VIEW_H
