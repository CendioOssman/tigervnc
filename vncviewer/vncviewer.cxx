#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QFont>
#include <QDebug>
#include "viewerconfig.h"
#include "appmanager.h"
#include "vncconnection.h"
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
  app.setApplicationName("TigerVNC Viewer");

//#if 0
//  QFont font("Arial");
//  font.setStyleStrategy(QFont::NoAntialias);
//  //font.setStyleHint(QFont::Monospace);
//  QVNCApplication::setFont(font);
//#else
//  QFont font = QVNCApplication::font();
//  font.setStyleHint(QFont::Helvetica, (QFont::StyleStrategy)(QFont::PreferAntialias | QFont::PreferQuality));
//  QVNCApplication::setFont(font);
//#endif

  ViewerConfig::initialize();
  AppManager::initialize();

  VNCTranslator translator;
  app.installTranslator(&translator);

  QQmlApplicationEngine engine;
  const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
    if (!obj && url == objUrl) {
      QCoreApplication::exit(-1);
    }
  }, Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
