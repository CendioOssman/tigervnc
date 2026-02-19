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

#include <FL/Fl_Check_Button.H>

#include "OptionsMisc.h"
#include "i18n.h"
#include "parameters.h"

#include "fltk/layout.h"

OptionsMisc::OptionsMisc(int tx, int ty, int tw, int th)
  : OptionsPage(tx, ty, tw, th, _("Miscellaneous"))
{
  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  sharedCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("Shared (don't disconnect other viewers)")));
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  reconnectCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("Ask to reconnect on connection errors")));
  ty += CHECK_HEIGHT + TIGHT_MARGIN;

  end();
}

void OptionsMisc::loadOptions()
{
  sharedCheckbox->value(shared);
  reconnectCheckbox->value(reconnectOnError);
}

void OptionsMisc::storeOptions()
{
  shared.setParam(sharedCheckbox->value());
  reconnectOnError.setParam(reconnectCheckbox->value());
}
