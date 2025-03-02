#include "pch.h"
#include "UboManager.h"
#include "BufferHelpers.h"
#include <string>

#include <vkn/Stopwatch.h>

#include <vkn/MemoryCollator.h>

#include <core/Core.inl>
#include <parallel/ThreadPool.h>

namespace idk::vkn
{
	struct UboManager::DataPair
	{
		vk::UniqueBuffer buffer{};
		std::basic_string<char, std::char_traits<char>, tallocator<char>> data{};
		size_t initial_offset{};
		size_t block_size;
		MemoryCollator collator;
		uint32_t alignment{};
		uint32_t sz_alignment{};
		bool resetted = false;
		size_t old_alignment = 0;
		bool dirty = false;

		//DataPair() = default;
		//DataPair(DataPair&&) noexcept = default;
		//DataPair(const DataPair&) = delete;
		bool CanAdd(size_t len)const;
		size_t AlignmentOffset()const;
		void Align();
		uint32_t Add(size_t len, const void* data_);
		uint32_t allocate(size_t len);
		void Free(uint32_t offset, size_t len);
		vk::Buffer& Buffer() { return *buffer; }
		span<std::byte> DataBuffer() {
			return span
			{
				reinterpret_cast<std::byte*>(data.data() + initial_offset),
				reinterpret_cast<std::byte*>(data.data() + data.size())
			};
		}

		void Reset();
	};
	namespace dbg
	{
		hash_table<string_view, float>& get_rendertask_durations();
		void add_rendertask_durations(string_view name, float duration);

	}
	using dbg::add_rendertask_durations;



	UboManager::Memory::Memory(VulkanView& view, vk::Buffer& buffer, size_t capacity_) :capacity{ capacity_ }
	{
		auto req = view.Device()->getBufferMemoryRequirements(buffer, view.Dispatcher());
		memory = hlp::AllocateBuffer(
			view.PDevice(), *view.Device(), buffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			view.Dispatcher()
		);
	}

	bool UboManager::Memory::can_allocate(size_t chunk,size_t alignment) const {
		return(Aligned(size,alignment) + chunk <= capacity);
	}

	//Returns the offset to start from

	std::optional<size_t> UboManager::Memory::allocate(size_t chunk,size_t alignment)
	{
		std::optional<size_t>result{};
		if (can_allocate(chunk,alignment))
		{
			result = Aligned(size,alignment);
			size = *result + chunk;
		}
		return result;
	}
//#pragma optimize("",off)
	size_t UboManager::AllocateAndBind(vk::Buffer& buffer)
	{
		auto req = View().Device()->getBufferMemoryRequirements(buffer);
		
		if (!_memory_blocks.size() || !_memory_blocks.back().can_allocate(_chunk_size,req.alignment))
		{
			_memory_blocks.emplace_back(view, buffer, _memory_chunk_size);
		}
		auto& memory = _memory_blocks.back();
		uint32_t offset = s_cast<uint32_t>(*memory.allocate(_chunk_size,req.alignment));

		hlp::BindBufferMemory(*view.Device(), buffer, *memory.memory, offset, view.Dispatcher());
		return offset;
	}

	std::pair<vk::UniqueBuffer,size_t> UboManager::MakeBuffer()
	{
		auto buffer = hlp::CreateBuffer(*view.Device(), _chunk_size, vk::BufferUsageFlagBits::eUniformBuffer, view.Dispatcher());
		;
		return std::make_pair( std::move(buffer),AllocateAndBind(*buffer));
	}

	UboManager::DataPair& UboManager::FindPair(size_t size)
	{
		dbg::stopwatch timer;
		timer.start();
		while (_buffers.size() > _curr_buffer_idx && !_buffers[_curr_buffer_idx].CanAdd(size))
		{
			++_curr_buffer_idx;
		}
		if (_buffers.size() <= _curr_buffer_idx)
		{
			auto&& [buffer, offset] = MakeBuffer();
			decltype(DataPair::data) buf{};
			buf.reserve(_chunk_size+_alignment);
			auto& tmp = _buffers.emplace_back(DataPair{ std::move(buffer),std::move(buf),offset,_chunk_size,MemoryCollator{index_span{0,_chunk_size}} });
			tmp.alignment = OffsetAlignment();
			tmp.sz_alignment = SizeAlignment();
			_allocation_table.emplace(_buffers.size() - 1,_memory_blocks.size() - 1);
			tmp.Reset();
		}
		timer.stop();
		add_rendertask_durations("Ubo Find Pair", timer.time().count());
		return _buffers[_curr_buffer_idx];
	}

