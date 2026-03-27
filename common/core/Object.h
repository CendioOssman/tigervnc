/* Copyright 2022-2025 Pierre Ossman for Cendio AB
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
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#include <core/any.h>

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
    // Inclusion of signal arguments must match how the signal is
    // emitted, or an exception will be thrown. Any method registered
    // will automatically be unregistered when the method's object is
    // destroyed.
    template<class T, typename... Args>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(Args...));
    template<class T, class S, typename... Args>
    Connection connectSignal(const char* name, T* obj,
                             void (T::*callback)(S*, const char*,
                                                 Args...));

    // Lambda friendly versions to register a signal callback. If the
    // lambda has a capture list, then an object must also be specified
    // to control the lifetime.
    template<typename... Args, typename Functor>
    Connection connectSignal(const char* name, Functor f);
    template<typename... Args, typename Functor>
    Connection connectSignal(const char* name, Object* obj,
                             Functor callback);

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class T, typename... Args>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(Args...));
    template<class T, class S, typename... Args>
    void disconnectSignal(const char* name, T* obj,
                          void (T::*callback)(S*, const char*,
                                              Args...));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object* obj);

  protected:
    // registerSignal() registers a new signal type with the specified
    // name. This must always be done before connectSignal() or
    // emitSignal() is used. If the signal will include signal
    // information, then the typed version must be called with the
    // intended types that will be used with emitSignal().
    void registerSignal(const char* name);
    template<typename... Args>
    void registerSignal(const char* name);

    // emitSignal() calls all the registered object methods for the
    // specified name. Inclusion of signal information must match the
    // types from registerSignal() or an exception will be thrown.
    template<typename... Args>
    void emitSignal(const char* name, Args... args);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void(const std::vector<any>&)> emitter_t;

    void registerSignal(const char* name,
                        const std::vector<size_t>& argTypes);

    void emitSignal(const char* name, const std::vector<any>& info);

    Connection connectSignal(const char* name, Object* obj,
                             const emitter_t& emitter,
                             const std::vector<size_t>& argTypes);
    Connection connectSignal(const char* name, Object* obj,
                             const any& callback,
                             bool (*comparer)(const any&, const any&),
                             const emitter_t& emitter,
                             const std::vector<size_t>& argTypes);

    // Compares two any objects, returning true if they are both type T
    // and have the same value
    template<class T>
    static bool compareAny(const any& a, const any& b);

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
    std::map<std::string, std::vector<size_t>> signalArgTypes;

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
    any callback;
    bool (*comparer)(const any&, const any&);
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  // Call a function with the specified vector of arguments
  template<typename... Args>
  void invoke_any(const std::function<void(Args...)>& f,
                  const std::vector<any>& info);

  template<class T, typename... Args>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(Args...))
  {
    std::function<void(Args...)> memfn = [obj, callback](Args... args) {
      (obj->*callback)(args...);
    };
    emitter_t emitter = [memfn](const std::vector<any>& info) {
      invoke_any(memfn, info);
    };
    assert(obj);
    return connectSignal(name, obj, callback,
                         compareAny<typeof(callback)>, emitter,
                         {typeid(Args).hash_code()...});
  }

  template<class T, class S, typename... Args>
  Connection Object::connectSignal(const char* name, T* obj,
                                   void (T::*callback)(S*, const char*,
                                                       Args...))
  {
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Incompatible signal callback");
    std::function<void(Args...)> memfn = [sender, name, obj,
                                          callback](Args... args) {
      (obj->*callback)(sender, name, args...);
    };
    emitter_t emitter = [memfn](const std::vector<any>& info) {
      invoke_any(memfn, info);
    };
    assert(obj);
    return connectSignal(name, obj, callback,
                         compareAny<typeof(callback)>, emitter,
                         {typeid(Args).hash_code()...});
  }

  template<typename... Args, typename Functor>
  Connection Object::connectSignal(const char* name,
                                   Functor callback)
  {
    static_assert(std::is_convertible<Functor, void(*)(Args...)>::value,
                  "Incompatible signal callback");
    emitter_t emitter = [callback](const std::vector<any>& info) {
      invoke_any((std::function<void(Args...)>)(void(*)(Args...))callback, info);
    };
    // It's not guaranteed if we get unique or identical addresses for
    // otherwise identical lambdas. Treat each as unique for consistent
    // behaviour by omitting any tracking information.
    return connectSignal(name, nullptr, emitter,
                         {typeid(Args).hash_code()...});
  }

  template<typename... Args, typename Functor>
  Connection Object::connectSignal(const char* name, Object* obj,
                                   Functor callback)
  {
    static_assert(std::is_convertible<Functor, std::function<void(Args...)>>::value,
                  "Incompatible signal callback");
    emitter_t emitter = [callback](const std::vector<any>& info) {
      invoke_any((std::function<void(Args...)>)callback, info);
    };
    assert(obj);
    // Lambdas cannot be compared, so we cannot tell if it's an identical
    // lambda, or just the same body but with different captures.
    return connectSignal(name, obj, emitter,
                         {typeid(Args).hash_code()...});
  }

  template<class T, typename... Args>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(Args...))
  {
    disconnectSignal({name, this, obj, callback,
                      compareAny<typeof(callback)>});
  }

  template<class T, class S, typename... Args>
  void Object::disconnectSignal(const char* name, T* obj,
                                void (T::*callback)(S*, const char*,
                                                    Args...))
  {
    disconnectSignal({name, this, obj, callback,
                      compareAny<typeof(callback)>});
  }

  inline void Object::registerSignal(const char* name)
  {
    registerSignal(name, {});
  }

  template<typename... Args>
  void Object::registerSignal(const char* name)
  {
    registerSignal(name, {typeid(Args).hash_code()...});
  }

  template<typename... Args>
  void Object::emitSignal(const char* name, Args... args)
  {
    emitSignal(name, {any(args)...});
  }

  template<class T>
  bool Object::compareAny(const any& a, const any& b)
  {
    try {
      const T& va = any_cast<T>(a);
      const T& vb = any_cast<T>(b);
      return std::equal_to<T>()(va, vb);
    } catch (const std::bad_cast&) {
      return false;
    }
  }

  template<typename... Args, std::size_t... idx>
  void _invoke_any_impl(const std::function<void(Args...)>& f,
                        const std::vector<any>& info,
                         std::index_sequence<idx...>)
  {
    f(any_cast<typename std::decay<Args>::type>(info[idx])...);
  }
  template<typename... Args>
  void invoke_any(const std::function<void(Args...)>& f,
                  const std::vector<any>& info)
  {
    assert(info.size() == sizeof...(Args));
    _invoke_any_impl(f, info, std::index_sequence_for<Args...>{});
  }

}

#endif
