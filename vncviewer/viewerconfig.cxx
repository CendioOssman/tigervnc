#include "viewerconfig.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QApplication>
#include <QDate>
#include <QDir>

#if !defined(WIN32)
#include <sys/stat.h>
#else
#include <QThread>
#endif
#if defined(__APPLE__)
#include "cocoa.h"

#include <Carbon/Carbon.h>
#endif
#ifdef Q_OS_LINUX
#include "x11utils.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <xcb/xcb.h>
#endif
#endif

#include "appmanager.h"
#include "parameters.h"
#include "os/os.h"
#include "rfb/Configuration.h"
#include "rfb/LogWriter.h"
#include "rfb/Hostname.h"
#ifdef HAVE_GNUTLS
#include "rfb/CSecurityTLS.h"
#endif
#include "i18n.h"
#include "network/TcpSocket.h"
#include "rfb/Exception.h"
#undef asprintf

using namespace rfb;
using namespace std;

static LogWriter vlog("ViewerConfig");

ViewerConfig::ViewerConfig()
  : QObject(nullptr)
{

}

void ViewerConfig::initialize()
{
  const char* homeDir = os::getvncconfigdir();
  if (homeDir == nullptr) {
    QDir dir;
    if (!dir.mkpath(homeDir)) {
      vlog.error(_("Could not create VNC home directory:"));
    }
  }

  rfb::Configuration::enableViewerParams();
  loadViewerParameters("");
  if (::fullScreenAllMonitors) {
    vlog.info(_("FullScreenAllMonitors is deprecated, set FullScreenMode to 'all' instead"));
    ::fullScreenMode.setParam("all");
  }
  QStringList argv = QGuiApplication::arguments();
  int argc = argv.length();
  for (int i = 1; i < argc;) {
    /* We need to resolve an ambiguity for booleans */
    if (argv[i][0] == '-' && i + 1 < argc) {
      QString name = argv[i].mid(1);
      rfb::VoidParameter* param = rfb::Configuration::getParam(name.toStdString().c_str());
      if ((param != nullptr) && (dynamic_cast<rfb::BoolParameter*>(param) != nullptr)) {
        QString opt = argv[i + 1];
        if ((opt.compare("0") == 0) || (opt.compare("1") == 0) || (opt.compare("true", Qt::CaseInsensitive) == 0)
            || (opt.compare("false", Qt::CaseInsensitive) == 0) || (opt.compare("yes", Qt::CaseInsensitive) == 0)
            || (opt.compare("no", Qt::CaseInsensitive) == 0)) {
          param->setParam(opt.toStdString().c_str());
          i += 2;
          continue;
        }
      }
    }

    if (rfb::Configuration::setParam(argv[i].toStdString().c_str())) {
      i++;
      continue;
    }

    if (argv[i][0] == '-') {
      if (i + 1 < argc) {
        if (rfb::Configuration::setParam(argv[i].mid(1).toStdString().c_str(), argv[i + 1].toStdString().c_str())) {
          i += 2;
          continue;
        }
      }

      usage();
    }

    serverName = argv[i];
    i++;
  }
  // Check if the server name in reality is a configuration file
  potentiallyLoadConfigurationFile(serverName);

  /* Specifying -via and -listen together is nonsense */
  if (::listenMode && ::via.getValueStr().length() > 0) {
    // TRANSLATORS: "Parameters" are command line arguments, or settings
    // from a file or the Windows registry.
    vlog.error(_("Parameters -listen and -via are incompatible"));
    QGuiApplication::exit(1);
  }

  parseServerName();
}

ViewerConfig *ViewerConfig::instance()
{
  static ViewerConfig config;
  return &config;
}

ViewerConfig::FullscreenType ViewerConfig::fullscreenType()
{
  if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "selected")) {
    return Selected;
  }

  if (!strcasecmp(::fullScreenMode.getValueStr().c_str(), "all")) {
    if (ViewerConfig::canFullScreenOnMultiDisplays()) {
      return All;
    }
  }

  return Current;
}

