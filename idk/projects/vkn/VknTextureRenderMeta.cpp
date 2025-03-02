#include "pch.h"
#include <vkn/VknTextureRenderMeta.h>
#include <vkn/utils/utils.h> //ReverseMap
#include <vkn/VulkanView.h>
#include <vkn/VulkanWin32GraphicsSystem.h>
#include <iostream>

namespace idk::vkn {

	static const hash_table<TextureFormat, vk::Format> formatMap
	{
		{TextureFormat::eD16Unorm, vk::Format::eD32Sfloat},
		{ TextureFormat::eRGBA32, vk::Format::eR8G8B8A8Unorm },
		{ TextureFormat::eBGRA32, vk::Format::eB8G8R8A8Unorm },
		{ TextureFormat::eBC1,vk::Format::eBc1RgbaSrgbBlock },//Auto srgb
		{ TextureFormat::eBC2,vk::Format::eBc2SrgbBlock },	  //Auto srgb
		{ TextureFormat::eBC3,vk::Format::eBc3SrgbBlock },	  //Auto srgb
		{ TextureFormat::eBC4,vk::Format::eBc4UnormBlock },
		{ TextureFormat::eBC5,vk::Format::eBc5UnormBlock },
	};
	static const hash_table<TextureInternalFormat, vk::Format> cFormatMap
	{
		{ TextureInternalFormat::DEPTH_16, vk::Format::eD16Unorm },
		{ TextureInternalFormat::DEPTH_24, vk::Format::eD24UnormS8Uint},
		{ TextureInternalFormat::DEPTH_32, vk::Format::eD32Sfloat},
		{ TextureInternalFormat::DEPTH_32_F_STENCIL_8, vk::Format::eD32Sfloat},
		{ TextureInternalFormat::R_8, vk::Format::eR8Unorm },
		{ TextureInternalFormat::R_16, vk::Format::eR16Unorm },
		{ TextureInternalFormat::R_32_F, vk::Format::eR32Sfloat },
		{ TextureInternalFormat::R_64_F, vk::Format::eR64Sfloat },
		{ TextureInternalFormat::R_8_I , vk::Format::eR8Sint },
		{ TextureInternalFormat::R_16_I, vk::Format::eR16Sint },
		{ TextureInternalFormat::R_32_I, vk::Format::eR32Sint },
		{ TextureInternalFormat::R_64_I, vk::Format::eR64Sint },
		{ TextureInternalFormat::R_8_UI , vk::Format::eR8Uint },
		{ TextureInternalFormat::R_16_UI, vk::Format::eR16Uint },
		{ TextureInternalFormat::R_32_UI, vk::Format::eR32Uint },
		{ TextureInternalFormat::RG_8, vk::Format::eR8G8Unorm },
		{ TextureInternalFormat::RG_16_F, vk::Format::eR16G16Sfloat },
		{ TextureInternalFormat::RGB_8, vk::Format::eR8G8B8Unorm },
		{ TextureInternalFormat::RGB_16_F, vk::Format::eR16G16B16Sfloat },
		{ TextureInternalFormat::RGB_32_F, vk::Format::eR16G16B16Sfloat },
		{ TextureInternalFormat::RGBA_8, vk::Format::eR8G8B8A8Unorm },
		{ TextureInternalFormat::ARGB_10, vk::Format::eA2B10G10R10UnormPack32 },
		{ TextureInternalFormat::ARGB_10_SNORM, vk::Format::eA2B10G10R10SnormPack32 },
		{ TextureInternalFormat::RGBA_8_SNORM, vk::Format::eR8G8B8A8Snorm },
		{ TextureInternalFormat::RGBA_16_F, vk::Format::eR16G16B16A16Sfloat },
		{ TextureInternalFormat::RGBA_32_F, vk::Format::eR16G16B16A16Sfloat },
		{ TextureInternalFormat::BGRA_8,   vk::Format::eB8G8R8A8Unorm },
		{ TextureInternalFormat::SBGRA_8,   vk::Format::eB8G8R8A8Srgb },
		{ TextureInternalFormat::SRGB_8 ,   vk::Format::eR8G8B8Srgb },
		{ TextureInternalFormat::SRGBA_8,   vk::Format::eR8G8B8A8Srgb },
		{ TextureInternalFormat::RGB_DXT1,vk::Format::eBc1RgbUnormBlock },
		{ TextureInternalFormat::RGBA_DXT3,vk::Format::eBc2UnormBlock },
		{ TextureInternalFormat::RGBA_DXT5,vk::Format::eBc3UnormBlock },
		{ TextureInternalFormat::RGBA_DXT1,vk::Format::eBc1RgbaUnormBlock },
		{ TextureInternalFormat::SRGB_DXT1,vk::Format::eBc1RgbSrgbBlock },
		{ TextureInternalFormat::SRGBA_DXT3,vk::Format::eBc2SrgbBlock },
		{ TextureInternalFormat::SRGBA_DXT5,vk::Format::eBc3SrgbBlock },
		{ TextureInternalFormat::SRGBA_DXT1,vk::Format::eBc1RgbaSrgbBlock },
	};

