#pragma once
#include <utility>
#include <tuple>

#include "vector_detail.h"
#include "vector.h"
#include <meta/tuple.h>
#include "linear.inl"
#include "constants.inl"
namespace idk
{
	namespace detail
	{
		inline __m128 sse_dot(const tvec<float, 4> & me, const tvec<float, 4> & other)
		{
			const __m128& x = me.sse;
			const __m128& y = other.sse;
			const auto packed_mul = _mm_mul_ps(x, y);
			const auto shuffle = _mm_shuffle_ps(packed_mul, packed_mul, _MM_SHUFFLE(2, 3, 0, 1));
			const auto add0 = _mm_add_ps(packed_mul, shuffle);
			const auto shuffle2 = _mm_shuffle_ps(add0, add0, _MM_SHUFFLE(0, 1, 2, 3));
			const auto add1 = _mm_add_ps(add0, shuffle2);
			return add1;
		}
	}

	template<typename T, unsigned D>
	inline constexpr tvec<T, D>::tvec(const T& fill)
	{
		for (auto& elem : *this)
			elem = fill;
	}

	template<typename T, unsigned D>
	template<typename ...Args, typename>
	constexpr tvec<T, D>::tvec(const Args& ... args)
		: tvec{ detail::VectorConcat<T>(args...) }
	{
	}

	template<typename T, unsigned D>
	template<unsigned D2, unsigned ...Indexes>
	inline constexpr tvec<T, D>::tvec(std::index_sequence<Indexes...>, const tvec<T, D2>& vec)
		: tvec{vec[Indexes]...}
	{
	}

	template<typename T, unsigned D>
	template<unsigned D2, typename>
	constexpr tvec<T, D>::tvec(const tvec<T, D2>& rhs)
		: tvec{ std::make_index_sequence<D>{}, rhs }
	{
	}

	template<typename T, unsigned D>
	inline constexpr tvec<T, D>::tvec(const T* ptr)
	{
		auto wrtr = begin();
		const auto etr = end();
		while (wrtr != etr)
			*wrtr++ = *ptr++;
	}

	template<typename T, unsigned D>
	T tvec<T, D>::length_sq() const
	{
		return dot(*this);
	}

	template<typename T, unsigned D>
	T tvec<T, D>::length() const
	{
		if constexpr (std::is_same_v<float, T>)
			return sqrtf(abs(length_sq()));
		else
			return sqrt(abs(static_cast<double>(length_sq())));
	}

	template<typename T, unsigned D>
	T tvec<T, D>::distance_sq(const tvec& rhs) const
	{
		return (rhs - *this).length_sq();
	}

	template<typename T, unsigned D>
	 T tvec<T, D>::distance(const tvec& rhs) const
	{
		 if constexpr (std::is_same_v<float, T>)
			 return sqrtf(abs(distance_sq(rhs)));
		 else
			 return sqrt(abs(static_cast<double>(distance_sq(rhs))));
	}

	template<typename T, unsigned D>
	T tvec<T, D>::dot(const tvec& rhs) const
	{
		if constexpr (std::is_same_v<T, float> && D == 4)
		{
			return detail::sse_dot(*this, rhs).m128_f32[0];
		}
		else if constexpr (D == 3)
		{
			return this->x * rhs.x + this->y * rhs.y + this->z * rhs.z;
		}
		else
		{
			T accum{};
			for (auto& elem : *this* rhs)
				accum += elem;
			return accum;
		}
	}

	template<typename T, unsigned D>
	tvec<T, D> tvec<T, D>::project_onto(const tvec& rhs)const
	{
		return (dot(rhs) * rhs)/ rhs.length_sq();
	}

	template<typename T, unsigned D>
	tvec<T, D>& tvec<T, D>::normalize()
	{
		auto mag = length();

		if (abs(mag) <= constants::epsilon<T>())
			return *this;

		for (auto& elem : *this)
			elem /= mag;

		return *this;
	}

	template<typename T, unsigned D>
	tvec<T, D> tvec<T, D>::get_normalized() const
	{
		auto copy = *this;
		return copy.normalize();
	}

	template<typename T, unsigned D>
	constexpr T* tvec<T, D>::begin() noexcept
	{
		return std::begin(values);
	}
	template<typename T, unsigned D>
	constexpr T* tvec<T, D>::end() noexcept
	{
		return std::end(values);
	}
	template<typename T, unsigned D>
	constexpr const T* tvec<T, D>::begin() const noexcept
	{
		return std::begin(values);
	}

	template<typename T, unsigned D>
	constexpr const T* tvec<T, D>::end() const noexcept
	{
		return std::end(values);
	}

	template<typename T, unsigned D>
	constexpr T* tvec<T, D>::data() noexcept
	{
		return begin();
	}

	template<typename T, unsigned D>
	constexpr const T* tvec<T, D>::data() const noexcept
	{
		return begin();
	}

	template<typename T, unsigned D>
	constexpr T& tvec<T, D>::operator[](size_t i) noexcept
	{
		return data()[i];
	}

	template<typename T, unsigned D>
	constexpr const T& tvec<T, D>::operator[](size_t i) const noexcept
	{
		return data()[i];
	}
}