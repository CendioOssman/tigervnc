#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QDeadlineTimer>
#include <QMessageBox>

#include <os/os.h>

#include <network/TcpSocket.h>

#include <rfb/Exception.h>
#include <rfb/Hostname.h>
#include <rfb/LogWriter.h>
#include <rfb/Logger_stdio.h>

#include "appmanager.h"
#include "loggerconfig.h"
#include "i18n.h"
#undef asprintf
#include "parameters.h"
#include "ServerDialog.h"
#include "viewerconfig.h"
#include "vncapplication.h"
#include "tunnelfactory.h"

static rfb::LogWriter vlog("main");

QString serverName;

static QString about_text()
{
  return QString::asprintf(_("TigerVNC Viewer v%s\n"
                             "Built on: %s\n"
                             "Copyright (C) 1999-%d TigerVNC Team and many others (see README.rst)\n"
                             "See https://www.tigervnc.org for information on TigerVNC."),
                           PACKAGE_VERSION,
                           BUILD_TIMESTAMP,
                           QDate::currentDate().year());
}

void about_vncviewer(QWidget* parent)
{
  QMessageBox* dlg;

  dlg = new QMessageBox(QMessageBox::Information,
                        _("About TigerVNC Viewer"),
                        about_text(), QMessageBox::Close, parent);
  AppManager::instance()->openDialog(dlg);
}

static void CleanupSignalHandler(int sig)
{
  // CleanupSignalHandler allows C++ object cleanup to happen because it calls
  // exit() rather than the default which is to abort.
  vlog.info(_("Termination signal %d has been received. TigerVNC Viewer will now exit."), sig);
  exit(1);
}

static bool loadCatalog(const QString &catalog, const QString &location)
{
  QTranslator* qtTranslator = new QTranslator(QCoreApplication::instance());
  if (!qtTranslator->load(QLocale::system(), catalog, QString(), location)) {
    return false;
  }
  QCoreApplication::instance()->installTranslator(qtTranslator);
  return true;
}

static void installQtTranslators()
{
  // FIXME: KDE first loads English translation for some reason. See:
  // https://invent.kde.org/frameworks/ki18n/-/blob/master/src/i18n/main.cpp
#ifdef Q_OS_LINUX
  QString location = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#else
  QString location = ":/i18n";
#endif
  if (loadCatalog(QStringLiteral("qt_"), location)) {
    return;
  }
  const auto catalogs = {
      QStringLiteral("qtbase_"),
      QStringLiteral("qtscript_"),
      QStringLiteral("qtmultimedia_"),
      QStringLiteral("qtxmlpatterns_"),
  };
  for (const auto &catalog : catalogs) {
    loadCatalog(catalog, location);
  }
}

static void usage()
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

static bool potentiallyLoadConfigurationFile(QString vncServerName)
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
      AppManager::instance()->publishError(str, true);
      return false;
    }
  }
  return true;
}

