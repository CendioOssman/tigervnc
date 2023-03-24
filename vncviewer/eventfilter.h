#ifndef EVENTLISTENER_H
#define EVENTLISTENER_H

#include <QObject>

class QEvent;

class EventListener : public QObject
{
    Q_OBJECT

public:
    EventListener();

    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // EVENTLISTENER_H
