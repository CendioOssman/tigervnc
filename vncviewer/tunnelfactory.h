#ifndef TUNNELFACTORY_H
#define TUNNELFACTORY_H

#include <QThread>
#include <QProcess>

class TunnelFactory : public QThread
{
  Q_OBJECT

public:
  TunnelFactory();
  virtual ~TunnelFactory();
  void close();
  bool hasErrorOccurred() const { return errorOccurred; }
  QProcess::ProcessError getError() const { return error; }

protected:
  void run() override;

private:
  bool errorOccurred;
  QProcess::ProcessError error;
  QString command;
#if !defined(WIN32)
  QString operationSocketName;
#endif
  QProcess *process;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  QStringList splitCommand(QStringView command);
#endif
};

#endif // TUNNELFACTORY_H