int main(int argc, char *argv[])
{
  QString serverHost;
  static const int SERVER_PORT_OFFSET = 5900; // ??? 5500;
  int serverPort = SERVER_PORT_OFFSET;
  int gatewayLocalPort = 0;

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

  QVNCApplication app(argc, argv);

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

  LoggerConfig logger;

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

  installQtTranslators();

  QString about = about_text();
  fprintf(stderr, "\n%s\n", about.toStdString().c_str());

#ifdef SIGHUP
  signal(SIGHUP, CleanupSignalHandler);
#endif
  signal(SIGINT, CleanupSignalHandler);
  signal(SIGTERM, CleanupSignalHandler);

  rfb::Configuration::enableViewerParams();
  loadViewerParameters(nullptr);
  if (::fullScreenAllMonitors) {
    vlog.info(_("FullScreenAllMonitors is deprecated, set FullScreenMode to 'all' instead"));
    ::fullScreenMode.setParam("all");
  }
  for (int i = 1; i < argc;) {
    /* We need to resolve an ambiguity for booleans */
    if (argv[i][0] == '-' && i + 1 < argc) {
      QString name = &argv[i][1];
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

    if (rfb::Configuration::setParam(argv[i])) {
      i++;
      continue;
    }

    if (argv[i][0] == '-') {
      if (i + 1 < argc) {
        if (rfb::Configuration::setParam(&argv[i][1], argv[i + 1])) {
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

  if (!QString(::via).isEmpty() && !gatewayLocalPort) {
    network::initSockets();
    gatewayLocalPort = network::findFreeTcpPort();
  }
  std::string shost;
  rfb::getHostAndPort(serverName.toStdString().c_str(), &shost, &serverPort);
  serverHost = shost.c_str();

  app.setQuitOnLastWindowClosed(false);

  AppManager::instance()->initialize();

  const char* homeDir = os::getvncconfigdir();
  if (homeDir == nullptr) {
    QDir dir;
    if (!dir.mkpath(homeDir)) {
      vlog.error(_("Could not create VNC home directory:"));
    }
  }

  TunnelFactory* tunnelFactory = nullptr;

  network::Socket* socket = nullptr;

  if (listenMode) {
    std::list<network::SocketListener*> listeners;
    try {
      bool ok;
      int port = serverName.toInt(&ok);
      if (!ok) {
        port = 5500;
      }
      network::createTcpListeners(&listeners, nullptr, port);

      vlog.info(_("Listening on port %d"), port);

      /* Wait for a connection */
      while (socket == nullptr) {
        fd_set rfds;
        FD_ZERO(&rfds);
        for (network::SocketListener* listener : listeners) {
          FD_SET(listener->getFd(), &rfds);
        }

        int n = select(FD_SETSIZE, &rfds, nullptr, nullptr, nullptr);
        if (n < 0) {
          if (errno == EINTR) {
            vlog.debug("Interrupted select() system call");
            continue;
          } else {
            throw rdr::SystemException("select", errno);
          }
        }

        for (network::SocketListener* listener : listeners) {
          if (FD_ISSET(listener->getFd(), &rfds)) {
            socket = listener->accept();
            if (socket) {
              /* Got a connection */
              break;
            }
          }
        }
      }
    } catch (rdr::Exception& e) {
      vlog.error("%s", e.str());
      AppManager::instance()->publishError(QString::asprintf(_("Failure waiting for incoming VNC connection:\n\n%s"), e.str()));
      QCoreApplication::exit(1);
    }

    while (!listeners.empty()) {
      delete listeners.back();
      listeners.pop_back();
    }
  } else {
    if (!serverName.isEmpty()) {
      AppManager::instance()->setCommandLine(true);
    } else {
      ServerDialog* dlg = new ServerDialog;

      QObject::connect(dlg, &ServerDialog::finished, []() { qApp->quit(); });
      dlg->open();

      qApp->exec();

      if (dlg->result() != QDialog::Accepted) {
        delete dlg;
        return 1;
      }

      serverName = dlg->getServerName();

      delete dlg;
    }

    rfb::getHostAndPort(serverName.toStdString().c_str(), &shost, &serverPort);
    serverHost = shost.c_str();

    QString gatewayHost = QString(::via);
    QString remoteHost = serverHost;
    if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
      tunnelFactory = new TunnelFactory(gatewayHost.toStdString().c_str(), remoteHost.toStdString().c_str(), serverPort, gatewayLocalPort);
      tunnelFactory->start();
  #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
      tunnelFactory->wait(20000);
  #else
      tunnelFactory->wait(QDeadlineTimer(20000));
  #endif
    }
  }

  QString finalAddress;
  if(!QString(::via).isEmpty()) {
    finalAddress = QString("localhost::%2").arg(gatewayLocalPort);
  } else {
    finalAddress = QString("%1::%2").arg(serverHost).arg(serverPort);
  }

  int ret = AppManager::instance()->exec(finalAddress.toStdString().c_str(), socket);

  delete tunnelFactory;

  return ret;
}
