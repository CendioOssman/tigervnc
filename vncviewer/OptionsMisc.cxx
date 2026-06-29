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

#include <QBoxLayout>
#include <QCheckBox>

#include <core/Configuration.h>
#include <core/i18n.h>

#include "OptionsMisc.h"
#include "parameters.h"

OptionsMisc::OptionsMisc(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* layout = new QVBoxLayout;

  sharedCheckbox = new QCheckBox(_("Shared (don't disconnect other viewers)"));
  layout->addWidget(sharedCheckbox);

  reconnectCheckbox = new QCheckBox(_("Ask to reconnect on connection errors"));
  layout->addWidget(reconnectCheckbox);

#ifdef HAVE_AUDIO
  audioCheckbox = new QCheckBox(_("Play audio from the server"));
  layout->addWidget(audioCheckbox);
#endif

  layout->addStretch(1);

  setLayout(layout);
}

void OptionsMisc::loadOptions()
{
  sharedCheckbox->setChecked(shared);
  reconnectCheckbox->setChecked(reconnectOnError);
#ifdef HAVE_AUDIO
  audioCheckbox->setChecked(playAudio);
#endif
}

void OptionsMisc::storeOptions()
{
  shared.setParam(sharedCheckbox->isChecked());
  reconnectOnError.setParam(reconnectCheckbox->isChecked());
#ifdef HAVE_AUDIO
  playAudio.setParam(audioCheckbox->isChecked());
#endif
}
