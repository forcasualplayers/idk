#pragma once
#define WIN32_LEAN_AND_MEAN

#include <array>
#include <vector>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <string_view>

#include <atomic>
#include <chrono>

#include <memory>
#include <optional>
#include <variant>

#include <stddef.h>
#include <functional>

#include <machine.h>

#include <idk_config.h>
#include <ds/span.h>
#include <ds/small_string.inl>
#include <debug/idk_assert.h>
#include <math/color.inl>
#include <math/constants.inl>
#include <math/arith.h>
#include <math/angle.inl>
#include <math/vector.inl>
#include <math/matrix.inl>
#include <math/quaternion.inl>

#include <debug/Log.h>

namespace idk
{
	// math
	using real = float;
	using vec2 = tvec<real, 2>;
	using vec3 = tvec<real, 3>;
	using vec4 = tvec<real, 4>;

	using ivec2 = tvec<int, 2>;
	using ivec3 = tvec<int, 3>;
	using ivec4 = tvec<int, 4>;

	using uvec2 = tvec<uint32_t, 2>;
	using uvec3 = tvec<uint32_t, 3>;
	using uvec4 = tvec<uint32_t, 4>;

	using bvec2 = tvec<bool, 2>;
	using bvec3 = tvec<bool, 3>;
	using bvec4 = tvec<bool, 4>;

	using idk::color;

	using quat = quaternion<real>;
	
	using mat2 = tmat<real, 2, 2>;
	using mat3 = tmat<real, 3, 3>;
	using mat4 = tmat<real, 4, 4>;

	using rad = trad<real>;
	using deg = tdeg<real>;

	// time
	using Clock = std::chrono::high_resolution_clock;

	using seconds = std::chrono::duration<real>;
	using time_point = Clock::time_point;

	template<typename T, typename U>
	auto duration_cast(U&& time) {	return std::chrono::duration_cast<T>(std::forward<U>(time));	};

	// math constants
	constexpr auto pi      = constants::pi<real>();
	constexpr auto half_pi = pi / 2;
	constexpr auto two_pi  = constants::tau<real>();
	constexpr auto epsilon = constants::epsilon<real>();

	// containers
	using byte = std::byte;

	template<typename T, size_t N>
	using array = std::array<T, N>;

	template<typename T>
	using vector = std::vector<T>;

	template<typename T>
	using small_vector = std::vector<T>;//Can be replaced with small vector(if we *ever* get around to it)

	template<typename T1, typename T2, typename Hasher = std::hash<T1>>
	using hash_table = std::unordered_map<T1, T2, Hasher>;

	template<typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
	using hash_set = std::unordered_set<T, Hash, Equal>;

	template<typename T,typename Equal = std::equal_to<T>>
	using set = std::set<T, Equal>;

	using string = idk::small_string<char>;

	using string_view = std::string_view;

	template<typename T>
	using atomic = std::atomic<T>;

	// utility
	template<typename Signature>
	using function = std::function<Signature>;

	template<typename T>
	using opt = std::optional<T>;

	template<typename T>
	using Ref = std::reference_wrapper<T>;

	using std::variant;
	using std::monostate;

	template<bool val>
	using sfinae = std::enable_if_t<val>;

	using std::true_type;
	using std::false_type;

	// smart pointers
	template<typename T, typename Deleter = std::default_delete<T>>
	using unique_ptr = std::unique_ptr<T, Deleter>;

	template<typename T>
	using shared_ptr = std::shared_ptr<T>;
}