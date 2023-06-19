#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>
#include "vncconnection.h"

class QAbstractVNCView;
class QVNCWindow;

class AppManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int error READ error NOTIFY errorOcurred)
  Q_PROPERTY(QVNCConnection *connection READ connection CONSTANT)
  Q_PROPERTY(QAbstractVNCView *view READ view CONSTANT)

public:
  virtual ~AppManager();
  static AppManager *instance() { return manager_; }
  static int initialize();
  QVNCConnection *connection() const { return facade_; }
  int error() const { return error_; }
  QAbstractVNCView *view() const { return view_; }
  QVNCWindow *window() const { return scroll_; }

signals:
  void errorOcurred(int seq, QString message, bool quit = false);
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
  void infoDialogRequested();
  void optionDialogRequested();
  void aboutDialogRequested();
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
  /**
   * @brief Request the framebuffer to add the given dirty region. Typically, called
   * by PlatformPixelBuffer::commitBufferRW().
   * @param x0 X of the top left point.
   * @param y0 Y of the top left point.
   * @param x1 X of the bottom right point.
   * @param y1 Y of the bottom right point.
   */
  void invalidate(int x0, int y0, int x1, int y1);
  void refresh();
  void openContextMenu();
  void openInfoDialog();
  void openOptionDialog();
  void openAboutDialog();
  void respondToMessage(int response);

private:
  static AppManager *manager_;
  int error_;
  QVNCConnection *facade_;
  QAbstractVNCView *view_;
  QVNCWindow *scroll_;
  AppManager();
};

#endif // APPMANAGER_H
