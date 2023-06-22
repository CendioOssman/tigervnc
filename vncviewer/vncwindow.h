#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>

class QMoveEvent;
class QResizeEvent;
class QVNCToast;

class QVNCWindow : public QScrollArea
{
  Q_OBJECT
public:
  QVNCWindow(QWidget *parent = nullptr);
  virtual ~QVNCWindow();
  void resize(int width, int height);
  void normalizedResize(int width, int height);

public slots:
  void popupToast();

protected:
  void moveEvent(QMoveEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void changeEvent(QEvent *e) override;

private:
  QVNCToast *toast_;
};

#endif // VNCWINDOW_H
