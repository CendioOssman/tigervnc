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

#include <assert.h>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include <core/any.h>
#include <core/comp_any.h>

namespace core {

  // Opaque identifier for tracking a connection to a signal
  struct Connection;

  class sigbase {
  public:
    virtual ~sigbase() {}

    friend class Object;

  protected:
    virtual void disconnect(const Connection connection) = 0;
  };

  template<typename... Is>
  class signal : public sigbase {
  public:
    static_assert((std::is_same_v<Is, std::decay_t<Is>> && ...), "Hello, Machiavelli!");

    signal() {}

    friend class Object;

  protected:
    // Wrapper to contain member function pointers
    typedef std::function<void(Is...)> emitter_t;

    Connection connect(class Object* src, class Object* dst,
                       const comp_any& callback,
                       const emitter_t& emitter);
    void disconnect(const Connection connection) override;

    void emit(const Is&... args);

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
    // Inclusion of signal information must match how the signal is
    // emitted, or an exception will be thrown. Any method registered
    // will automatically be unregistered when the method's object is
    // destroyed.
    template<class T, class S, typename... Is>
    Connection connectSignal(signal<Is...>& signal, T* obj,
                             void (T::*callback)(S*, Is...));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class T, class S, typename... Is>
    void disconnectSignal(signal<Is...>& signal, T* obj,
                          void (T::*callback)(S*, Is...));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object* obj);

  protected:
    // emitSignal() calls all the registered object methods for the
    // specified name. Inclusion of signal information must match the
    // connected methods or an exception will be thrown.
    template<typename... Is>
    void emitSignal(signal<Is...>& signal, Is... args);

  private:
    template<typename... Is>
    Connection connectSignal(signal<Is...>& signal, Object* obj,
                             const comp_any& callback,
                             const typename core::signal<Is...>::emitter_t& emitter);

  private:
    // Signal handling makes these objects difficult to copy, so it
    // is disabled for now
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

  private:
    std::list<Connection> connections;

    // Other objects that we have connected to signals on
    std::set<Object*> connectedObjects;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Internal structures
  //

  // Visible to everyone so it can be copied
  struct Connection {
    sigbase* sig;
    Object* src;
    Object* dst;
    comp_any callback;
  };

  bool operator==(const Connection& a, const Connection& b);

  template<typename... Is>
  struct signal<Is...>::SignalReceiver {
    Connection connection;
    signal<Is...>::emitter_t emitter;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  template<class T, class S, typename... Is>
  Connection Object::connectSignal(signal<Is...>& signal, T* obj,
                                   void (T::*callback)(S*, Is...))
  {
    static_assert(std::is_base_of<Object, S>::value,
                  "Incompatible signal callback");
    S* sender = static_cast<S*>(this);
    typename core::signal<Is...>::emitter_t emitter = [sender, obj, callback](const Is&... args) {
      (obj->*callback)(sender, args...);
    };
    return connectSignal(signal, obj, callback, emitter);
  }

  template<class T, class S, typename... Is>
  void Object::disconnectSignal(signal<Is...>& signal, T* obj,
                                void (T::*callback)(S*, Is...))
  {
    disconnectSignal({&signal, this, obj, callback});
  }

  template<typename... Is>
  void Object::emitSignal(signal<Is...>& signal, Is... args)
  {
    signal.emit(args...);
  }

  template<typename... Is>
  Connection Object::connectSignal(signal<Is...>& signal, class Object* obj,
                                  const comp_any& callback,
                                  const typename core::signal<Is...>::emitter_t& emitter)
  {
    Connection connection;

    connection = signal.connect(this, obj, callback, emitter);
    connections.push_back(connection);
    obj->connectedObjects.insert(this);

    return connection;
  }

  template<typename... Is>
  Connection core::signal<Is...>::connect(Object* src, Object* dst,
                                    const comp_any& callback,
                                    const emitter_t& emitter)
  {
    typename ReceiverList::iterator iter;
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

  template<typename... Is>
  void core::signal<Is...>::disconnect(const Connection connection)
  {
    typename ReceiverList::iterator iter;

    assert(connection.sig == this);

    for (iter = receivers.begin(); iter != receivers.end(); ++iter) {
      if (iter->connection == connection) {
        receivers.erase(iter);
        break;
      }
    }
  }

  template<typename... Is>
  void core::signal<Is...>::emit(const Is&... args)
  {
    ReceiverList siglist;
    typename ReceiverList::iterator iter;

    // Convoluted iteration so that we safely handle changes to
    // the list
    siglist = receivers;
    for (iter = siglist.begin(); iter != siglist.end(); ++iter) {
      if (std::find_if(receivers.begin(), receivers.end(),
                      [iter](const SignalReceiver& recv) {
                        return recv.connection == iter->connection;
                      }) == receivers.end())
        continue;
      iter->emitter(args...);
    }
  }

}

#endif
