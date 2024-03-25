#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <QApplication>
#include "parameters.h"
#include "appmanager.h"
#include "vncapplication.h"
#include "vnctranslator.h"
#include "serverdialog.h"

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

  ViewerConfig::initialize();
  AppManager::initialize();

  VNCTranslator translator;
  app.installTranslator(&translator);


  if (!ViewerConfig::config()->serverName().isEmpty()) {
    AppManager::instance()->connectToServer(ViewerConfig::config()->serverName());
    return app.exec();
  } else {
    ServerDialog serverDialog;
    serverDialog.setVisible(!ViewerConfig::config()->listenModeEnabled());
    QObject::connect(AppManager::instance(), &AppManager::vncWindowOpened, &serverDialog, &QWidget::hide);
    return app.exec();
  }
}
