#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class DesktopWindow;
class QTimer;
class CConn;
class ServerDialog;
class QDialog;

namespace network {
  class Socket;
}

class AppManager : public QObject
{
  Q_OBJECT

public:
  virtual ~AppManager();

  static AppManager* instance();

  void initialize();

  int exec(const char* vncserver, network::Socket* sock);

  void setCommandLine(bool b) { commandLine = b; }

  bool isCommandLine() const { return commandLine; }

signals:
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void authenticateRequested(QString user, QString password);
  void cancelAuthRequested();

public slots:
  void publishError(const QString message, bool quit = false);
  void publishUnexpectedError(QString message, bool quit = false);
  bool should_disconnect();
  void authenticate(QString user, QString password);
  void cancelAuth();
  void openDialog(QDialog* d);

private:
  bool commandLine = false;
  bool connectedOnce = false;
  bool fatalError = false;
  std::string exitError;
  CConn* connection = nullptr;
  QTimer* rfbTimerProxy;
  AppManager();
};

#endif // APPMANAGER_H
