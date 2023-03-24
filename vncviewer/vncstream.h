#ifndef VNCSTREAM_H
#define VNCSTREAM_H

#include <QObject>
#include <QDataStream>
#include "rdr/types.h"

class QVNCStream : public QObject, public QDataStream
{
    Q_OBJECT

public:
    QVNCStream();
    QVNCStream(const QByteArray &a);
    QVNCStream(QByteArray *a, QIODevice::OpenMode mode);
    QVNCStream(QIODevice *d);
    virtual ~QVNCStream();
    size_t avail();
    bool flush();
    bool skip(int len);
    bool pad(int len);
    bool hasData(int len);
    bool hasDataOrRestore(int len);
    void setRestorePoint();
    void clearRestorePoint();
    void gotoRestorePoint();
    int readBytes(char *buf, int len);
    int writeBytes(char *buf, int len);
    rdr::U8 readU8();
    void writeU8(rdr::U8 value);
    rdr::U16 readU16();
    void writeU16(rdr::U16 value);
    rdr::U32 readU32();
    void writeU32(rdr::U32 value);
    rdr::U64 readU64();
    void writeU64(rdr::U64 value);
    rdr::S8 readS8();
    void writeS8(rdr::S8 value);
    rdr::S16 readS16();
    void writeS16(rdr::S16 value);
    rdr::S32 readS32();
    void writeS32(rdr::S32 value);
};

#endif // VNCSTREAM_H
