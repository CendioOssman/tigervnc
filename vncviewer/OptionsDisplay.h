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

#ifndef __OPTIONSDISPLAY_H__
#define __OPTIONSDISPLAY_H__

#include "OptionsPage.h"

class Fl_Round_Button;
class Fl_Monitor_Arrangement;

class OptionsDisplay : public OptionsPage
{
public:
  OptionsDisplay(int tx, int ty, int tw, int th);
  ~OptionsDisplay();

  void loadOptions() override;
  void storeOptions() override;

protected:
  void handleFullScreenMode();

protected:
  Fl_Group *displayModeGroup;
  Fl_Round_Button *windowedButton;
  Fl_Round_Button *currentMonitorButton;
  Fl_Round_Button *allMonitorsButton;
  Fl_Round_Button *selectedMonitorsButton;
  Fl_Monitor_Arrangement *monitorArrangement;

private:
  static int fltk_event_handler(int event);
  static void handleScreenConfigTimeout(void *data);
};

#endif
