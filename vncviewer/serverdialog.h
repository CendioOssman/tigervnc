#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QWidget>

class QComboBox;

class ServerDialog : public QWidget
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

protected:
  void keyPressEvent(QKeyEvent* e) override;

private:
  QComboBox* comboBox;
};

#endif // SERVERDIALOG_H
