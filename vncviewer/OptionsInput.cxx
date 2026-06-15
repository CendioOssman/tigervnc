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

#include "OptionsInput.h"

#include "parameters.h"
#include "menukey.h"
#include "i18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QStringListModel>
#include <QVBoxLayout>

OptionsInput::OptionsInput(QWidget* parent)
  : OptionsPage{parent}
{
  QVBoxLayout* layout = new QVBoxLayout;

  viewOnlyCheckbox = new QCheckBox(_("View only (ignore mouse and keyboard)"));
  layout->addWidget(viewOnlyCheckbox);

  QGroupBox* groupBox1 = new QGroupBox(_("Mouse"));
  QVBoxLayout* vbox1 = new QVBoxLayout;
  emulateMBCheckbox = new QCheckBox(_("Emulate middle mouse button"));
  vbox1->addWidget(emulateMBCheckbox);
  dotCursorCheckbox = new QCheckBox(_("Show dot when no cursor"));
  vbox1->addWidget(dotCursorCheckbox);
  groupBox1->setLayout(vbox1);
  layout->addWidget(groupBox1);

  QGroupBox* groupBox2 = new QGroupBox(_("Keyboard"));
  QVBoxLayout* vbox2 = new QVBoxLayout;
  systemKeysCheckbox = new QCheckBox(_("Pass system keys directly to server (full screen)"));
  vbox2->addWidget(systemKeysCheckbox);
  QHBoxLayout* hbox2 = new QHBoxLayout;
  QLabel* label = new QLabel(_("Menu key"));
  hbox2->addWidget(label);
  menuKeyChoice = new QComboBox;
  QStringListModel* model = new QStringListModel;
  QStringList menuKeys;
  const MenuKeySymbol* keysyms = getMenuKeySymbols();
  for (int i = 0; i < getMenuKeySymbolCount(); i++) {
    menuKeys.append(keysyms[i].name);
  }
  model->setStringList(menuKeys);
  menuKeyChoice->setModel(model);
  hbox2->addWidget(menuKeyChoice);
  hbox2->addStretch(1);
  vbox2->addLayout(hbox2);
  groupBox2->setLayout(vbox2);
  layout->addWidget(groupBox2);

  QGroupBox* groupBox3 = new QGroupBox(_("Clipboard"));
  QVBoxLayout* vbox3 = new QVBoxLayout;
  acceptClipboardCheckbox = new QCheckBox(_("Accept clipboard from server"));
  vbox3->addWidget(acceptClipboardCheckbox);
#if !defined(WIN32) && !defined(__APPLE__)
  QHBoxLayout* h1 = new QHBoxLayout;
  h1->addSpacing(20);
  setPrimaryCheckbox = new QCheckBox(_("Also set primary selection"));
  h1->addWidget(setPrimaryCheckbox);
  vbox3->addLayout(h1);
#endif
  sendClipboardCheckbox = new QCheckBox(_("Send clipboard to server"));
  vbox3->addWidget(sendClipboardCheckbox);
#if !defined(WIN32) && !defined(__APPLE__)
  QHBoxLayout* h2 = new QHBoxLayout;
  h2->addSpacing(20);
  sendPrimaryCheckbox = new QCheckBox(_("Send primary selection as clipboard"));
  h2->addWidget(sendPrimaryCheckbox);
  vbox3->addLayout(h2);
#endif
  groupBox3->setLayout(vbox3);
  layout->addWidget(groupBox3);

  layout->addStretch(1);
  setLayout(layout);
}

void OptionsInput::apply()
{
  ::viewOnly.setParam(viewOnlyCheckbox->isChecked());
  ::emulateMiddleButton.setParam(emulateMBCheckbox->isChecked());
  ::dotWhenNoCursor.setParam(dotCursorCheckbox->isChecked());
  ::fullscreenSystemKeys.setParam(systemKeysCheckbox->isChecked());
  ::menuKey.setParam(menuKeyChoice->currentText().toStdString().c_str());
  ::acceptClipboard.setParam(acceptClipboardCheckbox->isChecked());
  ::sendClipboard.setParam(sendClipboardCheckbox->isChecked());
#if !defined(WIN32) && !defined(__APPLE__)
  ::setPrimary.setParam(setPrimaryCheckbox->isChecked());
  ::sendPrimary.setParam(sendPrimaryCheckbox->isChecked());
#endif
}

void OptionsInput::reset()
{
  viewOnlyCheckbox->setChecked(::viewOnly);
  emulateMBCheckbox->setChecked(::emulateMiddleButton);
  dotCursorCheckbox->setChecked(::dotWhenNoCursor);
  systemKeysCheckbox->setChecked(::fullscreenSystemKeys);
  menuKeyChoice->setCurrentText(::menuKey.getValueStr().c_str());
  acceptClipboardCheckbox->setChecked(::acceptClipboard);
  sendClipboardCheckbox->setChecked(::sendClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimaryCheckbox->setChecked(::setPrimary);
  sendPrimaryCheckbox->setChecked(::sendPrimary);
#endif
}
