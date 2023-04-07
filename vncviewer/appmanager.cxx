#include <QQmlEngine>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QScreen>
#include "rdr/Exception.h"
#include "vncstream.h"
#include "vncconnection.h"
#include "viewerconfig.h"
#include "qdesktopwindow.h"
#include "vncwinview.h"
#include "appmanager.h"

AppManager *AppManager::m_manager;

AppManager::AppManager()
    : QObject(nullptr)
    , m_error(0)
    , m_worker(new QVNCConnection)
    , m_view(nullptr)
{
    m_worker->start();
    connect(m_worker, &QVNCConnection::credentialRequested, this, [this](bool secured, bool userNeeded, bool passwordNeeded) {
        emit credentialRequested(secured, userNeeded, passwordNeeded);
    }, Qt::QueuedConnection);
    connect(this, &AppManager::connectToServerRequested, m_worker, &QVNCConnection::connectToServer, Qt::QueuedConnection);
    connect(this, &AppManager::authenticateRequested, m_worker, &QVNCConnection::authenticate, Qt::QueuedConnection);
    connect(m_worker, &QVNCConnection::newVncWindowRequested, this, &AppManager::openVNCWindow, Qt::QueuedConnection);
    connect(this, &AppManager::resetConnectionRequested, m_worker, &QVNCConnection::resetConnection, Qt::QueuedConnection);
}

AppManager::~AppManager()
{
    m_worker->exit();
    m_worker->wait(QDeadlineTimer(1000));
    m_worker->deleteLater();
    delete m_view;
}

int AppManager::initialize()
{
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
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

void AppManager::resetConnection()
{
    emit resetConnectionRequested();
}

void AppManager::publishError(const QString &message)
{
    emit errorOcurred(m_error++, message);
}

void AppManager::openVNCWindow(int width, int height, QString name)
{
  delete m_view;
#if defined(WIN32)
  m_view = new QVNCWinView();
#endif

  m_view->resize(width, height);
  m_view->setWindowTitle(QString::asprintf("%.240s - TigerVNC", name.toStdString().c_str()));
  m_view->show();
}

void AppManager::update(int x0, int y0, int x1, int y1)
{
  emit updateRequested(x0, y0, x1, y1);
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
        AppManager::instance()->publishError(e.str());
        if (e.abort) {
          quit();
        }
    }
    catch (int &e) {
    qDebug() << "Error: " << strerror(e);
        AppManager::instance()->publishError(strerror(e));
    }
    catch (...) {
        qDebug() << "Error: (unhandled)";
    }
    return true;
}

