#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class Viewport;
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

  QVNCConnection* getConnection() const { return connection; }

  int error() const { return errorCount; }

  Viewport* getView() const { return view; }

  DesktopWindow* getWindow() const { return window; }

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
  void openErrorDialog(QString message);
  void openInfoDialog();
  void openOptionDialog();
  void openAboutDialog();
  int openMessageDialog(int flags, QString title, QString text);
  void handleOptions();
  void openServerDialog();

private:
  QWidget* topWindow() const;
  void openDialog(QDialog* d);

  bool commandLine = false;
  bool connectedOnce = false;
  int errorCount;
  QVNCConnection* connection;
  Viewport* view = nullptr;
  DesktopWindow* window = nullptr;
  QTimer* rfbTimerProxy;
  ServerDialog* serverDialog = nullptr;
  AppManager();
};

#endif // APPMANAGER_H
