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

#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <typeinfo>

#include <core/any.h>
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
    // Inclusion of signal information must match how the signal is
    // emitted, or an exception will be thrown. Any method registered
    // will automatically be unregistered when the method's object is
    // destroyed.
    template<class T>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)());
    template<class T, class S>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(S*, const char*));
    template<class T, typename I>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(I));
    template<class T, class S, typename I>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(S*, const char*, I));

    // Lambda friendly version to register a signal callback. An object
    // must also be specified to control the lifetime of captured
    // variables.
    Connection connectSignal(const char* name, Object* obj,
                             const std::function<void()>& callback);

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class T>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)());
    template<class T, class S>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(S*, const char*));
    template<class T, typename I>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(I));
    template<class T, class S, typename I>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(S*, const char*, I));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object* obj);

  protected:
    // registerSignal() registers a new signal type with the specified
    // name. This must always be done before connectSignal() or
    // emitSignal() is used. If the signal will include signal
    // information, then the typed version must be called with the
    // intended type that will be used with emitSignal().
    void registerSignal(const char* name);
    template<typename I>
    void registerSignal(const char* name);

    // emitSignal() calls all the registered object methods for the
    // specified name. Inclusion of signal information must match the
    // type from registerSignal() or an exception will be thrown.
    void emitSignal(const char* name);
    template<typename I>
    void emitSignal(const char* name, const I& info);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void(const any&)> emitter_t;

    void registerSignal(const char* name, size_t argType);

    void emitSignal(const char* name, const any& info);

    Connection connectSignal(const char* name, Object* obj,
                             const emitter_t& emitter,
                             size_t argType);
    Connection connectSignal(const char* name, Object* obj,
                             const comp_any& callback,
                             const emitter_t& emitter,
                             size_t argType);

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
    // Signal argument type information (void if no argument)
    std::map<std::string, size_t> signalArgTypes;

    // Other objects that we have connected to signals on
    std::set<Object*> connectedObjects;
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
                                   void (T::*callback)())
  {
    emitter_t emitter = [obj, callback](const any& info) {
      assert(!info.has_value());
      (obj->*callback)();
    };
    assert(obj);
    return connectSignal(name, obj, callback, emitter,
                         typeid(void).hash_code());
  }

  template<class T, class S>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(S*, const char*))
  {
    static_assert(std::is_base_of<Object, S>::value,
                  "Incompatible signal callback");
    S* sender = static_cast<S*>(this);
    emitter_t emitter = [sender, name, obj, callback](const any& info) {
      assert(!info.has_value());
      (obj->*callback)(sender, name);
    };
    return connectSignal(name, obj, callback, emitter,
                         typeid(void).hash_code());
  }

  template<class T, typename I>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(I))
  {
    emitter_t emitter = [obj, callback](const any& info) {
      assert(info.has_value());
      using I_d = typename std::decay<I>::type;
      (obj->*callback)(any_cast<I_d>(info));
    };
    assert(obj);
    return connectSignal(name, obj, callback, emitter,
                         typeid(I).hash_code());
  }

  template<class T, class S, typename I>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(S*, const char*, I))
  {
    static_assert(std::is_base_of<Object, S>::value,
                  "Incompatible signal callback");
    S* sender = static_cast<S*>(this);
    emitter_t emitter = [sender, name, obj, callback](const any& info) {
      assert(info.has_value());
      using I_d = typename std::decay<I>::type;
      (obj->*callback)(sender, name, any_cast<I_d>(info));
    };
    return connectSignal(name, obj, callback, emitter,
                         typeid(I).hash_code());
  }

  template<class T>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)())
  {
    disconnectSignal({name, this, obj, callback});
  }

  template<class T, class S>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(S*, const char*))
  {
    disconnectSignal({name, this, obj, callback});
  }

  template<class T, typename I>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(I))
  {
    disconnectSignal({name, this, obj, callback});
  }

  template<class T, class S, typename I>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(S*, const char*, I))
  {
    disconnectSignal({name, this, obj, callback});
  }

  inline void Object::registerSignal(const char* name)
  {
    registerSignal(name, typeid(void).hash_code());
  }

  template<typename I>
  void Object::registerSignal(const char* name)
  {
    registerSignal(name, typeid(I).hash_code());
  }

  template<typename I>
  void Object::emitSignal(const char* name, const I& info)
  {
    emitSignal(name, any(info));
  }

}

#endif
