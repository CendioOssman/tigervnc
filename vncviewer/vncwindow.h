#ifndef VNCWINDOW_H
#define VNCWINDOW_H

#include <QScrollArea>

class QMoveEvent;
class QResizeEvent;

class QVNCWindow : public QScrollArea
{
  Q_OBJECT
public:
  QVNCWindow(QWidget *parent = nullptr);
  virtual ~QVNCWindow();
  void resize(int width, int height);
  void normalizedResize(int width, int height);

protected:
  void moveEvent(QMoveEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void changeEvent(QEvent *e) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
};

#endif // VNCWINDOW_H
