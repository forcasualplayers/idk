#pragma once
#include<vulkan/vulkan.hpp>
#include "idk.h"
#include "gfx/Texture.h"
#include <vkn/MemoryAllocator.h>
#include <gfx/CompiledTexture.h>
#include <meta/stl_hack.h>
#include <vkn/VulkanResourceManager.h>

#include <vkn/AsyncTexLoadInfo.h>

namespace idk::vkn {

	struct VknTexture
		: public Texture
	{
		uvec2					size{};
		vk::DeviceSize			sizeOnDevice{};
		void* rawData{};
		string					path{ "" };
		VulkanRsc<vk::Image>			image_{ nullptr };
		vk::Format				format{};
		vk::ImageUsageFlags     usage{};
		vk::ImageAspectFlagBits    img_aspect;
		vk::UniqueDeviceMemory  mem{ nullptr };
		hlp::UniqueAlloc        mem_alloc{};
		VulkanRsc<vk::ImageView>     imageView{ nullptr };
		VulkanRsc<vk::Sampler>       sampler{ nullptr };
		opt<vk::DescriptorSet>	descriptorSet{};
		vk::ImageSubresourceRange range;
		uint32_t mipmap_level=1;
		string dbg_name;

		vk::ImageType image_type = vk::ImageType::e2D;

		VknTexture() = default;
		~VknTexture();
		VknTexture(const VknTexture & rhs) = delete;
		VknTexture(VknTexture && rhs) noexcept;
		//VknTexture(const VknTexture& rhs);
		VknTexture(const CompiledTexture&);
		VknTexture(CompiledTexture&&);


		VknTexture& operator=(VknTexture&&) noexcept;
		vk::Sampler Sampler()const;
		vk::Image Image(bool ignore_effective=false)const;
		vk::ImageView ImageView()const;
		vk::ImageAspectFlags ImageAspects()const;
		uint32_t& Layers(uint32_t layers)noexcept;
		uint32_t Layers()const noexcept;

		vk::ImageSubresourceRange FullRange()const;
		void FullRange(vk::ImageSubresourceRange range);

		using Texture::Size;
		uvec2 Size(uvec2 new_size) override;
		//Required if you want the image to be able to be used in imgui (Cast to ImTextureID)
		void* ID() const override;

		bool MarkLoaded(bool loaded);
		bool IsLoaded()const;

		void SetTexLoadInfo(std::optional<AsyncTexLoadInfo>);
		const std::optional<AsyncTexLoadInfo>& GetTexLoadInfo()const;
		void BeginAsyncReload(std::optional<Guid> guid = {});

	private:
		std::optional<AsyncTexLoadInfo> _tex_load_info;
		const VknTexture& GetEffective(bool ignore_default=false)const;
		bool _loaded = true;
		uint32_t                _layers;
		void OnMetaUpdate(const TextureMeta&);
		void UpdateUV(UVMode);

	};

};

MARK_NON_COPY_CTORABLE(idk::vkn::VknTexture)