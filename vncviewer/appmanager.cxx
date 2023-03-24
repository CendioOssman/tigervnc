#include <QQmlEngine>
#include <QLocalSocket>
#include <QTcpSocket>
#include "rdr/Exception.h"
#include "vncstream.h"
#include "vncconnection.h"
#include "appmanager.h"

AppManager *AppManager::m_manager;

AppManager::AppManager()
    : QObject(nullptr)
    , m_error(0)
    , m_worker(new QVNCConnection)
    , m_socket(nullptr)
{
    m_worker->start();
    connect(m_worker, &QVNCConnection::credentialRequested, this, [this](bool secured, bool userNeeded, bool passwordNeeded) {
        emit credentialRequested(secured, userNeeded, passwordNeeded);
    }, Qt::QueuedConnection);
    connect(this, &AppManager::connectToServerRequested, m_worker, &QVNCConnection::connectToServer, Qt::QueuedConnection);
    connect(this, &AppManager::authenticateRequested, m_worker, &QVNCConnection::authenticate, Qt::QueuedConnection);
    connect(m_worker, &QVNCConnection::newVncWindowRequested, this, &AppManager::newVncWindowRequested, Qt::QueuedConnection);
}

AppManager::~AppManager()
{
    m_worker->exit();
    m_worker->wait(QDeadlineTimer(1000));
    m_worker->deleteLater();
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

void AppManager::bind(int fd)
{
    emit bindRequested(fd);
}

void AppManager::unbind(int fd)
{
    emit unbindRequested(fd);
}

void AppManager::connectToServer(const QString addressport)
{
    emit connectToServerRequested(addressport);
}

void AppManager::authenticate(QString user, QString password)
{
    emit authenticateRequested(user, password);
}

void AppManager::publishError(const QString &message)
{
    emit errorOcurred(m_error++, message);
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

