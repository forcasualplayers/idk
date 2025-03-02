#pragma once
#include <idk.h>
#include <vulkan/vulkan.hpp>
#include <vkn/ProcessedRO.h>
#include <gfx/pipeline_config.h>


#include <vkn/vulkan_enum_info.h>
#include <vkn/UboManager.h>
#include <vkn/ShaderModule.h> //UboInfo
#include <vkn/VulkanHashes.h>
#include <vkn/DescriptorUpdateData.h>
#include <vkn/DescriptorCountArray.h>
#include <ds/result.h>
#include <vkn/BufferHelpers.inl>
namespace idk
{
	struct renderer_attributes;
}
namespace idk::vkn
{
	struct VknTexture;
	class VulkanMesh;
	struct DescriptorUpdateData;
	struct VknCubemap;
	struct DescriptorsManager;

	using desc_type_info = DescriptorTypeI; // pair<num_ds,num_descriptors_per_type>
	using CollatedLayouts_t = hash_table < vk::DescriptorSetLayout, std::pair<uint32_t, DsCountArray>>;
	template<vk::DescriptorType type>
	static constexpr size_t desc_type_index = desc_type_info::map<type>();
#if 0
	class binding_manager 
	{
		using set_t = uint32_t;
		using binding_t = uint32_t;
		struct set_bindings
		{
			vk::DescriptorSetLayout layout;
			hash_table<uint32_t, vector<std::optional<ProcessedRO::BindingInfo>>>  bindings;
			DsCountArray total_desc;
			bool dirty = false;

			vector<ProcessedRO::BindingInfo> scratch_out;

			void SetLayout(vk::DescriptorSetLayout new_layout, const DsCountArray& total_descriptors, bool clear_bindings = false);
			void Bind(ProcessedRO::BindingInfo info);
			void Unbind(uint32_t binding);
			monadic::result< vector<ProcessedRO::BindingInfo>, string> FinalizeDC(CollatedLayouts_t& collated_layouts);
		};
		hash_table<set_t, set_bindings> curr_bindings;

		void AddBinding(set_t set, vk::DescriptorSetLayout layout, const DsCountArray& counts)
		{
			auto b_itr = curr_bindings.find(set);
			if (b_itr == curr_bindings.end())
			{
				curr_bindings[set];
				b_itr = curr_bindings.find(set);
			}
			b_itr->second.SetLayout(layout, counts);
		}
		void RemoveBinding(set_t set)
		{
			auto b_itr = curr_bindings.find(set);
			if (b_itr != curr_bindings.end())
				curr_bindings.erase(b_itr);
		}
		void MarkDirty()
		{
			for(auto& [set,bindings]:curr_bindings)
				bindings.dirty = true;
		}

		template<typename T>
		bool BindUniformBuffer(const string& uniform_name, uint32_t array_index, const T& data, bool skip_if_bound = false);
		bool BindSampler(const string& uniform_name, uint32_t array_index, const VknTexture& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eGeneral);
		bool BindAttachment(const string& uniform_name, uint32_t array_index, const VknTexture& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
		bool BindSampler(const string& uniform_name, uint32_t array_index, const VknCubemap& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eGeneral);

		bool is_bound(set_t set,uint32_t binding_index, uint32_t array_index)const noexcept
		{
			const auto s_itr = curr_bindings.find(set);
			if (s_itr == curr_bindings.end())
				return false;
			
			const set_bindings& existing_bindings = s_itr->second;
			auto& bindings = existing_bindings.bindings;
			auto itr = bindings.find(binding_index);
			return  itr != bindings.end() && itr->second.size() > array_index && itr->second[array_index];
		}
	};
#endif
	class PipelineThingy
	{
	public:
		using set_t = uint32_t;
		using binding_t = uint32_t;
		struct set_bindings
		{
			vk::DescriptorSetLayout layout;
			hash_table<uint32_t, vector<std::optional<ProcessedRO::BindingInfo>>>  bindings;
			DsCountArray total_desc;
			bool dirty = false;

			vector<ProcessedRO::BindingInfo> scratch_out;

			void SetLayout(vk::DescriptorSetLayout new_layout, const DsCountArray& total_descriptors, bool clear_bindings = false);
			void Bind(ProcessedRO::BindingInfo info);
			void Unbind(uint32_t binding);
			monadic::result< vector<ProcessedRO::BindingInfo>, string> FinalizeDC(CollatedLayouts_t& collated_layouts);
		};

