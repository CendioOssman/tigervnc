#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QDeadlineTimer>

#include <network/TcpSocket.h>

#include <rfb/LogWriter.h>

#include "appmanager.h"
#include "loggerconfig.h"
#include "i18n.h"
#undef asprintf
#include "parameters.h"
#include "ServerDialog.h"
#include "viewerconfig.h"
#include "vncapplication.h"
#include "vnctranslator.h"
#include "tunnelfactory.h"

static rfb::LogWriter vlog("main");

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

int main(int argc, char *argv[])
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

  VNCTranslator translator;
  app.installTranslator(&translator);
  installQtTranslators();

  app.setQuitOnLastWindowClosed(false);

  QObject::connect(ViewerConfig::instance(), &ViewerConfig::errorOccurred,
                   AppManager::instance(), [&](QString str){ AppManager::instance()->publishError(str, true); });
  AppManager::instance()->initialize();
  ViewerConfig::instance()->initialize();

  TunnelFactory* tunnelFactory = nullptr;

  network::Socket* socket = nullptr;

  if (listenMode) {
    std::list<network::SocketListener*> listeners;
    try {
      bool ok;
      int port = ViewerConfig::instance()->getServerName().toInt(&ok);
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
    if (!ViewerConfig::instance()->getServerName().isEmpty()) {
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

      delete dlg;
    }

    QString gatewayHost = ViewerConfig::instance()->getGatewayHost();
    QString remoteHost = ViewerConfig::instance()->getServerHost();
    if (!gatewayHost.isEmpty() && !remoteHost.isEmpty()) {
      tunnelFactory = new TunnelFactory;
      tunnelFactory->start();
  #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
      tunnelFactory->wait(20000);
  #else
      tunnelFactory->wait(QDeadlineTimer(20000));
  #endif
    }
  }

  int ret = AppManager::instance()->exec(ViewerConfig::instance()->getFinalAddress().toStdString().c_str(), socket);

  delete tunnelFactory;

  return ret;
}
