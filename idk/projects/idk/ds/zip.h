#pragma once

namespace idk
{
	template<typename ... Containers>
	constexpr auto zip(Containers&&...);
}
#include "zip.inl"