#pragma once
#include <vulkan/vulkan.hpp>
#include <vkn/UboManager.h>
#include <vkn/DescriptorsManager.h>
#include <vkn/utils/PresentationSignals.h>
namespace idk::vkn
{
struct RenderStateV2
{
	vk::CommandBuffer cmd_buffer;
	UboManager ubo_manager;//Should belong to each thread group.

	PresentationSignals signal;
	DescriptorsManager dpools;

	bool has_commands = false;
	void FlagRendered() { has_commands = true; }
	void Reset();
	RenderStateV2() = default;
	RenderStateV2(const RenderStateV2&) = delete;
	RenderStateV2(RenderStateV2&&) = default;
};
}