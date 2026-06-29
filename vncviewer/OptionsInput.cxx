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

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <rfb/Configuration.h>

#include "OptionsInput.h"
#include "i18n.h"
#include "menukey.h"
#include "parameters.h"

OptionsInput::OptionsInput(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* indent;
  QFormLayout* form;
  QBoxLayout* layout = new QVBoxLayout;

  viewOnlyCheckbox = new QCheckBox(_("View only (ignore mouse and keyboard)"));
  layout->addWidget(viewOnlyCheckbox);

  /* Mouse */
  mouseGroup = new QGroupBox(_("Mouse"));
  layout->addWidget(mouseGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    mouseGroup->setLayout(box);

    emulateMBCheckbox = new QCheckBox(_("Emulate middle mouse button"));
    box->addWidget(emulateMBCheckbox);

    dotCursorCheckbox = new QCheckBox(_("Show dot when no cursor"));
    box->addWidget(dotCursorCheckbox);
  }

  /* Keyboard */
  keyboardGroup = new QGroupBox(_("Keyboard"));
  layout->addWidget(keyboardGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    keyboardGroup->setLayout(box);

    systemKeysCheckbox = new QCheckBox(_("Pass system keys directly to server (full screen)"));
    box->addWidget(systemKeysCheckbox);

    form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    box->addLayout(form);
    menuKeyChoice = new QComboBox;
    form->addRow(_("Menu key"), menuKeyChoice);

    menuKeyChoice->addItem(_("None"));
    menuKeyChoice->insertSeparator(1);
    for (int idx = 0; idx < getMenuKeySymbolCount(); idx++)
      menuKeyChoice->addItem(getMenuKeySymbols()[idx].name);
  }

  /* Clipboard */
  clipboardGroup = new QGroupBox(_("Clipboard"));
  layout->addWidget(clipboardGroup);

  {
    QBoxLayout* box = new QVBoxLayout;
    clipboardGroup->setLayout(box);

    acceptClipboardCheckbox = new QCheckBox(_("Accept clipboard from server"));
    connect(acceptClipboardCheckbox, &QCheckBox::toggled,
            this, &OptionsInput::handleClipboard);
    box->addWidget(acceptClipboardCheckbox);

#if !defined(WIN32) && !defined(__APPLE__)
    indent = new QHBoxLayout;
    indent->addSpacing(20);
    setPrimaryCheckbox = new QCheckBox(_("Also set primary selection"));
    indent->addWidget(setPrimaryCheckbox);
    box->addLayout(indent);
#else
    (void)indent;
#endif

    sendClipboardCheckbox = new QCheckBox(_("Send clipboard to server"));
    connect(sendClipboardCheckbox, &QCheckBox::toggled,
            this, &OptionsInput::handleClipboard);
    box->addWidget(sendClipboardCheckbox);

#if !defined(WIN32) && !defined(__APPLE__)
    indent = new QHBoxLayout;
    indent->addSpacing(20);
    sendPrimaryCheckbox = new QCheckBox(_("Send primary selection as clipboard"));
    indent->addWidget(sendPrimaryCheckbox);
    box->addLayout(indent);
#else
    (void)indent;
#endif
  }

  layout->addStretch(1);

  setLayout(layout);
}

void OptionsInput::loadOptions()
{
  viewOnlyCheckbox->setChecked(viewOnly);
  emulateMBCheckbox->setChecked(emulateMiddleButton);
  dotCursorCheckbox->setChecked(dotWhenNoCursor);
  acceptClipboardCheckbox->setChecked(acceptClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimaryCheckbox->setChecked(setPrimary);
#endif
  sendClipboardCheckbox->setChecked(sendClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  sendPrimaryCheckbox->setChecked(sendPrimary);
#endif
  systemKeysCheckbox->setChecked(fullscreenSystemKeys);

  menuKeyChoice->setCurrentIndex(0);
  menuKeyChoice->setCurrentText((const char*)menuKey);
}

void OptionsInput::storeOptions()
{
  viewOnly.setParam(viewOnlyCheckbox->isChecked());
  emulateMiddleButton.setParam(emulateMBCheckbox->isChecked());
  dotWhenNoCursor.setParam(dotCursorCheckbox->isChecked());
  acceptClipboard.setParam(acceptClipboardCheckbox->isChecked());
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimary.setParam(setPrimaryCheckbox->isChecked());
#endif
  sendClipboard.setParam(sendClipboardCheckbox->isChecked());
#if !defined(WIN32) && !defined(__APPLE__)
  sendPrimary.setParam(sendPrimaryCheckbox->isChecked());
#endif
  fullscreenSystemKeys.setParam(systemKeysCheckbox->isChecked());

  if (menuKeyChoice->currentIndex() == 0)
    menuKey.setParam("");
  else {
    menuKey.setParam(menuKeyChoice->currentText().toStdString());
  }
}

void OptionsInput::handleClipboard()
{
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimaryCheckbox->setEnabled(acceptClipboardCheckbox->isChecked());
  sendPrimaryCheckbox->setEnabled(sendClipboardCheckbox->isChecked());
#endif
}