	UboManager::UboManager(VulkanView& view_) : view{view_}
	{
		_alignment = view.BufferOffsetAlignment();
	}

	UboManager::UboManager(UboManager&&) noexcept = default;


	//UboManager& UboManager::operator=(const UboManager&) = default;

	//UboManager& UboManager::operator=(UboManager&&) = default;


	uint32_t UboManager::OffsetAlignment()const { return view.BufferOffsetAlignment(); }

	uint32_t UboManager::SizeAlignment() const{ return view.BufferSizeAlignment(); }


	uint32_t InitialOffset(const void* ptr, uint32_t alignment)
	{
		uint32_t mod = s_cast<uint32_t>((r_cast<intptr_t>(ptr) % alignment));
		return (mod) ? alignment - mod : 0;
	}
	UboManager::UboSection UboManager::Acquire(uint32_t num_bytes)
	{
		dbg::stopwatch timer;
		timer.start();
		auto size = num_bytes;
		DataPair& pair = FindPair(size);
		timer.stop();
		add_rendertask_durations("Ubo Add-FindPair", timer.time().count());
		auto offset = pair.allocate(num_bytes);
		return UboSection{ pair.Buffer(),offset,pair.DataBuffer().subspan(offset,num_bytes)};
	}
	void UboManager::Update(vk::Buffer buffer, index_span range, string_view data)
	{
		for (auto& dp : this->_buffers)
		{
			if (dp.Buffer() == buffer)
			{
				dp.dirty = true;
				memcpy_s(dp.data.data()+dp.initial_offset+range._begin, dp.data.size()-range._begin, data.data(),data.size());
				return;
			}
		}
	}
	void UboManager::Free(vk::Buffer buffer, uint32_t offset, uint32_t size)
	{
		//Find the datapair with the buffer
		for (auto& dp: this->_buffers)
		{
			if (dp.Buffer() == buffer)
			{
				dp.Free(offset, size);
				return;
			}
		}
		//free it.
	}
	//TODO: Try having a set of buffers that are persistantly mapped instead.
	void UboManager::UpdateAllBuffers()
	{
		std::vector<mt::Future<void>> futures;
		for (auto& [buffer_idx,memory_idx] : _allocation_table)
		{
			//futures.emplace_back(Core::GetThreadPool().Post([&]() {
			auto& block = _memory_blocks[memory_idx];
			auto& memory = block.memory;
			auto& buffer = _buffers[buffer_idx];
			auto initial_offset = buffer.initial_offset;//InitialOffset(buffer.data.data(), _alignment);


			if (buffer.data.size() && buffer.dirty)
			{
				auto dst_size = block.size ;
				auto src_size = buffer.data.size() - initial_offset;
				IDK_ASSERT(src_size <= dst_size);
				hlp::MapMemory(*view.Device(), *memory, /*buffer.initial_offset*/0, std::data(buffer.data) + initial_offset, dst_size, view.Dispatcher());
				buffer.dirty = false;
			}
			//	}));
		}
		for (auto& future : futures)
		{
			future.get();
		}
	}

	struct memcpy_info
	{
		size_t total_bytes_copied = 0;
		//numbytes,num occurences
		hash_table<size_t, size_t> num_occurences;
		void log(size_t num_bytes)
		{
			return;
			num_occurences[num_bytes]++;
			total_bytes_copied += num_bytes;
		}
		void reset()
		{
			return;
			total_bytes_copied = 0;
			for (auto& [bytes, num] : num_occurences)
			{
				num = 0;
			}
		}
	};
	static memcpy_info mem_info;
	void UboManager::Clear()
	{
		mem_info.reset();
		for (auto& data_pair : _buffers)
		{
			data_pair.Reset();
		}
		_curr_buffer_idx = 0;
	}
	UboManager::~UboManager() = default;
	uint32_t SizeAlignmentOffset(size_t sz, size_t alignment)
	{
		return s_cast<uint32_t>(sz + ((sz % alignment) ? alignment - (sz % alignment) : 0));
	}

	uint32_t aaaa2 = 0;
	bool UboManager::DataPair::CanAdd(size_t len) const
	{
		aaaa2++;
		if (data.size() > block_size+initial_offset)
			throw std::runtime_error("data buffer overflowed.");
		return collator.can_allocate(Aligned(len, sz_alignment), alignment);
		return block_size >= 
			data.size() + AlignmentOffset() +
			SizeAlignmentOffset(len,sz_alignment);
	}

