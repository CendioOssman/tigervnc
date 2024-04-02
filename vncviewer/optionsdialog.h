#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>

class QStackedWidget;

class TabElement : public QWidget
{
  Q_OBJECT

public:
  TabElement(QWidget* parent = nullptr)
    : QWidget(parent)
  {
  }

  virtual void apply() = 0;
  virtual void reset() = 0;
};

class OptionsDialog : public QDialog
{
  Q_OBJECT

public:
  OptionsDialog(QWidget* parent = nullptr);

  void apply();
  void reset();

private:
  QStackedWidget* tabWidget = nullptr;
};

#endif // OPTIONSDIALOG_H
