#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QDialog>

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
  void openLoadConfigDialog();
  void openSaveConfigDialog();

private:
  void loadServerHistory();
  void saveServerHistory();

private:
  QComboBox* comboBox;
  QStringList serverHistory;
};

#endif // SERVERDIALOG_H
