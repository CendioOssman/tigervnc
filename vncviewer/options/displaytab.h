#ifndef DISPLAYTAB_H
#define DISPLAYTAB_H

#include "../OptionsDialog.h"

#include <QWidget>

class ScreensSelectionWidget;
class QRadioButton;

class DisplayTab : public TabElement
{
  Q_OBJECT

public:
  DisplayTab(QWidget* parent = nullptr);

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
