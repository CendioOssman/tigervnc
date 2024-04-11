#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class QAbstractVNCView;
class QVNCWindow;
class QTimer;
class QVNCConnection;
class ServerDialog;

class AppManager : public QObject
{
  Q_OBJECT

public:
  virtual ~AppManager();

  static AppManager* instance();

  QVNCConnection* getConnection() const { return connection; }

  int error() const { return errorCount; }

  QAbstractVNCView* getView() const { return view; }

  QVNCWindow* getWindow() const { return window; }

  bool isFullScreen() const;

signals:
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void messageDialogRequested(int flags, QString title, QString text);
  void dataReady(QByteArray bytes);
  void connectToServerRequested(const QString addressport);
  void authenticateRequested(QString user, QString password);
  void cancelAuthRequested();
  void messageResponded(int response);
  void newVncWindowRequested(int width, int height, QString name);
  void resetConnectionRequested();
  void invalidateRequested(int x0, int y0, int x1, int y1);
  void refreshRequested();
  void contextMenuRequested();
  void vncWindowOpened();
  void vncWindowClosed();

public slots:
  void publishError(const QString message, bool quit = false);
  void connectToServer(const QString addressport);
  void authenticate(QString user, QString password);
  void cancelAuth();
  void resetConnection();
  void openVNCWindow(int width, int height, QString name);
  void closeVNCWindow();
  void setWindowName(QString name);
  void refresh();
  void openContextMenu();
  void openInfoDialog();
  void openOptionDialog();
  void openAboutDialog();
  void openMessageDialog(int flags, QString title, QString text);
  void handleOptions();
  void openServerDialog();

private:
  QWidget* topWindow() const;

  int errorCount;
  QVNCConnection* connection;
  QAbstractVNCView* view = nullptr;
  QVNCWindow* window = nullptr;
  QTimer* rfbTimerProxy;
  ServerDialog* serverDialog = nullptr;
  AppManager();
};

#endif // APPMANAGER_H
