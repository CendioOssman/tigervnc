#ifndef MSGREADER_H
#define MSGREADER_H

#include <QObject>
#include "rdr/types.h"
#include "rfb/Rect.h"

class QMutex;
class QVNCConnection;

namespace rdr {
  class InStream;
}

class QMsgReader : public QObject
{
    Q_OBJECT

public:
    QMsgReader(QVNCConnection* handler, rdr::InStream *is);
    virtual ~QMsgReader();

    bool readServerInit();

    // readMsg() reads a message, calling the handler as appropriate.
    bool readMsg();

    rdr::InStream *getInStream() { return is; }

    int imageBufIdealSize;

protected:
    bool readSetColourMapEntries();
    bool readBell();
    bool readServerCutText();
    bool readExtendedClipboard(rdr::S32 len);
    bool readFence();
    bool readEndOfContinuousUpdates();

    bool readFramebufferUpdate();

    bool readRect(const rfb::Rect& r, int encoding);

    bool readSetXCursor(int width, int height, const rfb::Point& hotspot);
    bool readSetCursor(int width, int height, const rfb::Point& hotspot);
    bool readSetCursorWithAlpha(int width, int height, const rfb::Point& hotspot);
    bool readSetVMwareCursor(int width, int height, const rfb::Point& hotspot);
    bool readSetDesktopName(int x, int y, int w, int h);
    bool readExtendedDesktopSize(int x, int y, int w, int h);
    bool readLEDState();
    bool readVMwareLEDState();

private:
    QVNCConnection *handler;
    rdr::InStream *is;

    enum stateEnum {
        MSGSTATE_IDLE,
        MSGSTATE_MESSAGE,
        MSGSTATE_RECT_HEADER,
        MSGSTATE_RECT_DATA,
    };

    stateEnum state;

    rdr::U8 currentMsgType;
    int nUpdateRectsLeft;
    rfb::Rect dataRect;
    int rectEncoding;

    int cursorEncoding;
    QMutex *m_mutex;

    static const int maxCursorSize = 256;
};

#endif // MSGREADER_H
