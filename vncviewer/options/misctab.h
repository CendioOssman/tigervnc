#ifndef MISCTAB_H
#define MISCTAB_H

#include "../OptionsDialog.h"

#include <QWidget>

class QCheckBox;

class OptionsMisc : public TabElement
{
  Q_OBJECT

public:
  OptionsMisc(QWidget* parent = nullptr);

  void apply() override;
  void reset() override;

private:
  QCheckBox* shared;
  QCheckBox* reconnect;
};

#endif // MISCTAB_H
