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

#include "OptionsDialog.h"
#include "i18n.h"

#include "OptionsCompression.h"
#include "OptionsSecurity.h"
#include "OptionsInput.h"
#include "OptionsDisplay.h"
#include "OptionsMisc.h"

#include "fltk/layout.h"
#include "fltk/Fl_Navigation.h"

#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>

std::map<OptionsCallback*, void*> OptionsDialog::callbacks;

OptionsDialog::OptionsDialog()
  : Fl_Window(580, 420, _("TigerVNC Options"))
{
  int x, y;
  Fl_Navigation *navigation;
  Fl_Button *button;

  // Odd dimensions to get rid of extra borders
  // FIXME: We need to retain the top border on Windows as they don't
  //        have any separator for the caption, which looks odd
#ifdef WIN32
  navigation = new Fl_Navigation(-1, 0, w()+2,
                                 h() - OUTER_MARGIN - BUTTON_HEIGHT - OUTER_MARGIN);
#else
  navigation = new Fl_Navigation(-1, -1, w()+2,
                                 h()+1 - OUTER_MARGIN - BUTTON_HEIGHT - OUTER_MARGIN);
#endif
  {
    int tx, ty, tw, th;

    navigation->client_area(tx, ty, tw, th, 150);

    pages.push_back(new OptionsCompression(tx, ty, tw, th));
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
    pages.push_back(new OptionsSecurity(tx, ty, tw, th));
#endif
    pages.push_back(new OptionsInput(tx, ty, tw, th));
    pages.push_back(new OptionsDisplay(tx, ty, tw, th));
    pages.push_back(new OptionsMisc(tx, ty, tw, th));
  }

  navigation->end();

  x = w() - BUTTON_WIDTH * 2 - INNER_MARGIN - OUTER_MARGIN;
  y = h() - BUTTON_HEIGHT - OUTER_MARGIN;

  button = new Fl_Button(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("Cancel"));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsDialog*)data)->handleCancel();
    },
    this);

  x += BUTTON_WIDTH + INNER_MARGIN;

  button = new Fl_Return_Button(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, _("OK"));
  button->callback(
    [](Fl_Widget*, void* data) {
      ((OptionsDialog*)data)->handleOK();
    },
    this);

  callback(
    [](Fl_Widget*, void* data) {
      ((OptionsDialog*)data)->handleCancel();
    },
    this);

  set_modal();
}


OptionsDialog::~OptionsDialog()
{
}


void OptionsDialog::showDialog(void)
{
  static OptionsDialog *dialog = nullptr;

  if (!dialog)
    dialog = new OptionsDialog();

  if (dialog->shown())
    return;

  dialog->show();
}


void OptionsDialog::addCallback(OptionsCallback *cb, void *data)
{
  callbacks[cb] = data;
}


void OptionsDialog::removeCallback(OptionsCallback *cb)
{
  callbacks.erase(cb);
}


void OptionsDialog::show(void)
{
  /* show() gets called for raise events as well */
  if (!shown())
    loadOptions();

  Fl_Window::show();
}


void OptionsDialog::loadOptions(void)
{
  for (OptionsPage *page : pages)
    page->loadOptions();
}


void OptionsDialog::storeOptions(void)
{
  for (OptionsPage *page : pages)
    page->storeOptions();

  std::map<OptionsCallback*, void*>::const_iterator iter;

  for (iter = callbacks.begin();iter != callbacks.end();++iter)
    iter->first(iter->second);
}


void OptionsDialog::handleCancel()
{
  hide();
}


void OptionsDialog::handleOK()
{
  hide();

  storeOptions();
}
