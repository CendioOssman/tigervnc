#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QStyle>
#include "parameters.h"
#include "appmanager.h"
#include "vncapplication.h"
#include "vncconnection.h"
#include "vnctranslator.h"


class StandardIconProvider : public QQuickImageProvider
{
public:
  StandardIconProvider(QStyle *style)
    : QQuickImageProvider(Pixmap)
    , style_(style)
  {}

  QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
  {
    Q_UNUSED(size)
    const int defaultWidth = 48;
    const int defaultHeight = 48;
    QSize imageSize = { requestedSize.width() > 0 ? requestedSize.width() : defaultWidth, requestedSize.height() > 0 ? requestedSize.height() : defaultHeight };
    static const auto metaobject = QMetaEnum::fromType<QStyle::StandardPixmap>();
    const int value = metaobject.keyToValue(id.toLatin1());
    QIcon icon = style_->standardIcon(static_cast<QStyle::StandardPixmap>(value));
    return icon.pixmap(imageSize);
  }
private:
  QStyle *style_;
};

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

  ViewerConfig::initialize();
  AppManager::initialize();

  VNCTranslator translator;
  app.installTranslator(&translator);

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("qticons"), new StandardIconProvider(app.style()));
  const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
    if (!obj && url == objUrl) {
      QCoreApplication::exit(-1);
    }
  }, Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
