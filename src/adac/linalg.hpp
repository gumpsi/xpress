// SPDX-FileCopyrightText: 2024 Dennis Gläser <dennis.glaeser@iws.uni-stuttgart.de>
// SPDX-License-Identifier: MIT
/*!
 * \file
 * \ingroup LinearAlgebra
 * \brief Algebraic operations on types that represent tensors or vectors.
 */
#pragma once

#include <concepts>
#include <type_traits>
#include <algorithm>

#include "utils.hpp"
#include "traits.hpp"
#include "operators.hpp"


namespace adac::linalg {

//! \addtogroup LinearAlgebra
//! \{

template<typename T, typename shape> requires(adac::traits::is_scalar_v<T>)
struct tensor {
 public:
    constexpr tensor() = default;
    constexpr tensor(T value) noexcept { std::ranges::fill(_values, value); }

    template<std::convertible_to<T>... _T> requires(sizeof...(_T) == shape::count)
    constexpr tensor(const shape&, _T&&... values) noexcept
    : _values{static_cast<T>(std::forward<_T>(values))...}
    {}

    constexpr tensor(const shape&, std::array<T, shape::count>&& values) noexcept
    : _values{std::move(values)}
    {}

    template<typename S, std::size_t... i>
    constexpr decltype(auto) operator[](this S&& self, const md_index<i...>& idx) noexcept {
        static_assert(md_index<i...>::as_flat_index_in(shape{}) < shape::count);
        return self._values[md_index<i...>::as_flat_index_in(shape{})];
    }

    template<typename S, std::size_t i> requires(shape::size == 1)
    constexpr decltype(auto) operator[](this S&& self, const index_constant<i>& idx) noexcept {
        return self[md_i_c<i>];
    }

    template<typename V> requires(traits::is_scalar_v<V>)
    constexpr auto operator*(const V& value) const noexcept {
        return operators::traits::multiplication_of<tensor, V>{}(*this, value);
    }

    template<typename T2, typename shape2>
    constexpr bool operator==(const tensor<T2, shape2>& other) const noexcept {
        if constexpr (shape2{} != shape{}) {
            return false;
        } else {
            bool all_equal = true;
            visit_indices_in(shape{}, [&] (const auto& idx) constexpr {
                if ((*this)[idx] != other[idx])
                    all_equal = false;
            });
            return all_equal;
        }
    }

 private:
    std::array<T, shape::count> _values;
};

template<std::size_t... s, typename... Ts> requires(std::conjunction_v<traits::is_scalar<std::remove_cvref_t<Ts>>...>)
tensor(const md_shape<s...>&, Ts&&...) -> tensor<std::common_type_t<Ts...>, md_shape<s...>>;


namespace traits {

#ifndef DOXYGEN
namespace detail {

    template<std::size_t s>
    void _invoke_with_constexpr_size() {}

    template<typename T>
    concept has_constexpr_size = requires(const T& t) {
        { t.size() } -> std::convertible_to<std::size_t>;
        { _invoke_with_constexpr_size<T{}.size()>() };
    };

    template<typename T>
    concept has_constexpr_size_constant = requires {
        { T::size } -> std::convertible_to<std::size_t>;
    };

}  // namespace detail
#endif  // DOXYGEN

template<typename T>
struct size_of;
template<detail::has_constexpr_size T>
struct size_of<T> : std::integral_constant<std::size_t, T{}.size()> {};
template<detail::has_constexpr_size_constant T>
struct size_of<T> : std::integral_constant<std::size_t, T::size> {};
template<typename T> requires(is_complete_v<size_of<T>>)
inline constexpr std::size_t size_of_v = size_of<T>::value;


#ifndef DOXYGEN
namespace detail {

    template<typename T, std::size_t... s>
    struct shape_of_indexable;
    template<typename T, std::size_t... s> requires(!adac::traits::is_indexable_v<T>)
    struct shape_of_indexable<T, s...> : std::type_identity<md_shape<s...>> {};
    template<typename T, std::size_t... s> requires(adac::traits::is_indexable_v<T>)
    struct shape_of_indexable<T, s...> : shape_of_indexable<adac::traits::value_type_t<T>, s..., size_of_v<T>> {};

}  // namespace detail
#endif  // DOXYGEN

template<typename T>
struct shape_of;
template<typename T> requires(adac::traits::is_indexable_v<T>)
struct shape_of<T> : detail::shape_of_indexable<T> {};
template<typename T, typename shape>
struct shape_of<tensor<T, shape>> : std::type_identity<shape> {};
template<typename T>
using shape_of_t = typename shape_of<T>::type;

template<typename T>
struct access;
template<typename T, typename shape>
struct access<tensor<T, shape>> {
    template<concepts::same_remove_cvref_t_as<tensor<T, shape>> _T, std::size_t... i>
    static constexpr decltype(auto) at(const md_index<i...>& idx, _T&& tensor) noexcept {
        return tensor[idx];
    }
};
template<typename T> requires(adac::traits::is_indexable_v<T> and is_complete_v<shape_of<T>>)
struct access<T> {
    template<concepts::same_remove_cvref_t_as<T> _T, std::size_t... i> requires(sizeof...(i) == shape_of_t<T>::size)
    static constexpr decltype(auto) at(const md_index<i...>&, _T&& tensor) noexcept {
        return _at<i...>(std::forward<_T>(tensor));
    }

