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
#include <QLabel>
#include <QPushButton>

#include <core/Configuration.h>
#include <core/i18n.h>
#include <core/string.h>

#include "OptionsShortcuts.h"
#include "ShortcutHandler.h"
#include "parameters.h"

OptionsShortcuts::OptionsShortcuts(QWidget* parent)
  : OptionsPage(parent)
{
  QBoxLayout* layout = new QVBoxLayout;

  QLabel* intro = new QLabel;
  intro->setText(_("Modifier keys for keyboard shortcuts:"));
  layout->addWidget(intro);

  QBoxLayout* box = new QHBoxLayout;
  layout->addLayout(box);

  /*
   * TRANSLATORS: This refers to the
   * keyboard key
   */
  ctrlButton = new QPushButton(_("Ctrl"));
  ctrlButton->setCheckable(true);
  connect(ctrlButton, &QPushButton::toggled,
          this, &OptionsShortcuts::handleModifier);
  box->addWidget(ctrlButton);
  /*
   * TRANSLATORS: This refers to the
   * keyboard key
   */
  shiftButton = new QPushButton(_("Shift"));
  shiftButton->setCheckable(true);
  connect(shiftButton, &QPushButton::toggled,
          this, &OptionsShortcuts::handleModifier);
  box->addWidget(shiftButton);
  /*
   * TRANSLATORS: This refers to the
   * keyboard key
   */
  altButton = new QPushButton(_("Alt"));
  altButton->setCheckable(true);
  connect(altButton, &QPushButton::toggled,
          this, &OptionsShortcuts::handleModifier);
  box->addWidget(altButton);
  /*
   * TRANSLATORS: This refers to the
   * keyboard key
   */
  superButton = new QPushButton(_("Win"));
  superButton->setCheckable(true);
  connect(superButton, &QPushButton::toggled,
          this, &OptionsShortcuts::handleModifier);
  box->addWidget(superButton);

#ifdef __APPLE__
  /* TRANSLATORS: This refers to the keyboard key */
  ctrlButton->setText(_("⌃ Ctrl"));
  /* TRANSLATORS: This refers to the keyboard key */
  shiftButton->setText(_("⇧ Shift"));
  /* TRANSLATORS: This refers to the keyboard key */
  altButton->setText(_("⌥ Option"));
  /* TRANSLATORS: This refers to the keyboard key */
  superButton->setText(_("⌘ Cmd"));
#endif

  shortcutsText = new QLabel;
  shortcutsText->setWordWrap(true);
  shortcutsText->setTextFormat(Qt::PlainText);
  layout->addWidget(shortcutsText);

  layout->addStretch(1);

  setLayout(layout);
}

void OptionsShortcuts::loadOptions()
{
  unsigned modifierMask;

  modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  ctrlButton->setChecked(modifierMask & ShortcutHandler::Control);
  shiftButton->setChecked(modifierMask & ShortcutHandler::Shift);
  altButton->setChecked(modifierMask & ShortcutHandler::Alt);
  superButton->setChecked(modifierMask & ShortcutHandler::Super);
}

void OptionsShortcuts::storeOptions()
{
  std::list<std::string> modifierList;

  if (ctrlButton->isChecked())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Control));
  if (shiftButton->isChecked())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Shift));
  if (altButton->isChecked())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Alt));
  if (superButton->isChecked())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Super));

  shortcutModifiers.setParam(modifierList);
}

void OptionsShortcuts::handleModifier()
{
  unsigned mask;

  mask = 0;
  if (ctrlButton->isChecked())
    mask |= ShortcutHandler::Control;
  if (shiftButton->isChecked())
    mask |= ShortcutHandler::Shift;
  if (altButton->isChecked())
    mask |= ShortcutHandler::Alt;
  if (superButton->isChecked())
    mask |= ShortcutHandler::Super;

  if (mask == 0) {
    shortcutsText->setText(
      _("All keyboard shortcuts are disabled."));
  } else {
    char prefix[256];
    char prefix_noplus[256];

    std::string label;

    strcpy(prefix, ShortcutHandler::modifierPrefix(mask));
    strcpy(prefix_noplus, ShortcutHandler::modifierPrefix(mask, true));

    label += core::format(
      _("To release keyboard control from the session, press %s."),
      prefix_noplus);
    label += "\n\n";

    label += core::format(
      _("To pass all keyboard input to the session, press %sG."),
      prefix);
    label += "\n\n";

    label += core::format(
      _("To toggle full-screen mode, press %sEnter."), prefix);
    label += "\n\n";

    label += core::format(
      _("To open the session context menu, press %sM."), prefix);
    label += "\n\n";

    label += core::format(
      _("To send a key combination that includes %s directly to the "
        "session, press %sSpace, release the space bar without "
        "releasing %s, and press the desired key."),
      prefix_noplus, prefix, prefix_noplus);

    shortcutsText->setText(label.c_str());
  }
}
