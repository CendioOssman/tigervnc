#include "displaytab.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "parameters.h"
#include "screensselectionwidget.h"
#include "viewerconfig.h"
#include "i18n.h"

#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

DisplayTab::DisplayTab(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(_("Display mode"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  displayWindowed = new QRadioButton(_("Windowed"));
  vbox1->addWidget(displayWindowed);
  displayFullScreenOnCurrentMonitor = new QRadioButton(_("Full screen on current monitor"));
  vbox1->addWidget(displayFullScreenOnCurrentMonitor);
  displayFullScreenOnAllMonitors = new QRadioButton(_("Full screen on all monitors"));
  displayFullScreenOnAllMonitors->setEnabled(ViewerConfig::canFullScreenOnMultiDisplays());
  vbox1->addWidget(displayFullScreenOnAllMonitors);
  displayFullScreenOnSelectedMonitors = new QRadioButton(_("Full screen on selected monitor(s)"));
  vbox1->addWidget(displayFullScreenOnSelectedMonitors);
  QHBoxLayout* h1 = new QHBoxLayout;
  h1->addSpacing(20);
  selectedScreens = new ScreensSelectionWidget;
  selectedScreens->setEnabled(false);
  h1->addWidget(selectedScreens, 1);
  vbox1->addLayout(h1, 1);
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
    ::fullScreen.setParam(false);
  } else {
    auto newFullScreenMode = displayFullScreenOnAllMonitors->isChecked()      ? "all"
                           : displayFullScreenOnSelectedMonitors->isChecked() ? "selected"
                                                                              : "current";
    ::fullScreenMode.setParam(newFullScreenMode);
    ::fullScreen.setParam(true);
  }
  selectedScreens->apply();
}

void DisplayTab::reset()
{
  displayWindowed->setChecked(!::fullScreen);
  displayFullScreenOnCurrentMonitor->setChecked(::fullScreen
                                                && (ViewerConfig::fullscreenType() == ViewerConfig::Current
                                                    || (ViewerConfig::fullscreenType() == ViewerConfig::All && !ViewerConfig::canFullScreenOnMultiDisplays())));
  displayFullScreenOnAllMonitors->setChecked(::fullScreen && ViewerConfig::fullscreenType() == ViewerConfig::All && ViewerConfig::canFullScreenOnMultiDisplays());
  displayFullScreenOnSelectedMonitors->setChecked(::fullScreen && ViewerConfig::fullscreenType() == ViewerConfig::Selected);
  selectedScreens->reset();
}
