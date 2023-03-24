#ifndef VNCPACKETHANDLER_H
#define VNCPACKETHANDLER_H

#include <QObject>

class QVNCConnection;

class QVNCPacketHandler : public QObject
{
  Q_OBJECT

public:
  QVNCPacketHandler(QVNCConnection* cc = nullptr) : QObject(nullptr), m_cc(cc) {}
  virtual ~QVNCPacketHandler() {}
  virtual bool processMsg(int state) = 0;

protected:
  QVNCConnection *m_cc;
};

#endif // VNCPACKETHANDLER_H
