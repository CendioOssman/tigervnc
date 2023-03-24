#include <QAbstractSocket>
#include <QtEndian>
#include "vncstream.h"

QVNCStream::QVNCStream()
    : QDataStream()
{
}

QVNCStream::QVNCStream(const QByteArray &a)
    : QDataStream(a)
{
}

QVNCStream::QVNCStream(QByteArray *a, QIODevice::OpenMode mode)
    : QDataStream(a, mode)
{
}

QVNCStream::QVNCStream(QIODevice *d)
    : QDataStream(d)
{
}

QVNCStream::~QVNCStream()
{
}

size_t QVNCStream::avail()
{
    return device()->bytesAvailable();
}

bool QVNCStream::flush()
{
    QAbstractSocket *socket = dynamic_cast<QAbstractSocket*>(device());
    if (socket) {
        return socket->flush();
    }
    return true;
}

bool QVNCStream::skip(int len)
{
    return skipRawData(len) == len;
}

bool QVNCStream::pad(int len)
{
    QByteArray bytes(len, 0);
    return writeRawData(bytes.constData(), len) == len;
}

bool QVNCStream::hasData(int len)
{
    return device()->bytesAvailable() >= len;
}

bool QVNCStream::hasDataOrRestore(int len)
{
    if (hasData(len)) {
        return true;
    }
    rollbackTransaction();
    return false;
}

void QVNCStream::setRestorePoint()
{
    startTransaction();
}

void QVNCStream::clearRestorePoint()
{
    abortTransaction();
}

void QVNCStream::gotoRestorePoint()
{
    rollbackTransaction();
}

int QVNCStream::readBytes(char *buf, int len)
{
    return readRawData(buf, len);
}

int QVNCStream::writeBytes(char *buf, int len)
{
    return writeRawData(buf, len);
}

rdr::U8 QVNCStream::readU8()
{
    rdr::U8 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void QVNCStream::writeU8(rdr::U8 value)
{
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::U16 QVNCStream::readU16()
{
    rdr::U16 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qFromBigEndian<rdr::U16>(value);
#endif
    return value;
}

void QVNCStream::writeU16(rdr::U16 value)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qToBigEndian<rdr::U16>(value);
#endif
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::U32 QVNCStream::readU32()
{
    rdr::U32 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qFromBigEndian<rdr::U32>(value);
#endif
    return value;
}

void QVNCStream::writeU32(rdr::U32 value)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qToBigEndian<rdr::U32>(value);
#endif
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::U64 QVNCStream::readU64()
{
    rdr::U64 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qFromBigEndian<rdr::U64>(value);
#endif
    return value;
}

void QVNCStream::writeU64(rdr::U64 value)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qToBigEndian<rdr::U64>(value);
#endif
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::S8 QVNCStream::readS8()
{
    rdr::S8 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

void QVNCStream::writeS8(rdr::S8 value)
{
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::S16 QVNCStream::readS16()
{
    rdr::S16 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qFromBigEndian<rdr::S16>(value);
#endif
    return value;
}

void QVNCStream::writeS16(rdr::S16 value)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qToBigEndian<rdr::S16>(value);
#endif
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}

rdr::S32 QVNCStream::readS32()
{
    rdr::S32 value;
    readRawData(reinterpret_cast<char*>(&value), sizeof(value));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qFromBigEndian<rdr::S32>(value);
#endif
    return value;
}

void QVNCStream::writeS32(rdr::S32 value)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value = qToBigEndian<rdr::S32>(value);
#endif
    writeBytes(reinterpret_cast<char*>(&value), sizeof(value));
}
