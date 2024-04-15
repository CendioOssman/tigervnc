#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <QApplication>
#include <QIcon>
#include "appmanager.h"
#include "loggerconfig.h"
#include "viewerconfig.h"
#include "vncapplication.h"
#include "vnctranslator.h"
#include "vncconnection.h"

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

  app.setQuitOnLastWindowClosed(false);

  if (!ViewerConfig::instance()->getServerName().isEmpty()) {
    AppManager::instance()->setCommandLine(true);
    AppManager::instance()->connectToServer(ViewerConfig::instance()->getServerName());
    return AppManager::instance()->getConnection()->hasConnection() ? app.exec() : 0;
  } else {
    AppManager::instance()->openServerDialog();
    return app.exec();
  }
}
