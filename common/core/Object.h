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

#include <assert.h>

#include <any>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace core {

  // Identifier for a signal
  template<typename I=void>
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
    // be called whenever the specified signal is emitted. Inclusion of
    // signal information must convertible to how the signal is
    // declared. Any method registered will automatically be
    // unregistered when the method's object is destroyed.
    template<class S, class T>
    Connection connectSignal(const signal<> S::* signal, T* obj,
                             void (T::*callback)());
    template<class S, class T, typename SI, typename I>
    Connection connectSignal(const signal<SI> S::* signal, T* obj,
                             void (T::*callback)(I));

    // Lambda friendly versions to register a signal callback. If the
    // lambda has a capture list, then an object must also be specified
    // to control the lifetime.
    template<class S, typename Functor>
    Connection connectSignal(const signal<> S::* signal,
                             Functor callback);
    template<class S, typename SI, typename Functor>
    Connection connectSignal(const signal<SI> S::* signal,
                             Functor callback);

    template<class S, typename Functor>
    Connection connectSignal(const signal<> S::* signal, Object* obj,
                             Functor callback);
    template<class S, typename SI, typename Functor>
    Connection connectSignal(const signal<SI> S::* signal, Object* obj,
                             Functor callback);

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal()
    void disconnectSignal(const Connection connection);

    // Methods can be disconnected by reference, rather than tracking
    // the connection object
    template<class S, class T>
    void disconnectSignal(const signal<> S::* signal, T* obj,
                          void (T::*callback)());
    template<class S, class T, typename SI, typename I>
    void disconnectSignal(const signal<SI> S::* signal, T* obj,
                          void (T::*callback)(I));

    // disconnectSignals() unregisters all methods for all signals for
    // the specified object. This is automatically called when the
    // specified object is destroyed.
    void disconnectSignals(Object* obj);

  protected:
    // emitSignal() calls all the registered callbacks for the specified
    // signal. Inclusion of signal information must be convertible to
    // how the signal is declared.
    template<class S>
    void emitSignal(const signal<> S::* signal);
    template<class S, typename SI, typename I>
    void emitSignal(const signal<SI> S::* signal, const I& info);

  private:
    // Wrapper to contain member function pointers
    typedef std::function<void(const std::vector<std::any>&)> emitter_t;

    void emitSignalImpl(const void* signal,
                        const std::vector<std::any>& info);

    Connection connectSignalImpl(const void* signal, Object* obj,
                                 const emitter_t& emitter);
    Connection connectSignalImpl(const void* signal, Object* obj,
                                 const std::any& callback,
                                 bool (*comparer)(const std::any&,
                                                  const std::any&),
                                 const emitter_t& emitter);

    // Compares two any objects, returning true if they are both type T
    // and have the same value
    template<class T>
    static bool compareAny(const std::any& a, const std::any& b);

  private:
    // Signal handling makes these objects difficult to copy, so it
    // is disabled for now
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

  private:
    struct SignalReceiver;
    typedef std::list<SignalReceiver> ReceiverList;

    // Mapping between signals and the methods receiving them
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
    std::any callback;
    bool (*comparer)(const std::any&, const std::any&);
  };

  //////////////////////////////////////////////////////////////////////
  //
  // Inline methods definitions
  //

  template<class S, class T>
  Connection Object::connectSignal(const signal<> S::* signal, T* obj,
                                   void (T::*callback)())
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [obj, callback](const std::vector<std::any>& info) {
      assert(info.empty());
      (obj->*callback)();
    };
    assert(obj);
    return connectSignalImpl(&(sender->*signal), obj, callback,
                             compareAny<typeof(callback)>, emitter);
  }

  template<class S, class T, typename SI, typename I>
  Connection Object::connectSignal(const signal<SI> S::* signal, T* obj,
                                   void (T::*callback)(I))
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_convertible_v<SI, I>,
                  "Incompatible callback data argument for signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [obj, callback](const std::vector<std::any>& info) {
      assert(!info.empty());
      using SI_d = std::decay_t<SI>;
      (obj->*callback)(std::any_cast<SI_d>(info.front()));
    };
    assert(obj);
    return connectSignalImpl(&(sender->*signal), obj, callback,
                             compareAny<typeof(callback)>, emitter);
  }

  // Determine if a lambda has a capture list by using the fact that
  // the unary plus operator only exists without captures
  template<typename Functor>
  constexpr auto _test_captures(Functor* f)
    -> decltype(+(*f), void(), false) { return false; }
  constexpr bool _test_captures(void*) { return true; }
  template<typename Functor>
  struct has_captures
      : std::bool_constant<_test_captures((Functor*)nullptr)> {};
  template<typename Functor>
  constexpr bool has_captures_v = has_captures<Functor>::value;

  template<class S, typename Functor>
  Connection Object::connectSignal(const signal<> S::* signal,
                                  Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor>,
                  "Incompatible signal callback");
    static_assert(!has_captures_v<Functor>,
                  "Lambdas with captures not allowed as callbacks "
                  "unless connected to the lifetime of an object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [callback](const std::vector<std::any>& info) {
      assert(info.empty());
      callback();
    };
    // It's not guaranteed if we get unique or identical addresses for
    // otherwise identical lambdas. Treat each as unique for consistent
    // behaviour by omitting any tracking information.
    return connectSignalImpl(&(sender->*signal), nullptr, emitter);
  }

  template<class S, typename SI, typename Functor>
  Connection Object::connectSignal(const signal<SI> S::* signal,
                                   Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor, SI>,
                  "Incompatible signal callback");
    static_assert(!has_captures_v<Functor>,
                  "Lambdas with captures not allowed as callbacks "
                  "unless connected to the lifetime of an object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [callback](const std::vector<std::any>& info) {
      assert(!info.empty());
      using SI_d = std::decay_t<SI>;
      callback(std::any_cast<SI_d>(info.front()));
    };
    // It's not guaranteed if we get unique or identical addresses for
    // otherwise identical lambdas. Treat each as unique for consistent
    // behaviour by omitting any tracking information.
    return connectSignalImpl(&(sender->*signal), nullptr, emitter);
  }

  template<class S, typename Functor>
  Connection Object::connectSignal(const signal<> S::* signal,
                                   Object* obj, Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor>,
                  "Incompatible signal callback");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [callback](const std::vector<std::any>& info) {
      assert(info.empty());
      callback();
    };
    assert(obj);
    // Lambdas cannot be compared, so we cannot tell if it's an
    // identical lambda, or just the same body but with different
    // captures.
    return connectSignalImpl(&(sender->*signal), obj, emitter);
  }

  template<class S, typename SI, typename Functor>
  Connection Object::connectSignal(const signal<SI> S::* signal,
                                   Object* obj, Functor callback)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_invocable_v<Functor, SI>,
                  "Incompatible signal callback");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitter_t emitter = [callback](const std::vector<std::any>& info) {
      assert(!info.empty());
      using SI_d = std::decay_t<SI>;
      callback(std::any_cast<SI_d>(info.front()));
    };
    assert(obj);
    // Lambdas cannot be compared, so we cannot tell if it's an
    // identical lambda, or just the same body but with different
    // captures.
    return connectSignalImpl(&(sender->*signal), obj, emitter);
  }

  template<class S, class T>
  void Object::disconnectSignal(const signal<> S::* signal, T* obj,
                                void (T::*callback)())
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    disconnectSignal({&(sender->*signal), this, obj, callback,
                      compareAny<typeof(callback)>});
  }

  template<class S, class T, typename SI, typename I>
  void Object::disconnectSignal(const signal<SI> S::* signal, T* obj,
                                void (T::*callback)(I))
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_convertible_v<SI, I>,
                  "Incompatible callback data argument for signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    disconnectSignal({&(sender->*signal), this, obj, callback,
                      compareAny<typeof(callback)>});
  }

  template<class S>
  void Object::emitSignal(const signal<> S::* signal)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitSignalImpl(&(sender->*signal), {});
  }

  template<class S, typename SI, typename I>
  void Object::emitSignal(const signal<SI> S::* signal, const I& info)
  {
    static_assert(std::is_base_of_v<Object, S>,
                  "Signal owner is not subclass of core::Object");
    static_assert(std::is_convertible_v<I, SI>,
                  "Incompatible signal data emitting signal");
    S* sender = dynamic_cast<S*>(this);
    if (!sender)
      throw std::logic_error("Signal is not owned by sending object");
    emitSignalImpl(&(sender->*signal), {std::any((SI)info)});
  }

  template<class T>
  bool Object::compareAny(const std::any& a, const std::any& b)
  {
    try {
      const T& va = std::any_cast<T>(a);
      const T& vb = std::any_cast<T>(b);
      return std::equal_to<T>()(va, vb);
    } catch (const std::bad_cast&) {
      return false;
    }
  }

}

#endif