		PipelineThingy(std::shared_ptr<DescriptorUpdateData> d = std::make_shared<DescriptorUpdateData>())
			: dud{ d }
		{

		}
		
		void SetRef(
			//set, bindings
			UboManager& ubo_manager
		);
		void UnbindShader(ShaderStage stage);
		void BindShader(ShaderStage stage, RscHandle<ShaderProgram> shader);
		void BindAttrib(uint32_t location, vk::Buffer buffer, size_t offset);
		bool BindMeshBuffers(const RenderObject& ro);
		bool BindMeshBuffers(RscHandle<Mesh> mesh, const renderer_attributes& attribs);
		bool BindMeshBuffers(const VulkanMesh& mesh, const renderer_attributes& attribs);
		void SetVertexCount(uint32_t vertex_count);

		std::optional<UboInfo> GetUniform(const string& uniform_name) const;

		template<typename T>
		bool BindUniformBuffer(const string& uniform_name, uint32_t array_index, const T& data, bool skip_if_bound = false);
		bool BindSampler(const string& uniform_name, uint32_t array_index, const VknTexture& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eGeneral);
		bool BindAttachment(const string& uniform_name, uint32_t array_index, const VknTexture& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
		bool BindSampler(const string& uniform_name, uint32_t array_index, const VknCubemap& texture, bool skip_if_bound = false, vk::ImageLayout layout = vk::ImageLayout::eGeneral);

		void FinalizeDrawCall(const RenderObject& ro);
		void FinalizeDrawCall(const RenderObject& ro,size_t num_inst,size_t inst_offset);

		void GenerateDS(DescriptorsManager& d_manager,bool update_ubo_buffers=true);
		void UpdateUboBuffers();
		const vector<ProcessedRO>& DrawCalls() const { return draw_calls; }

		//reserves an extra size chunk
		void reserve(size_t size);

	private:
		struct Ref
		{
			CollatedLayouts_t collated_layouts;
			UboManager& ubo_manager;
			Ref(UboManager* u = nullptr)
				: ubo_manager{ *u }
			{}
		};

		hash_table<uint32_t, BoundVertexBuffer> attrib_buffers;
		std::optional<BoundIndexBuffer> index_buffer{};
		size_t num_vertices{};

		std::shared_ptr<DescriptorUpdateData> dud{};
		std::optional<RscHandle<ShaderProgram>> shaders[static_cast<size_t>(ShaderStage::Size)];
		vector<ProcessedRO> draw_calls;

		shared_ptr<const pipeline_config> prev_config;

		bool shader_changed = false;
		hash_table<set_t, set_bindings> curr_bindings;
		Ref ref;
	};

	void UpdateUniformDS(
		vk::DescriptorSet& dset,
		span<ProcessedRO::BindingInfo> bindings,
		DescriptorUpdateData& ds_update_data
	);

	template<typename T>
	ProcessedRO::BindingInfo CreateBindingInfo(const UboInfo& obj_uni, uint32_t arr_index, const T& val, UboManager& ubo_manager)
	{
		auto&& [trf_buffer, trf_offset] = ubo_manager.Add(val);
		//collated_bindings[obj_uni.set].emplace_back(
		return ProcessedRO::BindingInfo
		{
			obj_uni.binding,
			trf_buffer,
			trf_offset,
			arr_index,
			hlp::buffer_size(val),
			obj_uni.layout
		};
		//);
	}
	namespace detail
	{
		bool is_bound(PipelineThingy::set_bindings& existing_bindings, uint32_t binding_index, uint32_t array_index);
	}
	template<typename T>
	bool PipelineThingy::BindUniformBuffer(const string& uniform_name, uint32_t array_index, const T& data, bool skip_if_bound)
	{
		auto info = GetUniform(uniform_name);
		if (info)
		{
			auto itr = curr_bindings.find(info->set);
			if (!skip_if_bound || itr == curr_bindings.end() || !detail::is_bound(itr->second,info->binding,array_index))
				curr_bindings[info->set].Bind(CreateBindingInfo(*info, array_index, data, ref.ubo_manager));
		}
		return s_cast<bool>(info);
	}

}