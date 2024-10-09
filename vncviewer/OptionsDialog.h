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

typedef void (OptionsCallback)(void*);

class OptionsDialog : public QDialog
{
  Q_OBJECT

public:
  OptionsDialog(bool staysOnTop, QWidget* parent = nullptr);

  void apply();
  void reset();

  static void addCallback(OptionsCallback *cb, void *data = nullptr);
  static void removeCallback(OptionsCallback *cb);

private:
  static std::map<OptionsCallback*, void*> callbacks;

  QStackedWidget* tabWidget = nullptr;
};

#endif // OPTIONSDIALOG_H
