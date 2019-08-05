#pragma once
#include <utility>
#include "ranged_for.h"
namespace idk
{

	template<typename Container>
	auto make_range(Container&& cont)
	{
		return range_over<decltype(std::begin(std::declval<Container>())), Container>{cont, std::begin(cont), std::end(cont)};
	}
	template<typename Container>
	auto reverse(Container&& cont)
	{
		return range_over<decltype(std::rbegin(std::declval<Container>())), Container>{cont, std::rbegin(cont), std::rend(cont)};
	}
}