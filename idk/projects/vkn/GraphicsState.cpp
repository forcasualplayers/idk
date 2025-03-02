#include "pch.h"
#include "GraphicsState.h"
#include <gfx/RenderTarget.h>
#include <gfx/Framebuffer.h>
#include <ds/span.inl>
#include <vkn/MaterialInstanceCache.h>
#include <res/ResourceHandle.inl>

#include <vkn/time_log.h>
namespace idk::vkn
{
	dbg::time_log& GetGfxTimeLog();
	const LightData* GraphicsState::ActiveLight(size_t light_index) const
	{
		return &shared_gfx_state->Lights()[light_index];
	}
	void GraphicsState::Init(const GraphicsSystem::RenderRange& data, const vector<size_t>& all_active_lights, const vector<size_t>& active_directional_light, const vector<LightData>& lights_data, const std::map<Handle<GameObject>, CamLightData>& d_lm, const vector<RenderObject>& render_objects, const vector<AnimatedRenderObject>& skinned_render_objects, const vector<SkeletonTransforms>& s_transforms)
	{
		camera = data.camera;
		range = data;
		lights = &lights_data;
		d_lightmaps = &d_lm;
		mesh_render.clear();
		skinned_mesh_render.clear();
		active_lights.clear();
		active_dir_lights.clear();
		{
			//size_t i = 0;
			RscHandle<Texture> def_2d;
			RscHandle<CubeMap> def_cube;

			active_lights.insert(active_lights.end(), all_active_lights.begin() + data.light_begin, all_active_lights.begin() + data.light_end);
			//active_lights.emplace_back(all_active_lights);
			//shadow_maps_2d.resize(active_lights.size(), def_2d);
			active_dir_lights.insert(active_dir_lights.end(), active_directional_light.begin() + data.dir_light_begin, active_directional_light.begin() + data.dir_light_end);
			//shadow_maps_directional.resize(active_directional_light.size(), def_2d);
			//shadow_maps_cube.resize(active_lights.size(), def_cube);
			//for (auto& light_idx : active_lights)
			//{
			//	auto& light = lights_data[light_idx];
			//	if (light.index == 2)//spotlight
			//	{
			//		for(auto& elem: light.light_maps)
			//			if(elem.light_map)
			//				shadow_maps_2d[i]=(s_cast<RscHandle<Texture>>(elem.light_map->DepthAttachment()));
			//		//shadow_maps_cube[i]=(def_cube);
			//	}
			//	else
			//	{
			//		shadow_maps_2d[i] = RscHandle<Texture>{};
			//	}
			//	++i;				
			//}
			//for (auto& dir_light_idx : active_directional_light)
			//{
			//	auto& light = lights_data[dir_light_idx];
			//	//if (!light.light_maps.empty())
			//		//shadow_maps_2d[i] = (s_cast<RscHandle<Texture>>(light.light_maps[0].light_map->DepthAttachment()));
			//	for (auto& elem : light.light_maps)
			//		shadow_maps_directional[j++] = (s_cast<RscHandle<Texture>>(elem.light_map->DepthAttachment()));
			//	//++j;
			//}
		}
		skeleton_transforms = &s_transforms;
		CullAndAdd(render_objects, skinned_render_objects);
	}

