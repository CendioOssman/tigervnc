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

#include <assert.h>

#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QScreen>
#include <QVBoxLayout>

#include <FL/Fl.H>

#include "OptionsDisplay.h"
#include "i18n.h"
#include "parameters.h"

#include "qt/QMonitorArrangement.h"

#ifdef __APPLE__
#include "cocoa.h"
#endif

#if !defined(WIN32) && !defined(__APPLE__)
#include "x11.h"
#endif

OptionsDisplay::OptionsDisplay(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* indent;
  QBoxLayout* layout = new QVBoxLayout;

  /* Display mode */
  displayModeGroup = new QGroupBox(_("Display mode"));
  layout->addWidget(displayModeGroup, 1);

  {
    QBoxLayout* box = new QVBoxLayout;
    displayModeGroup->setLayout(box);

    windowedButton = new QRadioButton(_("Windowed"));
    box->addWidget(windowedButton);

    currentMonitorButton = new QRadioButton(_("Full screen on current monitor"));
    box->addWidget(currentMonitorButton);

    allMonitorsButton = new QRadioButton(_("Full screen on all monitors"));
    box->addWidget(allMonitorsButton);

    selectedMonitorsButton = new QRadioButton(_("Full screen on selected monitor(s)"));
    connect(selectedMonitorsButton, &QRadioButton::toggled,
            this, &OptionsDisplay::handleFullScreenMode);
    box->addWidget(selectedMonitorsButton);

    indent = new QHBoxLayout;
    indent->addSpacing(20);
    monitorArrangement = new QMonitorArrangement;
    indent->addWidget(monitorArrangement, 1);
    box->addLayout(indent, 1);

    bool supportsMultihead;

#if defined(WIN32)
    supportsMultihead = true;
#elif defined(__APPLE__)
    supportsMultihead = !cocoa_screens_have_separate_spaces();
#else
    // FLTK will emulate multihead support without a WM
    if (!x11_has_wm())
      supportsMultihead = true;
    else
      supportsMultihead =
        x11_wm_supports("_NET_WM_FULLSCREEN_MONITORS");
#endif

    if (!supportsMultihead) {
      QLabel* widget;
      const char* label;

      allMonitorsButton->setEnabled(false);
      selectedMonitorsButton->setEnabled(false);
      monitorArrangement->setEnabled(false);

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
      box->addWidget(widget);
    }
  }

  setLayout(layout);

  // Refresh monitor arrangement widget to match the parameter settings after
  // screen configuration has changed. The MonitorArrangement index doesn't work
  // the same way as the FLTK screen index.
  connect(qApp, &QGuiApplication::screenAdded,
          this, &OptionsDisplay::handleScreenConfigChange);
  connect(qApp, &QGuiApplication::screenRemoved,
          this, &OptionsDisplay::handleScreenConfigChange);
}

OptionsDisplay::~OptionsDisplay()
{
}

void OptionsDisplay::loadOptions()
{
  if (!fullScreen) {
    windowedButton->setChecked(true);
  } else {
    if (!strcasecmp(fullScreenMode, "all")) {
      allMonitorsButton->setChecked(true);
    } else if (!strcasecmp(fullScreenMode, "selected")) {
      selectedMonitorsButton->setChecked(true);
    } else {
      currentMonitorButton->setChecked(true);
    }
  }

  // The other stuff is still FLTK, so we need to map things
  std::set<int> indices;
  QList<QScreen*> screens;

  indices = fullScreenSelectedMonitors.getParam();

  for (int i : indices) {
    int x, y, w, h;
    QRect geom;

    Fl::screen_xywh(x, y, w, h, i);
    geom.setRect(x, y, w, h);

    for (QScreen* screen : qApp->screens()) {
      if (screen->geometry() == geom) {
        screens.append(screen);
        break;
      }
    }
  }

  monitorArrangement->setScreens(screens);

  handleFullScreenMode();
}

void OptionsDisplay::storeOptions()
{
  if (windowedButton->isChecked()) {
    fullScreen.setParam(false);
  } else {
    fullScreen.setParam(true);

    if (allMonitorsButton->isChecked()) {
      fullScreenMode.setParam("All");
    } else if (selectedMonitorsButton->isChecked()) {
      fullScreenMode.setParam("Selected");
    } else {
      fullScreenMode.setParam("Current");
    }
  }

  // The other stuff is still FLTK, so we need to map things
  QList<QScreen*> screens = monitorArrangement->screens();
  std::set<int> indices;

  for (int i = 0; i < Fl::screen_count(); i++) {
    int x, y, w, h;
    QRect geom;

    Fl::screen_xywh(x, y, w, h, i);
    geom.setRect(x, y, w, h);

    for (QScreen* screen : screens) {
      if (screen->geometry() != geom)
        continue;
      indices.insert(i);
      break;
    }
  }

  fullScreenSelectedMonitors.setParam(indices);
}

void OptionsDisplay::handleFullScreenMode()
{
  monitorArrangement->setEnabled(selectedMonitorsButton->isChecked());
}

void OptionsDisplay::handleScreenConfigChange()
{
    // The other stuff is still FLTK, so we need to map things
    std::set<int> indices;
    QList<QScreen*> screens;

    indices = fullScreenSelectedMonitors.getParam();

    for (int i : indices) {
      int x, y, w, h;
      QRect geom;

      Fl::screen_xywh(x, y, w, h, i);
      geom.setRect(x, y, w, h);

      for (QScreen* screen : qApp->screens()) {
        if (screen->geometry() == geom) {
          screens.append(screen);
          break;
        }
      }
    }

    monitorArrangement->setScreens(screens);
}
