#include "displaytab.h"

#include "parameters.h"
#include "screensselectionwidget.h"

#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

DisplayTab::DisplayTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(tr("Display mode"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  displayWindowed = new QRadioButton(tr("Windowed"));
  vbox1->addWidget(displayWindowed);
  displayFullScreenOnCurrentMonitor = new QRadioButton(tr("Full screen on current monitor"));
  vbox1->addWidget(displayFullScreenOnCurrentMonitor);
  displayFullScreenOnAllMonitors = new QRadioButton(tr("Full screen on all monitors"));
  displayFullScreenOnAllMonitors->setEnabled(ViewerConfig::config()->canFullScreenOnMultiDisplays());
  vbox1->addWidget(displayFullScreenOnAllMonitors);
  displayFullScreenOnSelectedMonitors = new QRadioButton(tr("Full screen on selected monitor(s)"));
  vbox1->addWidget(displayFullScreenOnSelectedMonitors);
  selectedScreens = new ScreensSelectionWidget;
  selectedScreens->setEnabled(false);
  vbox1->addWidget(selectedScreens, 1);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1, 1);

  setLayout(layout);

  connect(displayFullScreenOnSelectedMonitors, &QRadioButton::toggled, this, [=](bool checked) {
    selectedScreens->setEnabled(checked);
  });
}

void DisplayTab::apply()
{
  if (displayWindowed->isChecked()) {
    ViewerConfig::config()->setFullScreen(false);
  } else {
    auto newFullScreenMode = displayFullScreenOnAllMonitors->isChecked()      ? ViewerConfig::FSAll
                           : displayFullScreenOnSelectedMonitors->isChecked() ? ViewerConfig::FSSelected
                                                                              : ViewerConfig::FSCurrent;
    ViewerConfig::config()->setFullScreenMode(newFullScreenMode);
    // To reconfigure the fullscreen mode, set Config.fullScreen to false once, then set it to true.
    ViewerConfig::config()->setFullScreen(false);
    ViewerConfig::config()->setFullScreen(true);
  }
  selectedScreens->apply();
}

void DisplayTab::reset()
{
  displayWindowed->setChecked(!ViewerConfig::config()->fullScreen());
  displayFullScreenOnCurrentMonitor->setChecked(ViewerConfig::config()->fullScreen()
                                                && ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSCurrent);
  displayFullScreenOnAllMonitors->setChecked(ViewerConfig::config()->fullScreen()
                                             && ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSAll);
  displayFullScreenOnSelectedMonitors->setChecked(
      ViewerConfig::config()->fullScreen() && ViewerConfig::config()->fullScreenMode() == ViewerConfig::FSSelected);
  selectedScreens->reset();
}
