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

#include <set>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Round_Button.H>

#include "OptionsDisplay.h"
#include "i18n.h"
#include "parameters.h"

#include "fltk/Fl_Monitor_Arrangement.h"
#include "fltk/layout.h"
#include "fltk/util.h"

#ifdef __APPLE__
#include "cocoa.h"
#endif

#if !defined(WIN32) && !defined(__APPLE__)
#include "x11.h"
#endif

static std::set<OptionsDisplay*> instances;

OptionsDisplay::OptionsDisplay(int tx, int ty, int tw, int th)
  : OptionsPage(tx, ty, tw, th, _("Display"))
{
  int orig_tx;
  int width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  width = tw - OUTER_MARGIN * 2;

  orig_tx = tx;

  /* Display mode */
  ty += GROUP_LABEL_OFFSET;
  displayModeGroup = new Fl_Group(tx, ty, width, 0, _("Display mode"));
  displayModeGroup->labelfont(FL_BOLD);
  displayModeGroup->box(FL_FLAT_BOX);
  displayModeGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;
    width -= INDENT;

    windowedButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                  RADIO_MIN_WIDTH,
                                                  RADIO_HEIGHT,
                                                  _("Windowed")));
    windowedButton->type(FL_RADIO_BUTTON);
    windowedButton->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsDisplay*)data)->handleFullScreenMode();
      },
      this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    currentMonitorButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                        RADIO_MIN_WIDTH,
                                                        RADIO_HEIGHT,
                                                        _("Full screen on current monitor")));
    currentMonitorButton->type(FL_RADIO_BUTTON);
    currentMonitorButton->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsDisplay*)data)->handleFullScreenMode();
      },
      this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    allMonitorsButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                            RADIO_MIN_WIDTH,
                                            RADIO_HEIGHT,
                                            _("Full screen on all monitors")));
    allMonitorsButton->type(FL_RADIO_BUTTON);
    allMonitorsButton->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsDisplay*)data)->handleFullScreenMode();
      },
      this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    selectedMonitorsButton = new Fl_Round_Button(LBLRIGHT(tx, ty,
                                                 RADIO_MIN_WIDTH,
                                                 RADIO_HEIGHT,
                                                 _("Full screen on selected monitor(s)")));
    selectedMonitorsButton->type(FL_RADIO_BUTTON);
    selectedMonitorsButton->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsDisplay*)data)->handleFullScreenMode();
      },
      this);
    ty += RADIO_HEIGHT + TIGHT_MARGIN;

    monitorArrangement = new Fl_Monitor_Arrangement(
                              tx + INDENT, ty,
                              width - INDENT, 150);
    ty += 150 + INNER_MARGIN;

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
      Fl_Box* box;
      const char* label;
      int w, h;

      allMonitorsButton->deactivate();
      selectedMonitorsButton->deactivate();
      monitorArrangement->deactivate();

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

      fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
      w = width;
      fl_measure(label, w, h);

      box = new Fl_Box(tx, ty, w, h, label);
      box->align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
      ty += h + INNER_MARGIN;
    }
  }
  ty -= INNER_MARGIN;

  displayModeGroup->end();
  /* Needed for resize to work sanely */
  displayModeGroup->resizable(nullptr);
  displayModeGroup->size(displayModeGroup->w(),
                         ty - displayModeGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;
  width = tw - OUTER_MARGIN * 2;

  end();

  if (instances.size() == 0)
    Fl::add_handler(fltk_event_handler);
  instances.insert(this);
}

OptionsDisplay::~OptionsDisplay()
{
  instances.erase(this);

  if (instances.size() == 0)
    Fl::remove_handler(fltk_event_handler);
}

void OptionsDisplay::loadOptions()
{
  if (!fullScreen) {
    windowedButton->setonly();
  } else {
    if (!strcasecmp(fullScreenMode, "all")) {
      allMonitorsButton->setonly();
    } else if (!strcasecmp(fullScreenMode, "selected")) {
      selectedMonitorsButton->setonly();
    } else {
      currentMonitorButton->setonly();
    }
  }

  monitorArrangement->value(fullScreenSelectedMonitors.getParam());

  handleFullScreenMode();
}

void OptionsDisplay::storeOptions()
{
  if (windowedButton->value()) {
    fullScreen.setParam(false);
  } else {
    fullScreen.setParam(true);

    if (allMonitorsButton->value()) {
      fullScreenMode.setParam("All");
    } else if (selectedMonitorsButton->value()) {
      fullScreenMode.setParam("Selected");
    } else {
      fullScreenMode.setParam("Current");
    }
  }

  fullScreenSelectedMonitors.setParam(monitorArrangement->value());
}

void OptionsDisplay::handleFullScreenMode()
{
  if (selectedMonitorsButton->value()) {
    monitorArrangement->activate();
  } else {
    monitorArrangement->deactivate();
  }
}

int OptionsDisplay::fltk_event_handler(int event)
{
  std::set<OptionsDisplay*>::iterator iter;

  if (event != FL_SCREEN_CONFIGURATION_CHANGED)
    return 0;

  // Refresh monitor arrangement widget to match the parameter settings after
  // screen configuration has changed. The MonitorArrangement index doesn't work
  // the same way as the FLTK screen index.
  for (iter = instances.begin(); iter != instances.end(); iter++)
      Fl::add_timeout(0, handleScreenConfigTimeout, (*iter));

  return 0;
}

void OptionsDisplay::handleScreenConfigTimeout(void* data)
{
    OptionsDisplay* self = (OptionsDisplay*)data;

    assert(self);

    self->monitorArrangement->value(fullScreenSelectedMonitors.getParam());
}
