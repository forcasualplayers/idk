#pragma once
#include <utility>
#include <array>
#include <cassert>

#include "Vector.h"
#include "Matrix.h"
#include <ds/range.inl>
#include <ds/zip.inl>

namespace idk
{
	namespace detail
	{
		template<typename T, unsigned R, unsigned C, size_t ... Indexes>
		tvec<T, R> MatrixVectorMult(const tmat<T, R, C>& lhs, const tvec<T, C>& rhs, std::index_sequence<Indexes...>)
		{
			if constexpr (std::is_same_v<T, float> && R == 4 && C == 4)
			{
				const tvec<float, 4> & vec = rhs;
				const tmat<float, 4, 4>& mat = lhs;
				const __m128& v = vec.sse;
				
				__m128 v0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
				__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
				__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
				__m128 v3 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));

				__m128 a0 = _mm_add_ps(_mm_mul_ps(mat[0].sse, v0), _mm_mul_ps(mat[1].sse, v1));
				__m128 a1 = _mm_add_ps(_mm_mul_ps(mat[2].sse, v2), _mm_mul_ps(mat[3].sse, v3));

				return tvec<float, 4>{_mm_add_ps(a0, a1)};
			}
			else
				return ((lhs[Indexes] * rhs[Indexes]) + ...);
		}

		template<typename T, unsigned O, unsigned M, unsigned I, size_t ... Indexes>
		tmat<T, O, I> MatrixMatrixMult(const tmat<T, O, M>& lhs, const tmat<T, M, I>& rhs, std::index_sequence<Indexes...>)
		{
			return tmat<T, O, I>{ (lhs * rhs[Indexes]) ...};
		}

		template<size_t TransposeMe, typename T, unsigned R, unsigned C, size_t ... RowIndexes>
		auto MatrixTransposeRow(const tmat<T, R, C>& transposeme,
			std::index_sequence<RowIndexes...>)
		{
			return tvec<T, C>{
				(transposeme[RowIndexes][TransposeMe]) ...
			};
		}

		template<typename T, unsigned R, unsigned C, size_t ... RowIndexes>
		tmat<T, C, R> MatrixTranspose(const tmat<T, R, C>& transposeme, 
			std::index_sequence<RowIndexes...>)
		{
			return tmat<T, C, R> {
					(MatrixTransposeRow<RowIndexes>(transposeme, std::make_index_sequence<C>{})) ...
				};
		}

		template<unsigned Col, typename T, unsigned R, unsigned C, 
			unsigned ... RowIndexes>
		tvec<T, C> VectorOnRow(const std::array<T, R* C>& values, std::index_sequence<RowIndexes...>)
		{
			return tvec<T, C>{
				(values[R * RowIndexes + Col]) ...
			};
		}

		template<typename T, unsigned R, unsigned C, 
			size_t ... ColIndexes>
		tmat<T, R, C> MatrixFromRowMajor(const std::array<T, R*C> &values, std::index_sequence<ColIndexes...>)
		{
			return tmat<T, R, C>{
				(VectorOnRow<ColIndexes, T, R, C>(values, std::make_index_sequence<R>{})) ...
			};
		}
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C>::tmat()
		: intern{}
	{
		if constexpr (R <= C)
			for (auto& i : range<R>())
				intern[i][i] = T{ 1 };
		else
			for (auto& i : range<C>())
				intern[i][i] = T{ 1 };

	}
	
	template<typename T, unsigned R, unsigned C>
	template<typename ...U, typename>
	tmat<T, R, C>::tmat(const tvec<U, R>& ...vectors)
		: intern{vectors...}
	{
	}

	template<typename T, unsigned R, unsigned C>
	template<typename ...U, typename>
	tmat<T, R, C>::tmat(U ... values)
		: tmat{ detail::MatrixFromRowMajor<T,R,C>(std::array<T, R*C>{static_cast<T>(values)...}, std::make_index_sequence<C>{}) }
	{
	}

	template<typename T, unsigned R, unsigned C>
	inline tmat<T, R, C>::tmat(T* ptr)
	{
		auto writr = data();
		const auto etr = ptr + R * C;
		while (ptr != etr)
			*writr = *ptr++;
	}

	template<typename T, unsigned R, unsigned C>
	inline tmat<T, R, C>::tmat(const tmat<T, R - 1, C - 1>& mtx)
		: tmat{}
	{
		for (auto& elem : range<C - 1>())
			intern[elem] = column_t{ mtx[elem], 0.f };
	}

	template<typename T, unsigned R, unsigned C>
	inline tmat<T, R, C>::tmat(const tmat<T, R + 1, C + 1> & mtx)
		: tmat{}
	{
		for (auto& elem : range<C>())
			intern[elem] = column_t{ mtx[elem] };
	}

	template<typename T, unsigned R, unsigned C>
	T tmat<T, R, C>::determinant() const
	{
		static_assert(R == C, "determinant can only be called on square matrices");
		//static_assert(R < 4,  "we haven't developed determinants beyond this dimension");

		auto& m = *this;
		if constexpr (R == 1)
			return m[0][0];
		else
		if constexpr (R == 2)
			return m[0][0] * m[1][1] - m[1][0] * m[0][1];
		else
		if constexpr (R == 3)
		{
			auto& a = m[0][0]; auto& b = m[1][0]; auto& c = m[2][0];
			auto& d = m[0][1]; auto& e = m[1][1]; auto& f = m[2][1];
			auto& g = m[0][2]; auto& h = m[1][2]; auto& i = m[2][2];

			// det of 3x3 = aei + bfg + cdh - ceg - bdi - afh;
			return a * (e * i - f * h) + b * (f * g - d * i) + c * (d * h - e * g);
		}
		else
		{
			(m);
			return 0;
		}
	}

	template<typename T, unsigned R, unsigned C>
	inline tmat<T, C, R> tmat<T, R, C>::transpose() const
	{
		if constexpr (std::is_same_v<float, T> && R == 4 && C == 4)
		{
			const tmat<float, 4, 4> & me = *this;
			__m128 tmp0 = _mm_shuffle_ps(me[0].sse, me[1].sse, 0x44);
			__m128 tmp2 = _mm_shuffle_ps(me[0].sse, me[1].sse, 0xEE);
			__m128 tmp1 = _mm_shuffle_ps(me[2].sse, me[3].sse, 0x44);
			__m128 tmp3 = _mm_shuffle_ps(me[2].sse, me[3].sse, 0xEE);

			return tmat<float, 4, 4>
			{
				tvec<float, 4>{_mm_shuffle_ps(tmp0, tmp1, 0x88)},
					tvec<float, 4>{_mm_shuffle_ps(tmp0, tmp1, 0xDD)},
					tvec<float, 4>{_mm_shuffle_ps(tmp2, tmp3, 0x88)},
					tvec<float, 4>{_mm_shuffle_ps(tmp2, tmp3, 0xDD)}
			};
		}
		else
			return detail::MatrixTranspose(*this, std::make_index_sequence<R>{});
	}

	namespace detail
	{
		tmat<float, 4, 4> sse_invert(const tmat<float, 4, 4>&);
	}
	template<typename T, unsigned R, unsigned C>
	inline tmat<T, C, R> tmat<T, R, C>::inverse() const
	{
		static_assert(R == C, "inverse is only callable on square matrices");

		if constexpr (R == 1)
			return tmat{ 1.f } / determinant();
		else
		if constexpr (R == 2)
			return tmat{
				 intern[1][1],	-intern[1][0],
				-intern[0][1],	 intern[0][0]
			};
		else
		if constexpr(R == 3)
		{
			auto& m = *this;
			auto& a = m[0][0]; auto& b = m[1][0]; auto& c = m[2][0];
			auto& d = m[0][1]; auto& e = m[1][1]; auto& f = m[2][1];
			auto& g = m[0][2]; auto& h = m[1][2]; auto& i = m[2][2];

			return tmat{
				  (e * i - f * h), - (b * i - c * h),   (b * f - c * e),
				- (d * i - f * g),   (a * i - c * g), - (a * f - c * d),
				  (d * h - e * g), - (a * h - b * g),   (a * e - b * d)
			} / determinant();
		}
		else
		if constexpr(R == 4)
		{
			return detail::sse_invert(*this);
		}
		else
		return tmat<T, C, R>();
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, C - 1, R - 1> tmat<T, R, C>::cofactor(unsigned r, unsigned c) const
	{
		auto retval = tmat<T, C-1, R-1>{};

		for (unsigned i_track = 0, i_write = 0; i_track < R; ++i_track)
		{
			if (i_track == r)
				continue;

			for (unsigned j_track = 0, j_write = 0; j_track < C; ++j_track)
			{
				if (j_track == c)
					continue;

				retval[j_write++][i_write] = (*this)[j_track][i_track];
			}

			++i_write;
		}

		return retval;
	}

	template<typename T, unsigned R, unsigned C>
	typename tmat<T, R, C>::column_t* tmat<T, R, C>::begin()
	{
		return std::data(intern);
	}

	template<typename T, unsigned R, unsigned C>
	const typename tmat<T, R, C>::column_t* tmat<T, R, C>::begin() const
	{
		return std::data(intern);
	}

	template<typename T, unsigned R, unsigned C>
	typename tmat<T, R, C>::column_t* tmat<T, R, C>::end()
	{
		return std::data(intern) + std::size(intern);
	}

	template<typename T, unsigned R, unsigned C>
	const typename tmat<T, R, C>::column_t* tmat<T, R, C>::end() const
	{
		return std::data(intern) + std::size(intern);
	}

	template<typename T, unsigned R, unsigned C>
	T* tmat<T, R, C>::data()
	{
		return intern[0].data();
	}

	template<typename T, unsigned R, unsigned C>
	const T* tmat<T, R, C>::data() const
	{
		return intern[0].data();
	}

	template<typename T, unsigned R, unsigned C>
	typename tmat<T, R, C>::column_t& tmat<T,R,C>::operator[](size_t index)
	{
		return intern[index];
	}

	template<typename T, unsigned R, unsigned C>
	const typename tmat<T, R, C>::column_t& tmat<T, R, C>::operator[](size_t index) const
	{
		return intern[index];
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C>& tmat<T, R, C>::operator+=(const tmat& rhs)
	{
		auto ltr = this->begin();
		auto rtr = rhs.begin();
		const auto* etr = this->end();

		while (ltr != etr)
			*ltr++ += *rtr++;

		return *this;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C> tmat<T, R, C>::operator+(const tmat& rhs) const
	{
		auto copy = *this;
		return copy += rhs;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C>& tmat<T, R, C>::operator-=(const tmat& rhs)
	{
		auto ltr = this->begin();
		auto rtr = rhs.begin();
		const auto* etr = this->end();

		while (ltr != etr)
			*ltr++ -= *rtr++;

		return *this;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C> tmat<T, R, C>::operator-(const tmat& rhs) const
	{
		auto copy = *this;
		return copy -= rhs;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C>& tmat<T, R, C>::operator*=(const T& val)
	{
		for (auto& elem : *this)
			elem *= val;

		return *this;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C> tmat<T, R, C>::operator*(const T& val) const
	{
		auto copy = *this;
		return copy *= val;
	}


	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C>& tmat<T, R, C>::operator/=(const T& val)
	{
		for (auto& elem : *this)
			elem /= val;

		return *this;
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C> tmat<T, R, C>::operator/(const T& val) const
	{
		auto copy = *this;
		return copy /= val;
	}

	template<typename T, unsigned R, unsigned C>
	bool tmat<T, R, C>::operator==(const tmat& rhs) const
	{
		auto ltr = this->begin();
		auto rtr = rhs.begin();
		auto etr = this->end();

		while (ltr != etr)
			if (*ltr++ != *rtr++)
				return false;

		return true;
	}

	template<typename T, unsigned R, unsigned C>
	bool tmat<T, R, C>::operator!=(const tmat& rhs) const
	{
		return !operator==(rhs);
	}

	template<typename T, unsigned R, unsigned C>
	tmat<T, R, C> operator*(const T& coeff, const tmat<T, R, C>& m)
	{
		return m * coeff;
	}
#ifdef _DEBUG
//#pragma optimize("",on)
#endif
	template<typename T, unsigned R, unsigned C>
	tvec<T, R> operator*(const tmat<T, R, C>& lhs, const tvec<T, C>& rhs)
	{
		return detail::MatrixVectorMult(lhs, rhs, std::make_index_sequence<C>{});
	}

	template<typename T, unsigned O, unsigned M, unsigned I>
	tmat<T, O, I> operator*(const tmat<T, O, M>& lhs, const tmat<T, M, I>& rhs)
	{
		return detail::MatrixMatrixMult(lhs, rhs, std::make_index_sequence<I>{});
	}
#ifdef _DEBUG
//
#endif
}