#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QProcess>
#if !defined(WIN32)
#include <QCoreApplication>
#endif
#include "viewerconfig.h"
#include "tunnelfactory.h"

TunnelFactory::TunnelFactory()
  : QThread(nullptr)
  , errorOccurred(false)
  , error(QProcess::FailedToStart)
#if defined(WIN32)
  , command(QString(qgetenv("SYSTEMROOT")) + "\\System32\\OpenSSH\\ssh.exe")
#else
  , command("/usr/bin/ssh")
  , operationSocketName("vncviewer-tun-" + QString::number(QCoreApplication::applicationPid()))
#endif
  , process(nullptr)
{
}

void TunnelFactory::run()
{
  QString gatewayHost = ViewerConfig::instance()->gatewayHost();
  if (gatewayHost.isEmpty()) {
    return;
  }
  QString remoteHost = ViewerConfig::instance()->getServerHost();
  if (remoteHost.isEmpty()) {
    return;
  }
  int remotePort = ViewerConfig::instance()->getServerPort();
  int localPort = ViewerConfig::instance()->getGatewayLocalPort();

  QString viacmd(qgetenv("VNC_VIA_CMD"));
  qputenv("G", gatewayHost.toUtf8());
  qputenv("H", remoteHost.toUtf8());
  qputenv("R", QString::number(remotePort).toUtf8());
  qputenv("L", QString::number(localPort).toUtf8());

  QStringList args;
  if (viacmd.isEmpty()) {
    args = QStringList({
#if !defined(WIN32)
      "-fnNTM", "-S", operationSocketName,
#endif
          "-L", QString::number(localPort) + ":" + remoteHost + ":" + QString::number(remotePort), gatewayHost,
    });
  } else {
#if !defined(WIN32)
    /* Compatibility with TigerVNC's method. */
    viacmd.replace('%', '$');
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    args = splitCommand(viacmd);
#else
    args = QProcess::splitCommand(viacmd);
#endif
    command = args.length() > 0 ? args[0] : "";
    args.removeFirst();
  }
  delete process;
  process = new QProcess;

#if !defined(WIN32)
  if (!process->execute(command, args)) {
    QString serverName = "localhost::" + QString::number(ViewerConfig::instance()->getGatewayLocalPort());
    ViewerConfig::instance()->addServer(serverName);
  } else {
    errorOccurred = true;
  }
#else
  connect(process, &QProcess::started, this, []() {
    QString serverName = "localhost::" + QString::number(ViewerConfig::instance()->getGatewayLocalPort());
    ViewerConfig::instance()->addServer(serverName);
  });
  connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
    errorOccurred = true;
    error = e;
  });
  process->start(command, args);
  while (true) {
    if (process->state() == QProcess::Running || errorOccurred) {
      break;
    }
    QThread::usleep(10);
  }
#endif
}

TunnelFactory::~TunnelFactory()
{
  close();
  delete process;
}

void TunnelFactory::close()
{
#if !defined(WIN32)
  if (process) {
    QString gatewayHost = ViewerConfig::instance()->gatewayHost();
    QStringList args({
        "-S",
        operationSocketName,
        "-O",
        "exit",
        gatewayHost,
    });
    QProcess process;
    process.start(command, args);
    QThread::msleep(500);
  }
#endif
  if (process && process->state() != QProcess::NotRunning) {
    process->kill();
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
QStringList TunnelFactory::splitCommand(QStringView command)
{
  QStringList args;
  QString tmp;
  int quoteCount = 0;
  bool inQuote = false;
  // handle quoting. tokens can be surrounded by double quotes
  // "hello world". three consecutive double quotes represent
  // the quote character itself.
  for (int i = 0; i < command.size(); ++i) {
    if (command.at(i) == QLatin1Char('"')) {
      ++quoteCount;
      if (quoteCount == 3) {
        // third consecutive quote
        quoteCount = 0;
        tmp += command.at(i);
      }
      continue;
    }
    if (quoteCount) {
      if (quoteCount == 1)
        inQuote = !inQuote;
      quoteCount = 0;
    }
    if (!inQuote && command.at(i).isSpace()) {
      if (!tmp.isEmpty()) {
        args += tmp;
        tmp.clear();
      }
    } else {
      tmp += command.at(i);
    }
  }
  if (!tmp.isEmpty())
    args += tmp;
  return args;
}
#endif
