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

#include <FL/Fl_Box.H>
#include <FL/Fl_Toggle_Button.H>

#include <core/i18n.h>
#include <core/string.h>

#include "OptionsShortcuts.h"
#include "ShortcutHandler.h"
#include "parameters.h"

#include "fltk/layout.h"
#include "fltk/util.h"

OptionsShortcuts::OptionsShortcuts(int tx, int ty, int tw, int th)
  : OptionsPage(tx, ty, tw, th, _("Keyboard shortcuts"))
{
  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  Fl_Box *intro = fltk_box(tx, ty, tw - OUTER_MARGIN * 2, INPUT_HEIGHT);
  intro->align(FL_ALIGN_TOP_LEFT|FL_ALIGN_INSIDE);
  intro->label(_("Modifier keys for keyboard shortcuts:"));

  ty += INPUT_HEIGHT + INNER_MARGIN;

  int width;

  width = (tw - OUTER_MARGIN * 2 - INNER_MARGIN * 3) / 4;

  ctrlButton = new Fl_Toggle_Button(tx, ty,
                                    /*
                                     * TRANSLATORS: This refers to the
                                     * keyboard key
                                     * */
                                    width, BUTTON_HEIGHT, _("Ctrl"));
  ctrlButton->selection_color(FL_SELECTION_COLOR);
  ctrlButton->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsShortcuts*)data)->handleModifier();
    },
    this);
  shiftButton = new Fl_Toggle_Button(tx + width + INNER_MARGIN, ty,
                                     /*
                                      * TRANSLATORS: This refers to the
                                      * keyboard key
                                      * */
                                     width, BUTTON_HEIGHT, _("Shift"));
  shiftButton->selection_color(FL_SELECTION_COLOR);
  shiftButton->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsShortcuts*)data)->handleModifier();
    },
    this);
  altButton = new Fl_Toggle_Button(tx + width * 2 + INNER_MARGIN * 2, ty,
                                    /*
                                     * TRANSLATORS: This refers to the
                                     * keyboard key
                                     * */
                                   width, BUTTON_HEIGHT, _("Alt"));
  altButton->selection_color(FL_SELECTION_COLOR);
  altButton->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsShortcuts*)data)->handleModifier();
    },
    this);
  superButton = new Fl_Toggle_Button(tx + width * 3 + INNER_MARGIN * 3, ty,
                                     /*
                                      * TRANSLATORS: This refers to the
                                      * keyboard key
                                      * */
                                     width, BUTTON_HEIGHT, _("Win"));
  superButton->selection_color(FL_SELECTION_COLOR);
  superButton->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsShortcuts*)data)->handleModifier();
    },
    this);

#ifdef __APPLE__
  /* TRANSLATORS: This refers to the keyboard key */
  ctrlButton->label(_("⌃ Ctrl"));
  /* TRANSLATORS: This refers to the keyboard key */
  shiftButton->label(_("⇧ Shift"));
  /* TRANSLATORS: This refers to the keyboard key */
  altButton->label(_("⌥ Option"));
  /* TRANSLATORS: This refers to the keyboard key */
  superButton->label(_("⌘ Cmd"));
#endif

  ty += BUTTON_HEIGHT + INNER_MARGIN;

  shortcutsText = fltk_box(tx, ty, tw - OUTER_MARGIN * 2, th - ty - OUTER_MARGIN);
  shortcutsText->align(FL_ALIGN_TOP_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);

  end();
}

void OptionsShortcuts::loadOptions()
{
  unsigned modifierMask;

  modifierMask = 0;
  for (core::EnumListEntry key : shortcutModifiers)
    modifierMask |= ShortcutHandler::parseModifier(key.getValueStr().c_str());

  ctrlButton->value(modifierMask & ShortcutHandler::Control);
  shiftButton->value(modifierMask & ShortcutHandler::Shift);
  altButton->value(modifierMask & ShortcutHandler::Alt);
  superButton->value(modifierMask & ShortcutHandler::Super);

  handleModifier();
}

void OptionsShortcuts::storeOptions()
{
  std::list<std::string> modifierList;

  if (ctrlButton->value())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Control));
  if (shiftButton->value())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Shift));
  if (altButton->value())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Alt));
  if (superButton->value())
    modifierList.push_back(
      ShortcutHandler::modifierString(ShortcutHandler::Super));

  shortcutModifiers.setParam(modifierList);
}

void OptionsShortcuts::handleModifier()
{
  unsigned mask;

  mask = 0;
  if (ctrlButton->value())
    mask |= ShortcutHandler::Control;
  if (shiftButton->value())
    mask |= ShortcutHandler::Shift;
  if (altButton->value())
    mask |= ShortcutHandler::Alt;
  if (superButton->value())
    mask |= ShortcutHandler::Super;

  if (mask == 0) {
    shortcutsText->copy_label(
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

    shortcutsText->copy_label(label.c_str());
  }
}
