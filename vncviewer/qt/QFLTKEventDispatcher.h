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

#pragma once

#include <map>
#include <set>

#include <QAbstractEventDispatcher>

class QFLTKEventDispatcher : public QAbstractEventDispatcher
{
  Q_OBJECT
public:
  QFLTKEventDispatcher(QObject *parent=nullptr);
  ~QFLTKEventDispatcher();

  bool processEvents(QEventLoop::ProcessEventsFlags flags) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  bool hasPendingEvents() override;
#endif

  void registerSocketNotifier(QSocketNotifier *notifier) override;
  void unregisterSocketNotifier(QSocketNotifier *notifier) override;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#ifdef WIN32
  bool registerEventNotifier(QWinEventNotifier *notifier) override;
  void unregisterEventNotifier(QWinEventNotifier *notifier) override;
#endif
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void registerTimer(int timerId, int interval,
                     Qt::TimerType timerType, QObject* object) override;
#else
  void registerTimer(int timerId, qint64 interval,
                     Qt::TimerType timerType, QObject* object) override;
#endif
  QList<QAbstractEventDispatcher::TimerInfo>
    registeredTimers(QObject* object) const override;
  bool unregisterTimer(int timerId) override;
  bool unregisterTimers(QObject *object) override;
  int remainingTime(int timerId) override;

  void wakeUp() override;
  void interrupt() override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void flush() override;
#endif

private:
  static void dispatchTimer(void* data);
#ifdef WIN32
  static void dispatchSocket(unsigned long long fd, void* data);
#else
  static void dispatchSocket(int fd, void* data);
#endif

private:
  struct TimerContext {
    int timerId;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int interval;
#else
    qint64 interval;
#endif
    Qt::TimerType timerType;
    QObject* object;
    QFLTKEventDispatcher* dispatcher;
  };

  std::map<int, TimerContext> timers;
  std::set<QSocketNotifier*> notifiers;
};
