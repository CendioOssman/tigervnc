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

#include <stdexcept>

#include <core/Object.h>
#include <core/string.h>

using namespace core;

Object::Object()
{
}

Object::~Object()
{
}

void Object::registerSignal(const char* name)
{
  assert(name);

  if (signalReceivers.count(name) != 0)
    throw std::logic_error(format("Signal %s is already registered", name));

  // Just to force it being created
  signalReceivers[name].clear();
}

void Object::emitSignal(const char* name)
{
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot emit unknown signal %s", name));

  for (iter = signalReceivers[name].begin();
       iter != signalReceivers[name].end(); ++iter)
    (*iter)();
}

void Object::connectSignal(const char* name, const emitter_t& emitter)
{
  ReceiverList::iterator iter;

  assert(name);

  if (signalReceivers.count(name) == 0)
    throw std::logic_error(format("Cannot connect to unknown signal %s", name));

  signalReceivers[name].push_back(emitter);
}
