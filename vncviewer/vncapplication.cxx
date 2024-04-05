#include <QDebug>
#include "rdr/Exception.h"
#include "rfb/LogWriter.h"
#include "vncapplication.h"

static rfb::LogWriter vlog("QVNCApplication");

QVNCApplication::QVNCApplication(int &argc, char **argv)
  : QApplication(argc, argv)
{
}

QVNCApplication::~QVNCApplication()
{
}

bool QVNCApplication::notify(QObject *receiver, QEvent *e)
{
  try {
    return QApplication::notify(receiver, e);
  }
  catch (rdr::Exception &e) {
    vlog.error("Error: %s", e.str());
    //AppManager::instance()->publishError(e.str());
    // Above 'emit' code is functional only when VNC connection class is running on a thread
    // other than GUI main thread.
    // Now, VNC connection class is running on GUI main thread, by the customer's request.
    // Because GUI main thread cannot use exceptions at all (by Qt spec), the application
    // must exit when the exception is received.
    // To avoid the undesired application exit, all exceptions must be handled in each points
    // where an exception may occurr.
    QCoreApplication::exit(1);
  }
  catch (int &e) {
    vlog.error("Error: %s", strerror(e));
    //AppManager::instance()->publishError(strerror(e));
    QCoreApplication::exit(1);
  }
  catch (...) {
    vlog.error("Error: (unhandled)");
    QCoreApplication::exit(1);
  }
  return true;
}
