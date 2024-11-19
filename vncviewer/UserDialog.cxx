/* Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Pixmap.H>

#include <rfb/Exception.h>
#include <rfb/obfuscate.h>

#include "fltk/Fl_Message_Box.h"
#include "fltk/layout.h"
#include "fltk/util.h"
#include "i18n.h"
#include "parameters.h"
#include "UserDialog.h"

#include "AuthDialog.h"

using namespace rfb;

UserDialog::UserDialog()
{
}

UserDialog::~UserDialog()
{
}

void UserDialog::resetPassword()
{
  savedUsername.clear();
  savedPassword.clear();
}

void UserDialog::getUserPasswd(bool secure_, std::string* user,
                               std::string* password)
{
  const char *passwordFileName(passwordFile);
  int ret_val;

  assert(password);
  char *envUsername = getenv("VNC_USERNAME");
  char *envPassword = getenv("VNC_PASSWORD");

  if(user && envUsername && envPassword) {
    *user = envUsername;
    *password = envPassword;
    return;
  }

  if (!user && envPassword) {
    *password = envPassword;
    return;
  }

  if (user && !savedUsername.empty() && !savedPassword.empty()) {
    *user = savedUsername;
    *password = savedPassword;
    return;
  }

  if (!user && !savedPassword.empty()) {
    *password = savedPassword;
    return;
  }

  if (!user && passwordFileName[0]) {
    std::vector<uint8_t> obfPwd(8);
    FILE* fp;

    fp = fopen(passwordFileName, "rb");
    if (!fp)
      throw rdr::SystemException(_("Opening password file failed"), errno);

    obfPwd.resize(fread(obfPwd.data(), 1, obfPwd.size(), fp));
    fclose(fp);

    *password = deobfuscate(obfPwd.data(), obfPwd.size());

    return;
  }

  AuthDialog d(secure_, user != nullptr, password != nullptr);
  d.show();
  while (d.shown())
    Fl::wait();
  ret_val = d.result();

  if (ret_val == 0) {
    bool keepPasswd;

    if (reconnectOnError)
      keepPasswd = d.getKeepPassword();
    else
      keepPasswd = false;

    if (user) {
      *user = d.getUser();
      if (keepPasswd)
        savedUsername = d.getUser();
    }
    *password = d.getPassword();
    if (keepPasswd)
      savedPassword = d.getPassword();
  }

  if (ret_val != 0)
    throw rfb::AuthCancelledException();
}

bool UserDialog::showMsgBox(MsgBoxFlags flags, const char* title, const char* text)
{
  char buffer[1024];

  if (fltk_escape(text, buffer, sizeof(buffer)) >= sizeof(buffer))
    return 0;

  // FLTK doesn't give us a flexible choice of the icon, so we ignore those
  // bits for now.

  if ((flags & 0xf) == M_OKCANCEL) {
    Fl_Choice_Box* dlg;
    int ret;

    dlg = new Fl_Choice_Box(title, "%s",
                            nullptr, fl_ok, fl_cancel, buffer);
    dlg->set_modal();
    dlg->show();
    while (dlg->shown())
      Fl::wait();
    ret = dlg->result();
    delete dlg;

    return ret == 1;
  } else if ((flags & 0xf) == M_YESNO) {
    Fl_Choice_Box* dlg;
    int ret;

    dlg = new Fl_Choice_Box(title, "%s",
                            nullptr, fl_yes, fl_no, buffer);
    dlg->set_modal();
    dlg->show();
    while (dlg->shown())
      Fl::wait();
    ret = dlg->result();
    delete dlg;

    return ret == 1;
  } else {
    Fl_Window* dlg;

    if (((flags & 0xf0) == M_ICONERROR) ||
        ((flags & 0xf0) == M_ICONWARNING))
      dlg = new Fl_Alert_Box(title, "%s", buffer);
    else
      dlg = new Fl_Message_Box(title, "%s", buffer);

    dlg->set_modal();
    dlg->show();
    while (dlg->shown())
      Fl::wait();
    delete dlg;

    return true;
  }

  return false;
}
