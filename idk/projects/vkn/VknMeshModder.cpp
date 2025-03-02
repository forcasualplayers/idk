#include "pch.h"
#include "VknMeshModder.h"
#include <vkn/VulkanMesh.h>
#include <vkn/VulkanTypeDescriptors.h>
#include <vkn/VulkanState.h>
#include <vkn/VulkanView.h>
#include <vkn/VulkanWin32GraphicsSystem.h>

#include <core/Core.h>
#include <vkn/MemoryAllocator.h>


namespace idk::vkn
{
	namespace hlp
	{

		vk::UniqueCommandBuffer CreateCommandBuffer(vk::CommandPool command_pool, vk::Device device, vk::CommandBufferLevel cmd_level = vk::CommandBufferLevel::ePrimary)
		{
			//For RenderState
			//rss.resize(max_frames_in_flight);
			vk::CommandBufferAllocateInfo rs_alloc_info
			{
				command_pool
				,cmd_level
				,1
			};
			auto cmd_buffers = device.allocateCommandBuffersUnique(rs_alloc_info, vk::DispatchLoaderDefault{});
			return std::move(cmd_buffers[0]);
		}
		struct StagingStuff
		{
			vk::UniqueCommandBuffer cmd_buffer;
			vk::UniqueBuffer buffer;
			vk::UniqueDeviceMemory memory;
		};
		//Expensive (probably).
		StagingStuff TransferData(vk::CommandPool cmd_pool, vk::Queue queue, vk::PhysicalDevice pdevice, vk::Device device, size_t dst_offset, size_t num_bytes, const void* data, vk::Buffer dst_buffer, vk::Fence fence)
		{
			auto tmp_cmd_buffer = hlp::CreateCommandBuffer(cmd_pool, device);
			auto dispatcher = vk::DispatchLoaderDefault{};

			vk::DeviceSize bufferSize = num_bytes;

			auto [stagingBuffer, stagingBufferMemory] = hlp::CreateAllocBindBuffer(
				pdevice, device, bufferSize,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				dispatcher);

			{
				hlp::MapMemory(device, *stagingBufferMemory, dst_offset, data, bufferSize, dispatcher);
			}
			//m_vertex_buffers.emplace_back( std::move(instance_buffer));
			//m_vertex_memories.emplace_back(std::move(instance_memory));

			hlp::CopyBuffer(*tmp_cmd_buffer, queue, *stagingBuffer, dst_buffer, bufferSize,fence,false);
			return {
				std::move(tmp_cmd_buffer),
				std::move(stagingBuffer),
				std::move(stagingBufferMemory)
			};
		}

	}
	struct MeshModder::PImpl
	{
		vk::UniqueCommandPool cmd_pool_{};
		vk::UniqueFence copy_fence_{};
		PImpl()
		{
			vk::CommandPoolCreateInfo cpci
			{
				vk::CommandPoolCreateFlagBits::eTransient| vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				*View().QueueFamily().graphics_family
			};
			cmd_pool_ = View().Device()->createCommandPoolUnique(cpci);
			vk::FenceCreateInfo fci
			{
				
			};
			copy_fence_ = View().Device()->createFenceUnique(fci);
		}
	};
	MeshModder::MeshModder() :allocator{ *Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View().Device(),Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View().PDevice() }, pimpl_{std::make_unique<PImpl>()}
	{
		
	}
	extern std::vector<string> debug_messages;
	MeshModder::~MeshModder()
	{
	}
	std::shared_ptr<MeshBuffer::Managed> MeshModder::CreateBuffer(string_view raw_data)
	{
		auto& vview = Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View();
		//auto& pdevice = vview.PDevice();
		auto& m_device = vview.Device();
		auto num_vtx_bytes = hlp::buffer_size(raw_data);
		auto data = std::data(raw_data);
		std::unique_lock create_allock{ create_alloc_mutex };
		auto&& [pbuffer, palloc] = hlp::CreateAllocBindBuffer(vview.PDevice(), *vview.Device(), num_vtx_bytes, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, allocator, vview.Dispatcher());
		create_allock.unlock();
		vk::Result res;
		{
			auto& mtx = View().GraphicsTexMutex();
			std::lock_guard lock{ mtx };
			auto& fence = *pimpl_->copy_fence_;
			View().Device()->resetFences(fence);
			auto cmd_buffer = hlp::TransferData(*pimpl_->cmd_pool_, vview.GraphicsTexQueue(), vview.PDevice(), *m_device, 0, num_vtx_bytes, data, *pbuffer,fence);
			while((res=View().Device()->waitForFences(fence,true,0))==vk::Result::eTimeout);

		}
		return std::make_shared<MeshBuffer::Managed>(std::move(pbuffer), std::move(palloc), num_vtx_bytes);
	}

