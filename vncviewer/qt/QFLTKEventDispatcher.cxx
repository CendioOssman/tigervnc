/* Copyright 2026 Pierre Ossman for Cendio AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <unistd.h>

#include <FL/Fl.H>

#include <QGuiApplication>
#include <QScreen>
#include <QSocketNotifier>
#include <QTimerEvent>
#if !defined(WIN32) && !defined(__APPLE__)
#include <QtXcb/qxcbscreen.h>
#endif
#include <qpa/qwindowsysteminterface.h>

#include "QFLTKEventDispatcher.h"

QFLTKEventDispatcher::QFLTKEventDispatcher(QObject *parent)
  : QAbstractEventDispatcher(parent)
{
  // Initialize FLTK threading support for Fl::awake()
  Fl::lock();
  Fl::unlock();
}

QFLTKEventDispatcher::~QFLTKEventDispatcher()
{
  while (!timers.empty())
    unregisterTimer(timers.begin()->first);
}

bool QFLTKEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags)
{
  const double FOREVER = 1e20;
  bool sentEvents;

  if (flags & QEventLoop::WaitForMoreEvents)
    emit aboutToBlock();
  else
    emit awake();

  if (flags & QEventLoop::WaitForMoreEvents)
    sentEvents = Fl::wait(FOREVER) > 0;
  else
    sentEvents = Fl::wait(0) > 0;

  QCoreApplication::sendPostedEvents();

  if (flags & QEventLoop::WaitForMoreEvents)
    emit awake();

#if !defined(WIN32) && !defined(__APPLE__)
  // Ugly hack because the xcb QPA assumes it has its own dispatcher
  QScreen* screen = QGuiApplication::primaryScreen();
  assert(screen != nullptr);
  QXcbScreen* xcbScreen = dynamic_cast<QXcbScreen*>(screen->handle());
  assert(xcbScreen != nullptr);
  QXcbConnection* connection = xcbScreen->connection();
  assert(connection != nullptr);
  connection->processXcbEvents(flags);
#endif

  return QWindowSystemInterface::sendWindowSystemEvents(flags) || sentEvents;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool QFLTKEventDispatcher::hasPendingEvents()
{
  return false;
}
#endif

void QFLTKEventDispatcher::registerSocketNotifier(QSocketNotifier *notifier)
{
  int when;

  switch (notifier->type()) {
  case QSocketNotifier::Read:
    when = FL_READ;
    break;
  case QSocketNotifier::Write:
    when = FL_WRITE;
    break;
  case QSocketNotifier::Exception:
    when = FL_EXCEPT;
    break;
  default:
    assert(false);
    return;
  }

  assert(notifiers.count(notifier) == 0);

  notifiers.insert(notifier);
  Fl::add_fd(notifier->socket(), when, dispatchSocket, notifier);
}

void QFLTKEventDispatcher::unregisterSocketNotifier(QSocketNotifier *notifier)
{
  int when;

  switch (notifier->type()) {
  case QSocketNotifier::Read:
    when = FL_READ;
    break;
  case QSocketNotifier::Write:
    when = FL_WRITE;
    break;
  case QSocketNotifier::Exception:
    when = FL_EXCEPT;
    break;
  default:
    assert(false);
    return;
  }

  assert(notifiers.count(notifier) != 0);

  notifiers.erase(notifier);
  Fl::remove_fd(notifier->socket(), when);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#ifdef WIN32
bool QFLTKEventDispatcher::registerEventNotifier(QWinEventNotifier *notifier)
{
  (void)notifier;
  // FIXME
  return false;
}

void QFLTKEventDispatcher::unregisterEventNotifier(QWinEventNotifier *notifier)
{
  (void)notifier;
}
#endif
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void QFLTKEventDispatcher::registerTimer(int timerId, int interval,
                                         Qt::TimerType timerType,
                                         QObject* object)
#else
void QFLTKEventDispatcher::registerTimer(int timerId, qint64 interval,
                                         Qt::TimerType timerType,
                                         QObject* object)
#endif
{
  assert(timers.count(timerId) == 0);
  timers[timerId] = TimerContext{timerId, interval, timerType, object, this};
  Fl::add_timeout(interval / 1000.0, dispatchTimer, &timers[timerId]);
  (void)timerType;
}

QList<QAbstractEventDispatcher::TimerInfo>
QFLTKEventDispatcher::registeredTimers(QObject* object) const
{
  QList<QAbstractEventDispatcher::TimerInfo> infos;

  for (std::pair<int, TimerContext> timer : timers) {
    if (timer.second.object == object)
      infos.push_back({timer.first, (int)timer.second.interval,
                       timer.second.timerType});
  }

  return infos;
}

bool QFLTKEventDispatcher::unregisterTimer(int timerId)
{
  assert(timers.count(timerId) != 0);

  Fl::remove_timeout(dispatchTimer, &timers[timerId]);
  timers.erase(timerId);

  return true;
}

bool QFLTKEventDispatcher::unregisterTimers(QObject *object)
{
  bool done;

  do {
    done = true;
    for (std::pair<int, TimerContext> timer : timers) {
      if (timer.second.object == object) {
        if (!unregisterTimer(timer.first))
          return false;
        done = false;
        break;
      }
    }
  } while(!done);

  return true;
}

int QFLTKEventDispatcher::remainingTime(int timerId)
{
  (void)timerId;
  // FIXME
  return 0;
}

void QFLTKEventDispatcher::wakeUp()
{
  Fl::awake();
}

void QFLTKEventDispatcher::interrupt()
{
  // Not possible with FLTK, so we'll just do a normal wake
  wakeUp();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void QFLTKEventDispatcher::flush()
{
}
#endif

void QFLTKEventDispatcher::dispatchTimer(void* data)
{
  TimerContext* context = (TimerContext*)(data);
  int timerId = context->timerId;
  QFLTKEventDispatcher* self = context->dispatcher;

  assert(self != nullptr);

  if (self->timers.count(timerId) == 0)
    return;

  QObject* object = self->timers[timerId].object;

  QTimerEvent event(timerId);
  QCoreApplication::sendEvent(object, &event);
}

#ifdef WIN32
void QFLTKEventDispatcher::dispatchSocket(unsigned long long fd, void* data)
#else
void QFLTKEventDispatcher::dispatchSocket(int fd, void* data)
#endif
{
  QSocketNotifier* notifier = (QSocketNotifier*)data;

  (void)fd;

  QEvent event(QEvent::SockAct);
  QCoreApplication::sendEvent(notifier, &event);
}
