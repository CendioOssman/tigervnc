#ifndef VNCTOAST_H
#define VNCTOAST_H

#include <QLabel>

class QTimer;

class QVNCToast : public QLabel
{
  Q_OBJECT
public:
  QVNCToast(QWidget *parent = nullptr);
  virtual ~QVNCToast();
  void show();

private:
  QTimer *closeTimer_;
};

#endif // VNCTOAST_H
