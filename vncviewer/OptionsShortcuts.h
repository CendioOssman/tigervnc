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

#ifndef __OPTIONSSHORTCUTS_H__
#define __OPTIONSSHORTCUTS_H__

#include "OptionsPage.h"

class Fl_Box;
class Fl_Toggle_Button;

class OptionsShortcuts : public OptionsPage
{
public:
  OptionsShortcuts(int tx, int ty, int tw, int th);

  void loadOptions() override;
  void storeOptions() override;

protected:
  void handleModifier();

protected:
  Fl_Toggle_Button *ctrlButton;
  Fl_Toggle_Button *altButton;
  Fl_Toggle_Button *shiftButton;
  Fl_Toggle_Button *superButton;

  Fl_Box *shortcutsText;
};

#endif
