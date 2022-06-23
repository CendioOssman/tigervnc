/* Copyright 2022 Pierre Ossman for Cendio AB
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

#include <string>
#include <list>
#include <set>
#include <map>

#include <core/Exception.h>

namespace core {

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
    template<class T, class S>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(S*, const char*));
    template<class T, class S, typename I>
    void connectSignal(const char *name, T *obj,
                       void (T::*callback)(S*, const char*, I));

    // disconnectSignal() unregisters a method that was previously
    // registered using connectSignal(). Only the specified object and
    // the specific name will be unregistered.
    template<class T, class S>
    void disconnectSignal(const char *name, T *obj,
                          void (T::*callback)(S*, const char*));
    template<class T, class S, typename I>
    void disconnectSignal(const char *name, T *obj,
                          void (T::*callback)(S*, const char*, I));

    // disconnectSignals() unregisters all methods for all names for the
    // specified object. This is automatically called when the specified
    // object is destroyed.
    void disconnectSignals(Object *obj);

  protected:
    // registerSignal() registers a new signal type with the specified
    // name. This must always be done before connectSignal() or
    // emitSignal() is used.
    void registerSignal(const char *name);

    // emitSignal() calls all the registered object methods for the
    // specified name. Inclusion of signal information must match the
    // registered connected methods or an exception will be thrown.
    void emitSignal(const char *name);
    template<typename I>
    void emitSignal(const char *name, I info);

  private:
    // Helper classes to handle the type glue for calling object methods
    class SignalReceiver;
    template<class T, class S> class SignalReceiverTS;
    template<class T, class S, class I> class SignalReceiverTSI;

    struct SignalInfo;
    template<typename I> struct TypedInfo;

    void emitSignal(const char *name, SignalInfo &info);

    void connectSignal(const char *name, Object *obj,
                       const SignalReceiver *receiver);
    void disconnectSignal(const char *name, Object *obj,
                          const SignalReceiver *receiver);

  private:
    typedef std::list<const SignalReceiver*> ReceiverList;

    // Mapping between signal names and the methods receiving them
    std::map<std::string, ReceiverList> signalReceivers;

    // Other objects that we have connected to signals on
    std::set<Object*> connectedObjects;
  };

  struct Object::SignalInfo {
    virtual ~SignalInfo() {}
  };

  template<typename T>
  struct Object::TypedInfo : public SignalInfo {
    TypedInfo(T &d) : data(d) {}
    T& getData() { return data; }
  private:
    T &data;
  };

  //
  // Object::SignalReceiver - Glue objects that allow us to call any
  //                          method on any object, as long as the
  //                          object is derived from Object.
  //

  class Object::SignalReceiver {
  public:
    SignalReceiver() {}
    virtual ~SignalReceiver() {}

    virtual void emit(Object*, const char*) const = 0;
    virtual void emit(Object*, const char*, SignalInfo&) const = 0;

    virtual Object* getObject() const = 0;
    virtual bool operator==(const SignalReceiver&) const = 0;
  };

  template<class T, class S>
  class Object::SignalReceiverTS: public Object::SignalReceiver {
  public:
    SignalReceiverTS(T *obj, void (T::*callback)(S*, const char*));
    virtual ~SignalReceiverTS() {}

    void emit(Object*, const char*) const override;
    void emit(Object*, const char*, SignalInfo&) const override;

    Object* getObject() const override;
    bool operator==(const SignalReceiver&) const override;

  private:
    T *obj;
    void (T::*callback)(S*, const char*);
  };

  template<class T, class S, typename I>
  class Object::SignalReceiverTSI: public Object::SignalReceiver {
  public:
    SignalReceiverTSI(T *obj, void (T::*callback)(S*, const char*, I));
    virtual ~SignalReceiverTSI() {}

    void emit(Object*, const char*) const override;
    void emit(Object*, const char*, SignalInfo&) const override;

    Object* getObject() const override;
    bool operator==(const SignalReceiver&) const override;

  private:
    T *obj;
    void (T::*callback)(S*, const char*, I);
  };

  // Object - Inline methods definitions

  template<class T, class S>
  void Object::connectSignal(const char *name, T *obj,
                             void (T::*callback)(S*, const char*))
  {
    if (dynamic_cast<S*>(this) == nullptr)
      throw Exception("Incorrect callback object type for signal %s",
                      name);
    connectSignal(name, obj, new SignalReceiverTS<T,S>(obj, callback));
  }

  template<class T, class S, typename I>
  void Object::connectSignal(const char *name, T *obj,
                             void (T::*callback)(S*, const char*, I))
  {
    if (dynamic_cast<S*>(this) == nullptr)
      throw Exception("Incorrect callback object type for signal %s",
                      name);
    connectSignal(name, obj,
                  new SignalReceiverTSI<T,S,I>(obj, callback));
  }

  template<class T, class S>
  void Object::disconnectSignal(const char *name, T *obj,
                                void (T::*callback)(S*, const char*))
  {
    SignalReceiverTS<T,S> other(obj, callback);
    disconnectSignal(name, obj, &other);
  }

  template<class T, class S, typename I>
  void Object::disconnectSignal(const char *name, T *obj,
                                void (T::*callback)(S*, const char*, I))
  {
    SignalReceiverTSI<T,S,I> other(obj, callback);
    disconnectSignal(name, obj, &other);
  }

  template<typename I>
  void Object::emitSignal(const char *name, I info)
  {
    TypedInfo<I> tinfo(info);
    emitSignal(name, (SignalInfo&)tinfo);
  }

  // Object::SignalReceiver - Inline methods definitions

  template<class T, class S>
  Object::SignalReceiverTS<T,S>::SignalReceiverTS(T *obj_, void (T::*callback_)(S*, const char*))
    : obj(obj_), callback(callback_)
  {
    assert(obj_);
    assert(callback_);
  }

  template<class T, class S>
  void Object::SignalReceiverTS<T,S>::emit(Object *_sender,
                                           const char *name) const
  {
    S *sender;

    sender = dynamic_cast<S*>(_sender);
    assert(sender != nullptr);

    (obj->*callback)(sender, name);
  }

  template<class T, class S>
  void Object::SignalReceiverTS<T,S>::emit(Object * /*sender*/,
                                           const char * /*name*/,
                                           SignalInfo & /*_info*/) const
  {
    assert(false);
  }

  template<class T, class S>
  Object* Object::SignalReceiverTS<T,S>::getObject() const
  {
    return obj;
  }

  template<class T, class S>
  bool Object::SignalReceiverTS<T,S>::operator==(const Object::SignalReceiver &_other) const
  {
    const Object::SignalReceiverTS<T,S> *other;

    other = dynamic_cast<const Object::SignalReceiverTS<T,S>*>(&_other);
    if (other == nullptr)
      return false;

    if (other->obj != obj)
      return false;
    if (other->callback != callback)
      return false;

    return true;
  }

  template<class T, class S, typename I>
  Object::SignalReceiverTSI<T, S, I>::SignalReceiverTSI(T *obj_, void (T::*callback_)(S*, const char*, I))
    : obj(obj_), callback(callback_)
  {
    assert(obj_);
    assert(callback_);
  }

  template<class T, class S, class I>
  void Object::SignalReceiverTSI<T, S, I>::emit(Object * /*sender*/,
                                                const char * /*name*/) const
  {
    assert(false);
  }

  template<class T, class S, typename I>
  void Object::SignalReceiverTSI<T, S, I>::emit(Object *_sender,
                                                const char *name,
                                                SignalInfo &_info) const
  {
    S *sender;
    TypedInfo<I> *info;

    sender = dynamic_cast<S*>(_sender);
    assert(sender != nullptr);

    info = dynamic_cast<TypedInfo<I>*>(&_info);
    assert(info != nullptr);

    (obj->*callback)(sender, name, info->getData());
  }

  template<class T, class S, typename I>
  Object* Object::SignalReceiverTSI<T, S, I>::getObject() const
  {
    return obj;
  }

  template<class T, class S, typename I>
  bool Object::SignalReceiverTSI<T, S, I>::operator==(const Object::SignalReceiver &_other) const
  {
    const Object::SignalReceiverTSI<T, S, I> *other;

    other = dynamic_cast<const Object::SignalReceiverTSI<T, S, I>*>(&_other);
    if (other == nullptr)
      return false;

    if (other->obj != obj)
      return false;
    if (other->callback != callback)
      return false;

    return true;
  }

}

#endif
