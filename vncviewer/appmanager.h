#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QApplication>

class QIODevice;
class QVNCConnection;

class AppManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int error READ error NOTIFY errorOcurred)
    Q_PROPERTY(QVNCConnection *connection READ connection CONSTANT)

public:
    virtual ~AppManager();
    static AppManager *instance() { return m_manager; }
    static int initialize();
    QVNCConnection *connection() const { return m_worker; }
    int error() const { return m_error; }
    void bind(int fd);
    void unbind(int fd);

signals:
    void errorOcurred(int seq, QString message);
    void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
    void dataReady(QByteArray bytes);
    void bindRequested(int fd);
    void unbindRequested(int fd);
    void connectToServerRequested(const QString addressport);
    void authenticateRequested(QString user, QString password);
    void newVncWindowRequested(int width, int height, QString name);

public slots:
    void publishError(const QString &message);
    void connectToServer(const QString addressport);
    void authenticate(QString user, QString password);

private:
    static AppManager *m_manager;
    int m_error;
    QVNCConnection *m_worker;
    QIODevice *m_socket;
    AppManager();
};

class QVNCApplication : public QApplication
{
    Q_OBJECT

public:
    QVNCApplication(int &argc, char **argv);
    virtual ~QVNCApplication();
    bool notify(QObject *receiver, QEvent *e) override;
};

#endif // APPMANAGER_H
