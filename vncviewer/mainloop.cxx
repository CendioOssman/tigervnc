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

#include <rdr/Exception.h>

#include <network/TcpSocket.h>

#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/util.h>

#include "CConn.h"
#include "ServerDialog.h"
#include "appmanager.h"
#include "i18n.h"
#include "mainloop.h"
#include "parameters.h"
#include "tunnelfactory.h"

using namespace network;
using namespace rfb;

static std::string vncServerName;

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

static void run_mainloop(const char* vncserver, network::Socket* sock)
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

static void
potentiallyLoadConfigurationFile(const char *filename)
{
  const bool hasPathSeparator = (strchr(filename, '/') != nullptr ||
                                 (strchr(filename, '\\')) != nullptr);

  if (hasPathSeparator) {
#ifndef WIN32
    struct stat sb;

    // This might be a UNIX socket, we need to check
    if (stat(filename, &sb) == -1) {
      // Some access problem; let loadViewerParameters() deal with it...
    } else {
      if ((sb.st_mode & S_IFMT) == S_IFSOCK)
        return;
    }
#endif

    try {
      // The server name might be empty, but we still need to clear it
      // so we don't try to connect to the filename
      vncServerName = loadViewerParameters(filename);
    } catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
      abort_vncviewer(_("Unable to load the specified configuration "
                        "file:\n\n%s"), e.str());
    }
  }
}

// Establish a connection and run it until completion
int mainloop(const char* configServerName,
             const char* cmdlineServerName)
{
  Socket *sock = nullptr;

  vncServerName = cmdlineServerName;

  potentiallyLoadConfigurationFile(vncServerName.c_str());

  /* Specifying -via and -listen together is nonsense */
  if (listenMode && strlen(via) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    abort_vncviewer(_("Parameters -listen and -via are incompatible"));
    return 1; /* Not reached */
  }

  TunnelFactory* tunnelFactory = nullptr;

  if (listenMode) {
    std::list<SocketListener*> listeners;
    try {
      int port = 5500;
      if (!vncServerName.empty() && isdigit(vncServerName[0]))
        port = atoi(vncServerName.c_str());

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
      ServerDialog dlg;

      dlg.setServerName(configServerName);

      QObject::connect(&dlg, &ServerDialog::finished, []() { qApp->quit(); });
      dlg.open();

      qApp->exec();

      if (dlg.result() != QDialog::Accepted)
        return 1;

      vncServerName = dlg.getServerName();
    }

    if (strlen(via) > 0) {
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
    }
  }

  run_mainloop(vncServerName.c_str(), sock);

  delete tunnelFactory;

  return 0;
}
