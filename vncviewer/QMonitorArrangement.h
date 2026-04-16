#ifndef SCREENSSELECTIONWIDGET_H
#define SCREENSSELECTIONWIDGET_H

#include <QCheckBox>
#include <QWidget>

class QMonitorArrangement : public QWidget
{
  Q_OBJECT

public:
  QMonitorArrangement(QWidget* parent = nullptr);

  void getGlobalScreensGeometry(QList<int> screens, int& xmin, int& ymin, qreal& w, qreal& h);

  void apply();
  void reset();

protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void moveCheckBoxes();
  void updatePartiallyChecked();

  QList<QCheckBox*> checkBoxes;
  QRect selectedRect;
};

#endif // DISPLAYTAB_H
