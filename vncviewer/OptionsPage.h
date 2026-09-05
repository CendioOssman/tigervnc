/* Copyright 2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#ifndef __OPTIONSPAGE_H__
#define __OPTIONSPAGE_H__

#include <FL/Fl_Group.H>

class OptionsPage : public Fl_Group
{
public:
  OptionsPage(int tx, int ty, int tw, int th, const char* label);

  virtual void loadOptions() = 0;
  virtual void storeOptions() = 0;
};

#endif
