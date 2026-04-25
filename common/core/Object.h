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
#include <vector>

#include <core/any.h>

namespace core {

  // Identifier for a signal
  template<typename... Args>
  class signal {};

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
    // emitted. Any method registered will automatically be unregistered
    // when the method's object is destroyed.
    template<class S, class T, typename... SigArgs, typename... Args>
    Connection connectSignal(const signal<SigArgs...> S::* signal,
                             T* obj, void (T::*callback)(Args...));

    // Lambda friendly versions to register a signal callback. If the
    // lambda has a capture list, then an object must also be specified
    // to control the lifetime.
    template<class S, typename... Args, typename Functor>
    Connection connectSignal(const signal<Args...> S::* signal,
                             Functor f);
    template<class S, typename... Args, typename Functor>
    Connection connectSignal(const signal<Args...> S::* signal,
                             Object* obj, Functor callback);

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal().
    void disconnectSignal(const Connection connection);

    // Methods can be disconneced by reference, rather than tracking
    // the connection object.
    template<class S, class T, typename... SigArgs, typename... Args>
    void disconnectSignal(const signal<SigArgs...> S::* signal, T* obj,
                          void (T::*callback)(Args...));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object* obj);

  protected:
    // emitSignal() calls all the registered object methods for the
    // specified signal. Inclusion of signal information must match the
    // types from the signal.
    template<class S, typename... SigArgs, typename... Args>
    void emitSignal(const signal<SigArgs...> S::* signal, Args... args);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void(const std::vector<any>&)> emitter_t;

    void emitSignalImpl(const void* signal,
                        const std::vector<any>& info);

    Connection connectSignalImpl(const void* signal, Object* obj,
                                 const emitter_t& emitter);
    Connection connectSignalImpl(const void* signal, Object* obj,
                                 const any& callback,
                                 bool (*comparer)(const any&,
                                                  const any&),
                                 const emitter_t& emitter);

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

    // Mapping between signal objects and the methods receiving them
    std::map<const void*, ReceiverList> signalReceivers;

    // Other objects that we have connected to signals on
    std::set<Object*> connectedObjects;
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Internal structures
  //

  // Visible to everyone so it can be copied
  struct Connection {
    const void* signal;
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
  template<typename... Args, typename Functor>
  void invoke_any(Functor f, const std::vector<any>& info);

  template<class S, class T, typename... SigArgs, typename... Args>
  Connection Object::connectSignal(const signal<SigArgs...> S::* signal,
                                   T* obj, void (T::*callback)(Args...))
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_base_of_v<Object, T>,
                  "Target object is not subclass of core::Object");
    static_assert(sizeof...(SigArgs) == sizeof...(Args),
                  "Wrong number of arguments for signal callback");
    static_assert((std::is_convertible_v<SigArgs, Args> && ...),
                  "Incompatible callback data arguments for signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    auto memfn = [obj, callback](SigArgs... args) {
      (obj->*callback)(args...);
    };
    emitter_t emitter = [memfn](const std::vector<any>& info) {
      invoke_any<SigArgs...>(memfn, info);
    };
    assert(obj);
    return connectSignalImpl(&(sender->*signal), obj, callback,
                             compareAny<typeof(callback)>, emitter);
  }

  // Determine if a lambda has a capture list by using the fact that
  // the unary plus operator only exists without captures
  template<typename Functor>
  constexpr auto _test_captures(Functor* f)
    -> decltype(+(*f), void(), false)
  {
    return false;
  }
  constexpr bool _test_captures(void*) {
      return true;
  }
  template<typename Functor>
  struct has_captures
      : std::bool_constant<_test_captures((Functor*)nullptr)> {};
  template<typename Functor>
  constexpr bool has_captures_v = has_captures<Functor>::value;

  template<class S, typename... Args, typename Functor>
  Connection Object::connectSignal(const signal<Args...> S::* signal,
                                   Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor, Args...>,
                  "Incompatible signal callback");
    static_assert(!has_captures_v<Functor>,
                  "Lambdas with captures not allowed as callbacks "
                  "unless connected to the lifetime of an object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    auto wrapper = [callback](Args... args) {
      callback(args...);
    };
    emitter_t emitter = [wrapper](const std::vector<any>& info) {
      invoke_any<Args...>(wrapper, info);
    };
    // It's not guaranteed if we get unique or identical addresses for
    // otherwise identical lambdas. Treat each as unique for consistent
    // behaviour by omitting any tracking information.
    return connectSignalImpl(&(sender->*signal), nullptr, emitter);
  }

  template<class S, typename... Args, typename Functor>
  Connection Object::connectSignal(const signal<Args...> S::* signal,
                                   Object* obj, Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor, Args...>,
                  "Incompatible signal callback");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    auto wrapper = [callback](Args... args) {
      callback(args...);
    };
    emitter_t emitter = [wrapper](const std::vector<any>& info) {
      invoke_any<Args...>(wrapper, info);
    };
    assert(obj);
    // Lambdas cannot be compared, so we cannot tell if it's an identical
    // lambda, or just the same body but with different captures.
    return connectSignalImpl(&(sender->*signal), obj, emitter);
  }

  template<class S, class T, typename... SigArgs, typename... Args>
  void Object::disconnectSignal(const signal<SigArgs...> S::* signal,
                                T* obj, void (T::*callback)(Args...))
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_base_of_v<Object, T>,
                  "Target object is not subclass of core::Object");
    static_assert(sizeof...(SigArgs) == sizeof...(Args),
                  "Wrong number of arguments for signal callback");
    static_assert((std::is_convertible_v<SigArgs, Args> && ...),
                  "Incompatible callback data arguments for signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    disconnectSignal({(void*)&(sender->*signal), this, obj, callback,
                      compareAny<typeof(callback)>});
  }

  template<class S, typename... SigArgs, typename... Args>
  void Object::emitSignal(const signal<SigArgs...> S::* signal,
                          Args... args)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(sizeof...(SigArgs) == sizeof...(Args),
                  "Wrong number of arguments emitting signal");
    static_assert((std::is_convertible_v<Args, SigArgs> && ...),
                  "Incompatible signal data emitting signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitSignalImpl(&(sender->*signal), {any((SigArgs)args)...});
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

  template<typename... Args, std::size_t... idx, typename Functor>
  void _invoke_any_impl(Functor f, const std::vector<any>& info,
                        std::index_sequence<idx...>)
  {
    static_assert(std::is_invocable_v<Functor, Args...>);
    f(any_cast<typename std::decay<Args>::type>(info[idx])...);
  }
  template<typename... Args, typename Functor>
  void invoke_any(Functor f, const std::vector<any>& info)
  {
    assert(info.size() == sizeof...(Args));
    _invoke_any_impl<Args...>(f, info,
                              std::index_sequence_for<Args...>{});
  }

}

#endif
