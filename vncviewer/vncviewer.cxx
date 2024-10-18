/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <fcntl.h>
#include <signal.h>

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QDeadlineTimer>
#include <QMessageBox>
#include <QTimer>

#include <os/os.h>

#include <network/TcpSocket.h>

#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/Logger_stdio.h>
#include <rfb/Timer.h>

#include "appmanager.h"
#include "CConn.h"
#include "i18n.h"
#undef asprintf
#undef vasprintf
#include "parameters.h"
#include "ServerDialog.h"
#include "viewerconfig.h"
#include "vncviewer.h"
#include "tunnelfactory.h"

static rfb::LogWriter vlog("main");

using namespace network;
using namespace rfb;

static std::string vncServerName;

static bool inMainloop = false;
static bool exitMainloop = false;
static std::string exitError;
static bool fatalError = false;

static const char *about_text()
{
  static char buffer[1024];

  // This is used in multiple places with potentially different
  // encodings, so we need to make sure we get a fresh string every
  // time.
  snprintf(buffer, sizeof(buffer),
           _("TigerVNC Viewer v%s\n"
             "Built on: %s\n"
             "Copyright (C) 1999-%d TigerVNC Team and many others (see README.rst)\n"
             "See https://www.tigervnc.org for information on TigerVNC."),
           PACKAGE_VERSION, BUILD_TIMESTAMP, 2024);

  return buffer;
}


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

void abort_connection_with_unexpected_error(const rdr::Exception &e) {
  abort_connection(_("An unexpected error occurred when communicating "
                     "with the server:\n\n%s"), e.str());
}

void disconnect()
{
  exitMainloop = true;
  qApp->quit();
}

bool should_disconnect()
{
  return exitMainloop;
}

void about_vncviewer(QWidget* parent)
{
  QMessageBox* dlg;

  dlg = new QMessageBox(QMessageBox::Information,
                        _("About TigerVNC Viewer"),
                        about_text(), QMessageBox::Close, parent);
  AppManager::instance()->openDialog(dlg);
}

static void mainloop(const char* vncserver, network::Socket* sock)
{
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
        AppManager::instance()->openDialog(d);
      }
      break;
    }

    if (exitError.empty())
      break;

    if(reconnectOnError && (sock == nullptr)) {
      QString text;
      text = QString::asprintf(_("%s\n\nAttempt to reconnect?"), exitError.c_str());

      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                        _("Connection error"), text,
                                        QMessageBox::NoButton);
      d->addButton(_("Reconnect"), QMessageBox::AcceptRole);
      d->addButton(QMessageBox::Close);

      AppManager::instance()->openDialog(d);
      exitError.clear();
      if (d->buttonRole(d->clickedButton()) == QMessageBox::AcceptRole)
        continue;
      else
        break;
    }

    if (alertOnFatalError) {
      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                      _("Connection error"),
                                      exitError.c_str(),
                                      QMessageBox::Close);
      AppManager::instance()->openDialog(d);
    }

    break;
  }
}

static void CleanupSignalHandler(int sig)
{
  // CleanupSignalHandler allows C++ object cleanup to happen because it calls
  // exit() rather than the default which is to abort.
  vlog.info(_("Termination signal %d has been received. TigerVNC Viewer will now exit."), sig);
  exit(1);
}

