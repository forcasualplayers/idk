#include "stdafx.h"
#include "Handle.h"
#include <core/IncludeComponents.h>
#include <reflect/reflect.h>

#undef GetObject

namespace idk
{
	namespace detail
	{
		template<typename T>
		struct GenericHandle_helper;

		template<typename ... Ts>
		struct GenericHandle_helper < std::tuple<Ts...> >
		{
			static auto GenerateDerefLUT()
			{
				using Fn = reflect::dynamic(*)(const GenericHandle & h);

				return array<Fn, sizeof...(Ts)>{
					[](const GenericHandle& h) -> reflect::dynamic
					{
						return reflect::dynamic{ *GameState::GetGameState().GetObject(r_cast<const Handle<Ts>&>(h)) };
					}...
				};
			}
		};

		struct GenericHandleHelper
		{
			static inline auto lut = GenericHandle_helper<Handleables>::GenerateDerefLUT();
		};
	}


	GenericHandle::GenericHandle(uint64_t id)
		: id{id}
	{
	}
	GenericHandle::GenericHandle(uint8_t type, uint8_t scene, uint16_t gen, uint32_t index)
		: type{ type }, scene{ scene }, gen{gen}, index{index}
	{
	}
	reflect::dynamic GenericHandle::operator*() const
	{
		if (operator bool())
			return detail::GenericHandleHelper::lut[type];
		else
			return reflect::dynamic();
	}
	GenericHandle::operator bool() const
	{
		return GameState::GetGameState().ValidateHandle(*this);
	}
	bool GenericHandle::operator==(const GenericHandle& rhs)
	{
		return id == rhs.id;
	}
	bool GenericHandle::operator!=(const GenericHandle& rhs)
	{
		return id != rhs.id;
	}
}