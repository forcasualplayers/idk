#pragma once
#include <idk.h>
#include <vulkan/vulkan.hpp>
#include <vkn/vulkan_state_fwd.h>
namespace idk::vkn
{
	class VulkanState;
	struct RenderState;
	//Interface for the vulkan details. 
	//Ideally we should move all of the vk:: related data and into VulkanDetail
	//Effectively PIMPLing the vk stuff
	class VulkanView
	{
	public:
		vk::DispatchLoaderDefault& Dispatcher()const;
		vk::DispatchLoaderDynamic& DynDispatcher()const;
		vk::UniqueInstance&        Instance()const;
		vk::UniqueSurfaceKHR&      Surface()const;
		vk::PhysicalDevice&        PDevice()const;
		uint32_t                   BufferOffsetAlignment()const;
		uint32_t                   BufferSizeAlignment()const;
		vk::UniqueDevice&          Device()const;
		QueueFamilyIndices&        QueueFamily()const;
		vk::Queue&                 GraphicsQueue()const;
		vk::Queue&                 PresentQueue()const;
		SwapChainInfo&             Swapchain()const;

		vk::UniquePipeline&        Pipeline()const;
		vk::UniqueCommandPool&     Commandpool()const;
		vector<vk::UniqueCommandBuffer>& Commandbuffers()const;

		//Render State info
		void                       SwapRenderState()const;//Probably a bad decision to const this shit.
		vector<RenderState>&       RenderStates()const;
		RenderState&               PrevRenderState()const;
		RenderState&               CurrRenderState()const;
		vk::UniqueRenderPass&      Renderpass()const;
		vk::UniqueCommandBuffer&   CurrCommandbuffer()const;
		vk::Buffer&                CurrMasterVtxBuffer()const;
		//Copies the data into the master buffer and returns the offset to start from.
		uint32_t                   AddToMasterBuffer(const void* data, uint32_t len)const;
		void                       ResetMasterBuffer()const;
		bool&					   ImguiResize()const;
		window_info&			   GetWindowsInfo()const;



		vk::UniqueShaderModule     CreateShaderModule(const string_view& code);

		void WaitDeviceIdle() const;

		VulkanView(VulkanState& vulkan);
		VulkanView(VulkanView&&) noexcept = default;
		VulkanView& operator=(VulkanView&&) noexcept = default;
		~VulkanView();
	private:
		struct pimpl;
		std::unique_ptr<pimpl> impl_;
		VulkanState& vulkan() const;
		VulkanState* vulkan_;
	};
}