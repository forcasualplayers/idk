#pragma once
#include <idk.h>
#include <forward_list>
#include <idk/memory/ArenaAllocator.inl>
#include <vkn/FreeList.h>

namespace idk
{
	struct arena_block_free;
	void free_block(arena_block_free* ptr);
	struct arena_block_free
	{
		using ctrl_block_ptr_t = std::shared_ptr<arena_block_free>;
		template<typename T>
		static std::pair<T*, T*> emplace_align(void* ptr)
		{
			auto aligned_ptr = align<T>(ptr);
			auto start = aligned_ptr + 1;
			return { aligned_ptr,start };
		}
		static ctrl_block_ptr_t init(unsigned char* ptr, size_t bytes)
		{
			//auto [sptr, next] = emplace_align<ctrl_block_ptr_t>(ptr);
			auto [aligned, start] = emplace_align <arena_block_free>(ptr);
			new (aligned) arena_block_free{ start, bytes - static_cast<size_t>(reinterpret_cast<unsigned char*>(start) - ptr) };
			return ctrl_block_ptr_t{ aligned ,[](arena_block_free* ptr) {ptr->~arena_block_free(); } };
			;
			//{
			//
			//	aligned_ptr = align<ctrl_block_ptr_t>(start);
			//	auto start = aligned_ptr + 1;
			//	new (aligned_ptr) arena_block_free{ start, bytes - static_cast<size_t>(reinterpret_cast<unsigned char*>(start) - ptr) };
			//}
			//
			//return aligned_ptr;
		}
		unsigned char* arena{};
		unsigned char* next{};
		unsigned char* rnext{};
		unsigned char* end{};
		bool use_reverse = false;
		template<typename Y>
		arena_block_free(Y* pool, size_t bytes) noexcept :
			arena{ reinterpret_cast<unsigned char*>(pool) },
			next{ arena },
			rnext{ arena + bytes },
			end{ arena + bytes }
		{

		}
		template<typename T>
		T* plain_alloc(size_t num_bytes)
		{
			if (use_reverse)
			{
				//assert(rnext);
				auto start = rnext - (num_bytes + alignof(T));
				auto result = align<T>(start);
				auto new_next = reinterpret_cast<unsigned char*>(result);

				if (new_next < next)
				{
					DebugBreak();
					return nullptr;
				}
				auto offset = start - arena;
				auto align_padding = reinterpret_cast<unsigned char*>(result) - start;
				_free_list.Free(offset + num_bytes + align_padding, alignof(T) - align_padding);
				rnext = new_next;
				use_reverse = !use_reverse;
				return result;
			}
			else
			{

				auto result = align<T>(next);
				auto new_next = reinterpret_cast<unsigned char*>(result) + num_bytes;

				if (new_next > rnext)
				{
					DebugBreak();
					return nullptr;
				}
				auto offset = next - arena;
				auto align_padding = reinterpret_cast<unsigned char*>(result) - next;
				if (align_padding)
					_free_list.Free(offset, align_padding);
				next = new_next;
				use_reverse = !use_reverse;
				return result;

			}
		}


		template<typename T>
		T* try_allocate(size_t num_bytes)
		{
			auto offset = _free_list.Allocate(num_bytes + alignof(T));
			if (offset > static_cast<size_t>(end - arena))
			{
				return plain_alloc<T>(num_bytes);
			}
			auto unaligned = arena + offset;
			auto res = align<T>(unaligned);
			auto align_padding = reinterpret_cast<unsigned char*>(res) - unaligned;
			_free_list.Free(offset + align_padding + num_bytes, alignof(T) - align_padding);
			_free_list.Free(offset, align_padding);
			return res;
		}
		unsigned char* pop_boundary(unsigned char* boundary, ptrdiff_t sign)
		{

			auto bound = _free_list.PopBoundary(boundary - arena);
			if (bound)
			{
				boundary -= sign * bound->second;
			}
			return boundary;
		}
		template<typename T>
		bool try_release(T* ptr, size_t N) noexcept
		{
			auto cptr = reinterpret_cast<unsigned char*>(ptr);
			bool release = cptr + N == next;
			if (release)
				next = cptr;
			else if (arena <= cptr && cptr < end)
			{
				_free_list.Free(cptr - arena, N);
				next = pop_boundary(next, 1);
				rnext = pop_boundary(rnext, -1);
				release = true;
			}

			return release;
		}
		FreeList _free_list;
	};
}

