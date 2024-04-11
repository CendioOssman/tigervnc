#include "viewerconfig.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QTextStream>

#if !defined(WIN32)
#include <sys/stat.h>
#else
#include <QThread>
#endif
#if defined(__APPLE__)
#include "cocoa.h"

#include <Carbon/Carbon.h>
#endif

#include "parameters.h"
#include "rfb/Configuration.h"
#include "rfb/LogWriter.h"
#ifdef HAVE_GNUTLS
#include "rfb/CSecurityTLS.h"
#endif
#include "i18n.h"
#include "network/TcpSocket.h"
#include "rfb/Exception.h"
#undef asprintf

#define SERVER_HISTORY_SIZE 20

using namespace rfb;
using namespace std;

static LogWriter vlog("ViewerConfig");

namespace os
{
extern const char* getvnchomedir();
}

ViewerConfig::ViewerConfig()
  : QObject(nullptr)
{
  const char* homeDir = os::getvnchomedir();
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
      if ((param != NULL) && (dynamic_cast<rfb::BoolParameter*>(param) != NULL)) {
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

  loadServerHistory();
  parseServerName();
}

ViewerConfig *ViewerConfig::instance()
{
  static ViewerConfig config;
  return &config;
}

QString ViewerConfig::aboutText()
{
  return QString::asprintf(_("TigerVNC Viewer v%s\n"
                             "Built on: %s\n"
                             "Copyright (C) 1999-%d TigerVNC Team and many others (see README.rst)\n"
                             "See https://www.tigervnc.org for information on TigerVNC."),
                           PACKAGE_VERSION,
                           BUILD_TIMESTAMP,
                           QDate::currentDate().year());
}

bool ViewerConfig::canFullScreenOnMultiDisplays()
{
#if defined(__APPLE__)
  return !cocoa_displays_have_separate_spaces();
#else
  return true;
#endif
}

void ViewerConfig::saveViewerParameters(QString path, QString serverName)
{
  addServer(serverName);
  ::saveViewerParameters(path.isEmpty() ? nullptr : path.toStdString().c_str(), serverName.toStdString().c_str());
}

QString ViewerConfig::loadViewerParameters(QString path)
{
  return QString(::loadViewerParameters(path.trimmed().length() > 0 ? path.toStdString().c_str() : nullptr));
}

void ViewerConfig::loadServerHistory()
{
  serverHistory.clear();

#ifdef _WIN32
  std::vector<std::string> vector;
  ::loadHistoryFromRegKey(vector);
  for (auto const& s : vector)
    serverHistory.push_back(s.c_str());
  return;
#endif

  const char* homeDir = os::getvnchomedir();
  if (homeDir == nullptr)
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));

  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

  /* Read server history from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (errno == ENOENT) {
      // no history file
      return;
    }
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }

  int lineNr = 0;
  while (!feof(f)) {
    char line[256];

    // Read the next line
    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;

      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, strerror(errno));
    }

    int len = strlen(line);

    if (len == (sizeof(line) - 1)) {
      fclose(f);
      throw rdr::Exception(_("Failed to read line %d in file %s: %s"), lineNr, filepath, _("Line too long"));
    }

    if ((len > 0) && (line[len - 1] == '\n')) {
      line[len - 1] = '\0';
      len--;
    }
    if ((len > 0) && (line[len - 1] == '\r')) {
      line[len - 1] = '\0';
      len--;
    }

    if (len == 0)
      continue;

    serverHistory.push_back(line);
  }

  fclose(f);
}

void ViewerConfig::saveServerHistory()
{
  serverName = serverHistory.length() > 0 ? serverHistory[0] : "";
  parseServerName();
#ifdef _WIN32
  std::vector<std::string> vector;
  for (auto const& s : qAsConst(serverHistory))
    vector.push_back(s.toStdString());
  ::saveHistoryToRegKey(vector);
#else
  const char* homeDir = os::getvnchomedir();
  if (homeDir == nullptr) {
    throw rdr::Exception("%s", _("Could not obtain the home directory path"));
  }
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", homeDir, SERVER_HISTORY);

  /* Write server history to file */
  FILE* f = fopen(filepath, "w");
  if (!f) {
    throw rdr::Exception(_("Could not open \"%s\": %s"), filepath, strerror(errno));
  }
  QTextStream stream(f, QIODevice::WriteOnly | QIODevice::WriteOnly);

  // Save the last X elements to the config file.
  for (int i = 0; i < serverHistory.size() && i <= SERVER_HISTORY_SIZE; i++) {
    stream << serverHistory[i] << "\n";
  }
  stream.flush();
  fclose(f);
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
      serverName = loadViewerParameters(vncServerName);
    } catch (rfb::Exception& e) {
      vlog.error("%s", e.str());
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

QString ViewerConfig::gatewayHost() const
{
  return QString(::via);
}

void ViewerConfig::parseServerName()
{
  if (!QString(::via).isEmpty() && !gatewayLocalPort) {
    network::initSockets();
    gatewayLocalPort = network::findFreeTcpPort();
  }
  bool ok;
  int ix = serverName.indexOf(':');
  if (ix >= 0) {
    int ix2 = serverName.indexOf("::");
    if (ix2 < 0) {
      int port = SERVER_PORT_OFFSET + serverName.mid(ix + 1).toInt(&ok, 10);
      if (ok) {
        serverPort = port;
      }
    } else {
      int port = serverName.mid(ix + 2).toInt(&ok, 10);
      if (ok) {
        serverPort = port;
      }
    }
    serverHost = serverName.left(ix);
  } else {
    int port = serverName.toInt(&ok, 10);
    if (ok) {
      serverPort = port;
    } else {
      serverHost = serverName;
    }
  }
}

void ViewerConfig::addServer(QString serverName)
{
  if (serverName.isEmpty())
    return;

  serverHistory.removeOne(serverName);
  serverHistory.push_front(serverName);
  parseServerName();
  saveServerHistory();
}

