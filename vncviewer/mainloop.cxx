/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2026 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
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

#include <string>

#include <FL/Fl.H>
#include <FL/fl_ask.H>

#include <rdr/Exception.h>

#include <rfb/LogWriter.h>
#include <rfb/Timer.h>
#include <rfb/util.h>

#include "fltk/Fl_Message_Box.h"
#include "CConn.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"

using namespace rfb;

static bool inMainloop = false;
static bool exitMainloop = false;
static std::string exitError;
static bool fatalError = false;

static rfb::LogWriter vlog("mainloop");

void abort_vncviewer(const char *error, ...)
{
  fatalError = true;

  // Prioritise the first error we get as that is probably the most
  // relevant one.
  if (exitError.empty()) {
    va_list ap;

    va_start(ap, error);
    exitError = vformat(error, ap);
    va_end(ap);
  }

  if (inMainloop)
    exitMainloop = true;
  else {
    // We're early in the startup. Assume we can just exit().
    if (alertOnFatalError) {
      Fl_Alert_Box* dlg;

      dlg = new Fl_Alert_Box(_("Error"), "%s", exitError.c_str());
      dlg->set_modal();
      dlg->show();
      while (dlg->shown())
        Fl::wait();
      delete dlg;
    }
    exit(EXIT_FAILURE);
  }
}

void abort_connection(const char *error, ...)
{
  assert(inMainloop);

  // Prioritise the first error we get as that is probably the most
  // relevant one.
  if (exitError.empty()) {
    va_list ap;

    va_start(ap, error);
    exitError = vformat(error, ap);
    va_end(ap);
  }

  exitMainloop = true;
}

void abort_connection_with_unexpected_error(const rdr::Exception &e) {
  abort_connection(_("An unexpected error occurred when communicating "
                     "with the server:\n\n%s"), e.str());
}

void disconnect()
{
  exitMainloop = true;
}

void mainloop(const char* vncserver, network::Socket* sock)
{
  inMainloop = true;

  while (true) {
    CConn *cc;

    exitMainloop = false;

    cc = new CConn(vncserver, sock);

    while (!exitMainloop) {
      int next_timer;

      next_timer = Timer::checkTimeouts();
      if (next_timer < 0)
        next_timer = INT_MAX;

      if (Fl::wait((double)next_timer / 1000.0) < 0.0) {
        vlog.error(_("Internal FLTK error. Exiting."));
        exit(-1);
      }
    }

    delete cc;

    if (fatalError) {
      assert(!exitError.empty());
      if (alertOnFatalError) {
        Fl_Alert_Box* dlg;

        dlg = new Fl_Alert_Box(_("Connection error"),
                               "%s", exitError.c_str());
        dlg->set_modal();
        dlg->show();
        while (dlg->shown())
          Fl::wait();
        delete dlg;
      }
      break;
    }

    if (exitError.empty())
      break;

    if(reconnectOnError && (sock == nullptr)) {
      Fl_Choice_Box* dlg;
      int ret;

      dlg = new Fl_Choice_Box(_("Connection error"),
                              _("%s\n\nAttempt to reconnect?"),
                              nullptr, fl_yes, fl_no,
                              exitError.c_str());
      dlg->set_modal();
      dlg->show();
      while (dlg->shown())
        Fl::wait();
      ret = dlg->result();
      delete dlg;

      exitError.clear();
      if (ret == 1)
        continue;
      else
        break;
    }

    if (alertOnFatalError) {
      Fl_Alert_Box* dlg;

      dlg = new Fl_Alert_Box(_("Connection error"),
                              "%s", exitError.c_str());
      dlg->set_modal();
      dlg->show();
      while (dlg->shown())
        Fl::wait();
      delete dlg;
    }

    break;
  }

  inMainloop = false;
}