static void usage(const char *programName)
{
#ifdef WIN32
  // If we don't have a console then we need to create one for output
  if (GetConsoleWindow() == nullptr) {
    HANDLE handle;
    int fd;

    AllocConsole();

    handle = GetStdHandle(STD_ERROR_HANDLE);
    fd = _open_osfhandle((intptr_t)handle, O_TEXT);
    *stderr = *fdopen(fd, "w");
  }
#endif

  fprintf(stderr,
          "\n"
          "usage: %s [parameters] [host][:displayNum]\n"
          "       %s [parameters] [host][::port]\n"
#ifndef WIN32
          "       %s [parameters] [unix socket]\n"
#endif
          "       %s [parameters] -listen [port]\n"
          "       %s [parameters] [.tigervnc file]\n",
          programName, programName,
#ifndef WIN32
          programName,
#endif
          programName, programName);

#if !defined(WIN32) && !defined(__APPLE__)
  fprintf(stderr,"\n"
          "Options:\n\n"
          "  -display Xdisplay  - Specifies the X display for the viewer window\n"
          "  -geometry geometry - Initial position of the main VNC viewer window. See the\n"
          "                       man page for details.\n");
#endif

  fprintf(stderr,"\n"
          "Parameters can be turned on with -<param> or off with -<param>=0\n"
          "Parameters which take a value can be specified as "
          "-<param> <value>\n"
          "Other valid forms are <param>=<value> -<param>=<value> "
          "--<param>=<value>\n"
          "Parameter names are case-insensitive.  The parameters are:\n\n");
  Configuration::listParams(79, 14);

#ifdef WIN32
  // Just wait for the user to kill the console window
  Sleep(INFINITE);
#endif

  exit(1);
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

static void
migrateDeprecatedOptions()
{
  if (fullScreenAllMonitors) {
    vlog.info(_("FullScreenAllMonitors is deprecated, set FullScreenMode to 'all' instead"));

    fullScreenMode.setParam("all");
  }
}

static void
create_base_dirs()
{
  const char *dir;

  dir = os::getvncconfigdir();
  if (dir == nullptr) {
    vlog.error(_("Could not determine VNC config directory path"));
    return;
  }

#ifndef WIN32
  const char *dotdir = strrchr(dir, '.');
  if (dotdir != nullptr && strcmp(dotdir, ".vnc") == 0)
    vlog.info(_("~/.vnc is deprecated, please consult 'man vncviewer' for paths to migrate to."));
#else
  const char *vncdir = strrchr(dir, '\\');
  if (vncdir != nullptr && strcmp(vncdir, "vnc") == 0)
    vlog.info(_("%%APPDATA%%\\vnc is deprecated, please switch to the %%APPDATA%%\\TigerVNC location."));
#endif

  if (os::mkdir_p(dir, 0755) == -1) {
    if (errno != EEXIST)
      vlog.error(_("Could not create VNC config directory \"%s\": %s"),
                 dir, strerror(errno));
  }

  dir = os::getvncdatadir();
  if (dir == nullptr) {
    vlog.error(_("Could not determine VNC data directory path"));
    return;
  }

  if (os::mkdir_p(dir, 0755) == -1) {
    if (errno != EEXIST)
      vlog.error(_("Could not create VNC data directory \"%s\": %s"),
                 dir, strerror(errno));
  }

  dir = os::getvncstatedir();
  if (dir == nullptr) {
    vlog.error(_("Could not determine VNC state directory path"));
    return;
  }

  if (os::mkdir_p(dir, 0755) == -1) {
    if (errno != EEXIST)
      vlog.error(_("Could not create VNC state directory \"%s\": %s"),
                 dir, strerror(errno));
  }
}

int main(int argc, char** argv)
{
  if (qEnvironmentVariableIsEmpty("QTGLESSTREAM_DISPLAY")) {
    qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", QByteArray("213"));
    qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", QByteArray("120"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif
  }

#ifdef Q_OS_LINUX
  qputenv("QT_QPA_PLATFORM", "xcb");
#endif

  QApplication app(argc, argv);

  app.setOrganizationName("TigerVNC Team");
  app.setOrganizationDomain("tigervnc.org");
  app.setApplicationName("vncviewer");
  app.setApplicationDisplayName("TigerVNC Viewer");
  QIcon icon;
  icon.addFile(":/tigervnc_16.png", QSize(16, 16));
  icon.addFile(":/tigervnc_22.png", QSize(22, 22));
  icon.addFile(":/tigervnc_24.png", QSize(24, 24));
  icon.addFile(":/tigervnc_32.png", QSize(32, 32));
  icon.addFile(":/tigervnc_48.png", QSize(48, 48));
  icon.addFile(":/tigervnc_64.png", QSize(64, 64));
  icon.addFile(":/tigervnc_128.png", QSize(128, 128));
  app.setWindowIcon(icon);

  i18n_init();

  fprintf(stderr,"\n%s\n", about_text());

  rfb::initStdIOLoggers();
#ifdef WIN32
  QString tmp = "C:\\temp";
  if (!QFileInfo::exists(tmp)) {
    tmp = QString(qgetenv("TMP"));
    if (!QFileInfo::exists(tmp)) {
      tmp = QString(qgetenv("TEMP"));
    }
  }
  QString log = tmp + "\\vncviewer.log";
  rfb::initFileLogger(log.toStdString().c_str());
#else
  rfb::initFileLogger("/tmp/vncviewer.log");
#endif
#ifdef QT_DEBUG
  rfb::LogWriter::setLogParams("*:stderr:100");
#else
  rfb::LogWriter::setLogParams("*:stderr:30");
#endif

#ifdef SIGHUP
  signal(SIGHUP, CleanupSignalHandler);
#endif
  signal(SIGINT, CleanupSignalHandler);
  signal(SIGTERM, CleanupSignalHandler);

  Configuration::enableViewerParams();

  /* Load the default parameter settings */
  std::string defaultServerName;
  try {
    defaultServerName = loadViewerParameters(nullptr);
  } catch (rfb::Exception& e) {
    vlog.error("%s", e.str());
  }

  for (int i = 1; i < argc;) {
    /* We need to resolve an ambiguity for booleans */
    if (argv[i][0] == '-' && i+1 < argc) {
        VoidParameter *param;

        param = Configuration::getParam(&argv[i][1]);
        if ((param != nullptr) &&
            (dynamic_cast<BoolParameter*>(param) != nullptr)) {
          if ((strcasecmp(argv[i+1], "0") == 0) ||
              (strcasecmp(argv[i+1], "1") == 0) ||
              (strcasecmp(argv[i+1], "true") == 0) ||
              (strcasecmp(argv[i+1], "false") == 0) ||
              (strcasecmp(argv[i+1], "yes") == 0) ||
              (strcasecmp(argv[i+1], "no") == 0)) {
              param->setParam(argv[i+1]);
              i += 2;
              continue;
          }
      }
    }

    if (Configuration::setParam(argv[i])) {
      i++;
      continue;
    }

    if (argv[i][0] == '-') {
      if (i+1 < argc) {
        if (Configuration::setParam(&argv[i][1], argv[i+1])) {
          i += 2;
          continue;
        }
      }

      usage(argv[0]);
    }

    vncServerName = argv[i];
    i++;
  }
  // Check if the server name in reality is a configuration file
  potentiallyLoadConfigurationFile(vncServerName.c_str());

  migrateDeprecatedOptions();

  create_base_dirs();

  Socket *sock = nullptr;

  /* Specifying -via and -listen together is nonsense */
  if (listenMode && strlen(via) > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    abort_vncviewer(_("Parameters -listen and -via are incompatible"));
    return 1; /* Not reached */
  }

  app.setQuitOnLastWindowClosed(false);

  QTimer rfbTimerProxy;
  QObject::connect(&rfbTimerProxy, &QTimer::timeout,
                   []() { rfb::Timer::checkTimeouts(); });
  QObject::connect(QApplication::eventDispatcher(),
                   &QAbstractEventDispatcher::aboutToBlock,
                   [&rfbTimerProxy]() {
                     int next = rfb::Timer::checkTimeouts();
                       if (next != -1)
                         rfbTimerProxy.start(next);
                   });
  rfbTimerProxy.setSingleShot(true);

  AppManager::instance()->initialize();

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

      dlg.setServerName(defaultServerName.c_str());

      QObject::connect(&dlg, &ServerDialog::finished, []() { qApp->quit(); });
      dlg.open();

      qApp->exec();

      if (dlg.result() != QDialog::Accepted)
        return 1;

      vncServerName = dlg.getServerName().toStdString();
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

  inMainloop = true;
  mainloop(vncServerName.c_str(), sock);
  inMainloop = false;

  delete tunnelFactory;

  return 0;
}
