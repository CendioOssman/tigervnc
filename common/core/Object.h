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

//
// Object - Base class for all non-trival objects. Handles signal
//          infrastructure for passing events between objects.
//

#ifndef __CORE_OBJECT_H__
#define __CORE_OBJECT_H__

#include <functional>
#include <list>
#include <map>
#include <stdexcept>

namespace core {

  // Identifier for a signal
  class signal {};

  class Object {
  protected:
    // Must always be sub-classed
    Object();
  public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever the specified signal is emitted
    template<class S, class T>
    void connectSignal(const signal S::* signal, T* obj,
                       void (T::*callback)());

  protected:
    // emitSignal() calls all the registered object methods for the
    // specified signal
    template<class S>
    void emitSignal(const signal S::* signal);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void()> emitter_t;

    void emitSignalImpl(const void* signal);

    void connectSignalImpl(const void* signal,
                           const emitter_t& emitter);

  private:
    // Signal handling makes these objects difficult to copy, so it
    // is disabled for now
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

  private:
    typedef std::list<emitter_t> ReceiverList;

    // Mapping between signals and the methods receiving them
    std::map<const void*, ReceiverList> signalReceivers;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  template<class S, class T>
  void Object::connectSignal(const signal S::* signal, T* obj,
                             void (T::*callback)())
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [obj, callback]() {
      (obj->*callback)();
    };
    connectSignalImpl(&(sender->*signal), emitter);
  }

  template<class S>
  void Object::emitSignal(const signal S::* signal)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitSignalImpl(&(sender->*signal));
  }

}

#endif