	void MeshModder::RegisterAttribs(VulkanMesh& mesh, const hash_table<attrib_index, std::pair<shared_ptr<MeshBuffer::Managed>, size_t>>& attribs)
	{
		for (auto& attrib : attribs)
		{
			mesh.SetBuffer(attrib.first, MeshBuffer{ attrib.second.first, attrib.second.second });
		}
	}

	void MeshModder::SetIndexBuffer(VulkanMesh& mesh, const vector<uint16_t>& index_buffer)
	{
		auto& vview = Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View();
		//auto& pdevice = vview.PDevice();
		auto& m_device = vview.Device();
		//auto& dispatcher = vview.Dispatcher();
		{
			auto num_vtx_bytes = hlp::buffer_size(index_buffer);
			auto data = std::data(index_buffer);
			auto&& [pbuffer, palloc] = hlp::CreateAllocBindBuffer(vview.PDevice(), *vview.Device(), num_vtx_bytes, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, allocator, vview.Dispatcher());
			{
				auto& mtx = View().GraphicsTexMutex();
				std::lock_guard lock{ mtx };
				auto& fence = *pimpl_->copy_fence_;
				View().Device()->resetFences(fence);
				auto cmd_buffer =hlp::TransferData(*vview.Commandpool(), vview.GraphicsTexQueue(), vview.PDevice(), *m_device, 0, num_vtx_bytes, data, *pbuffer,fence);
				while (View().Device()->waitForFences(fence, true, 0) == vk::Result::eTimeout);
			}
			mesh.SetIndexBuffer(
				MeshBuffer{ std::make_shared<MeshBuffer::Managed>(std::move(pbuffer),std::move(palloc) , num_vtx_bytes) },
				s_cast<uint32_t>(index_buffer.size()),
				vk::IndexType::eUint16
			);
		}
	}

	//Index buffer elements are 16 bytes

	void MeshModder::SetIndexBuffer16(VulkanMesh& mesh, shared_ptr<MeshBuffer::Managed> index_buffer, uint32_t num_indices)
	{
		[[maybe_unused]] auto& vview = Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View();
		//auto& pdevice = vview.PDevice();
		//auto& m_device = vview.Device();
		//auto& dispatcher = vview.Dispatcher();
		{
			mesh.SetIndexBuffer(
				MeshBuffer{ index_buffer },
				num_indices,
				vk::IndexType::eUint16
			);
		}
	}

	//Index buffer elements are 32 bytes

	void MeshModder::SetIndexBuffer32(VulkanMesh& mesh, shared_ptr<MeshBuffer::Managed> index_buffer, uint32_t num_indices)
	{
		[[maybe_unused]] auto& vview = Core::GetSystem<VulkanWin32GraphicsSystem>().Instance().View();
		//auto& pdevice = vview.PDevice();
		//auto& m_device = vview.Device();
		//auto& dispatcher = vview.Dispatcher();
		{
			mesh.SetIndexBuffer(
				MeshBuffer{ index_buffer },
				num_indices,
				vk::IndexType::eUint32
			);
		}
	}

}