#ifndef DISPLAYTAB_H
#define DISPLAYTAB_H

#include "../OptionsDialog.h"

#include <QWidget>

class ScreensSelectionWidget;
class QRadioButton;

class OptionsDisplay : public TabElement
{
  Q_OBJECT

public:
  OptionsDisplay(QWidget* parent = nullptr);

  void apply() override;
  void reset() override;

private:
  QRadioButton* displayWindowed;
  QRadioButton* displayFullScreenOnCurrentMonitor;
  QRadioButton* displayFullScreenOnAllMonitors;
  QRadioButton* displayFullScreenOnSelectedMonitors;
  ScreensSelectionWidget* selectedScreens;
};

#endif // DISPLAYTAB_H
