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

  class signal {
  public:
    signal();

    friend class Object;

  protected:
    // Wrapper to contain member function pointers
    typedef std::function<void()> emitter_t;

    Connection connect(class Object* src, class Object* dst,
                       const comp_any& callback,
                       const emitter_t& emitter);
    void disconnect(const Connection connection);

    void emit();

  protected:
    struct SignalReceiver;
    typedef std::list<SignalReceiver> ReceiverList;

    ReceiverList receivers;
  };

  class Object {
  protected:
    // Must always be sub-classed
    Object();
  public:
    virtual ~Object();

    // connectSignal() registers an object and method on that object to
    // be called whenever a signal of the specified name is emitted.
    template<class T>
    Connection connectSignal(signal& signal, T* obj,
                             void (T::*callback)(Object*));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class T>
    void disconnectSignal(signal& signal, T* obj,
                          void (T::*callback)(Object*));

  protected:
    // emitSignal() calls all the registered object methods for the
    // specified name.
    void emitSignal(signal& signal);

  private:
    // Signal handling makes these objects difficult to copy, so it
    // is disabled for now
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Internal structures
  //

  // Visible to everyone so it can be copied
  struct Connection {
    signal* sig;
    Object* src;
    Object* dst;
    comp_any callback;
  };

  struct signal::SignalReceiver {
    Connection connection;
    signal::emitter_t emitter;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  template<class T>
  Connection Object::connectSignal(signal& signal, T* obj,
                                   void (T::*callback)(Object*))
  {
    signal::emitter_t emitter = [this, obj, callback]() {
      (obj->*callback)(this);
    };
    return signal.connect(this, obj, callback, emitter);
  }

  template<class T>
  void Object::disconnectSignal(signal& signal, T* obj,
                                void (T::*callback)(Object*))
  {
    disconnectSignal({&signal, this, obj, callback});
  }

}

#endif
