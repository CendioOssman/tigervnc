#ifndef TOAST_H
#define TOAST_H

#include <QWidget>

class Toast : public QWidget
{
  Q_OBJECT

public:
  Toast(QWidget* parent = nullptr);

  void showToast();
  void hideToast();

  QFont toastFont() const;
  QString toastText() const;
  QRect toastGeometry() const;

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QTimer* toastTimer;
};

#endif // TOAST_H
