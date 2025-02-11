// SPDX-FileCopyrightText: 2024 Dennis Gläser <dennis.a.glaeser@gmail.com>
// SPDX-License-Identifier: MIT
/*!
 * \file
 * \ingroup Operators
 * \brief Defines power operations on expressions.
 */
#pragma once

#include <cmath>

#include "../values.hpp"
#include "../expressions.hpp"
#include "../linalg.hpp"
#include "common.hpp"


namespace xp {

//! \addtogroup Operators
//! \{

namespace operators {

namespace traits { template<typename A, typename B> struct power_of; }

struct default_pow_operator {
    template<typename A, typename B>
    constexpr auto operator()(A&& a, B&& b) const noexcept {
        return std::pow(std::forward<A>(a), std::forward<B>(b));
    }
};

struct pow : operator_base<traits::power_of, default_pow_operator> {};

namespace traits {

//! (Default) specialization for tensors
template<tensorial T, typename E>
struct power_of<T, E> {
    template<same_remove_cvref_t_as<T> _T, same_remove_cvref_t_as<E> _E>
    constexpr auto operator()(_T&& t, _E&& e) const noexcept {
        using scalar = scalar_type_t<T>;
        using shape = shape_of_t<T>;
        linalg::tensor<scalar, shape> result{};
        visit_indices_in(shape{}, [&] (const auto& idx) {
            result[idx] = operators::pow{}(access<T>::at(idx, t), e);
        });
        return result;
    }
};

}  // namespace traits
}  // namespace operators

template<expression A, expression B>
inline constexpr auto pow(const A&, const B&) noexcept {
    if constexpr (traits::is_zero_value_v<A>)
        return val<0>;
    else if constexpr (traits::is_unit_value_v<A> || traits::is_unit_value_v<B>)
        return A{};
    else if constexpr (traits::is_zero_value_v<B>)
        return val<1>;
    else
        return operation<operators::pow, A, B>{};
}

namespace traits {

template<typename T1, typename T2>
struct derivative_of<operation<operators::pow, T1, T2>> {
    template<typename V>
    static constexpr auto wrt(const type_list<V>& var) noexcept {
        return T2{}*pow(T1{}, T2{} - val<1>)*xp::detail::differentiate<T1>(var)
            + pow(T1{}, T2{})*log(T2{})*xp::detail::differentiate<T2>(var);
    }
};

template<typename T1, typename T2>
struct stream<operation<operators::pow, T1, T2>> {
    template<typename... V>
    static constexpr void to(std::ostream& out, const bindings<V...>& values) noexcept {
        write_to(out, T1{}, values);
        out << "^";
        static constexpr bool exponent_has_subterms = nodes_of_t<T2>::size > 1;
        if constexpr (exponent_has_subterms) out << "(";
        write_to(out, T2{}, values);
        if constexpr (exponent_has_subterms) out << ")";
    }
};

}  // namespace traits

//! \} group Operators

}  // namespace xp
