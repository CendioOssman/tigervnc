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
#include <FL/Fl_Choice.H>

#include <core/i18n.h>

#include "OptionsInput.h"
#include "parameters.h"

#include "fltk/layout.h"
#include "fltk/util.h"

#ifdef __APPLE__
#include "cocoa.h"
#endif

OptionsInput::OptionsInput(int tx, int ty, int tw, int th)
  : OptionsPage(tx, ty, tw, th, _("Input"))
{
  int orig_tx;
  int width;

  tx += OUTER_MARGIN;
  ty += OUTER_MARGIN;

  width = tw - OUTER_MARGIN * 2;

  viewOnlyCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                  CHECK_MIN_WIDTH,
                                                  CHECK_HEIGHT,
                                                  _("View only (ignore mouse and keyboard)")));
  ty += CHECK_HEIGHT + INNER_MARGIN;

  orig_tx = tx;

  /* Mouse */
  ty += GROUP_LABEL_OFFSET;
  mouseGroup = new Fl_Group(tx, ty, width, 0, _("Mouse"));
  mouseGroup->labelfont(FL_BOLD);
  mouseGroup->box(FL_FLAT_BOX);
  mouseGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    emulateMBCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                     CHECK_MIN_WIDTH,
                                                     CHECK_HEIGHT,
                                                     _("Emulate middle mouse button")));
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    alwaysCursorCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                       CHECK_MIN_WIDTH,
                                                       CHECK_HEIGHT,
                                                       _("Show local cursor when not provided by server")));
    alwaysCursorCheckbox->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsInput*)data)->handleAlwaysCursor();
      },
      this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

    /* Cursor type */
    cursorTypeChoice = new Fl_Choice(LBLLEFT(tx, ty, 150, CHOICE_HEIGHT, _("Cursor type")));

    fltk_menu_add(cursorTypeChoice, _("Dot"), 0, nullptr, nullptr, 0);
    fltk_menu_add(cursorTypeChoice, _("System"), 0, nullptr, nullptr, 0);

    fltk_adjust_choice(cursorTypeChoice);

    ty += CHOICE_HEIGHT + TIGHT_MARGIN;

  }
  ty -= TIGHT_MARGIN;

  mouseGroup->end();
  /* Needed for resize to work sanely */
  mouseGroup->resizable(nullptr);
  mouseGroup->size(mouseGroup->w(), ty - mouseGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Keyboard */
  ty += GROUP_LABEL_OFFSET;
  keyboardGroup = new Fl_Group(tx, ty, width, 0, _("Keyboard"));
  keyboardGroup->labelfont(FL_BOLD);
  keyboardGroup->box(FL_FLAT_BOX);
  keyboardGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    systemKeysCheckbox = new Fl_Check_Button(
      LBLRIGHT(tx, ty, CHECK_MIN_WIDTH, CHECK_HEIGHT,
               _("Always send all keyboard input in full screen")));
    systemKeysCheckbox->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsInput*)data)->handleSystemKeys();
      },
      this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
  }
  ty -= TIGHT_MARGIN;

  keyboardGroup->end();
  /* Needed for resize to work sanely */
  keyboardGroup->resizable(nullptr);
  keyboardGroup->size(keyboardGroup->w(), ty - keyboardGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  /* Clipboard */
  ty += GROUP_LABEL_OFFSET;
  clipboardGroup = new Fl_Group(tx, ty, width, 0, _("Clipboard"));
  clipboardGroup->labelfont(FL_BOLD);
  clipboardGroup->box(FL_FLAT_BOX);
  clipboardGroup->align(FL_ALIGN_LEFT | FL_ALIGN_TOP);

  {
    tx += INDENT;
    ty += TIGHT_MARGIN;

    acceptClipboardCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                           CHECK_MIN_WIDTH,
                                                           CHECK_HEIGHT,
                                                           _("Accept clipboard from server")));
    acceptClipboardCheckbox->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsInput*)data)->handleClipboard();
      },
      this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

