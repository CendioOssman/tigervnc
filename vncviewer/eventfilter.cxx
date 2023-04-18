#include <QDebug>
#include <QKeyEvent>
#include "eventfilter.h"

EventListener::EventListener()
    : QObject(nullptr)
{
}

bool EventListener::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        qDebug() << "EventListener: VirtualKey=" << Qt::hex << keyEvent->nativeVirtualKey() << ", ScanCode=" << Qt::hex << keyEvent->nativeScanCode() << ", key=" << Qt::hex << keyEvent->key() << ", modifiers=" << Qt::hex << keyEvent->modifiers();
#endif
//        return true;
    }
    QObject::eventFilter(watched, event);
    return false;
}
