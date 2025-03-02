#pragma once
#include <type_traits>

namespace idk
{
	template<typename T, unsigned D> struct tvec;

	namespace detail
	{
		template <typename T>
		struct Dim
		{
			static constexpr unsigned value = std::is_arithmetic_v<T> ? 1 : 16;
		};

		template<typename T>
		static constexpr unsigned Dim_v = Dim<T>::value;

		template <typename T, unsigned D>
		struct Dim < tvec<T, D>>
		{
			static constexpr unsigned value = Dim_v<T> * D;
		};
	}
}