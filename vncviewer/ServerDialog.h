#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QDialog>

class QComboBox;

class ServerDialog : public QDialog
{
  Q_OBJECT

public:
  ServerDialog(QWidget* parent = nullptr);

  void updateServerList(QStringList list);
  void validateServerText(QString text);
  void connectTo();

  void openOptionDialog();
  void openAboutDialog();
  void openLoadConfigDialog();
  void openSaveConfigDialog();

private:
  QComboBox* comboBox;
};

#endif // SERVERDIALOG_H
