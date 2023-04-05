#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QApplication>

class QIODevice;
class QVNCConnection;
class QAbstractVNCView;

class AppManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int error READ error NOTIFY errorOcurred)
  Q_PROPERTY(QVNCConnection *connection READ connection CONSTANT)
  Q_PROPERTY(QAbstractVNCView *view READ view CONSTANT)

public:
  virtual ~AppManager();
  static AppManager *instance() { return m_manager; }
  static int initialize();
  QVNCConnection *connection() const { return m_worker; }
  int error() const { return m_error; }
  QAbstractVNCView *view() const { return m_view; }

signals:
  void errorOcurred(int seq, QString message);
  void credentialRequested(bool secured, bool userNeeded, bool passwordNeeded);
  void dataReady(QByteArray bytes);
  void connectToServerRequested(const QString addressport);
  void authenticateRequested(QString user, QString password);
  void resetConnectionRequested();
  void newVncWindowRequested(int width, int height, QString name);
  void updateRequested(int x0, int y0, int x1, int y1);
  void contextMenuRequested();
  void infoDialogRequested();
  void optionDialogRequested();
  void aboutDialogRequested();

public slots:
  void publishError(const QString &message);
  void connectToServer(const QString addressport);
  void authenticate(QString user, QString password);
  void resetConnection();
  void openVNCWindow(int width, int height, QString name);
  /**
   * @brief Request the framebuffer to add the given dirty region. Typically, called
   * by PlatformPixelBuffer::commitBufferRW().
   * @param x0 X of the top left point.
   * @param y0 Y of the top left point.
   * @param x1 X of the bottom right point.
   * @param y1 Y of the bottom right point.
   */
  void update(int x0, int y0, int x1, int y1);
  void openContextMenu();
  void openInfoDialog();
  void openOptionDialog();
  void openAboutDialog();

private:
  static AppManager *m_manager;
  int m_error;
  QVNCConnection *m_worker;
  QAbstractVNCView *m_view;
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
