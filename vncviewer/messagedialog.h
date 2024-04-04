#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QDialog>

class MessageDialog : public QDialog
{
  Q_OBJECT

public:
  MessageDialog(bool staysOnTop, int flags, QString title, QString text, QWidget* parent = nullptr);
};

#endif // MESSAGEDIALOG_H
