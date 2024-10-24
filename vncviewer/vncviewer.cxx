/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2024 Pierre Ossman <ossman@cendio.se> for Cendio AB
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#if !defined(WIN32) && !defined(__APPLE__)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#endif
#endif

#if defined(__APPLE__)
#include <QMenuBar>
#endif

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/XKBlib.h>
// Qt breaks if this is defined
#undef None
#endif

#include <rfb/Logger_stdio.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/Timer.h>
#include <rfb/Exception.h>
#include <rfb/util.h>

#include <rdr/Exception.h>

#include <network/TcpSocket.h>

#include <os/os.h>

#include "appmanager.h"
#include "i18n.h"
#include "parameters.h"
#include "CConn.h"
#include "ServerDialog.h"
#include "vncviewer.h"
#include "tunnelfactory.h"

static rfb::LogWriter vlog("main");

using namespace network;
using namespace rfb;

static std::string vncServerName;

static const char *argv0 = nullptr;

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
      std::string text;
      text = format(_("%s\n\nAttempt to reconnect?"),
                    exitError.c_str());

      QMessageBox* d = new QMessageBox(QMessageBox::Critical,
                                       _("Connection error"),
                                       text.c_str(),
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

static void init_qt()
{
  i18n_qt_init();

  qApp->setOrganizationName("TigerVNC Team");
  qApp->setOrganizationDomain("tigervnc.org");
  qApp->setApplicationName("vncviewer");
  qApp->setApplicationDisplayName(_("TigerVNC Viewer"));
  qApp->setApplicationVersion(PACKAGE_VERSION);

  // Proper Gnome Shell integration requires that we set a sensible
  // WM_CLASS for the window.
  qApp->setDesktopFileName("vncviewer");

#if !defined(WIN32) && !defined(__APPLE__)
  const int icon_sizes[] = {128, 64, 48, 32, 24, 22, 16};

  QIcon fallback;

  for (int icon_size : icon_sizes) {
      char icon_path[PATH_MAX];

      sprintf(icon_path, "%s/icons/hicolor/%dx%d/apps/tigervnc.png",
              CMAKE_INSTALL_FULL_DATADIR, icon_size, icon_size);

      fallback.addFile(icon_path, QSize(icon_size, icon_size));
  }

  qApp->setWindowIcon(QIcon::fromTheme("tigersdvnc", fallback));
#endif

  qApp->setQuitOnLastWindowClosed(false);

  QTimer* rfbTimerProxy = new QTimer(qApp);
  QObject::connect(rfbTimerProxy, &QTimer::timeout,
                   []() { rfb::Timer::checkTimeouts(); });
  QObject::connect(QApplication::eventDispatcher(),
                   &QAbstractEventDispatcher::aboutToBlock,
                   rfbTimerProxy,
                   [rfbTimerProxy]() {
                     int next = rfb::Timer::checkTimeouts();
                       if (next != -1)
                         rfbTimerProxy->start(next);
                   });
  rfbTimerProxy->setSingleShot(true);

#ifdef __APPLE__
  QMenuBar* menuBar = new QMenuBar(nullptr); // global menu bar for mac
  QMenu* appMenu = new QMenu(menuBar);

  QAction* aboutAction = new QAction(appMenu);
  QObject::connect(aboutAction, &QAction::triggered,
                   []() { about_vncviewer(nullptr); });
  aboutAction->setText(_("About"));
  aboutAction->setMenuRole(QAction::AboutRole);
  appMenu->addAction(aboutAction);
  menuBar->addMenu(appMenu);

  QMenu *file = new QMenu(p_("SysMenu|", "&File"), menuBar);
  file->addAction(p_("SysMenu|File|", "&New Connection"),
                  [=]() {
                    QProcess process;
                    if (process.startDetached(argv0, QStringList())) {
                      vlog.error(_("Error starting new TigerVNC Viewer: %s"),
                                 QVariant::fromValue(process.error()).toString().toStdString().c_str());
                    }
                  }, QKeySequence("Ctrl+N"));
  menuBar->addMenu(file);
#endif
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
  argv0 = argv[0];

  i18n_init();

  fprintf(stderr,"\n%s\n", about_text());

  rfb::initStdIOLoggers();
#ifdef WIN32
  const char* tmp;
  struct stat st;
  std::string logfn;

  tmp = getenv("TMP");
  if ((tmp == nullptr) || (stat(tmp, &st) != 0))
    tmp = getenv("TEMP");
  if ((tmp == nullptr) || (stat(tmp, &st) != 0))
    tmp = getenv("USERPROFILE");
  if ((tmp == nullptr) || (stat(tmp, &st) != 0))
    tmp = "C:\\temp";

  logfn = format("%s\\vncviewer.log", tmp);
  rfb::initFileLogger(logfn.c_str());
#else
  rfb::initFileLogger("/tmp/vncviewer.log");
#endif
  rfb::LogWriter::setLogParams("*:stderr:30");

#ifdef SIGHUP
  signal(SIGHUP, CleanupSignalHandler);
#endif
  signal(SIGINT, CleanupSignalHandler);
  signal(SIGTERM, CleanupSignalHandler);

#ifdef Q_OS_LINUX
  qputenv("QT_QPA_PLATFORM", "xcb");
#endif

  // FIXME: Should we really let Qt respect command line args? We didn't
  //        for FLTK. And -geometry is currently caught by Qt with
  //        unknown effects.
  QApplication app(argc, argv);

  init_qt();

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

#if !defined(WIN32) && !defined(__APPLE__)
  Display* dpy;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  dpy = QX11Info::display();
#else
  dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif

  XkbSetDetectableAutoRepeat(dpy, True, nullptr);
#endif

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
