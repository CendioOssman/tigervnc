/* Copyright 2022-2026 Pierre Ossman for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include <algorithm>
#include <stdexcept>

#include <core/Object.h>

using namespace core;

static bool operator==(const Connection& a, const Connection& b)
{
  return a.signal == b.signal && a.src == b.src && a.dst == b.dst &&
         a.comparer(a.callback, b.callback);
}

struct Object::SignalReceiver {
  Connection connection;
  emitter_t emitter;
};

Object::Object()
{
}

Object::~Object()
{
}

void Object::emitSignalImpl(const void* signal)
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  assert(signal);

  if (signalReceivers.count(signal) == 0)
    return;

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = signalReceivers[signal];
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find_if(signalReceivers[signal].begin(),
                     signalReceivers[signal].end(),
                     [iter](const SignalReceiver& recv) {
                       return recv.connection == iter->connection;
                     }) == signalReceivers[signal].end())
      continue;
    iter->emitter();
  }
}

Connection Object::connectSignalImpl(const void* signal, Object* obj,
                                     const std::any& callback,
                                     bool (*comparer)(const std::any&,
                                                      const std::any&),
                                     const emitter_t& emitter)
{
  ReceiverList::iterator iter;
  Connection connection;

  assert(signal);
  assert(obj);

  if (signalReceivers.count(signal) == 0)
    signalReceivers[signal].clear();

  connection = {signal, this, obj, callback, comparer};

  for (iter = signalReceivers[signal].begin();
       iter != signalReceivers[signal].end(); ++iter) {
    if (iter->connection == connection)
      throw std::logic_error("Signal is already connected");
  }

  signalReceivers[signal].push_back({connection, emitter});

  return connection;
}

void Object::disconnectSignal(const Connection connection)
{
  ReceiverList::iterator iter;

  if (connection.src != this)
    throw std::logic_error("Disconnecting signal from wrong object");

  for (iter = signalReceivers[connection.signal].begin();
       iter != signalReceivers[connection.signal].end(); ++iter) {
    if (iter->connection == connection) {
      signalReceivers[connection.signal].erase(iter);
      break;
    }
  }
}

void Object::disconnectSignals(Object* obj)
{
  std::map<const void*, ReceiverList>::iterator sigiter;

  assert(obj);

  for (sigiter = signalReceivers.begin();
       sigiter != signalReceivers.end(); ++sigiter) {
    ReceiverList* siglist;
    ReceiverList::iterator iter;

    siglist = &sigiter->second;
    iter = siglist->begin();
    while (iter != siglist->end()) {
      if (iter->connection.dst == obj)
        siglist->erase(iter++);
      else
        ++iter;
    }
  }
}
