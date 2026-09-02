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

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "qt/QNavigation.h"

#include "OptionsDialog.h"
#include "i18n.h"

#include "OptionsCompression.h"
#include "OptionsSecurity.h"
#include "OptionsInput.h"
#include "OptionsDisplay.h"
#include "OptionsMisc.h"

std::map<OptionsCallback*, void*> OptionsDialog::callbacks;

OptionsDialog::OptionsDialog(QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(_("TigerVNC Options"));

  QBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  navigation = new QNavigation;
  navigation->addWidget(_("Compression"), new OptionsCompression);
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  navigation->addWidget(_("Security"), new OptionsSecurity);
#endif
  navigation->addWidget(_("Input"), new OptionsInput);
  navigation->addWidget(_("Display"), new OptionsDisplay);
  navigation->addWidget(_("Miscellaneous"), new OptionsMisc);
  layout->addWidget(navigation);

  QFrame* hFrame = new QFrame;
  hFrame->setFrameShape(QFrame::StyledPanel);
  hFrame->setFixedHeight(1);
  layout->addWidget(hFrame);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(10,10,10,10);

  QDialogButtonBox* buttonBox = new QDialogButtonBox;
  buttonBox->addButton(QDialogButtonBox::Ok);
  buttonBox->addButton(QDialogButtonBox::Cancel);
  connect(buttonBox, &QDialogButtonBox::accepted,
          this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected,
          this, &QDialog::reject);
  btnsLayout->addWidget(buttonBox);

  layout->addLayout(btnsLayout);

  setLayout(layout);
  adjustSize();

  connect(this, &QDialog::accepted, this, &OptionsDialog::storeOptions);

  loadOptions();
}


OptionsDialog::~OptionsDialog()
{
}


void OptionsDialog::addCallback(OptionsCallback *cb, void *data)
{
  callbacks[cb] = data;
}


void OptionsDialog::removeCallback(OptionsCallback *cb)
{
  callbacks.erase(cb);
}

void OptionsDialog::loadOptions(void)
{
  OptionsPage* page;
  for (int i = 0; i < navigation->count(); ++i) {
    page = dynamic_cast<OptionsPage*>(navigation->widget(i));
    assert(page);
    page->loadOptions();
  }
}

void OptionsDialog::storeOptions(void)
{
  OptionsPage* page;
  for (int i = 0; i < navigation->count(); ++i) {
    page = dynamic_cast<OptionsPage*>(navigation->widget(i));
    assert(page);
    page->storeOptions();
  }

  std::map<OptionsCallback*, void*>::const_iterator iter;

  for (iter = callbacks.begin();iter != callbacks.end();++iter)
    iter->first(iter->second);
}
