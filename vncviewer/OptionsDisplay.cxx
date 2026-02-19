/* Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "OptionsDisplay.h"

#include "parameters.h"
#include "QMonitorArrangement.h"
#include "viewerconfig.h"
#include "i18n.h"

#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

OptionsDisplay::OptionsDisplay(QWidget* parent)
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
  selectedScreens = new QMonitorArrangement;
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

void OptionsDisplay::apply()
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

void OptionsDisplay::reset()
{
  bool allMonitors = !strcasecmp(fullScreenMode, "all");
  bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");
  displayWindowed->setChecked(!::fullScreen);
  displayFullScreenOnCurrentMonitor->setChecked(::fullScreen
                                                && ((!allMonitors && !selectedMonitors)
                                                    || (allMonitors && !ViewerConfig::canFullScreenOnMultiDisplays())));
  displayFullScreenOnAllMonitors->setChecked(::fullScreen && allMonitors && ViewerConfig::canFullScreenOnMultiDisplays());
  displayFullScreenOnSelectedMonitors->setChecked(::fullScreen && selectedMonitors);
  selectedScreens->reset();
}
