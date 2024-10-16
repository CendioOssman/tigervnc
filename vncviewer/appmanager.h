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

public slots:
  void openDialog(QDialog* d);

private:
  AppManager();
};

#endif // APPMANAGER_H