	static const hash_table<FilterMode::_enum, vk::Filter> filterMap
	{
		{FilterMode::_enum::Nearest, vk::Filter::eNearest},
		{ FilterMode::_enum::Linear ,vk::Filter::eLinear },
		{ FilterMode::_enum::Cubic  ,vk::Filter::eCubicIMG },
	};
	static const hash_table<UVMode::_enum, vk::SamplerAddressMode> uvModeMap
	{
		{UVMode::_enum::Clamp, vk::SamplerAddressMode::eClampToBorder},
		{ UVMode::_enum::Repeat,vk::SamplerAddressMode::eRepeat },
		{ UVMode::_enum::MirrorRepeat,vk::SamplerAddressMode::eMirroredRepeat },
	};
	static const hash_table<vk::Format, vk::Format> unSrgbMap
	{
		{ vk::Format::eR8G8B8Srgb, vk::Format::eR8G8B8Unorm       },
		{ vk::Format::eR8G8B8A8Srgb     ,vk::Format::eR8G8B8A8Unorm },
		{ vk::Format::eBc1RgbSrgbBlock  ,vk::Format::eBc1RgbUnormBlock },
		{ vk::Format::eBc2SrgbBlock     ,vk::Format::eBc2UnormBlock },
		{ vk::Format::eBc3SrgbBlock     ,vk::Format::eBc3UnormBlock },
		{ vk::Format::eBc1RgbaSrgbBlock ,vk::Format::eBc1RgbaUnormBlock },
	};
	static const hash_table<CompareOp, vk::CompareOp> compareOpMap
	{
		{CompareOp::eNever, vk::CompareOp::eNever          },
		{ CompareOp::eLess,			  vk::CompareOp::eLess },
		{ CompareOp::eEqual ,		  vk::CompareOp::eEqual },
		{ CompareOp::eLessOrEqual ,	  vk::CompareOp::eLessOrEqual },
		{ CompareOp::eGreater ,		  vk::CompareOp::eGreater },
		{ CompareOp::eNotEqual,		  vk::CompareOp::eNotEqual },
		{ CompareOp::eGreaterOrEqual , vk::CompareOp::eGreaterOrEqual },
		{ CompareOp::eAlways,          vk::CompareOp::eAlways },
	};

	hash_table<TextureFormat, vk::Format> FormatMap()
	{
		return formatMap;
	}
	hash_table<TextureInternalFormat, vk::Format> CFormatMap()
	{
		return cFormatMap;
	}
	hash_table<FilterMode::_enum, vk::Filter> FilterMap()
	{
		return filterMap;
	}
	hash_table<UVMode::_enum, vk::SamplerAddressMode> UVModeMap()
	{
		return uvModeMap;
	}
	hash_table<vk::Format, vk::Format> UnSrgbMap()
	{
		return unSrgbMap;

	}
	hash_table<CompareOp, vk::CompareOp> CompareOpMap()
	{
		return compareOpMap;
	}

