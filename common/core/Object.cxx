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

#include <core/Object.h>
#include <core/string.h>

using namespace core;

bool core::operator==(const Connection& a, const Connection& b)
{
  return a.sig == b.sig && a.src == b.src && a.dst == b.dst &&
         a.callback == b.callback;
}

Object::Object()
{
}

Object::~Object()
{
  // Disconnect from any signals we might have subscribed to
  while (!connectedObjects.empty())
    (*connectedObjects.begin())->disconnectSignals(this);

  // And prevent other objects from trying to disconnect from us as we
  // are going away
  while (!connections.empty())
    disconnectSignal(connections.front());
}

void Object::disconnectSignal(const Connection connection)
{
  std::list<Connection>::iterator iter;
  bool hasOthers;

  assert(connection.dst);

  if (connection.src != this)
    throw std::logic_error("Disconnecting signal from wrong object");

  connection.sig->disconnect(connection);
  for (iter = connections.begin(); iter != connections.end(); ++iter) {
    if (*iter == connection) {
      connections.erase(iter);
      break;
    }
  }

  hasOthers = false;

  // Then check if the object is attached in more ways to this or
  // to some other signal
  for (iter = connections.begin(); iter != connections.end(); ++iter) {
    if (iter->dst == connection.dst) {
      hasOthers = true;
      break;
    }
  }

  if (!hasOthers)
    connection.dst->connectedObjects.erase(this);
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
