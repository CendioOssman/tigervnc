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

#include "OptionsMisc.h"

#include "parameters.h"
#include "i18n.h"

#include <QCheckBox>
#include <QVBoxLayout>

OptionsMisc::OptionsMisc(QWidget* parent)
  : TabElement{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;
  shared = new QCheckBox(_("Shared (don't disconnect other viewers)"));
  layout->addWidget(shared);
  reconnect = new QCheckBox(_("Ask to reconnect on connection errors"));
  layout->addWidget(reconnect);
  layout->addStretch(1);
  setLayout(layout);
}

void OptionsMisc::apply()
{
  ::shared.setParam(shared->isChecked());
  ::reconnectOnError.setParam(reconnect->isChecked());
}

void OptionsMisc::reset()
{
  shared->setChecked(::shared);
  reconnect->setChecked(::reconnectOnError);
}