namespace idk::vkn
{




	namespace detail
	{
		template<typename T, size_t N>
		static constexpr size_t num_bytes = std::max(sizeof(T) * N, sizeof(size_t) * N);
	}
	bool some_func();
	namespace detail
	{
		struct InternalBase
		{
			span<char> buffer{};
		};
		template<size_t NumBytes>
		struct FixedInternal :InternalBase
		{
			std::array<char, NumBytes> _buffer;
			FixedInternal() :InternalBase{ {std::data(_buffer),std::data(_buffer) + std::size(_buffer)} } { }
			~FixedInternal() { }
		};
	}
	template<typename proxy_t>
	struct AllocatorProxy
	{
		using ptr_t = typename proxy_t::ptr_t;
		using value_type = typename proxy_t::value_type;
		using size_type = typename proxy_t::size_type;
		using difference_type = typename proxy_t::difference_type;
		using reference = typename proxy_t::reference;
		using const_reference = typename proxy_t::const_reference;
		using pointer = typename proxy_t::pointer;
		using const_pointer = typename proxy_t::const_pointer;

		proxy_t* proxy_ptr;

		ptr_t allocate(size_t n)
		{
			return proxy_ptr->allocate(n);
		}

		void deallocate(ptr_t ptr, size_t N)noexcept
		{
			return proxy_ptr->deallocate(ptr, N);
		}
		template<typename A>
		bool operator==(const A& rhs)const noexcept
		{
			return proxy_ptr->operator==(rhs);
		}
		bool operator!=(const proxy_t& rhs)const noexcept
		{
			return proxy_ptr->operator!=(rhs);
		}

	};

	template<typename T, size_t NumBytes = 1 << 20, typename proxy_t = ArenaAllocator<T, arena_block_free>, typename base_t = AllocatorProxy<proxy_t>>
	struct FixedArenaAllocator : base_t
	{
		;
		template<typename T>
		static ArenaAllocator<T>& GetAllocatorBase(ArenaAllocator<T>& base) noexcept
		{
			return base;
		}

		using Internal = detail::FixedInternal<NumBytes>;
		template<typename U>
		struct rebind
		{
			using other = FixedArenaAllocator<U, NumBytes>;
		};
		std::shared_ptr<detail::InternalBase> _internal;
		proxy_t allocator;
		template<typename U, typename Base>
		bool operator==(const FixedArenaAllocator<U, NumBytes, Base>& rhs) noexcept
		{
			return *_internal == *rhs._internal;
		}
		//alignas(T) std::array<char, sizeof(T) * 2048> buffer;
		FixedArenaAllocator() :
			base_t{ &allocator },
			_internal{ std::make_shared<Internal>() },
			allocator{ _internal->buffer }
		{
			//_internal = std::make_shared<Internal>();
			//proxy_t* ptr = &allocator;
			//new (ptr) ;
			//this->proxy_ptr = ptr;
		}
		template<typename U, size_t N>
		FixedArenaAllocator(const FixedArenaAllocator<U, N>& rhs)
			: base_t{ &allocator },
			_internal{ rhs._internal },
			allocator{ rhs.allocator }
		{

		}
		FixedArenaAllocator(const FixedArenaAllocator& rhs)
			: base_t{ &allocator },
			_internal{ rhs._internal },
			allocator{ rhs.allocator }
		{

		}
		template<
			typename U,
			typename =
			std::enable_if_t<!std::is_same_v<FixedArenaAllocator<T, NumBytes>, FixedArenaAllocator<U, NumBytes> >>
		>
			FixedArenaAllocator(const FixedArenaAllocator<U, NumBytes>& rhs) :
			base_t{ &allocator },
			_internal{ rhs._internal },
			allocator{ rhs.allocator }
		{
		}

		FixedArenaAllocator& operator=(const FixedArenaAllocator&) = delete;
		template<
			typename U,
			typename =
			std::enable_if_t<!std::is_same_v<FixedArenaAllocator<T, NumBytes>, FixedArenaAllocator<U, NumBytes> >>
		>
			FixedArenaAllocator& operator=(const FixedArenaAllocator<U, NumBytes>&) = delete;
		~FixedArenaAllocator() = default;
	};
}