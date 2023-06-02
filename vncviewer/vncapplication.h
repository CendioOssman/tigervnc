#ifndef VNCAPPLICATION_H
#define VNCAPPLICATION_H

#include <QApplication>

class QVNCApplication : public QApplication
{
  Q_OBJECT

public:
  QVNCApplication(int &argc, char **argv);
  virtual ~QVNCApplication();
  bool notify(QObject *receiver, QEvent *e) override;
};

#endif // VNCAPPLICATION_H