	size_t UboManager::DataPair::AlignmentOffset() const
	{
		auto mod = (r_cast<intptr_t>(data.data()) + data.size()) % alignment;
		return (mod) ? alignment - (mod) : 0;
	}

	void UboManager::DataPair::Align()
	{
		auto padding = std::min(block_size-data.size(),AlignmentOffset());
		data.append(padding, 0);
	}
	bool operator==(index_span lhs, index_span rhs)
	{
		return lhs._begin == rhs._begin && lhs._end == rhs._end;
	}

	void alt_memcpy(void* dest, const void* data, size_t len)
	{
		using elem_t = size_t;
		constexpr size_t cacheline_sz = 64;
		constexpr size_t stride = sizeof(elem_t);
		constexpr size_t cacheline_stride = cacheline_sz/stride;
		size_t num_elems = len / stride;
		const elem_t* ptr = (const elem_t*)data;
		const elem_t* end = ptr + num_elems-cacheline_stride;
		elem_t* dst = (elem_t*)dest;
		while(ptr<end)
		{
			PrefetchForWrite(dst + cacheline_stride);
			PreFetchCacheLine(PF_NON_TEMPORAL_LEVEL_ALL,ptr + cacheline_stride);
			*(dst+0) = (*ptr+0);
			*(dst + 1) = (*ptr + 1);
			*(dst + 2) = (*ptr + 2);
			*(dst + 3) = (*ptr + 3);
			*(dst + 4) = (*ptr + 4);
			*(dst + 5) = (*ptr + 5);
			*(dst + 6) = (*ptr + 6);
			*(dst + 7) = (*ptr + 7);
			ptr+=8;
			dst+=8;
		}
		auto d2 = (byte*)dst;
		auto src = (const byte*)ptr;
		auto end2 = ((const byte*)data) + len;
		while (src < end2)
		{
			*d2 = *src;
			++src;
			++d2;
		}
	}
	uint32_t UboManager::DataPair::Add(size_t len, const void* data_) 
	{
		auto aligned_offset=allocate(len);
		auto actual_offset = initial_offset + aligned_offset;
#if DEBUG_TIMER
		add_rendertask_durations("DP pre memcpy", timer.lap().count());
#endif
		memcpy_s(data.data() +actual_offset, data.size()-actual_offset, data_, len);
		mem_info.log(len);
#if DEBUG_TIMER
		timer.stop();
		add_rendertask_durations("DP memcpy", timer.lap().count());
		add_rendertask_durations("DP Add", timer.time().count());
#endif
		dirty = true;
		return static_cast<uint32_t>(aligned_offset);
	}

	uint32_t UboManager::DataPair::allocate(size_t len)
	{
#if DEBUG_TIMER
		dbg::stopwatch timer;
		timer.start();
#endif
		if (!resetted || alignment != old_alignment)
			throw std::runtime_error("Attempting to reallocate before resetting.");
		collator.skip_free = true;
		auto opt = collator.allocate(Aligned(len, sz_alignment), alignment);
#if DEBUG_TIMER
		add_rendertask_durations("DP collator allocate", timer.lap().count());
#endif
		if (!opt)
			throw std::runtime_error("Failed to allocate from collator");
		auto [unaligned_offset, aligned_offset] = *opt;
		//free the alignment stuff so that we just need to free the allocated block later
		if (aligned_offset - unaligned_offset != 0)
			collator.mark_freed({ unaligned_offset,aligned_offset });
#if DEBUG_TIMER
		add_rendertask_durations("DP mark free", timer.lap().count());
#endif
		return static_cast<uint32_t>(aligned_offset);
	}

	void UboManager::DataPair::Free(uint32_t offset, size_t len)
	{
		collator.mark_freed(index_span{ offset,offset + len });
	}

	void UboManager::DataPair::Reset() 
	{
		resetted = true;
		collator.reset();
		data.reserve(block_size + alignment);
		initial_offset = InitialOffset(data.data(), alignment);
		data.resize(block_size + initial_offset);
		old_alignment = alignment;
		dirty = false;
	}

	std::pair<vk::Buffer, uint32_t> UboManager::make_buffer_pair(DataPair& pair, size_t buffer_len, const void* data)
	{
		return std::make_pair(pair.Buffer(), pair.Add(buffer_len, data));
	}
}