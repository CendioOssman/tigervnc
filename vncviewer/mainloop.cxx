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

#include <assert.h>

#include <string>

#include <QApplication>
#include <QMessageBox>

#include <rdr/Exception.h>

#include <rfb/util.h>

#include "CConn.h"
#include "appmanager.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"

using namespace rfb;

static bool inMainloop = false;
static bool exitMainloop = false;
static std::string exitError;
static bool fatalError = false;

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

  if (inMainloop) {
    exitMainloop = true;
    qApp->quit();
  } else {
    // We're early in the startup. Assume we can just exit().
    if (alertOnFatalError) {
      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                      _("Error"),
                                      exitError.c_str(),
                                      QMessageBox::Close);
      AppManager::instance()->openDialog(d);
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
  qApp->quit();
}

void abort_connection_unexpected(const rdr::Exception &e)
{
  abort_connection_unexpected("%s", e.str());
}

void abort_connection_unexpected(const char *error, ...)
{
  va_list ap;
  std::string error_str;

  va_start(ap, error);
  error_str = vformat(error, ap);
  va_end(ap);

  abort_connection(_("An unexpected error occurred when communicating "
                     "with the server:\n\n%s"), error_str.c_str());
}

void disconnect()
{
  exitMainloop = true;
  qApp->quit();
}

void mainloop(const char* vncserver, network::Socket* sock)
{
  inMainloop = true;

  while (true) {
    CConn *cc;

    exitMainloop = false;

    cc = new CConn(vncserver, sock);

    if (!exitMainloop)
      qApp->exec();

    delete cc;

    if (fatalError) {
      assert(!exitError.empty());
      if (alertOnFatalError) {
        QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                        _("Connection error"),
                                        exitError.c_str(),
                                        QMessageBox::Close);
        d->exec();
        delete d;
      }
      break;
    }

    if (exitError.empty())
      break;

    if(reconnectOnError && (sock == nullptr)) {
      std::string text;
      text = format(_("%s\n\nAttempt to reconnect?"),
                    exitError.c_str());

      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                       _("Connection error"),
                                       text.c_str(),
                                       QMessageBox::NoButton);
      d->addButton(_("Reconnect"), QMessageBox::AcceptRole);
      d->addButton(QMessageBox::Close);

      d->exec();
      QMessageBox::ButtonRole clickedButtonRole = d->buttonRole(d->clickedButton());
      delete d;
      exitError.clear();
      if (clickedButtonRole == QMessageBox::AcceptRole)
        continue;
      else
        break;
    }

    if (alertOnFatalError) {
      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                      _("Connection error"),
                                      exitError.c_str(),
                                      QMessageBox::Close);
      d->exec();
      delete d;
    }

    break;
  }

  inMainloop = false;
}
