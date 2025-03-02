#pragma once
#include "angle.h"
#include "vector.h"
#include "matrix.h"

namespace idk
{
	template <typename T>
	struct __declspec(empty_bases) quaternion
		: linear<quaternion<T>, T>
		, private tvec<T, 4>
	{
		using linear<quaternion<T>, T>::Scalar;
		using Base = tvec<T, 4>;
		using Base::x;
		using Base::y;
		using Base::z;
		using Base::w;
		using Base::values;

		quaternion();
		quaternion(T x, T y, T z, T w);
		quaternion(const tvec<T, 3> & axis, trad<T> angle);

		// accessors
		T          dot(const quaternion&) const;
		quaternion inverse() const;
		quaternion get_normalized() const;
		quaternion integrate(const tvec<T, 3>& dv) const;

		// modifiers
		quaternion& normalize();

		// iterators
		using Base::begin;
		using Base::end;
		using Base::data;
		using Base::size;

		// operator overloads
		using linear<quaternion<T>, T>::operator*;
		using linear<quaternion<T>, T>::operator==;
		using linear<quaternion<T>, T>::operator!=;
		using Base::operator[];
		quaternion& operator*=(const quaternion&);
		quaternion  operator*(const quaternion&) const;

		// conversion to rotation matrix
		explicit operator tmat<T, 3, 3>();
		explicit operator tmat<T, 3, 3>() const;
		explicit operator tmat<T, 4, 4>();
		explicit operator tmat<T, 4, 4>() const;
	};
}

namespace idk
{
	template<typename M, typename T> auto quat_cast(quaternion<T>& q);
	template<typename M, typename T> auto quat_cast(quaternion<T>&& q);
	template<typename M, typename T> auto quat_cast(const quaternion<T>& q);


	extern template struct quaternion<float>;
}
