#pragma once
#include "matrix.h"
#include "vector.h"
#include "angle.h"

#undef near
#undef far

namespace idk
{
	template<typename T>
	constexpr T identity();
	
	template<typename T, unsigned D>
	constexpr tmat<T, D+1, D+1> scale(const tvec<T, D>&);

	template<typename T, unsigned D>
	constexpr tmat<T, D, D> prescale(const tmat<T, D, D>& mat, const tvec<T, D>& scale);

	template<typename T, unsigned D>
	constexpr tmat<T, D, D>& prescale(tmat<T, D, D>& mat, const tvec<T, D>& scale);

	template<typename T>
	tmat<T, 4, 4> rotate(const tvec<T, 3> & axis, trad<T> angle);

	template<typename T, unsigned D>
	constexpr tmat<T, D + 1, D + 1> translate(const tvec<T, D> & translate);

	template<typename T, unsigned D>
	constexpr tmat<T, D, D> translate(const tmat<T, D, D>& mat, const tvec<T, D - 1>& translate);

	template<typename T, unsigned D>
	constexpr tmat<T, D, D>& translate(tmat<T, D, D>& mat, const tvec<T, D - 1>& translate);

	template<typename T>
	tmat<T, 4, 4> perspective(trad<T> fov, T aspect_ratio, T near, T far);

	template<typename T>
	tmat<T, 4, 4> perspective(tdeg<T> fov, T aspect_ratio, T near, T far);

	template<typename T>
	constexpr tmat<T, 4, 4> ortho(T left, T right, T bottom, T top, T near, T far);

	template<typename T>
	constexpr tmat<T, 4, 4> look_at(const tvec<T, 3>& eye, const tvec<T, 3>& object, const tvec<T, 3>& up);

	template< typename T >
	tmat<T, 4, 4> orient(const tvec<T, 3> & z_prime);

	// convert a basis to an orthonormal basis using gramm-schidt
	template<typename T, unsigned D>
	constexpr tmat<T, D, D> orthonormalize(const tmat<T, D, D>&);

	template<typename T>
	constexpr tmat<T, 4, 4> invert_rotation(const tmat<T, 4, 4>&);
}