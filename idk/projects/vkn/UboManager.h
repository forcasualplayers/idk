#pragma once
#include  <idk.h>
#include <vulkan/vulkan.hpp>
#include <vkn/VulkanView.h>
#include <meta/stl_hack.h>
#include <ds/index_span.h>
namespace idk::vkn
{
	template<typename T>
	using tallocator = std::allocator<T>;
	struct UboManager
	{
		using memory_idx_t = size_t;
		using buffer_idx_t = size_t;

		struct UboSection
		{
			vk::Buffer buffer;
			uint32_t buffer_offset; //offset from buffer's begin
			span<std::byte> data_buffer; //section of memory that will map directly to buffer.
		};

		UboManager(VulkanView& view = View());
		UboManager(UboManager&&) noexcept;

		uint32_t OffsetAlignment()const;
		uint32_t SizeAlignment()  const;

		template<typename T>
		std::pair<vk::Buffer, uint32_t> Add(const T& data);
		template<typename T>
		std::pair<vk::Buffer, uint32_t> Add(span<const T> data);

		UboSection Acquire(uint32_t num_bytes);
		
		void Update(vk::Buffer buffer, index_span range, string_view data);
		void Free(vk::Buffer buffer, uint32_t offset,uint32_t size);

		void UpdateAllBuffers();
		void Clear();
		~UboManager();
		constexpr static uint32_t              _chunk_size = 1 << 20;
	private:
		struct DataPair;
		struct Memory
		{
			vk::UniqueDeviceMemory memory;
			size_t size{}, capacity{};
			Memory(VulkanView& view, vk::Buffer& buffer, size_t capacity_);
			bool can_allocate(size_t chunk,size_t alignment)const;
			//Returns the offset to start from
			std::optional<size_t> allocate(size_t chunk,size_t alignment);
		};
		VulkanView& view;
		uint32_t                               _alignment = 0x16;
		size_t                                 _memory_chunk_size = 1 << 20; //Replace this with the limit obtained from device.
		//Maybe replace with allocator
		vector<Memory>                         _memory_blocks;

		hash_table<buffer_idx_t, memory_idx_t> _allocation_table;
		vector<DataPair>                       _buffers;
		size_t                                 _curr_buffer_idx{};



		size_t AllocateAndBind(vk::Buffer& buffer);

		std::pair<vk::UniqueBuffer,size_t> MakeBuffer();

		DataPair& FindPair(size_t size);
		std::pair<vk::Buffer, uint32_t> make_buffer_pair(DataPair& pair, size_t buffer_len, const void* data);
	};
	/*
	template<typename T>
	struct test_allocator
	{
		using const_pointer = const T*;
		using difference_type = ptrdiff_t;
		using pointer = T *;
		using size_type = size_t;
		using value_type = T;

		template<typename U>
		struct rebind
		{
			using other = test_allocator<U>;
		};
		template<typename ...Args>
		test_allocator(Args&& ...) noexcept {}

		static 	T* allocate(size_t sz)
		{
			return new T[sz + 32] + 11;

		}
		//const test_allocator& allocator()const { return *this; }

		static void deallocate(T* ptr, size_t)
		{
			delete[](ptr - 11);
		}
	};*/
}
MARK_NON_COPY_CTORABLE(idk::vkn::UboManager::DataPair)
MARK_NON_COPY_CTORABLE(idk::vkn::UboManager)
