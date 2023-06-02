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
  bool errorOccurred() const { return errorOccurred_; }
  QProcess::ProcessError error() const { return error_; }

protected:
  void run() override;

private:
  bool errorOccurred_;
  QProcess::ProcessError error_;
  QString command_;
#if !defined(WIN32)
  QString operationSocketName_;
#endif
  QProcess *process_;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  QStringList splitCommand(QStringView command);
#endif
};

#endif // TUNNELFACTORY_H
