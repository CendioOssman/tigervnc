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
#include <sys/stat.h>

#include <string>

#include <FL/fl_ask.H>

#include <rdr/Exception.h>

#include <network/TcpSocket.h>

#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/Timer.h>
#include <rfb/util.h>

#include "fltk/Fl_Message_Box.h"
#include "CConn.h"
#include "ServerDialog.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"

using namespace network;
using namespace rfb;

static bool inMainloop = false;
static bool exitMainloop = false;
static std::string exitError;
static bool fatalError = false;

static std::string vncServerName;
static network::Socket* sock = nullptr;
static CConn* cc = nullptr;

static rfb::LogWriter vlog("mainloop");

static void start_connection(void*);
static void stop_connection(void*);

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

  if (!Fl::has_idle(stop_connection))
    Fl::add_idle(stop_connection);
}

void abort_connection_with_unexpected_error(const rdr::Exception &e) {
  abort_connection(_("An unexpected error occurred when communicating "
                     "with the server:\n\n%s"), e.str());
}

void disconnect()
{
  if (!Fl::has_idle(stop_connection))
    Fl::add_idle(stop_connection);
}

static void start_connection(void*)
{
  assert(cc == nullptr);

  Fl::remove_idle(start_connection);

  cc = new CConn(vncServerName.c_str(), sock);
}

static void stop_connection(void*)
{
  assert(cc != nullptr);

  Fl::remove_idle(stop_connection);

  delete cc;
  cc = nullptr;

  if (exitError.empty()) {
    exitMainloop = true;
    return;
  }

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
    if (ret == 1) {
      Fl::add_idle(start_connection);
      return;
    } else {
      exitMainloop = true;
      return;
    }
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

  exitMainloop = true;
  return;
}

static void run_mainloop()
{
  inMainloop = true;

  exitMainloop = false;
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
  }

  inMainloop = false;
}

static bool is_path(const char *maybe)
{
  if (strchr(maybe, '/') != nullptr)
    return true;
  if (strchr(maybe, '\\') != nullptr)
    return true;

  return false;
}

static bool is_unix_socket(const char *filename)
{
#ifndef WIN32
  struct stat sb;

  // This might be a UNIX socket, we need to check
  if (stat(filename, &sb) == -1) {
    // Some access problem; let loadViewerParameters() deal with it...
  } else {
    if ((sb.st_mode & S_IFMT) == S_IFSOCK)
      return true;
  }
#else
  (void)filename;
#endif

  return false;
}

#ifndef WIN32
static void
createTunnel(const char *gatewayHost, const char *remoteHost,
             int remotePort, int localPort)
{
  const char *cmd = getenv("VNC_VIA_CMD");
  char *cmd2, *percent;
  char lport[10], rport[10];
  sprintf(lport, "%d", localPort);
  sprintf(rport, "%d", remotePort);
  setenv("G", gatewayHost, 1);
  setenv("H", remoteHost, 1);
  setenv("R", rport, 1);
  setenv("L", lport, 1);
  if (!cmd)
    cmd = "/usr/bin/ssh -f -L \"$L\":\"$H\":\"$R\" \"$G\" sleep 20";
  /* Compatibility with TigerVNC's method. */
  cmd2 = strdup(cmd);
  while ((percent = strchr(cmd2, '%')) != nullptr)
    *percent = '$';
  system(cmd2);
  free(cmd2);
}

static std::string mktunnel(const char* server)
{
  const char *gatewayHost;
  std::string remoteHost;
  int localPort = findFreeTcpPort();
  int remotePort;

  getHostAndPort(server, &remoteHost, &remotePort);
  gatewayHost = (const char*)via;
  createTunnel(gatewayHost, remoteHost.c_str(), remotePort, localPort);

  return format("localhost::%d", localPort);
}
#endif /* !WIN32 */

int mainloop(const char* configServerName,
             const char* cmdlineServerName)
{
  // Check if the server name in reality is a configuration file
  if (is_path(cmdlineServerName) &&
      !is_unix_socket(cmdlineServerName)) {
    try {
      vncServerName = loadViewerParameters(cmdlineServerName);
    } catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
      abort_vncviewer(_("Unable to load the specified configuration "
                        "file:\n\n%s"), e.str());
    }
  } else {
    vncServerName = cmdlineServerName;
  }

#ifndef WIN32
  /* Specifying -via and -listen together is nonsense */
  if (listenMode && strlen(via) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    abort_vncviewer(_("Parameters -listen and -via are incompatible"));
    return 1; /* Not reached */
  }
#endif

  if (listenMode) {
    std::list<SocketListener*> listeners;
    try {
      int port = 5500;
      if ((cmdlineServerName[0] != '\0') &&
          isdigit(cmdlineServerName[0]))
        port = atoi(cmdlineServerName);

      createTcpListeners(&listeners, nullptr, port);
      if (listeners.empty())
        throw Exception(_("Unable to listen for incoming connections"));

      vlog.info(_("Listening on port %d"), port);

      /* Wait for a connection */
      while (sock == nullptr) {
        fd_set rfds;
        FD_ZERO(&rfds);
        for (SocketListener* listener : listeners)
          FD_SET(listener->getFd(), &rfds);

        int n = select(FD_SETSIZE, &rfds, nullptr, nullptr, nullptr);
        if (n < 0) {
          if (errno == EINTR) {
            vlog.debug("Interrupted select() system call");
            continue;
          } else {
            throw rdr::SystemException("select", errno);
          }
        }

        for (SocketListener* listener : listeners)
          if (FD_ISSET(listener->getFd(), &rfds)) {
            sock = listener->accept();
            if (sock)
              /* Got a connection */
              break;
          }
      }
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      abort_vncviewer(_("Failure waiting for incoming VNC connection:\n\n%s"), e.str());
      return 1; /* Not reached */
    }

    while (!listeners.empty()) {
      delete listeners.back();
      listeners.pop_back();
    }
  } else {
    if (vncServerName.empty()) {
      ServerDialog dialog;

      dialog.setServerName(configServerName);

      dialog.show();
      while (dialog.shown())
        Fl::wait();

      if (!dialog.result())
        return 1;

      vncServerName = dialog.getServerName();
    }

#ifndef WIN32
    if (strlen(via) > 0) {
      try {
        vncServerName = mktunnel(vncServerName.c_str());
      } catch (rdr::Exception& e) {
        vlog.error("%s", e.str());
        abort_vncviewer(_("Failure setting up encrypted tunnel:\n\n%s"), e.str());
      }
    }
#endif
  }

  Fl::add_idle(start_connection);

  run_mainloop();

  // Clean up CConn on fatal errors
  if (cc != nullptr)
    delete cc;

  return 0;
}
