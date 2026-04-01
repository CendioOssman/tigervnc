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

#include <FL/Fl.H>
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

static bool exitMainloop = false;
static std::string exitError;

static std::string configServerName;
static std::string cmdlineServerName;

static std::string vncServerName;
static network::Socket* sock = nullptr;
static CConn* cc = nullptr;

static std::list<SocketListener*> listeners;

static rfb::LogWriter vlog("mainloop");

static void abort_startup(const char *error, ...)
  __attribute__((__format__ (__printf__, 1, 2)));

static void start_connection(void*);
static void stop_connection(void*);

static void load_cmdline_config(void*);
static void setup_listen(void*);
static void start_server_dialog(void*);
#ifndef WIN32
static void setup_via(void*);
#endif

static void alert_done(Fl_Widget* widget, void*)
{
  Fl::delete_widget(widget);
  exitMainloop = true;
}

static void abort_startup(const char *error, ...)
{
  // Prioritise the first error we get as that is probably the most
  // relevant one.
  if (exitError.empty()) {
    va_list ap;

    va_start(ap, error);
    exitError = vformat(error, ap);
    va_end(ap);
  }

  if (alertOnFatalError) {
    Fl_Alert_Box* dlg;

    dlg = new Fl_Alert_Box(_("Error"), "%s", exitError.c_str());
    dlg->set_modal();
    dlg->finished(alert_done);
    dlg->show();
  } else {
    exitMainloop = true;
  }
}

void abort_connection(const char *error, ...)
{
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
  if (!Fl::has_idle(stop_connection))
    Fl::add_idle(stop_connection);
}

static void start_connection(void*)
{
  assert(cc == nullptr);

  Fl::remove_idle(start_connection);

  cc = new CConn(vncServerName.c_str(), sock);
}

static void reconnect_done(Fl_Widget* widget, void*)
{
  Fl_Choice_Box* dlg = (Fl_Choice_Box*)widget;

  Fl::delete_widget(dlg);

  if (dlg->result() == 1)
    Fl::add_idle(start_connection);
  else
    exitMainloop = true;
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

    dlg = new Fl_Choice_Box(_("Connection error"),
                            _("%s\n\nAttempt to reconnect?"),
                            nullptr, fl_yes, fl_no,
                            exitError.c_str());
    exitError.clear();
    dlg->set_modal();
    dlg->finished(reconnect_done);
    dlg->show();
    return;
  }

  if (alertOnFatalError) {
    Fl_Alert_Box* dlg;

    dlg = new Fl_Alert_Box(_("Connection error"),
                            "%s", exitError.c_str());
    dlg->set_modal();
    dlg->finished(alert_done);
    return;
  }

  exitMainloop = true;
  return;
}

static void run_mainloop()
{
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

static void load_cmdline_config(void*)
{
  Fl::remove_idle(load_cmdline_config);

  // Check if the server name in reality is a configuration file
  if (is_path(cmdlineServerName.c_str()) &&
      !is_unix_socket(cmdlineServerName.c_str())) {
    try {
      vncServerName = loadViewerParameters(cmdlineServerName.c_str());
    } catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
      abort_startup(_("Unable to load the specified configuration "
                      "file:\n\n%s"), e.str());
      return;
    }
  } else {
    vncServerName = cmdlineServerName;
  }

  if (listenMode)
    Fl::add_idle(setup_listen);
  else if (vncServerName.empty())
    Fl::add_idle(start_server_dialog);
#ifndef WIN32
  else if (strlen(via) > 0)
    Fl::add_idle(setup_via);
#endif
  else
    Fl::add_idle(start_connection);
}

static void handle_connection(FL_SOCKET, void* data)
{
  SocketListener* listener = (SocketListener*)data;

  assert(listener);

  try {
    sock = listener->accept();
    if (!sock)
      return;
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_startup(_("Failure waiting for incoming VNC connection:\n\n%s"), e.str());
    /* Continue for cleanup */
  }

  while (!listeners.empty()) {
    Fl::remove_fd(listeners.back()->getFd());
    delete listeners.back();
    listeners.pop_back();
  }

  if (sock)
    Fl::add_idle(start_connection);
}

static void setup_listen(void*)
{
  Fl::remove_idle(setup_listen);

  assert(listenMode);

#ifndef WIN32
  /* Specifying -via and -listen together is nonsense */
  if (strlen(via) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    abort_startup(_("Parameters -listen and -via are incompatible"));
    return;
  }
#endif

  try {
    int port = 5500;
    if (!cmdlineServerName.empty() &&
        isdigit(cmdlineServerName[0]))
      port = atoi(cmdlineServerName.c_str());

    createTcpListeners(&listeners, nullptr, port);
    if (listeners.empty())
      throw Exception(_("Unable to listen for incoming connections"));

    vlog.info(_("Listening on port %d"), port);
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_startup(_("Failure waiting for incoming VNC connection:\n\n%s"), e.str());
    return;
  }

  /* Wait for a connection */
  for (SocketListener* listener : listeners)
    Fl::add_fd(listener->getFd(), FL_READ | FL_EXCEPT,
                handle_connection, listener);
}

static void server_dialog_finished(Fl_Widget* widget, void*)
{
  ServerDialog* dialog = (ServerDialog*)widget;

  Fl::delete_widget(dialog);

  if (!dialog->result()) {
    exitMainloop = true;
    return;
  }

  vncServerName = dialog->getServerName();

#ifndef WIN32
  if (strlen(via) > 0)
    Fl::add_idle(setup_via);
  else
#endif
    Fl::add_idle(start_connection);
}

static void start_server_dialog(void*)
{
  ServerDialog* dialog;

  Fl::remove_idle(start_server_dialog);

  dialog = new ServerDialog();
  dialog->setServerName(configServerName.c_str());

  dialog->finished(server_dialog_finished);
  dialog->show();
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

static void setup_via(void*)
{
  Fl::remove_idle(setup_via);

  try {
    vncServerName = mktunnel(vncServerName.c_str());
  } catch (rdr::Exception& e) {
    vlog.error("%s", e.str());
    abort_startup(_("Failure setting up encrypted tunnel:\n\n%s"), e.str());
    return;
  }

  Fl::add_idle(start_connection);
}
#endif /* !WIN32 */

int mainloop(const char* configServerName_,
             const char* cmdlineServerName_)
{
  configServerName = configServerName_;
  cmdlineServerName = cmdlineServerName_;

  Fl::add_idle(load_cmdline_config);

  run_mainloop();

  return exitError.empty() ? 0 : 1;
}