#if !defined(WIN32) && !defined(__APPLE__)
    setPrimaryCheckbox = new Fl_Check_Button(LBLRIGHT(tx + INDENT, ty,
                                                      CHECK_MIN_WIDTH,
                                                      CHECK_HEIGHT,
                                                      _("Also set primary selection")));
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
#endif

    sendClipboardCheckbox = new Fl_Check_Button(LBLRIGHT(tx, ty,
                                                         CHECK_MIN_WIDTH,
                                                         CHECK_HEIGHT,
                                                         _("Send clipboard to server")));
    sendClipboardCheckbox->callback(
      [](Fl_Widget*, void* data) {
        ((OptionsInput*)data)->handleClipboard();
      },
      this);
    ty += CHECK_HEIGHT + TIGHT_MARGIN;

#if !defined(WIN32) && !defined(__APPLE__)
    sendPrimaryCheckbox = new Fl_Check_Button(LBLRIGHT(tx + INDENT, ty,
                                                       CHECK_MIN_WIDTH,
                                                       CHECK_HEIGHT,
                                                       _("Send primary selection as clipboard")));
    ty += CHECK_HEIGHT + TIGHT_MARGIN;
#endif
  }
  ty -= TIGHT_MARGIN;

  clipboardGroup->end();
  /* Needed for resize to work sanely */
  clipboardGroup->resizable(nullptr);
  clipboardGroup->size(clipboardGroup->w(), ty - clipboardGroup->y());

  /* Back to normal */
  tx = orig_tx;
  ty += INNER_MARGIN;

  end();
}

void OptionsInput::loadOptions()
{
  viewOnlyCheckbox->value(viewOnly);
  emulateMBCheckbox->value(emulateMiddleButton);
  alwaysCursorCheckbox->value(alwaysCursor);
  if (cursorType == "system") {
    cursorTypeChoice->value(1);
  } else {
    // Default
    cursorTypeChoice->value(0);
  }
  handleAlwaysCursor();
  acceptClipboardCheckbox->value(acceptClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimaryCheckbox->value(setPrimary);
#endif
  sendClipboardCheckbox->value(sendClipboard);
#if !defined(WIN32) && !defined(__APPLE__)
  sendPrimaryCheckbox->value(sendPrimary);
#endif
  systemKeysCheckbox->value(fullscreenSystemKeys);
}

void OptionsInput::storeOptions()
{
  viewOnly.setParam(viewOnlyCheckbox->value());
  emulateMiddleButton.setParam(emulateMBCheckbox->value());
  alwaysCursor.setParam(alwaysCursorCheckbox->value());
  if (cursorTypeChoice->value() == 1) {
    cursorType.setParam("System");
  } else {
    // Default
    cursorType.setParam("Dot");
  }
  acceptClipboard.setParam(acceptClipboardCheckbox->value());
#if !defined(WIN32) && !defined(__APPLE__)
  setPrimary.setParam(setPrimaryCheckbox->value());
#endif
  sendClipboard.setParam(sendClipboardCheckbox->value());
#if !defined(WIN32) && !defined(__APPLE__)
  sendPrimary.setParam(sendPrimaryCheckbox->value());
#endif
  fullscreenSystemKeys.setParam(systemKeysCheckbox->value());
}

void OptionsInput::handleAlwaysCursor()
{
  if (alwaysCursorCheckbox->value()) {
    cursorTypeChoice->activate();
  } else {
    cursorTypeChoice->deactivate();
  }
}

void OptionsInput::handleSystemKeys()
{
#ifdef __APPLE__
  // Pop up the access dialog if needed
  if (systemKeysCheckbox->value())
    cocoa_is_trusted(true);
#endif
}

void OptionsInput::handleClipboard()
{
#if !defined(WIN32) && !defined(__APPLE__)
  if (acceptClipboardCheckbox->value())
    setPrimaryCheckbox->activate();
  else
    setPrimaryCheckbox->deactivate();
  if (sendClipboardCheckbox->value())
    sendPrimaryCheckbox->activate();
  else
    sendPrimaryCheckbox->deactivate();
#endif
}