	vk::Format MapFormat(TextureFormat tf)
	{
		static const auto map = FormatMap();
		return map.find(tf)->second;
	}
	vk::Format MapFormat(TextureInternalFormat tf)
	{
		static const auto map = CFormatMap();
		return map.find(tf)->second;
	}
	TextureFormat MapFormat(vk::Format tf)
	{
		static const auto map = hlp::ReverseMap(FormatMap());
		return map.find(tf)->second;
	}
	vk::Format UnSrgb(vk::Format f)
	{
		static const auto map = UnSrgbMap();
		vk::Format result = f;
		auto itr = map.find(f);
		if (itr != map.end())
			result = itr->second;
		return result;
	}
	vk::Format ToSrgb(vk::Format f)
	{
		static const auto map = hlp::ReverseMap(UnSrgbMap());
		vk::Format result = f;
		auto itr = map.find(f);
		if (itr != map.end())
			result = itr->second;
		return result;
	}

	vk::CompareOp MapCompareOp(CompareOp compare_op)
	{
		vk::CompareOp mode = vk::CompareOp::eNever;
		static const hash_table<CompareOp, vk::CompareOp> map = CompareOpMap();
		auto itr = map.find(compare_op);
		if (itr != map.end())
			mode = itr->second;

		return mode;
	}


	struct BlitCompatibility
	{
		using tiling_info =BlitCompatFlags;
		hash_table<vk::Format, tiling_info > tiling;
		
		void RegisterTiling(vk::Format format, tiling_info tile);
		bool HasCompatibleTiling(vk::Format format, BlitCompatUsageMasks usage)const;
		bool IsCompatible(vk::Format format, BlitCompatFlags mask)const;
		vector<std::pair<vk::Format, vk::ImageTiling>> GetCompatible(BlitCompatUsageMasks usage)const;
	};

	BlitCompatibility& GetBlitComp()
	{
		static BlitCompatibility comp;
		return comp;
	}


	bool BlitCompatible(vk::Format format, BlitCompatUsageMasks usage)
	{
		return GetBlitComp().HasCompatibleTiling(format,usage);
	}
	template<typename T>
	T other(T lhs, T rhs, T val)
	{
		return (lhs == val) ? rhs : lhs;
	}

	std::optional<vk::ImageTiling> GetNearestTiling(vk::Format format, BlitCompatTileMasks tiling,BlitCompatUsageMasks usage)
	{
		auto curr_tiling = tiling == BlitCompatTileMasks::eOptimal ? vk::ImageTiling::eOptimal: vk::ImageTiling::eLinear;
		if (GetBlitComp().HasCompatibleTiling(format,usage))
		{
			return GetBlitComp().IsCompatible(format, tiling&usage) ? curr_tiling : other(vk::ImageTiling::eOptimal,vk::ImageTiling::eLinear,curr_tiling);
		}
		return {};
	}

