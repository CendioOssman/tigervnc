#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>

class QLineEdit;

class AuthDialog : public QDialog
{
  Q_OBJECT

public:
  AuthDialog(bool secured, bool userNeeded, bool passwordNeeded, QWidget* parent = nullptr);

  QString getUser() const;
  QString getPassword() const;

private:
  QLineEdit* userText = nullptr;
  QLineEdit* passwordText = nullptr;
};

#endif // AUTHDIALOG_H
