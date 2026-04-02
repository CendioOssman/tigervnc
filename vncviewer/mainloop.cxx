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
#include <sys/stat.h>

#include <string>

#include <QApplication>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QTimer>

#include <rdr/Exception.h>

#include <network/TcpSocket.h>

#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/util.h>

#include "CConn.h"
#include "ServerDialog.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"
#include "tunnelfactory.h"

using namespace network;
using namespace rfb;

static std::string exitError;
static bool disconnecting = false;

static std::string configServerName;
static std::string cmdlineServerName;

static std::string vncServerName;
static network::Socket* sock = nullptr;
static CConn* cc = nullptr;

static std::list<SocketListener*> listeners;
static std::list<QSocketNotifier*> listenNotifiers;

static TunnelFactory* tunnelFactory = nullptr;

static rfb::LogWriter vlog("mainloop");

static void abort_startup(const char *error, ...)
  __attribute__((__format__ (__printf__, 1, 2)));

static void start_connection();
static void stop_connection();

static void load_cmdline_config();
static void setup_listen();
static void start_server_dialog();
static void setup_via();

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
    QMessageBox* dlg;

    dlg = new QMessageBox(QMessageBox::Critical, _("Error"),
                          exitError.c_str(), QMessageBox::Close);
    QObject::connect(dlg, &QDialog::finished, []() { qApp->quit(); });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
  } else {
    qApp->quit();
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

  if (!disconnecting) {
    disconnecting = true;
    QTimer::singleShot(0, stop_connection);
  }
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
  if (!disconnecting) {
    disconnecting = true;
    QTimer::singleShot(0, stop_connection);
  }
}

static void start_connection()
{
  assert(cc == nullptr);
  cc = new CConn(vncServerName.c_str(), sock);
}

static void stop_connection()
{
  assert(cc != nullptr);

  disconnecting = false;

  delete cc;
  cc = nullptr;

  if (exitError.empty()) {
    qApp->quit();
    return;
  }

  if(reconnectOnError && (sock == nullptr)) {
    std::string text;
    QMessageBox* dlg;

    text = format(_("%s\n\nAttempt to reconnect?"),
                  exitError.c_str());
    dlg = new QMessageBox(QMessageBox::Critical, _("Connection error"),
                          text.c_str(), QMessageBox::NoButton);
    dlg->addButton(_("Reconnect"), QMessageBox::AcceptRole);
    dlg->addButton(QMessageBox::Close);
    QObject::connect(dlg, &QDialog::accepted, start_connection);
    QObject::connect(dlg, &QDialog::rejected, []() { qApp->quit(); });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
    return;
  }

  if (alertOnFatalError) {
    QMessageBox* dlg;

    dlg = new QMessageBox(QMessageBox::Critical, _("Connection error"),
                          exitError.c_str(), QMessageBox::Close);
    QObject::connect(dlg, &QDialog::finished, []() { qApp->quit(); });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
    return;
  }

  qApp->quit();
}

static void run_mainloop()
{
  qApp->exec();
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

static void load_cmdline_config()
{
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
    QTimer::singleShot(0, setup_listen);
  else if (vncServerName.empty())
    QTimer::singleShot(0, start_server_dialog);
  else if (strlen(via) > 0)
    QTimer::singleShot(0, setup_via);
  else
    QTimer::singleShot(0, start_connection);
}

static void handle_connection(int socket)
{
  SocketListener* listener = nullptr;

  for (SocketListener* l : listeners) {
    if (l->getFd() == socket) {
      listener = l;
      break;
    }
  }

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

  while (!listenNotifiers.empty()) {
    delete listenNotifiers.back();
    listenNotifiers.pop_back();
  }

  while (!listeners.empty()) {
    delete listeners.back();
    listeners.pop_back();
  }

  if (sock)
    QTimer::singleShot(0, start_connection);
}

static void setup_listen()
{
  assert(listenMode);

  /* Specifying -via and -listen together is nonsense */
  if (strlen(via) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    abort_startup(_("Parameters -listen and -via are incompatible"));
    return;
  }

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
  for (SocketListener* listener : listeners) {
    QSocketNotifier* notifier;

    notifier =
      new QSocketNotifier(listener->getFd(), QSocketNotifier::Read);
    listenNotifiers.push_back(notifier);
    QObject::connect(notifier, &QSocketNotifier::activated,
                     handle_connection);
  }
}

static void server_dialog_finished(ServerDialog* dialog)
{
  vncServerName = dialog->getServerName();

  if (strlen(via) > 0)
    QTimer::singleShot(0, setup_via);
  else
    QTimer::singleShot(0, start_connection);
}

static void start_server_dialog()
{
  ServerDialog* dialog;

  dialog = new ServerDialog();
  dialog->setServerName(configServerName.c_str());

  QObject::connect(dialog, &ServerDialog::accepted,
                   [dialog]() { server_dialog_finished(dialog); });
  QObject::connect(dialog, &ServerDialog::rejected,
                   []() { qApp->quit(); });

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->open();
}

static void setup_via()
{
  const char *gatewayHost;
  std::string remoteHost;
  int localPort = findFreeTcpPort();
  int remotePort;

  getHostAndPort(vncServerName.c_str(), &remoteHost, &remotePort);
  vncServerName = format("localhost::%d", localPort);
  gatewayHost = (const char*)via;

  tunnelFactory = new TunnelFactory(gatewayHost, remoteHost.c_str(),
                                    remotePort, localPort);
  tunnelFactory->start();
  tunnelFactory->wait(QDeadlineTimer(20000));

  QTimer::singleShot(0, start_connection);
}

int mainloop(const char* configServerName_,
             const char* cmdlineServerName_)
{
  configServerName = configServerName_;
  cmdlineServerName = cmdlineServerName_;

  QTimer::singleShot(0, load_cmdline_config);

  run_mainloop();

  // Clean up CConn on fatal errors
  if (cc != nullptr)
    delete cc;

  delete tunnelFactory;

  return exitError.empty() ? 0 : 1;
}
