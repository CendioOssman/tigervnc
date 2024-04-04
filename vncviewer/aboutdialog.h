#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  AboutDialog(bool staysOnTop, QWidget* parent = nullptr);
};

#endif // ABOUTDIALOG_H