	std::optional<vk::Format> NearestBlittableFormat(vk::Format format, BlitCompatUsageMasks usage)
	{
		//Idk how to do the other ver, so this is just a hack.
		vk::Format preferred_list[] =
		{
			vk::Format::eR32G32B32A32Sfloat,
			vk::Format::eR8G8B8A8Unorm,
			vk::Format::eR8G8B8A8Srgb,
		};


		auto& compat = GetBlitComp();
		if (!compat.HasCompatibleTiling(format, usage))
		{
			auto formats = compat.GetCompatible(usage);
			std::pair<uint32_t, std::optional<vk::Format>> best{};

			//This is a stupid hack because we have no way to get any properties about the VK format.
			for (auto& nformat : preferred_list)
			{
				if (compat.HasCompatibleTiling(nformat, usage))
				{
					best.second = nformat;
					break;
				}
			}

			return best.second;
		}
		return format;
	}
	void RegisterFormatBlitCompatibility()
	{
		auto& comp = GetBlitComp();
		auto map = CFormatMap();
		auto pdevice = View().PDevice();
		std::stringstream ss;
		for (auto& [ecf, format] : map)
		{
			auto prop = pdevice.getFormatProperties(format);
			BlitCompatFlags flags{};
			if (prop.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
				flags |= CompatMask(vkn::BlitCompatTileMasks::eLinear, vkn::BlitCompatUsageMasks::eDst);

			if (prop.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
				flags |= CompatMask(vkn::BlitCompatTileMasks::eOptimal, vkn::BlitCompatUsageMasks::eDst);

			if (prop.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
				flags |= CompatMask(vkn::BlitCompatTileMasks::eLinear, vkn::BlitCompatUsageMasks::eSrc);

			if (prop.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
				flags |= CompatMask(vkn::BlitCompatTileMasks::eOptimal, vkn::BlitCompatUsageMasks::eSrc);
			comp.RegisterTiling(format, flags);
		}
	}

	void PrintFormatBlitCompatibility()
	{
		RegisterFormatBlitCompatibility();
		auto map = CFormatMap();
		auto pdevice = View().PDevice();
		std::stringstream ss;
		for (auto& [ecf, format] : map)
		{
			auto prop = pdevice.getFormatProperties(format);

			if (prop.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
				ss << "\tLinear tiling has eBlitDst\n";
			else
				ss << "\tLinear tiling does not have eBlitDst\n";
			LOG_TO(LogPool::GFX, "%s", ss.str().c_str());
			std::stringstream{}.swap(ss);
			if (prop.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
				ss << "\toptimal tiling has eBlitDst\n";
			else
				ss << "\toptimal tiling does not have eBlitDst\n";
			LOG_TO(LogPool::GFX, "%s", ss.str().c_str());
			std::stringstream{}.swap(ss);
			if (prop.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
				ss << "\tLinear tiling has eBlitSrc\n";
			else
				ss << "\tLinear tiling does not have eBlitSrc\n";
			LOG_TO(LogPool::GFX, "%s", ss.str().c_str());
			std::stringstream{}.swap(ss);
			if (prop.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
				ss << "\toptimal tiling has eBlitSrc\n";
			else
				ss << "\toptimal tiling does not have eBlitSrc\n";
			LOG_TO(LogPool::GFX, "%s", ss.str().c_str());
			std::stringstream{}.swap(ss);
		}
	}

	void BlitCompatibility::RegisterTiling(vk::Format format, tiling_info tile)
	{
		tiling[format] = tile;
	}
	bool BlitCompatibility::HasCompatibleTiling(vk::Format format, BlitCompatUsageMasks usage) const
	{
		auto itr = tiling.find(format);
		return (itr != tiling.end()) && (itr->second & s_cast<BlitCompatFlags>(s_cast<uint32_t>(usage)));
	}

	bool BlitCompatibility::IsCompatible(vk::Format format, BlitCompatFlags mask) const
	{
		auto titr = tiling.find(format);
		if (titr != tiling.end())
		{
			return static_cast<bool>((titr->second & mask));
		}
		return false;
	}
	vector<std::pair<vk::Format, vk::ImageTiling>> BlitCompatibility::GetCompatible(BlitCompatUsageMasks usage) const
	{
		vector<std::pair<vk::Format, vk::ImageTiling>> result{};
		for (auto& [format, info] : tiling)
		{
			auto masked = info & usage;
			if (masked & BlitCompatTileMasks::eLinear)
			{
				result.emplace_back(format, vk::ImageTiling::eLinear);
			}
			if (masked & BlitCompatTileMasks::eOptimal)
			{
				result.emplace_back(format, vk::ImageTiling::eOptimal);
			}
		}
		return result;
	}
}