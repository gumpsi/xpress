// SPDX-FileCopyrightText: 2024 Dennis Gläser <dennis.glaeser@iws.uni-stuttgart.de>
// SPDX-License-Identifier: MIT
/*!
 * \file
 * \ingroup Operators
 * \brief Defines addition operations on expressions.
 */
#pragma once

#include <functional>

#include "../values.hpp"
#include "../expressions.hpp"
#include "common.hpp"


namespace xp {

//! \addtogroup Operators
//! \{

namespace operators {

namespace traits { template<typename A, typename B> struct addition_of; }

struct add : operator_base<traits::addition_of, std::plus<void>> {};

namespace traits { template<> struct is_commutative<add> : std::true_type {}; }

}  // namespace operators

template<expression A, expression B>
    requires( not requires(const A& a, const B& b) { { a.operator+(b) }; } )
inline constexpr auto operator+(const A&, const B&) noexcept {
    if constexpr (traits::is_zero_value_v<A>)
        return B{};
    else if constexpr (traits::is_zero_value_v<B>)
        return A{};
    else if constexpr (std::is_same_v<A, B>)
        return val<2>*A{};
    else
        return operation<operators::add, A, B>{};
}

namespace traits {

template<typename T1, typename T2>
struct derivative_of<operation<operators::add, T1, T2>> {
    template<typename V>
    static constexpr auto wrt(const type_list<V>& var) noexcept {
        return xp::detail::differentiate<T1>(var) + xp::detail::differentiate<T2>(var);
    }
};

template<typename T1, typename T2>
struct stream<operation<operators::add, T1, T2>> {
    template<typename... V>
    static constexpr void to(std::ostream& out, const bindings<V...>& values) noexcept {
        write_to(out, T1{}, values);
        out << " + ";
        write_to(out, T2{}, values);
    }
};

}  // namespace traits

//! \} group Operators

}  // namespace xp
