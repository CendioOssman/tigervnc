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
  app.setWindowIcon(QIcon(":/tigervnc.png"));

  LoggerConfig logger;

  VNCTranslator translator;
  app.installTranslator(&translator);

  if (!ViewerConfig::instance()->getServerName().isEmpty()) {
    AppManager::instance()->connectToServer(ViewerConfig::instance()->getServerName());
    app.setQuitOnLastWindowClosed(false);
    return app.exec();
  } else {
    AppManager::instance()->openServerDialog();
    return app.exec();
  }
}