 private:
    template<std::size_t i, std::size_t... is>
    static constexpr decltype(auto) _at(auto&& t) noexcept {
        if constexpr (sizeof...(is) == 0)
            return t[i];
        else
            return _at<is...>(t[i]);
    }
};


}  // namespace traits


namespace concepts {

template<typename T>
concept tensor
= std::is_default_constructible_v<T>  // TODO: can we relax this?
and is_complete_v<traits::shape_of<T>>
and is_complete_v<adac::traits::scalar_type<T>>
and is_complete_v<traits::access<T>>
and requires(const T& t) {
    { traits::access<T>::at( *(md_index_iterator{traits::shape_of_t<T>{}}), t ) };
};

}  // namespace concepts

//! \} group LinearAlgebra

}  // namespace adac::linalg


namespace adac::operators::traits {

template<linalg::concepts::tensor T, typename S> requires(adac::traits::is_scalar_v<S>)
struct multiplication_of<T, S> {
    template<concepts::same_remove_cvref_t_as<T> _T, concepts::same_remove_cvref_t_as<S> _S>
    constexpr T operator()(_T&& tensor, _S&& scalar) const noexcept {
        T result;
        visit_indices_in(linalg::traits::shape_of_t<T>{}, [&] (const auto& idx) {
            adac::traits::scalar_type_t<T>& value_at_idx = adac::linalg::traits::access<T>::at(idx, result);
            value_at_idx = adac::linalg::traits::access<T>::at(idx, tensor)*scalar;
        });
        return result;
    }
};
template<typename S, linalg::concepts::tensor T> requires(adac::traits::is_scalar_v<S>)
struct multiplication_of<S, T> {
    template<concepts::same_remove_cvref_t_as<S> _S, concepts::same_remove_cvref_t_as<T> _T>
    constexpr T operator()(_S&& scalar, _T&& tensor) const noexcept {
        return multiplication_of<T, S>{}(std::forward<_T>(tensor), std::forward<_S>(scalar));
    }
};

template<linalg::concepts::tensor T1, linalg::concepts::tensor T2>
    requires(linalg::traits::shape_of_t<T1>{} == linalg::traits::shape_of_t<T2>{})
struct multiplication_of<T1, T2> {
    template<concepts::same_remove_cvref_t_as<T1> _T1, concepts::same_remove_cvref_t_as<T2> _T2>
    constexpr auto operator()(_T1&& A, _T2&& B) const noexcept {
        adac::traits::scalar_type_t<T1> result{0};
        visit_indices_in(linalg::traits::shape_of_t<T1>{}, [&] (const auto& idx) {
            result += adac::linalg::traits::access<T1>::at(idx, A)
                        *adac::linalg::traits::access<T2>::at(idx, B);
        });
        return result;
    }
};

template<linalg::concepts::tensor T1, linalg::concepts::tensor T2>
    requires(linalg::traits::shape_of_t<T1>{} == linalg::traits::shape_of_t<T2>{})
struct addition_of<T1, T2> {
    template<concepts::same_remove_cvref_t_as<T1> _T1, concepts::same_remove_cvref_t_as<T2> _T2>
    constexpr auto operator()(_T1&& A, _T2&& B) const noexcept {
        using scalar = std::common_type_t<adac::traits::scalar_type_t<T1>, adac::traits::scalar_type_t<T2>>;
        using shape = linalg::traits::shape_of_t<T1>;
        linalg::tensor<scalar, shape> result{};
        visit_indices_in(shape{}, [&] (const auto& idx) {
            result[idx] = adac::linalg::traits::access<T1>::at(idx, A) + adac::linalg::traits::access<T2>::at(idx, B);
        });
        return result;
    }
};

template<linalg::concepts::tensor T1, linalg::concepts::tensor T2>
    requires(linalg::traits::shape_of_t<T1>{} == linalg::traits::shape_of_t<T2>{})
struct subtraction_of<T1, T2> {
    template<concepts::same_remove_cvref_t_as<T1> _T1, concepts::same_remove_cvref_t_as<T2> _T2>
    constexpr auto operator()(_T1&& A, _T2&& B) const noexcept {
        using scalar = std::common_type_t<adac::traits::scalar_type_t<T1>, adac::traits::scalar_type_t<T2>>;
        using shape = linalg::traits::shape_of_t<T1>;
        linalg::tensor<scalar, shape> result{};
        visit_indices_in(shape{}, [&] (const auto& idx) {
            result[idx] = adac::linalg::traits::access<T1>::at(idx, A) - adac::linalg::traits::access<T2>::at(idx, B);
        });
        return result;
    }
};

}  // namespace adac::operators::traits


namespace adac::traits {

template<typename T, typename shape>
struct scalar_type<linalg::tensor<T, shape>> : std::type_identity<T> {};

}  // namespace adac::traits
