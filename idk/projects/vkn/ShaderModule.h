#pragma once
#include <idk.h>
#include <vulkan/vulkan.hpp>
#include <gfx/ShaderProgram.h>
#include <gfx/buffer_desc.h>
#include <gfx/pipeline_config.h>
#include <vkn/BufferedObj.h>
#include <vkn/DescriptorCountArray.h>
#include <vkn/VulkanResourceManager.h>
namespace idk::vkn
{

	struct UboInfo
	{
		vk::DescriptorSetLayout layout;
		uniform_layout_t::UniformStage stage;
		uniform_layout_t::UniformType type;
		uint32_t binding, set;
		uint32_t input_attachment_index; //Only valid if type == Attachment
		uint32_t size;

		//static void AddToConfig(pipeline_config& config, const UboInfo& info)
		//{
		//	config.uniform_layouts[info.set].bindings.emplace_back(uniform_layout_t::bindings_t{ info.binding,1,{info.stage},info.type });
		//}
	};
	struct DsLayoutInfo
	{
		vk::UniqueDescriptorSetLayout layout;
		DsCountArray entry_counts;
		operator vk::UniqueDescriptorSetLayout& ()
		{
			return layout;
		}
		vk::DescriptorSetLayout& operator*()
		{
			return *layout;
		}
		vk::UniqueDescriptorSetLayout& operator->()
		{
			return layout;
		}

		const vk::DescriptorSetLayout& operator*()const
		{
			return *layout;
		}
		const vk::UniqueDescriptorSetLayout& operator->()const
		{
			return layout;
		}
	};

	class ShaderModule :public ShaderProgram, public IBufferedObj
	{
	public:
		using LayoutTable = hash_table<uint32_t, DsLayoutInfo>;


		operator bool()const { return buf_obj->HasCurrent(); }

		void Load(vk::ShaderStageFlagBits single_stage, vector<buffer_desc> descriptors, const vector<unsigned int>& byte_code, string glsl = {});
		void Load(vk::ShaderStageFlagBits single_stage, vector<buffer_desc> descriptors, string_view byte_code, string glsl = {});
		vk::ShaderStageFlagBits Stage()const { return Current().stage; }
		vk::ShaderModule        Module()const { return *Current().module; }
		string_view				GLSL() const { return Current().glsl; }
		void AttribDescriptions(vector<buffer_desc>&& attribs){ Current().attrib_descriptions = std::move(attribs); }
		const vector<buffer_desc>& AttribDescriptions()const { return Current().attrib_descriptions; }
		bool HasLayout(const string& uniform_name)const;
		hash_table<string, UboInfo>::const_iterator InfoBegin()const;
		hash_table<string, UboInfo>::const_iterator InfoEnd()const;

		LayoutTable::const_iterator LayoutsBegin()const;
		LayoutTable::const_iterator LayoutsEnd()const;
		//UboInfo& GetLayout(string uniform_name);
		const UboInfo& GetLayout(const string& uniform_name)const;
		std::optional<UboInfo> TryGetLayout(const string& uniform_name)const;
		bool NeedUpdate()const { return buf_obj->HasUpdate(); }
		bool HasUpdate()const override { return buf_obj->HasUpdate(); }
		void UpdateCurrent(size_t index)override { buf_obj->UpdateCurrent(index); }
		bool HasCurrent()const { return buf_obj->HasCurrent(); }
		std::optional<uint32_t> GetBinding(uint32_t location)const { return buf_obj->Current().GetBinding(location); }

		bool NewlyLoaded()const { return _newly_loaded_flag; }
		void NewlyLoaded(bool val){ _newly_loaded_flag = val; }

		ShaderModule();
		ShaderModule(const ShaderModule&) = default;
		ShaderModule(ShaderModule&&) = default;
		ShaderModule& operator=(ShaderModule&&) = default;
		ShaderModule& operator=(const ShaderModule&) = default;
		~ShaderModule();

	private:
		bool _newly_loaded_flag = false;
		struct Data
		{
			vk::ShaderStageFlagBits stage;
			hash_table<string, UboInfo> ubo_info;
			LayoutTable layouts;
			vector<buffer_desc> attrib_descriptions;
			hash_table<uint32_t, uint32_t> loc_to_bind;
			std::optional<uint32_t> GetBinding(uint32_t location)const;
			vk::UniqueShaderModule module;
			string glsl;
		};
		Data& Current()
		{
			return buf_obj->Current();
		}
		const Data& Current()const
		{
			return buf_obj->Current();
		}
		unique_ptr<BufferedObj<Data>> buf_obj = std::make_unique<BufferedObj<Data>>();
		//vk::UniqueShaderModule back_module;//To load into, will move into module when no buffers are using it
	};
}