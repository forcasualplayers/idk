#pragma once
#include <type_traits>

namespace idk
{
    template<typename T, template<typename ...> typename Template>
    struct is_template;

    template<typename T, template<typename ...> typename Template>
    constexpr auto is_template_v = is_template<T, Template>::value;


	template<typename Seq1, typename Seq2>
	struct index_sequence_cat;

	template<typename Seq1, typename Seq2>
	using index_sequence_cat_t = typename index_sequence_cat<Seq1, Seq2>::type;


	template<typename T>
	struct index_sequence_rev;

	template<typename T>
	using index_sequence_rev_t = typename index_sequence_rev<T>::type;


	template<typename T, typename>
	struct is_iterable;

	template<typename T>
	constexpr auto is_iterable_v = is_iterable<T>::value;


	template<typename T, typename>
	struct is_sequential_container;

	template<typename T>
	constexpr auto is_sequential_container_v = is_sequential_container<T>::value;


	template<typename T, typename>
	struct is_associative_container;

	template<typename T>
	constexpr auto is_associative_container_v = is_associative_container<T>::value;

	template<typename T>
	struct is_container;

	template<typename T>
	constexpr auto is_container_v = is_container<T>::value;

	template<typename T, typename>
	struct is_contiguous_container;

	template<typename T>
	constexpr auto is_contiguous_container_v = is_contiguous_container<T>::value;

	template<typename T, typename>
	struct is_macro_enum;

	template<typename T>
	constexpr auto is_macro_enum_v = is_macro_enum<T>::value;


	template<typename T, typename VariantT>
	struct is_variant_member;

	template<typename T, typename VariantT>
	constexpr auto is_variant_member_v = is_variant_member<T, VariantT>::value;


    // true if is integral/floating point, or is convertible to string
    template<typename T>
    struct is_basic_serializable;

    // true if is integral/floating point, or is convertible to string
    template<typename T>
    constexpr auto is_basic_serializable_v = is_basic_serializable<T>::value;
}