bool ViewerConfig::canFullScreenOnMultiDisplays()
{
#if defined(__APPLE__)
  return !cocoa_displays_have_separate_spaces();
#elif defined(Q_OS_LINUX)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  auto dpy = QX11Info::display();
#else
  auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  bool supported = X11Utils::isEWMHsupported(dpy);
  vlog.debug("isEWMHsupported %d", supported);
  bool wm = hasWM();
  return supported || !wm;
#else
  return true;
#endif
}

bool ViewerConfig::hasWM()
{
#if defined(Q_OS_LINUX)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto dpy = QX11Info::display();
#else
    auto dpy = qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#endif
  bool hasWM = X11Utils::hasWM(dpy);
  vlog.debug("hasWM %d", hasWM);
  return hasWM;
#else
  return true;
#endif
}

bool ViewerConfig::potentiallyLoadConfigurationFile(QString vncServerName)
{
  bool hasPathSeparator = vncServerName.contains('/') || vncServerName.contains('\\');
  if (hasPathSeparator) {
#ifndef WIN32
    struct stat sb;

    // This might be a UNIX socket, we need to check
    if (stat(vncServerName.toStdString().c_str(), &sb) == -1) {
      // Some access problem; let loadViewerParameters() deal with it...
    } else {
      if ((sb.st_mode & S_IFMT) == S_IFSOCK) {
        return true;
      }
    }
#endif

    try {
      serverName = loadViewerParameters(vncServerName.toStdString().c_str());
    } catch (rfb::Exception& e) {
      QString str = QString::asprintf(_("Unable to load the specified configuration file:\n\n%s"), e.str());
      vlog.error("%s", str.toStdString().c_str());
      emit errorOccurred(str);
      return false;
    }
  }
  return true;
}

void ViewerConfig::usage()
{
  QString argv0 = QGuiApplication::arguments().at(0);
  std::string str = argv0.toStdString();
  const char* programName = str.c_str();

  fprintf(stderr,
          "\n"
          "usage: %s [parameters] [host][:displayNum]\n"
          "       %s [parameters] [host][::port]\n"
#ifndef WIN32
          "       %s [parameters] [unix socket]\n"
#endif
          "       %s [parameters] -listen [port]\n"
          "       %s [parameters] [.tigervnc file]\n",
          programName,
          programName,
#ifndef WIN32
          programName,
#endif
          programName,
          programName);

#if !defined(WIN32) && !defined(__APPLE__)
  fprintf(stderr,
          "\n"
          "Options:\n\n"
          "  -display Xdisplay  - Specifies the X display for the viewer window\n"
          "  -geometry geometry - Initial position of the main VNC viewer window. See the\n"
          "                       man page for details.\n");
#endif

  fprintf(stderr,
          "\n"
          "Parameters can be turned on with -<param> or off with -<param>=0\n"
          "Parameters which take a value can be specified as "
          "-<param> <value>\n"
          "Other valid forms are <param>=<value> -<param>=<value> "
          "--<param>=<value>\n"
          "Parameter names are case-insensitive.  The parameters are:\n\n");
  rfb::Configuration::listParams(79, 14);

#if defined(WIN32)
  // Just wait for the user to kill the console window
  Sleep(INFINITE);
#endif

  QGuiApplication::exit(1);
  exit(1);
}

QString ViewerConfig::getGatewayHost() const
{
  return QString(::via);
}

void ViewerConfig::parseServerName()
{
  if (!QString(::via).isEmpty() && !gatewayLocalPort) {
    network::initSockets();
    gatewayLocalPort = network::findFreeTcpPort();
  }
  std::string shost;
  rfb::getHostAndPort(serverName.toStdString().c_str(), &shost, &serverPort);
  serverHost = shost.c_str();
}

void ViewerConfig::setServer(QString name)
{
  serverName = name;
  parseServerName();
}

QString ViewerConfig::getFinalAddress() const
{
  if(!getGatewayHost().isEmpty()) {
    return QString("localhost::%2").arg(getGatewayLocalPort());
  } else {
    return QString("%1::%2").arg(serverHost).arg(serverPort);
  }
}

