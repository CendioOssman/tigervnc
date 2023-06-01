#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QQmlEngine>
#include <QTcpSocket>
#include <QScreen>
#include <QProcess>
#include "rdr/Exception.h"
#include "i18n.h"
#include "vncconnection.h"
#include "viewerconfig.h"
#include "abstractvncview.h"
#include "parameters.h"
#include "appmanager.h"
#undef asprintf

#if defined(WIN32)
#include "vncwinview.h"
#elif defined(__APPLE__)
#include "vncmacview.h"
#elif defined(Q_OS_UNIX)
#include "vncx11view.h"
#endif

AppManager *AppManager::m_manager;

AppManager::AppManager()
 : QObject(nullptr)
 , m_error(0)
 , m_facade(new QVNCConnection)
 , m_view(nullptr)
 , m_scroll(new QVNCWindow)
{
  connect(this, &AppManager::connectToServerRequested, m_facade, &QVNCConnection::connectToServer);
  connect(m_facade, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow);
  connect(this, &AppManager::resetConnectionRequested, m_facade, &QVNCConnection::resetConnection);
}

AppManager::~AppManager()
{
  m_facade->deleteLater();
  m_scroll->takeWidget();
  delete m_scroll;
  delete m_view;
}

int AppManager::initialize()
{
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
  qmlRegisterType<QVNCConnection>("Qt.TigerVNC", 1, 0, "VNCConnection");
  m_manager = new AppManager();
  qmlRegisterSingletonType<AppManager>("Qt.TigerVNC", 1, 0, "AppManager", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return m_manager;
  });
  return 0;
}

void AppManager::connectToServer(const QString addressport)
{
  emit connectToServerRequested(addressport);
}

void AppManager::authenticate(QString user, QString password)
{
  emit authenticateRequested(user, password);
}

void AppManager::cancelAuth()
{
  emit cancelAuthRequested();
}

void AppManager::resetConnection()
{
  emit resetConnectionRequested();
}

void AppManager::publishError(const QString message, bool quit)
{
  emit errorOcurred(m_error++, message, quit);
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  QWidget *parent = nullptr;
  if (!::remoteResize) {
    m_scroll->takeWidget();
    parent = m_scroll;
  }
  delete m_view;
#if defined(WIN32)
  m_view = new QVNCWinView(parent);
#elif defined(__APPLE__)
  m_view = new QVNCMacView(parent);
#elif defined(Q_OS_UNIX)
  QString platform = QGuiApplication::platformName();
  if (platform == "xcb") {
    m_view = new QVNCX11View(parent);
  }
  else if (platform == "wayland") {
    ;
  }
#endif

  if (!m_view) {
    throw rdr::Exception(_("Platform not supported."));
  }
  connect(m_view, &QAbstractVNCView::fullscreenChanged, this, [this](bool enabled) {
    m_scroll->setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
  }, Qt::QueuedConnection);

  if (!::remoteResize) {
    connect(m_view, &QAbstractVNCView::delayedInitialized, m_scroll, &QVNCWindow::popupToast);
    m_view->resize(width, height);
    m_scroll->setWidget(m_view);
    m_scroll->resize(width, height);
    m_scroll->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
    m_scroll->show();
  }
  else {
    connect(m_view, &QAbstractVNCView::delayedInitialized, m_view, &QAbstractVNCView::popupToast);
    m_view->resize(width, height);
    m_view->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
    m_view->show();
  }

  if (ViewerConfig::config()->fullScreen()) {
    m_view->fullscreen(true);
  }

  emit vncWindowOpened();
}

void AppManager::setWindowName(QString name)
{
  if (!::remoteResize) {
    m_scroll->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  }
  else {
    m_view->setWindowTitle(QString::asprintf(_("%s - TigerVNC"), name.toStdString().c_str()));
  }
}

void AppManager::invalidate(int x0, int y0, int x1, int y1)
{
  emit invalidateRequested(x0, y0, x1, y1);
}

void AppManager::refresh()
{
  emit refreshRequested();
}

void AppManager::openContextMenu()
{
  emit contextMenuRequested();
}

void AppManager::openInfoDialog()
{
  emit infoDialogRequested();
}

void AppManager::openOptionDialog()
{
  emit optionDialogRequested();
}

void AppManager::openAboutDialog()
{
  emit aboutDialogRequested();
}

void AppManager::respondToMessage(int response)
{
  emit messageResponded(response);
}

QVNCApplication::QVNCApplication(int &argc, char **argv)
  : QApplication(argc, argv)
{
}

QVNCApplication::~QVNCApplication()
{
}

bool QVNCApplication::notify(QObject *receiver, QEvent *e)
{
  try {
    return QApplication::notify(receiver, e);
  }
  catch (rdr::Exception &e) {
    qDebug() << "Error: " << e.str();
    //AppManager::instance()->publishError(e.str());
    // Above 'emit' code is functional only when VNC connection class is running on a thread
    // other than GUI main thread.
    // Now, VNC connection class is running on GUI main thread, by the customer's request.
    // Because GUI main thread cannot use exceptions at all (by Qt spec), the application
    // must exit when the exception is received.
    // To avoid the undesired application exit, all exceptions must be handled in each points
    // where an exception may occurr.
    QCoreApplication::exit(1);
  }
  catch (int &e) {
    qDebug() << "Error: " << strerror(e);
    //AppManager::instance()->publishError(strerror(e));
    QCoreApplication::exit(1);
  }
  catch (...) {
    qDebug() << "Error: (unhandled)";
    QCoreApplication::exit(1);
  }
  return true;
}
