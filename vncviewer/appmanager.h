#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class DesktopWindow;
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

  void setCommandLine(bool b) { commandLine = b; }

  bool isCommandLine() const { return commandLine; }

public slots:
  void openDialog(QDialog* d);

private:
  bool commandLine = false;
  bool connectedOnce = false;
  AppManager();
};

#endif // APPMANAGER_H
