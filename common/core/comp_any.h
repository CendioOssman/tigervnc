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
// comp_any - Comparable core::any
//

#ifndef __CORE_COMP_ANY_H__
#define __CORE_COMP_ANY_H__

#include <core/any.h>

namespace core {

  class comp_any {
  public:
    comp_any() {}
    comp_any(const comp_any&) = default;
    template<class ValueType>
    comp_any(const ValueType& value_);

    ~comp_any() {}

    comp_any& operator=(const comp_any&) = default;

    bool operator==(const comp_any& other) const;
    bool operator!=(const comp_any& other) const;

  protected:
    any value;

    std::function<bool(const any&, const any&)> equal_to;
    std::function<bool(const any&, const any&)> not_equal_to;
  };

  template<class ValueType>
  comp_any::comp_any(const ValueType& value_) : value{value_}
  {
    typedef std::function<bool(
      const any&, const any&,
      std::function<bool(const ValueType&, const ValueType&)>)>
      basefn_t;

    basefn_t base =
      [](const any& a, const any& b,
         std::function<bool(const ValueType&, const ValueType&)> f) {
      try {
        const ValueType& va = any_cast<ValueType>(a);
        const ValueType& vb = any_cast<ValueType>(b);
        return f(va, vb);
      } catch (const std::bad_cast&) {
        return false;
      }
    };

    equal_to = [base](const any& a, const any& b) {
      return base(a, b, std::equal_to<ValueType>());
    };
    not_equal_to = [base](const any& a, const any& b) {
      return base(a, b, std::not_equal_to<ValueType>());
    };
  }

  inline bool comp_any::operator==(const comp_any& other) const
  {
    return equal_to(value, other.value);
  }

  inline bool comp_any::operator!=(const comp_any& other) const
  {
    return not_equal_to(value, other.value);
  }
}
#endif
