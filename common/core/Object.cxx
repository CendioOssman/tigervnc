/* Copyright 2022 Pierre Ossman for Cendio AB
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
    ReceiverList *siglist;

    siglist = &sigiter->second;
    while (!siglist->empty()) {
      if (siglist->front()->getObject() != nullptr)
        disconnectSignals(siglist->front()->getObject());
      else {
        delete *(siglist->begin());
        siglist->erase(siglist->begin());
      }
    }
  }

  while (!signalCheckers.empty()) {
    delete signalCheckers.begin()->second;
    signalCheckers.erase(signalCheckers.begin());
  }
}

void Object::registerSignal(const char *name,
                            const InfoChecker *checker)
{
  assert(name);

  if (signalReceivers.count(name) != 0)
    throw std::logic_error(format("Signal %s is already registered", name));

  // Just to force it being created
  signalReceivers[name].clear();

  signalCheckers[name] = checker;
}

void Object::emitSignal(const char *name)
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot emit unknown signal %s", name));

  if (signalCheckers[name] != nullptr)
    throw std::logic_error(format("Missing data when emitting signal %s", name));

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = signalReceivers[name];
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find(signalReceivers[name].begin(),
                  signalReceivers[name].end(),
                  *iter) == signalReceivers[name].end())
      continue;
    (*iter)->emit(this, name);
  }
}

void Object::emitSignal(const char *name, SignalInfo &info)
{
  ReceiverList siglist;
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot emit unknown signal %s", name));

  if (signalCheckers[name] == nullptr)
    throw std::logic_error(format("Unexpected data emitting signal %s", name));

  if (!signalCheckers[name]->isInstanceOf(info))
    throw std::logic_error(format("Incompatible signal data for signal %s", name));

  // Convoluted iteration so that we safely handle changes to
  // the list
  siglist = signalReceivers[name];
  for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
    if (std::find(signalReceivers[name].begin(),
                  signalReceivers[name].end(),
                  *iter) == signalReceivers[name].end())
      continue;
    (*iter)->emit(this, name, info);
  }
}

void Object::connectSignal(const char *name, void (*callback)())
{
  connectSignal(name, nullptr,
                new SignalReceiverFunctor(nullptr, callback), nullptr);
}

void Object::connectSignal(const char *name, Object *obj,
                           std::function<void()> callback)
{
  assert(obj);
  connectSignal(name, obj,
                new SignalReceiverFunctor(obj, callback), nullptr);
}

void Object::connectSignal(const char *name, Object *obj,
                           const SignalReceiver *receiver,
                           const std::type_info *info)
{
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot connect to unknown signal %s", name));

  if (signalCheckers[name] == nullptr) {
    if (info != nullptr)
      throw std::logic_error(format("Unexpected callback data argument "
                                    "for signal %s", name));
  } else {
    if (info == nullptr)
      throw std::logic_error(format("Missing callback data argument "
                                    "for signal %s", name));
    if (!signalCheckers[name]->isType(*info))
      throw std::logic_error(format("Incompatible callback data "
                                    "argument for signal %s", name));
  }

  for (iter = signalReceivers[name].begin();
       iter != signalReceivers[name].end(); ++iter) {
    if (**iter == *receiver)
      throw std::logic_error(format("Signal %s is already connected", name));
  }

  signalReceivers[name].push_back(receiver);

  if (obj)
    obj->connectedObjects.insert(this);
}

void Object::disconnectSignal(const char *name, Object *obj,
                              const SignalReceiver *receiver)
{
  std::map<std::string, ReceiverList>::iterator sigiter;
  ReceiverList::iterator iter;
  bool hasOthers;

  assert(name);
  assert(obj);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot disconnect unknown signal %s", name));

  // Find and remove the specific connection
  for (iter = signalReceivers[name].begin();
       iter != signalReceivers[name].end(); ++iter) {
    if (**iter == *receiver) {
      delete *iter;
      signalReceivers[name].erase(iter);
      break;
    }
  }

  hasOthers = false;

  // Then check if the object is attached in more ways to this or
  // to some other signal
  for (sigiter = signalReceivers.begin();
       sigiter != signalReceivers.end(); ++sigiter) {
    for (iter = sigiter->second.begin();
         iter != sigiter->second.end(); ++iter) {
      if ((*iter)->getObject() == obj) {
        hasOthers = true;
        break;
      }
    }
    if (hasOthers)
      break;
  }

  if (!hasOthers)
    obj->connectedObjects.erase(this);
}

void Object::disconnectSignal(const char *name, Object *obj)
{
  std::map<std::string, ReceiverList>::iterator sigiter;
  ReceiverList::iterator iter;

  bool hasOthers;

  assert(name);
  assert(obj);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot disconnect unknown signal %s", name));

  // Remove every reference to this object for the specific signal
  iter = signalReceivers[name].begin();
  while (iter != signalReceivers[name].end()) {
    if ((*iter)->getObject() == obj) {
      delete *iter;
      signalReceivers[name].erase(iter++);
    } else {
      ++iter;
    }
  }

  hasOthers = false;

  // Then check if the object is attached in more ways to this or
  // to some other signal
  for (sigiter = signalReceivers.begin();
       sigiter != signalReceivers.end(); ++sigiter) {
    for (iter = sigiter->second.begin();
         iter != sigiter->second.end(); ++iter) {
      if ((*iter)->getObject() == obj) {
        hasOthers = true;
        break;
      }
    }
    if (hasOthers)
      break;
  }

  if (!hasOthers)
    obj->connectedObjects.erase(this);
}

void Object::disconnectSignals(Object *obj)
{
  std::map<std::string, ReceiverList>::iterator sigiter;

  assert(obj);

  for (sigiter = signalReceivers.begin();
       sigiter != signalReceivers.end(); ++sigiter)
    disconnectSignal(sigiter->first.c_str(), obj);
}

Object::SignalReceiverFunctor::SignalReceiverFunctor(Object *obj_, std::function<void()> callback_)
  : obj(obj_), callback(callback_)
{
}

void Object::SignalReceiverFunctor::emit(Object * /*sender*/,
                                         const char * /*name*/) const
{
  callback();
}

void Object::SignalReceiverFunctor::emit(Object * /*sender*/,
                                         const char * /*name*/,
                                         SignalInfo & /*info*/) const
{
  assert(false);
}

Object* Object::SignalReceiverFunctor::getObject() const
{
  return obj;
}

bool Object::SignalReceiverFunctor::operator==(const Object::SignalReceiver & /*other*/) const
{
  /* std::function cannot be checked for equality */
  return false;
}
