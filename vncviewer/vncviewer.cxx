#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include "appmanager.h"
#include "loggerconfig.h"
#include "viewerconfig.h"
#include "vncapplication.h"
#include "vnctranslator.h"
#include "vncconnection.h"

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
  AppManager::instance()->getConnection()->initialize();

  if (!ViewerConfig::instance()->getServerName().isEmpty()) {
    AppManager::instance()->setCommandLine(true);
    AppManager::instance()->connectToServer(ViewerConfig::instance()->getServerName());
    return AppManager::instance()->getConnection()->hasConnection() ? app.exec() : 0;
  } else {
    AppManager::instance()->openServerDialog();
    return app.exec();
  }
}
