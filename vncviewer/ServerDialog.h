#ifndef SERVERDIALOG_H
#define SERVERDIALOG_H

#include <QDialog>
// Conflict with /usr/include/X11/X.h
#undef Unsorted
#include <QFileDialog>

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
  void handleLoad(const QFileDialog* filechooser);
  void initSaveAs();
  void handleSaveAs(const QFileDialog* filechooser);

private:
  void loadServerHistory();
  void saveServerHistory();

private:
  QComboBox* comboBox;
  std::list<std::string> serverHistory;

protected:
  QFileDialog *fileChooser;
};

#endif // SERVERDIALOG_H
