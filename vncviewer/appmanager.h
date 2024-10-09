#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class DesktopWindow;
class QTimer;
class QVNCConnection;
class ServerDialog;
class QDialog;

class AppManager : public QObject
{
  Q_OBJECT

public:
  virtual ~AppManager();

  static AppManager* instance();

  void initialize();

  int exec();

  bool isFullScreen() const;

  void setCommandLine(bool b) { commandLine = b; }

  bool isCommandLine() const { return commandLine; }

signals:
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void messageDialogRequested(int flags, QString title, QString text);
  void dataReady(QByteArray bytes);
  void connectToServerRequested();
  void authenticateRequested(QString user, QString password);
  void cancelAuthRequested();
  void newVncWindowRequested(int width, int height, QString name);
  void resetConnectionRequested();
  void invalidateRequested(int x0, int y0, int x1, int y1);
  void refreshRequested();
  void contextMenuRequested();
  void vncWindowOpened();
  void vncWindowClosed();

public slots:
  void publishError(const QString message, bool quit = false);
  void publishUnexpectedError(QString message, bool quit = false);
  void connectToServer();
  void authenticate(QString user, QString password);
  void cancelAuth();
  void resetConnection();
  void openVNCWindow(int width, int height, QString name);
  void closeVNCWindow();
  void setWindowName(QString name);
  void refresh();
  void openContextMenu();
  void openServerDialog();
  void openDialog(QDialog* d);

private:
  QWidget* topWindow() const;

  bool commandLine = false;
  bool connectedOnce = false;
  bool fatalError = false;
  std::string exitError;
  QVNCConnection* connection;
  DesktopWindow* window = nullptr;
  QTimer* rfbTimerProxy;
  ServerDialog* serverDialog = nullptr;
  AppManager();
};

#endif // APPMANAGER_H
