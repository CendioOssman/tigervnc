#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>

class QMoveEvent;
class QVNCToast;

class QVNCWindow : public QScrollArea
{
  Q_OBJECT
public:
  QVNCWindow(QWidget *parent = nullptr);
  virtual ~QVNCWindow();

public slots:
  void popupToast();

protected:
  void moveEvent(QMoveEvent *e) override;

private:
  QVNCToast *toast_;
};

#endif // VNCWINDOW_H
