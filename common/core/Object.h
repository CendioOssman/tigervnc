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

//
// Object - Base class for all non-trival objects. Handles signal
//          infrastructure for passing events between objects.
//

#ifndef __CORE_OBJECT_H__
#define __CORE_OBJECT_H__

#include <functional>
#include <list>
#include <map>
#include <string>

#include <core/comp_any.h>

namespace core {

  // Opaque identifier for tracking a connection to a signal
  struct Connection;

  class Object {
  protected:
    // Must always be sub-classed
    Object();
  public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever a signal of the specified name is emitted.
    template<class T>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(Object*, const char*));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class T>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(Object*, const char*));

  protected:
    // registerSignal() registers a new signal type with the specified
    // name. This must always be done before connectSignal() or
    // emitSignal() is used.
    void registerSignal(const char* name);

    // emitSignal() calls all the registered object methods for the
    // specified name.
    void emitSignal(const char* name);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void()> emitter_t;

    Connection connectSignal(const char* name, Object* obj,
                             const comp_any& callback,
                             const emitter_t& emitter);

  private:
    // Signal handling makes these objects difficult to copy, so it
    // is disabled for now
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

  private:
    struct SignalReceiver;
    typedef std::list<SignalReceiver> ReceiverList;

    // Mapping between signal names and the methods receiving them
    std::map<std::string, ReceiverList> signalReceivers;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Internal structures
  //

  // Visible to everyone so it can be copied
  struct Connection {
    std::string name;
    Object* src;
    Object* dst;
    comp_any callback;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  template<class T>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(Object*,
                                                       const char*))
  {
    emitter_t emitter = [this, name, obj, callback]() {
      (obj->*callback)(this, name);
    };
    return connectSignal(name, obj, callback, emitter);
  }

  template<class T>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(Object*,
                                                    const char*))
  {
    disconnectSignal({name, this, obj, callback});
  }

}

#endif
