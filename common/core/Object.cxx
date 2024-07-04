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

#include <algorithm>
#include <stdexcept>

#include <core/Object.h>
#include <core/string.h>

using namespace core;

static bool operator==(const Connection& a, const Connection& b)
{
  return a.name == b.name && a.src == b.src && a.dst == b.dst &&
         a.callback == b.callback;
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
  std::map<std::string, ReceiverList>::iterator sigiter;

  // Disconnect from any signals we might have subscribed to
  while (!connectedObjects.empty())
    (*connectedObjects.begin())->disconnectSignals(this);

  // And prevent other objects from trying to disconnect from us as we
  // are going away
  for (sigiter = signalReceivers.begin();
       sigiter != signalReceivers.end(); ++sigiter) {
    ReceiverList* siglist;

    siglist = &sigiter->second;
    while (!siglist->empty())
      disconnectSignal(siglist->front().connection);
  }
}

void Object::registerSignal(const char* name, size_t argType)
{
  assert(name);

  if (signalReceivers.count(name) != 0)
    throw std::logic_error(format("Signal %s is already registered", name));

  // Just to force it being created
  signalReceivers[name].clear();

  signalArgTypes[name] = argType;
}

void Object::emitSignal(const char* name)
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot emit unknown signal %s", name));

  if (signalArgTypes[name] != typeid(void).hash_code())
    throw std::logic_error(format("Missing data when emitting signal %s", name));

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = signalReceivers[name];
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find_if(signalReceivers[name].begin(),
                     signalReceivers[name].end(),
                     [iter](const SignalReceiver& recv) {
                       return recv.connection == iter->connection;
                     }) == signalReceivers[name].end())
      continue;
    iter->emitter(any());
  }
}

void Object::emitSignal(const char* name, const any& info)
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot emit unknown signal %s", name));

  if (signalArgTypes[name] == typeid(void).hash_code())
    throw std::logic_error(format("Unexpected data emitting signal %s", name));

  if (signalArgTypes[name] != info.type().hash_code())
    throw std::logic_error(format("Incompatible signal data emitting signal %s", name));

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = signalReceivers[name];
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find_if(signalReceivers[name].begin(),
                     signalReceivers[name].end(),
                     [iter](const SignalReceiver& recv) {
                       return recv.connection == iter->connection;
                     }) == signalReceivers[name].end())
      continue;
    iter->emitter(info);
  }
}

Connection Object::connectSignal(const char* name, void (*callback)())
{
  emitter_t emitter = [callback](const any& info) {
    assert(!info.has_value());
    callback();
  };
  // It's not guaranteed if we get unique or identical addresses for
  // otherwise identical lambdas. Treat each as unique for consistent
  // behaviour by omitting any tracking information.
  return connectSignal(name, nullptr, emitter, typeid(void).hash_code());
}

Connection Object::connectSignal(const char* name, Object* obj,
                                 const std::function<void()>& callback)
{
  emitter_t emitter = [callback](const any& info) {
    assert(!info.has_value());
    callback();
  };
  assert(obj);
  // Lambdas cannot be compared, so we cannot tell if it's an identical
  // lambda, or just the same body but with different captures.
  return connectSignal(name, obj, emitter, typeid(void).hash_code());
}

Connection Object::connectSignal(const char* name, Object* obj,
                                 const emitter_t& emitter,
                                 size_t argType)
{
  static uint64_t index = 0;
  // This callback is not possible to check for uniqueness, so instead
  // we assume every call is unique and track them using an index.
  return connectSignal(name, obj, index++, emitter, argType);
}

Connection Object::connectSignal(const char* name, Object* obj,
                                 const comp_any& callback,
                                 const emitter_t& emitter,
                                 size_t argType)
{
  ReceiverList::iterator iter;
  Connection connection;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot connect to unknown signal %s", name));

  if (signalArgTypes[name] == typeid(void).hash_code()) {
    if (argType != typeid(void).hash_code())
      throw std::logic_error(format("Unexpected callback data argument "
                                    "for signal %s", name));
  } else {
    if (argType == typeid(void).hash_code())
      throw std::logic_error(format("Missing callback data argument "
                                    "for signal %s", name));
    if (argType != signalArgTypes[name])
      throw std::logic_error(format("Incompatible callback data "
                                    "argument for signal %s", name));
  }

  connection = {name, this, obj, callback};

  for (iter = signalReceivers[name].begin();
       iter != signalReceivers[name].end(); ++iter) {
    if (iter->connection == connection)
      throw std::logic_error(format("Signal %s is already connected", name));
  }

  signalReceivers[name].push_back({connection, emitter});

  if (obj)
    obj->connectedObjects.insert(this);

  return connection;
}

void Object::disconnectSignal(const Connection connection)
{
  ReceiverList::iterator iter;

  if (connection.src != this)
    throw std::logic_error("Disconnecting signal from wrong object");
  if (signalReceivers.count(connection.name) == 0)
    throw std::logic_error(format("Cannot disconnect unknown signal %s",
                                  connection.name.c_str()));

  for (iter = signalReceivers[connection.name].begin();
       iter != signalReceivers[connection.name].end(); ++iter) {
    if (iter->connection == connection) {
      signalReceivers[connection.name].erase(iter);
      break;
    }
  }

  if (connection.dst) {
    std::map<std::string, ReceiverList>::iterator sigiter;
    bool hasOthers;

    hasOthers = false;

    // Then check if the object is attached in more ways to this or
    // to some other signal
    for (sigiter = signalReceivers.begin();
         sigiter != signalReceivers.end(); ++sigiter) {
      for (iter = sigiter->second.begin();
           iter != sigiter->second.end(); ++iter) {
        if (iter->connection.dst == connection.dst) {
          hasOthers = true;
          break;
        }
      }
      if (hasOthers)
        break;
    }

    if (!hasOthers)
      connection.dst->connectedObjects.erase(this);
  }
}

void Object::disconnectSignals(Object* obj)
{
  std::map<std::string, ReceiverList>::iterator sigiter;

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

  obj->connectedObjects.erase(this);
}
