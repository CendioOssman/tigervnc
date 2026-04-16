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
#include "i18n.h"

#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#ifdef __APPLE__
#include "cocoa.h"
#endif

#if !defined(WIN32) && !defined(__APPLE__)
#include "x11.h"
#endif

OptionsDisplay::OptionsDisplay(QWidget* parent)
  : OptionsPage{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  QGroupBox* groupBox1 = new QGroupBox(_("Display mode"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  windowedButton = new QRadioButton(_("Windowed"));
  vbox1->addWidget(windowedButton);
  currentMonitorButton = new QRadioButton(_("Full screen on current monitor"));
  vbox1->addWidget(currentMonitorButton);
  allMonitorsButton = new QRadioButton(_("Full screen on all monitors"));
  vbox1->addWidget(allMonitorsButton);
  selectedMonitorsButton = new QRadioButton(_("Full screen on selected monitor(s)"));
  vbox1->addWidget(selectedMonitorsButton);
  QHBoxLayout* h1 = new QHBoxLayout;
  h1->addSpacing(20);
  monitorArrangement = new QMonitorArrangement;
  monitorArrangement->setEnabled(false);
  h1->addWidget(monitorArrangement, 1);
  vbox1->addLayout(h1, 1);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1, 1);

  setLayout(layout);

  bool supportsMultihead;

#if defined(WIN32)
  supportsMultihead = true;
#elif defined(__APPLE__)
  supportsMultihead = !cocoa_screens_have_separate_spaces();
#else
  // We will emulate multihead support without a WM
  // FIXME: check this behaviour
  if (!x11_has_wm())
    supportsMultihead = true;
  else
    supportsMultihead = x11_wm_supports("_NET_WM_FULLSCREEN_MONITORS");
#endif

  if (!supportsMultihead) {
    QLabel* widget;
    const char* label;

#if defined(WIN32)
    assert(false);
#elif defined(__APPLE__)
    label = _("Full screen on multiple monitors is not supported when "
              "the system setting \"Displays have separate Spaces\" "
              "is enabled");
#else
    label = _("Full screen on multiple monitors is not supported by "
              "the current desktop environment");
#endif

    widget = new QLabel();
    widget->setWordWrap(true);
    widget->setTextFormat(Qt::PlainText);
    widget->setText(label);
    vbox1->addWidget(widget);

    allMonitorsButton->setEnabled(false);
    selectedMonitorsButton->setEnabled(false);
    monitorArrangement->setEnabled(false);
  }

  connect(selectedMonitorsButton, &QRadioButton::toggled, this, [=](bool checked) {
    monitorArrangement->setEnabled(checked);
  });
}

void OptionsDisplay::apply()
{
  if (windowedButton->isChecked()) {
    ::fullScreen.setParam(false);
  } else {
    auto newFullScreenMode = allMonitorsButton->isChecked()      ? "all"
                           : selectedMonitorsButton->isChecked() ? "selected"
                                                                              : "current";
    ::fullScreenMode.setParam(newFullScreenMode);
    ::fullScreen.setParam(true);
  }
  monitorArrangement->apply();
}

void OptionsDisplay::reset()
{
  bool allMonitors = !strcasecmp(fullScreenMode, "all");
  bool selectedMonitors = !strcasecmp(fullScreenMode, "selected");
  windowedButton->setChecked(!::fullScreen);
  currentMonitorButton->setChecked(::fullScreen
                                                && ((!allMonitors && !selectedMonitors)));
  allMonitorsButton->setChecked(::fullScreen && allMonitors);
  selectedMonitorsButton->setChecked(::fullScreen && selectedMonitors);
  monitorArrangement->reset();
}
