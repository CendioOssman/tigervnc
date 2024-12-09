#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QDialog>
// Conflict with /usr/include/X11/X.h
#undef Unsorted
#include <QDir>

class QComboBox;

class ServerDialog : public QDialog
{
  Q_OBJECT

public:
  ServerDialog(QWidget* parent = nullptr);

  void setServerName(const char* name);
  QString getServerName();

  void connectTo();

  void openOptionDialog();
  void openAboutDialog();
  void initLoad();
  void handleLoad(const QString& filename);
  void initSaveAs();
  void handleSaveAs(const QString& filename);

private:
  void loadServerHistory();
  void saveServerHistory();

private:
  QComboBox* comboBox;
  QStringList serverHistory;

protected:
  QDir usedDir;
};

#endif // SERVERDIALOG_H
