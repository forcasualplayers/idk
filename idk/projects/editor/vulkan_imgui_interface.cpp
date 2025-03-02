#include "pch.h"
#include "vulkan_imgui_interface.h"

#include <win32/WindowsApplication.h>

#include <vkn/VulkanState.h>
#include <vkn/VulkanView.h>
#include <vkn/BufferHelpers.h>

//Imgui
#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_win32.h>

#include <iostream> //std::cerr

namespace idk
{
	static void check_vk_result(VkResult err)
	{
		if (err == 0) return;
		printf("VkResult %d\n", err);
		if (err < 0)
			abort();
	}
	static void check_vk_result(vk::Result err)
	{
		if (err == vk::Result::eSuccess) return;
		printf("VkResult %d\n", err);
		if (err != vk::Result::eSuccess)
			abort();
	}


	vulkan_imgui_interface::vulkan_imgui_interface(vkn::VulkanState* v)
		: vkObj{ v }
	{}

	void vulkan_imgui_interface::Init()
	{
		//Creation
		vkn::VulkanView& vknViews = vkObj->View();

		editorInit.edt_imageCount = vkn::hlp::arr_count(vknViews.Swapchain().m_swapchainGraphics.image_views);
		editorControls.edt_imageCount = editorInit.edt_imageCount;
		editorControls.edt_frameIndex;
		editorControls.edt_semaphoreIndex;


		//New changes
		editorControls.edt_buffer = std::make_shared<vkn::TriBuffer>(vknViews, true);
		editorControls.edt_buffer->CreateImagePool(vknViews);
		//editorControls.edt_buffer->CreateImageViewWithCurrImgs(vknViews);
		//editorControls.edt_buffer->images = vknViews.Swapchain().m_swapchainGraphics.images;
		//editorControls.edt_buffer->CreateImageViewWithCurrImgs(vknViews);
		//editorControls.edt_buffer->CreatePresentationSignals(vknViews);
		editorControls.edt_buffer->enabled = true;

		////vknViews.Swapchain().m_inBetweens.emplace_back(editorControls.edt_buffer);
		//

		editorInit.edt_min_imageCount = 2;
		editorInit.edt_pipeCache;

		//Setup vulkan window for imgui
		ImGuiRecreateSwapChain();
		ImGuiRecreateCommandBuffer();

		ImGui_ImplVulkan_InitInfo info{};
		info.Instance = *vknViews.Instance();
		info.PhysicalDevice = vknViews.PDevice();
		info.Device = *vknViews.Device();

		info.QueueFamily = *vknViews.QueueFamily().graphics_family;
		info.Queue = vknViews.GraphicsQueue();

		info.PipelineCache = VK_NULL_HANDLE;

		vk::DescriptorPoolSize pSizes[] =
		{
			{ vk::DescriptorType::eSampler, 1000 },
			{ vk::DescriptorType::eCombinedImageSampler, 1000 },
			{ vk::DescriptorType::eSampledImage, 1000 },
			{ vk::DescriptorType::eStorageImage, 1000 },
			{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
			{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
			{ vk::DescriptorType::eUniformBuffer, 1000 },
			{ vk::DescriptorType::eStorageBuffer, 1000 },
			{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
			{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
			{ vk::DescriptorType::eInputAttachment, 1000 }
		};
		vk::DescriptorPoolCreateInfo pInfo{
			vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			1000 * vkn::hlp::arr_count(pSizes),
			vkn::hlp::arr_count(pSizes),
			pSizes
		};
		//info.DescriptorPool = *(vknViews.Device()->createDescriptorPoolUnique(pInfo, nullptr, vknViews.Dispatcher()));
		VkResult res = vkCreateDescriptorPool(*vknViews.Device(), &s_cast<VkDescriptorPoolCreateInfo&>(pInfo), nullptr, &info.DescriptorPool);
		res;
		info.Allocator = nullptr;
		info.MinImageCount = editorInit.edt_min_imageCount;
		info.ImageCount = editorInit.edt_imageCount;
		info.CheckVkResultFn = check_vk_result;

		//IMGUI setup
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io;

		//Platform/renderer bindings
		ImGui_ImplWin32_Init(vknViews.GetWindowsInfo().wnd);

		Core::GetSystem<Windows>().PushWinProcEvent(ImGui_ImplWin32_WndProcHandler);

		ImGui_ImplVulkan_Init(&info, *(vknViews.Renderpass()));

		editorControls.im_clearColor = vec4{ 0,0,0,1 };

		//Upload fonts leave it to later
		// Upload Fonts
	}

	void vulkan_imgui_interface::Shutdown()
	{
		ImGuiCleanUpSwapChain();

		editorInit.edt_pipeCache.reset();
		editorControls.edt_buffer.reset();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void vulkan_imgui_interface::ImGuiFrameBegin()
	{
		bool& resize = vkObj->View().ImguiResize();
		if (resize)
		{
			resize = false;
			ImGui_ImplVulkan_SetMinImageCount(editorInit.edt_min_imageCount);
			ImGuiRecreateSwapChain();
			ImGuiRecreateCommandBuffer();
		}

		ImGui_ImplWin32_NewFrame();
		if (!font_initialized)
		{
			vkn::VulkanView& vknViews = vkObj->View();
			// Use any command queue
			vk::CommandBuffer& command_buffer = *editorControls.edt_frames[editorControls.edt_frameIndex].edt_cBuffer;

			vknViews.Device()->resetCommandPool(*editorControls.edt_frames[editorControls.edt_frameIndex].edt_cPool, vk::CommandPoolResetFlags::Flags(), vknViews.Dispatcher());
			vk::CommandBufferBeginInfo begin_info = {};
			begin_info.flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

			command_buffer.begin(begin_info, vknViews.Dispatcher());

			ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

			vk::SubmitInfo end_info = {};
			end_info.commandBufferCount = 1;
			end_info.pCommandBuffers = &command_buffer;
			//err = vkEndCommandBuffer(command_buffer);
			//check_vk_result(err);
			command_buffer.end(vknViews.Dispatcher());

			//m_device->waitForFences(1, &*current_sc_signal.inflight_fence, VK_TRUE, std::numeric_limits<uint64_t>::max(), dispatcher);

			//err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
			//check_vk_result(err);
			vknViews.GraphicsQueue().submit(1, &end_info, vk::Fence(), vknViews.Dispatcher());


			//vknViews.Device()->waitForFences(1, &*editorControls.edt_buffer->pSignals[vknViews.CurrSemaphoreFrame()].inflight_fence, VK_TRUE, std::numeric_limits<uint64_t>::max(), vknViews.Dispatcher());

			//vknViews.Device()->resetFences(1, &*editorControls.edt_buffer->pSignals[vknViews.CurrSemaphoreFrame()].inflight_fence,vknViews.Dispatcher());
			//err = vkDeviceWaitIdle(g_Device);
			//check_vk_result(err);
			vknViews.Device()->waitIdle(vknViews.Dispatcher());
			ImGui_ImplVulkan_DestroyFontUploadObjects();
			font_initialized = true;
		}
		ImGui::NewFrame();
	}

	void vulkan_imgui_interface::ImGuiFrameUpdate()
	{
		//vkn::VulkanView& vknViews = vkObj->View();

		bool& resize = vkObj->View().ImguiResize();
		if (resize)
		{
			resize = false;
			ImGui_ImplVulkan_SetMinImageCount(editorInit.edt_min_imageCount);
			ImGuiRecreateSwapChain();
			ImGuiRecreateCommandBuffer();
		}

		ImGuiFrameBegin();

		//////////////////////////IMGUI DATA HANDLING//////////////////////
		if (editorControls.im_demoWindow)
			ImGui::ShowDemoWindow(&editorControls.im_demoWindow);

		static float f = 0.0f;
		static int counter = 0;


		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &editorControls.im_demoWindow);      // Edit bools storing our window open/close state

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&editorControls.im_clearColor); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
		///////////////////////////////END///////////////////////////////

		//ImGuiFrameEnd();
	}

	void vulkan_imgui_interface::ImGuiFrameEnd()
	{
		//memcpy(&editorControls.edt_clearValue.color.float32[0], &editorControls.im_clearColor, 4 * sizeof(float));
		//ImGuiFrameRender();
		//ImGuiFramePresent();
	}

	void vulkan_imgui_interface::ImGuiFrameRender()
	{
		//Vulk interface params
		vkn::VulkanView& vknViews = vkObj->View();

		if (vknViews.ImguiResize())
			ImGuiResizeWindow();
		else
		{
			ImGui::Render();
			memcpy(&editorControls.edt_clearValue.color.float32[0], &editorControls.im_clearColor, 4 * sizeof(float));


			//To get result of certain vulk operation
			vk::Result err;


			//Retrieving the semaphore info for the current imgui frame 
			//vk::Semaphore image_acquired_semaphore = *editorControls.edt_frameSemophores[editorControls.edt_semaphoreIndex].edt_imageAvailable;
			//vk::Semaphore render_complete_semaphore = *editorControls.edt_frameSemophores[editorControls.edt_semaphoreIndex].edt_renderFinished;
			vk::Semaphore& render_complete_semaphore = *editorControls.edt_buffer->pSignals[vknViews.CurrSemaphoreFrame()].render_finished;
			//vk::Semaphore& wait1_semaphore = *vknViews.GetCurrentSignals().image_available;
			//vk::Semaphore& wait2_semaphore = *vknViews.GetCurrentSignals().render_finished;

			//auto result = vknViews.Device()->acquireNextImageKHR(*vknViews.Swapchain().swap_chain, std::numeric_limits<uint32_t>::max(), image_acquired_semaphore, {},vknViews.Dispatcher());

			editorControls.edt_frameIndex = vknViews.AcquiredImageValue();
			editorControls.edt_semaphoreIndex = vknViews.CurrSemaphoreFrame();


			EditorFrame* fd = &(editorControls.edt_frames[editorControls.edt_frameIndex]);
			{
				err = vknViews.Device()->waitForFences(1, &*fd->edt_fence, VK_TRUE, std::numeric_limits<uint64_t>::max(), vknViews.Dispatcher());
				check_vk_result(err);

				err = vknViews.Device()->resetFences(1, &*fd->edt_fence, vknViews.Dispatcher());
				check_vk_result(err);
			}
			//Reset command pool for imgui's own cPool
			{
				vknViews.Device()->resetCommandPool(*fd->edt_cPool, vk::CommandPoolResetFlags::Flags(), vknViews.Dispatcher());

				vk::CommandBufferBeginInfo info = {};
				info.flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
				fd->edt_cBuffer->begin(info, vknViews.Dispatcher());

				vk::ImageSubresourceRange imgRange = {};
				imgRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				imgRange.levelCount = 1;
				imgRange.layerCount = 1;
				// Note: previous layout doesn't matter, which will likely cause contents to be discarded
				vk::ImageMemoryBarrier presentToClearBarrier = {};
				presentToClearBarrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
				presentToClearBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				presentToClearBarrier.oldLayout = vk::ImageLayout::eUndefined;
				presentToClearBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				presentToClearBarrier.srcQueueFamilyIndex = *vknViews.QueueFamily().graphics_family;
				presentToClearBarrier.dstQueueFamilyIndex = *vknViews.QueueFamily().graphics_family;
				presentToClearBarrier.image = fd->edt_backbuffer;
				presentToClearBarrier.subresourceRange = imgRange;

				// Change layout of image to be optimal for presenting
				vk::ImageMemoryBarrier clearToPresentBarrier = {};
				clearToPresentBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				clearToPresentBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
				clearToPresentBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				clearToPresentBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
				clearToPresentBarrier.srcQueueFamilyIndex = *vknViews.QueueFamily().graphics_family;
				clearToPresentBarrier.dstQueueFamilyIndex = *vknViews.QueueFamily().graphics_family;
				clearToPresentBarrier.image = fd->edt_backbuffer;
				clearToPresentBarrier.subresourceRange = imgRange;

				fd->edt_cBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, nullptr, nullptr, presentToClearBarrier, vknViews.Dispatcher());
				fd->edt_cBuffer->clearColorImage(fd->edt_backbuffer, vk::ImageLayout::eTransferDstOptimal, editorControls.edt_clearValue.color, imgRange, vknViews.Dispatcher());
				fd->edt_cBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags{}, nullptr, nullptr, clearToPresentBarrier, vknViews.Dispatcher());
			}
			//Begin renderpass for imgui cbuffer
			{
				vk::RenderPassBeginInfo info{};
				info.renderPass = *editorControls.edt_renderPass;
				info.framebuffer = *fd->edt_framebuffer;
				info.renderArea.extent.width = vknViews.Swapchain().extent.width;
				info.renderArea.extent.height = vknViews.Swapchain().extent.height;
				info.clearValueCount = 1;
				info.pClearValues = &editorControls.edt_clearValue;
				fd->edt_cBuffer->beginRenderPass(info, vk::SubpassContents::eInline, vknViews.Dispatcher());
			}

			// Record Imgui Draw Data and draw funcs into command buffer
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *fd->edt_cBuffer);

			vk::Semaphore waitSArr[] = { *vknViews.GetCurrentSignals().render_finished };

			// Submit command buffer
			fd->edt_cBuffer->endRenderPass(vknViews.Dispatcher());
			{
				vk::PipelineStageFlags wait_stage[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
				vk::SubmitInfo info = {};
				info.waitSemaphoreCount = 1;
				info.pWaitSemaphores = waitSArr;
				info.pWaitDstStageMask = wait_stage;
				info.commandBufferCount = 1;
				info.pCommandBuffers = &*fd->edt_cBuffer;
				info.signalSemaphoreCount = 1;
				info.pSignalSemaphores = &render_complete_semaphore;


				//vkn::hlp::TransitionImageLayout(true,*fd->edt_cBuffer, vknViews.GraphicsQueue(), editorControls.edt_frames[editorControls.edt_frameIndex].edt_backbuffer, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
				fd->edt_cBuffer->end(vknViews.Dispatcher());

				//Submit to queue
				err = vknViews.GraphicsQueue().submit(1, &info, *fd->edt_fence, vknViews.Dispatcher());
				check_vk_result(err);
			}
		}
	}

	void vulkan_imgui_interface::ImGuiFramePresent()
	{
		//Vulk interface params
		vkn::VulkanView& vknViews = vkObj->View();

		//To get result of certain vulk operation
		vk::Result err;

		//Fencing semophore after rendering is completed
		vk::Semaphore render_complete_semaphore = *editorControls.edt_frameSemophores[editorControls.edt_semaphoreIndex].edt_renderFinished;
		vk::PresentInfoKHR info = {};
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &render_complete_semaphore;
		info.swapchainCount = 1;
		info.pSwapchains = &*vknViews.Swapchain().swap_chain;
		info.pImageIndices = &editorControls.edt_frameIndex;

		if (vknViews.ImguiResize())
		{
			ImGuiRecreateSwapChain();
			ImGuiRecreateCommandBuffer();
			vknViews.ImguiResize() = false;
		}
		else
		{
			try
			{

				try {
					err = vknViews.GraphicsQueue().presentKHR(info, vknViews.Dispatcher());
					check_vk_result(err);
				}
				catch (const vk::OutOfDateKHRError & e) {
					e;

					vkObj->RecreateSwapChain();
					ImGuiRecreateSwapChain();
					ImGuiRecreateCommandBuffer();
				}
			}
			catch (const vk::Error & err)
			{
				std::cerr << "Err imgui failed to present: " << err.what() << std::endl;
				return;
			}
		}

		//Next frame
		//editorControls.edt_semaphoreIndex = (editorControls.edt_semaphoreIndex + 1) % editorControls.edt_imageCount; // Now we can use the next set of semaphores

	}

	void vulkan_imgui_interface::ImGuiRecreateSwapChain()
	{
		//Recreation of swapchain is already done in the vulkan state recreation

		//Clean up before recreation
		ImGuiCleanUpSwapChain();

		//What is required to be recreated for imGui swapchain will be done here
		idk::vkn::VulkanView& vknViews = vkObj->View();

		editorControls.edt_buffer->CreateImagePool(vknViews);
		editorControls.edt_buffer->enabled = true;

		vknViews.Swapchain().m_inBetweens.emplace_back(editorControls.edt_buffer);

		if (editorInit.edt_min_imageCount == 0)
			editorInit.edt_min_imageCount = ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(static_cast<VkPresentModeKHR>(vknViews.Swapchain().present_mode));

		for (uint32_t i = 0; i < editorInit.edt_imageCount; ++i)
		{
			editorControls.edt_frames.push_back(EditorFrame{});
			editorControls.edt_frameSemophores.push_back(EditorPresentationSignal{});
		}

		vk::AttachmentDescription attachment{};
		attachment.format = vknViews.Swapchain().surface_format.format;
		attachment.samples = vk::SampleCountFlagBits::e1;
		attachment.loadOp = static_cast<vk::AttachmentLoadOp>((editorControls.edt_clearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE));
		attachment.storeOp = vk::AttachmentStoreOp::eStore;
		attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachment.initialLayout = vk::ImageLayout::eUndefined;
		attachment.finalLayout = vk::ImageLayout::eGeneral;
		vk::AttachmentReference color_attachment = {};
		color_attachment.attachment = 0;
		color_attachment.layout = vk::ImageLayout::eColorAttachmentOptimal;
		vk::SubpassDescription subpass = {};
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;
		vk::SubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		vk::RenderPassCreateInfo info = {};
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dependency;

		editorControls.edt_renderPass = vknViews.Device()->createRenderPassUnique(info, nullptr, vknViews.Dispatcher());

		vk::ImageView att[1];
		for (uint32_t i = 0; i < editorControls.edt_imageCount; i++)
		{
			EditorFrame* fd = &editorControls.edt_frames[i];
			att[0] = fd->edt_backbufferView = *editorControls.edt_buffer->image_views[i];
			fd->edt_backbuffer = editorControls.edt_buffer->Images()[i];
			vk::FramebufferCreateInfo fbInfo{
				vk::FramebufferCreateFlags{},
				*editorControls.edt_renderPass,
				1,
				att,
				vknViews.Swapchain().extent.width,
				vknViews.Swapchain().extent.height,
				1
			};
			fd->edt_framebuffer = vknViews.Device()->createFramebufferUnique(fbInfo, nullptr, vknViews.Dispatcher());
		}
	}

	void vulkan_imgui_interface::ImGuiRecreateCommandBuffer()
	{
		vkn::VulkanView& vknViews = vkObj->View();

		for (uint32_t i = 0; i < editorControls.edt_imageCount; i++)
		{
			EditorFrame* fd = &editorControls.edt_frames[i];
			EditorPresentationSignal* fsd = &editorControls.edt_frameSemophores[i];
			{
				vk::CommandPoolCreateInfo info = {};
				info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
				info.queueFamilyIndex = vknViews.QueueFamily().graphics_family.value();
				fd->edt_cPool = vknViews.Device()->createCommandPoolUnique(info, nullptr, vknViews.Dispatcher());
			}
			{
				vk::CommandBufferAllocateInfo info = {};
				info.commandPool = *fd->edt_cPool;
				info.level = vk::CommandBufferLevel::ePrimary;
				info.commandBufferCount = 1;
				fd->edt_cBuffer = std::move(vknViews.Device()->allocateCommandBuffersUnique(info, vknViews.Dispatcher()).front());
				//err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
				//check_vk_result(err);
			}
			{
				vk::FenceCreateInfo info = {};
				info.flags = vk::FenceCreateFlagBits::eSignaled;
				//err = vkCreateFence(device, &info, allocator, &fd->Fence);
				//check_vk_result(err);
				fd->edt_fence = vknViews.Device()->createFenceUnique(info, nullptr, vknViews.Dispatcher());
			}
			{
				vk::SemaphoreCreateInfo info = {};
				fsd->edt_imageAvailable = vknViews.Device()->createSemaphoreUnique(info, nullptr, vknViews.Dispatcher());
				fsd->edt_renderFinished = vknViews.Device()->createSemaphoreUnique(info, nullptr, vknViews.Dispatcher());

				//info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				//err = vkCreateSemaphore(device, &info, allocator, &fsd->ImageAcquiredSemaphore);
				///check_vk_result(err);
				//err = vkCreateSemaphore(device, &info, allocator, &fsd->RenderCompleteSemaphore);
				//check_vk_result(err);
			}
		}
	}

	void vulkan_imgui_interface::ImGuiResizeWindow()
	{
		ImGuiRecreateSwapChain();
		ImGuiRecreateCommandBuffer();
	}

	void vulkan_imgui_interface::ImGuiCleanUpSwapChain()
	{
		vkn::VulkanView& vknViews = vkObj->View();

		vknViews.Device()->waitIdle(vknViews.Dispatcher());

		editorControls.edt_frames.clear();
		editorControls.edt_frameSemophores.clear();
		editorControls.edt_renderPass.reset();
	}
}