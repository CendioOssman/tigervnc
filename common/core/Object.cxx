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

signal::signal()
{
}

void signal::connect(const emitter_t& emitter)
{
  receivers.push_back(emitter);
}

void signal::emit()
{
  ReceiverList::iterator iter;

  for (iter = receivers.begin(); iter != receivers.end(); ++iter)
    (*iter)();
}

Object::Object()
{
}

Object::~Object()
{
}

void Object::emitSignal(signal& signal)
{
  signal.emit();
}
