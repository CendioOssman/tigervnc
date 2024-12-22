/* Copyright 2024 Pierre Ossman for Cendio AB
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
// any - Generic container for any type, similar to std::any
//

#ifndef __CORE_ANY_H__
#define __CORE_ANY_H__

#include <typeinfo>

namespace core {

  class any;

  template<class T>
  T any_cast(const any& value);

  class any {
  public:
    any();
    any(const any&);
    template<class ValueType>
    any(const ValueType& value);

    ~any();

    any& operator=(const any&);

    bool has_value() const;
    const std::type_info& type() const;

  private:
    class any_value;
    template<class ValueType>
    class any_value_impl;

    any_value* value;

    template<class T>
    friend T any_cast(const any& value);
  };

  template<class ValueType>
  any::any(const ValueType& value_)
  {
    value = new any_value_impl<ValueType>(value_);
  }

  class any::any_value {
  protected:
    any_value() {}
  public:
    virtual ~any_value() {}
    virtual any_value* clone() const = 0;
    virtual const std::type_info& type() const = 0;
    any_value(const any_value&) = delete;
    any_value& operator=(const any_value&) = delete;
  };

  template<class ValueType>
  class any::any_value_impl : public any_value {
  public:
    any_value_impl(const ValueType& value);
    any_value* clone() const override;
    const std::type_info& type() const override;
  public:
    ValueType value;
  };

  template<class T>
  T any_cast(const any& value) {
    if (value.value == nullptr)
      throw std::bad_cast();
    any::any_value_impl<T>* v;
    v = dynamic_cast<any::any_value_impl<T>*>(value.value);
    if (v == nullptr)
      throw std::bad_cast();
    return v->value;
  }

  template<class ValueType>
  any::any_value_impl<ValueType>::any_value_impl(const ValueType& value_)
    : value(value_)
  {
  }

  template<class ValueType>
  any::any_value* any::any_value_impl<ValueType>::clone() const
  {
    return new any::any_value_impl<ValueType>(value);
  }

  template<class ValueType>
  const std::type_info& any::any_value_impl<ValueType>::type() const
  {
    return typeid(ValueType);
  }
}

#endif
