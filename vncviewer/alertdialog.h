#ifndef ALERTDIALOG_H
#define ALERTDIALOG_H

#include <QDialog>

class AlertDialog : public QDialog
{
  Q_OBJECT

public:
  AlertDialog(bool staysOnTop, QString message, bool quit, QWidget* parent = nullptr);
};

#endif // ALERTDIALOG_H
