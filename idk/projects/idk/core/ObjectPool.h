#pragma once
#include <idk.h>
#include <core/Handle.h>
#include <ds/pool.h>

namespace idk
{
	template<typename T>
	class ObjectPool
	{
	public:
		using Handle = Handle<T>;
		using index_t = GenericHandle::index_t;
		using gen_t  = GenericHandle::gen_t;
		using scene_t = GenericHandle::scene_t;
		
		ObjectPool();
		~ObjectPool();

		// iterators
		span<T> GetSpan();

		// accessors
		bool   Validate(const Handle&);
		T*     Get(const Handle&);

		// modifiers
		template<typename ... Args>
		Handle Create(scene_t scene_id, Args&& ...);
		template<typename ... Args>
		Handle Create(const Handle&, Args&& ...);
		bool   Destroy(const Handle&);

		// defrags the list using insertion sort
		template<typename SortFn = std::less<T>>
		unsigned Defrag(SortFn&& functor = SortFn{});

		bool ActivateScene(scene_t scene_id, size_t reserve = 8192);
		bool DeactivateScene(scene_t scene_id);
		bool ValidateScene(scene_t scene_id);

	private:
		static constexpr index_t invalid = index_t{ 0xFFFFFFFF };
		struct Inflect
		{
			index_t index = invalid;
			gen_t  gen  = 0;
		};

		struct Map
		{
			vector<Inflect> slots;
			unsigned first_free = 0;
			void shift() { while (first_free != slots.size() && slots[first_free].index != invalid) ++first_free; }
			void grow() { slots.resize(slots.size() * 3 / 2); }
		};

		array<Map, MaxScene> _scenes;
		pool<T> _pool;
	};
}