	const hash_table<RscHandle<MaterialInstance>, ProcessedMaterial>& CoreGraphicsState::material_instances()const
	{
		return shared_gfx_state->material_instances;
	}
	void CoreGraphicsState::ProcessMaterialInstances(hash_table<RscHandle<MaterialInstance>, ProcessedMaterial>& material_instances)
	{
		
		auto AddMatInst = [](auto& material_instances,RscHandle<MaterialInstance> mat_inst)
		{
			GetGfxTimeLog().start("find");
			auto itr = material_instances.find(mat_inst);
			GetGfxTimeLog().end();
			if (itr == material_instances.end())
			{
				//auto [itr,success]=
					material_instances.emplace(mat_inst, ProcessedMaterial{ mat_inst });
			}
		};
		for (auto& p_ro : mesh_render)
		{
			AddMatInst(material_instances, p_ro->material_instance);
		}
		for (auto& p_ro : skinned_mesh_render)
		{
			AddMatInst(material_instances, p_ro->material_instance);
		}
		for (auto& p_ro : *shared_gfx_state->instanced_ros)
		{
			AddMatInst(material_instances, p_ro.material_instance);
		}
		if(shared_gfx_state->particle_range)
			for (auto& part_range : *shared_gfx_state->particle_range)
			{
				AddMatInst(material_instances, part_range.material_instance);
			}
		//mat_inst_cache.ProcessCreation();
	}

void GraphicsState::CullAndAdd(const vector<RenderObject>& render_objects, const vector<AnimatedRenderObject>& skinned_render_objects)
{
	mesh_render.reserve(render_objects.size() + mesh_render.size());
	for (auto& ro : render_objects)
	{
		mesh_render.emplace_back(&ro);
	}
	skinned_mesh_render.reserve(skinned_render_objects.size() + skinned_mesh_render.size());
	for (auto& ro : skinned_render_objects)
	{
		skinned_mesh_render.emplace_back(&ro);
	}
}

void SharedGraphicsState::Init(const vector<LightData>& light_data, const vector<InstRenderObjects>& iro)
{
	lights = &light_data;
	instanced_ros = &iro;
	//shadow_maps.resize(light_data.size());
}

void SharedGraphicsState::ProcessMaterialInstances(hash_set<RscHandle<MaterialInstance>> active_materials)
{
	for (auto& mat_inst : active_materials)
	{
		GetGfxTimeLog().start("find");
		auto itr = material_instances.find(mat_inst);
		GetGfxTimeLog().end();
		if (itr == material_instances.end())
		{
			//auto [itr,success]=
			material_instances.emplace(mat_inst, ProcessedMaterial{ mat_inst });
		}
	}
}

void SharedGraphicsState::Reset()
{
	lights=nullptr;
	instanced_ros = nullptr;
	material_instances.clear();
	update_instructions.resize(0);
}

const vector<LightData>& SharedGraphicsState::Lights() const
{ 
	return *lights; 
}
/*
vector<shadow_map_t>& SharedGraphicsState::ShadowMaps()
{
	return shadow_maps;
}

const vector<shadow_map_t>& SharedGraphicsState::ShadowMaps() const
{
	return shadow_maps;
}
*/
void PreRenderData::Init(const vector<RenderObject>& render_objects, const vector<AnimatedRenderObject>& skinned_render_objects, const vector<SkeletonTransforms>& s_transforms, const vector<InstancedData>& inst_mesh_render_buffer)
{
	mesh_render.clear();
	skinned_mesh_render.clear();

	mesh_render.reserve(render_objects.size());
	for (auto& ro : render_objects)
	{
		mesh_render.emplace_back(&ro);
	}
	skinned_mesh_render.reserve(skinned_render_objects.size() );
	for (auto& ro : skinned_render_objects)
	{
		skinned_mesh_render.emplace_back(&ro);
	}
	this->skeleton_transforms = &s_transforms;

	inst_mesh_buffer = &inst_mesh_render_buffer;
}

ProcessedMaterial::ProcessedMaterial(RscHandle<MaterialInstance> inst)
{
	GetGfxTimeLog().start("ProcessedMaterial Ctor");
	inst_guid = inst.guid;
	{
		auto& mat_inst = *inst;
		[[maybe_unused]] auto& mat = *mat_inst.material;
		auto mat_cache = mat_inst.get_cache();
		using offset_t =size_t;
		data_block.reserve(std::max(data_block.size(), 32ui64));
		hash_table<string, std::pair<offset_t, size_t>> ubo_stored;
		ubo_stored.reserve(mat_cache.uniforms.size());
		hash_table<string, std::pair<offset_t, size_t>> tex_stored;
		tex_stored.reserve(mat_cache.uniforms.size());
		for (auto itr = mat_cache.uniforms.begin(); itr != mat_cache.uniforms.end(); ++itr)
		{
			if (mat_cache.IsUniformBlock(itr))
			{
				auto block = mat_cache.GetUniformBlock(itr);
				offset_t offset = data_block.size();
				size_t size = block.size();
				//Add to block
				data_block.append(block.data(), size);
				//store offset and stuff ( don't store ptrs due to invalidation)
				ubo_stored[itr->first]={ offset,size };
			}
			else if (mat_cache.IsImageBlock(itr))
			{
				auto block = mat_cache.GetImageBlock(itr);
				offset_t offset = texture_block.size();
				size_t size = block.size();
				//Add to block
				texture_block.insert(texture_block.end(),block.begin(),block.end());
				auto name = itr->first.substr(0, itr->first.find_first_of('['));
				//store offset and stuff ( don't store ptrs due to invalidation)
				tex_stored[name] = { offset,size };
			}
		}
		for (auto& [name, block_range] : ubo_stored)
		{
			auto& [offset, size] = block_range;
			auto ptr = data_block.data();
			ubo_table[name] = string_view(ptr + offset, size);
		}
		for (auto& [name, block_range] : tex_stored)
		{
			auto& [offset, size] = block_range;
			auto ptr = texture_block.data();
			tex_table[name] = span<RscHandle<Texture>>{ ptr + offset, ptr + offset + size };
		}
		shader = mat._shader_program;
	}
	GetGfxTimeLog().end();
}

void DbgDrawCall::RegisterBuffer([[maybe_unused]]DbgBufferType type, uint32_t binding, buffer_info info)
{
	mesh_buffer[binding] = info;
}

void DbgDrawCall::SetNum(uint32_t num_inst, uint32_t num_vert) { num_instances = num_inst; num_vert = num_vertices; }

void DbgDrawCall::Bind(vk::CommandBuffer cmd_buffer) const
{
	/*auto& buffers = mesh_buffer.find(DbgBufferType::ePerVtx)->second;
	if (!prev || !(prev->mesh_buffer.find(DbgBufferType::ePerVtx)->second == buffers))
	{
	for (auto& [binding, buffer] : buffers)
	{
	cmd_buffer.bindVertexBuffers(binding,buffer.buffer,buffer.offset);
	}
	}*/
	auto& buffers = mesh_buffer;
	for (auto& [binding, buffer] : buffers)
	{
		cmd_buffer.bindVertexBuffers(binding, buffer.buffer, buffer.offset);
	}
}

void DbgDrawCall::Draw(vk::CommandBuffer cmd_buffer) const
{

	if (num_indices)
	{
		cmd_buffer.bindIndexBuffer(index_buffer.buffer, index_buffer.offset, vk::IndexType::eUint16);
		cmd_buffer.drawIndexed(num_indices, num_instances, index_buffer.offset, 0, 0);
	}
	else
		cmd_buffer.draw(num_vertices, num_instances, 0, 0);
}

}