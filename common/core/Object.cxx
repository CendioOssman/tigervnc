/* Copyright 2022-2024 Pierre Ossman for Cendio AB
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
#include <core/string.h>

using namespace core;

static bool operator==(const Connection& a, const Connection& b)
{
  return a.sig == b.sig && a.src == b.src && a.dst == b.dst &&
         a.callback == b.callback;
}

signal::signal()
{
}

Connection signal::connect(Object* src, Object* dst,
                           const comp_any& callback,
                           const emitter_t& emitter)
{
  ReceiverList::iterator iter;
  Connection connection;

  assert(src);
  assert(dst);

  connection = {this, src, dst, callback};

  for (iter = receivers.begin(); iter != receivers.end(); ++iter) {
    if (iter->connection == connection)
      throw std::logic_error("Signal is already connected");
  }

  receivers.push_back({connection, emitter});

  return connection;
}

void signal::disconnect(const Connection connection)
{
  ReceiverList::iterator iter;

  assert(connection.sig == this);

  for (iter = receivers.begin(); iter != receivers.end(); ++iter) {
    if (iter->connection == connection) {
      receivers.erase(iter);
      break;
    }
  }
}

void signal::emit()
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = receivers;
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find_if(receivers.begin(), receivers.end(),
                     [iter](const SignalReceiver& recv) {
                       return recv.connection == iter->connection;
                     }) == receivers.end())
      continue;
    iter->emitter();
  }
}

Object::Object()
{
}

Object::~Object()
{
}

void Object::disconnectSignal(const Connection connection)
{
  std::list<Connection>::iterator iter;
  if (connection.src != this)
    throw std::logic_error("Disconnecting signal from wrong object");

  connection.sig->disconnect(connection);
  for (iter = connections.begin(); iter != connections.end(); ++iter) {
    if (*iter == connection) {
      connections.erase(iter);
      break;
    }
  }
}

void Object::emitSignal(signal& signal)
{
  signal.emit();
}

void Object::disconnectSignals(Object* obj)
{
  bool done;

  assert(obj);

  do {
    done = true;
    for (Connection& connection : connections) {
      if (connection.dst == obj) {
        disconnectSignal(connection);
        done = false;
        break;
      }
    }
  } while (!done);
}

Connection Object::connectSignal(signal& signal, class Object* obj,
                                 const comp_any& callback,
                                 const signal::emitter_t& emitter)
{
  Connection connection;

  connection = signal.connect(this, obj, callback, emitter);
  connections.push_back(connection);

  return connection;
}
