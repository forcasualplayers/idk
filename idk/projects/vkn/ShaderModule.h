#pragma once
#include <idk.h>
#include <vulkan/vulkan.hpp>
#include <gfx/ShaderProgram.h>
#include <gfx/buffer_desc.h>
#include <gfx/pipeline_config.h>
#include <vkn/BufferedObj.h>
namespace idk::vkn
{

	struct UboInfo
	{
		vk::DescriptorSetLayout layout;
		uniform_layout_t::UniformStage stage;
		uniform_layout_t::UniformType type;
		uint32_t binding, set;
		uint32_t size;

		static void AddToConfig(pipeline_config& config, const UboInfo& info)
		{
			config.uniform_layouts[info.set].bindings.emplace_back(uniform_layout_t::bindings_t{ info.binding,1,{info.stage},info.type });
		}
	};

	class ShaderModule :public ShaderProgram, public IBufferedObj
	{
	public:
		void Load(vk::ShaderStageFlagBits single_stage, vector<buffer_desc> descriptors, const vector<unsigned int>& byte_code);
		void Load(vk::ShaderStageFlagBits single_stage, vector<buffer_desc> descriptors,string_view byte_code);
		vk::ShaderStageFlagBits Stage()const { return Current().stage; }
		vk::ShaderModule        Module()const { return *Current().module; }
		void SetLayout(uint32_t set, vk::DescriptorSetLayout layout)
		{
			for (auto& info : Current().ubo_info)
			{
				if (info.second.set == set)
					info.second.layout = layout;
			}
		}
		void ApplyUniformToConfig(pipeline_config& config)
		{
			for (auto& pair: Current().ubo_info)
			{
				auto& info = pair.second;
				info.AddToConfig(config,info);
			}
		}
		void AttribDescriptions(vector<buffer_desc>&& attribs){ Current().attrib_descriptions = std::move(attribs); }
		const vector<buffer_desc>& AttribDescriptions()const { return Current().attrib_descriptions; }
		bool HasLayout(string uniform_name)const;
		hash_table<string, UboInfo>::const_iterator LayoutsBegin()const;
		hash_table<string, UboInfo>::const_iterator LayoutsEnd()const;
		//UboInfo& GetLayout(string uniform_name);
		const UboInfo& GetLayout(string uniform_name)const;
		bool NeedUpdate()const { return buf_obj.HasUpdate(); }
		bool HasUpdate()const override { return buf_obj.HasUpdate(); }
		void UpdateCurrent(size_t index)override { buf_obj.UpdateCurrent(index); }
		
	private:
		struct Data
		{
			vk::ShaderStageFlagBits stage;
			hash_table<string, UboInfo> ubo_info;
			vector<buffer_desc> attrib_descriptions;
			vk::UniqueShaderModule module;
		};
		Data& Current()
		{
			return buf_obj.Current();
		}
		const Data& Current()const
		{
			return buf_obj.Current();
		}
		BufferedObj<Data> buf_obj;
		//vk::UniqueShaderModule back_module;//To load into, will move into module when no buffers are using it
	};
}