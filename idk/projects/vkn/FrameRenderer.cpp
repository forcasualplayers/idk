#include "pch.h"
#include <forward_list>
#include "FrameRenderer.h"
#include <core/Core.h>
#include <gfx/GraphicsSystem.h> //GraphicsState
#include <vkn/VulkanPipeline.h>
#include <vkn/VulkanMesh.h>
#include <file/FileSystem.h>
#include <vkn/ShaderModule.h>
#include <vkn/PipelineManager.h>
#include <math/matrix_transforms.inl>
#include <vkn/GraphicsState.h>
#include <gfx/RenderTarget.h>
#include <vkn/VknFrameBuffer.h>
#include <vkn/VknRenderTarget.h>
#include <math/matrix.inl>
#include <gfx/Light.h>

#include <gfx/MeshRenderer.h>
#include <anim/SkinnedMeshRenderer.h>
#include <vkn/vulkan_enum_info.h>
#include <vkn/VknTexture.h>
#include <vkn/VulkanHashes.h>

#include <vkn/VknCubemap.h>

#include <gfx/ViewportUtil.h>
#include <vkn/VknCubeMapLoader.h>
#include <vkn/VulkanCbmLoader.h>
#include <ui/Canvas.h>

#include <vkn/vector_buffer.h>

#include <cstdint>

#include <vkn/DescriptorUpdateData.h>
#include <thread>
#include <mutex>
#include <queue>
#include <parallel/ThreadPool.h>
#include <vkn/PipelineBinders.inl>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>
#include <vkn/UboManager.inl>
#include <math/color.inl>
#include <ds/result.inl>

#include <vkn/DebugUtil.h>

#include <vkn/ColorPickRenderer.h>

#include <vkn/graph_test.h>

#include <vkn/FrameGraph.h>

#include <vkn/RenderBundle.h>

#include <vkn/renderpasses/DeferredPasses.h>
#include <vkn/renderpasses/ShadowPasses.h>
#include <vkn/renderpasses/MaskedPass.h>
#include <vkn/renderpasses/DebugDrawPass.h>
#include <vkn/renderpasses/PostDeferredPasses.h>

#include <vkn/MaterialInstanceCache.h>

#include <vkn/time_log.h>
#include <vkn/Stopwatch.h>

namespace idk::vkn
{
#define CreateRenderThread() std::make_unique<NonThreadedRender>()

	struct FrameRenderer::PImpl
	{
		hash_table<RscHandle<Texture>, RscHandle<VknFrameBuffer>> deferred_buffers;
		vector<ColorPickRequest> color_pick_requests;
		ColorPickRenderer color_picker;
		FrameGraph graph{};
		FrameGraph gtest_graph{};
		gt::GraphTest test{gtest_graph};

		MaterialInstanceCache mat_inst_cache;

		renderpasses::ParticleRenderer particle_renderer;
		renderpasses::TextMeshRenderer text_mesh_renderer;

		gfxdbg::FgRscLifetimes _dbg_lifetimes;

		uint32_t gfx_state_index = 0;

		uint32_t testing = 0;

	};

	//from: https://riptutorial.com/cplusplus/example/30142/semaphore-cplusplus-11
	class Semaphore {
	public:
		Semaphore(int count_ = 0)
			: count(count_)
		{
		}

		inline void notify() {
			std::unique_lock<std::mutex> lock(mtx);
			count++;
			//notify the waiting thread
			cv.notify_one();
		}
		inline void wait() {
			std::unique_lock<std::mutex> lock(mtx);
			while (count == 0) {
				//wait on the mutex until notify is called
				cv.wait(lock);
			}
			count--;
		}
	private:
		std::mutex mtx;
		std::condition_variable cv;
		int count;
	};
	/*
	template<typename job_t>
	struct job_queue
	{
		using container_t = std::list<job_t>;
		using iterator_t = typename container_t::iterator;
		container_t container;
		iterator_t begin, end;
		std::mutex fml;
		job_queue() :begin{ container.begin() }, end{ container.end() }{}
		void push(job_t job)
		{
			fml.lock();
			if (container.end() == end)
			{
				container.push_back(std::move(job));
				end = container.end();
				if (begin == container.end())
					begin = container.begin();
			}
			else
				new (&(*end++)) job_t{ std::move(job) };
			if (begin == container.end())
			{
				begin = end;
				--begin;
			}
			fml.unlock();
		}
		job_t pop()
		{
			fml.lock();
			job_t result = std::move(*begin);
			begin->~job_t();
			++begin;
			fml.unlock();
			return result;
		}
		bool empty()const
		{
			return begin == end;
		}
		void reset()
		{
			begin = end = container.begin();
		}
	};*/

	template<typename RT, typename ...Args>
	auto GetFuture(RT(*func)(Args...)) -> decltype(Core::GetThreadPool().Post(func, std::declval<Args>()...));


	class FrameRenderer::ThreadedRender : public FrameRenderer::IRenderThread
	{
	public:
		static void RunFunc(FrameRenderer* _renderer, const GraphicsState* m, RenderStateV2* rs,std::atomic<int>*counter) noexcept
		{
			try
			{
				(*counter)--;
				_renderer->RenderGraphicsState(*m, *rs);
			}catch (std::exception& e)
			{
				LOG_TO(LogPool::GFX, "Exception thrown during threaded render: %s", e.what());
			}
			catch (vk::Error& e)
			{
				LOG_TO(LogPool::GFX, "Vk Exception thrown during threaded render: %s", e.what());
			}
			catch (...)
			{
				LOG_TO(LogPool::GFX, "Unknown Exception thrown during threaded render");
			}
		}
		using Future_t =decltype(GetFuture(ThreadedRender::RunFunc));
		void Init(FrameRenderer* renderer)
		{
			
			_renderer = renderer;
		}
		void Render(const GraphicsState& state, RenderStateV2& rs)override
		{
			counter++;
			futures.emplace_back(Core::GetThreadPool().Post(&RunFunc, _renderer, &state, &rs,&counter));
		}
//// 
		void Join() override
		{
			for(auto& future : futures)
				future.get();
			futures.clear();
		}
	private:
		FrameRenderer* _renderer;
		std::atomic<int> counter{};
		vector<Future_t> futures;
		
		std::thread my_thread;
	};

	using collated_bindings_t = hash_table < uint32_t, vector<ProcessedRO::BindingInfo>>;//Set, bindings
	std::pair<ivec2, uvec2> ComputeVulkanViewport(const vec2& sz, const rect& vp)
	{
		auto pair = ComputeViewportExtents(sz, vp);
		auto& [offset, size] = pair;
		//offset.y = sz.y - offset.y - size.y;
		//offset = ivec2{ (translate(ivec2{ 0,sz.y }) * tmat<int,3,3> { scale(ivec2{ 1,-1 }) }).transpose()* ivec3 { offset,1 } };//  -ivec2{ 0,size.y };
		return pair;
	}

	template<typename T>
	ProcessedRO::BindingInfo CreateBindingInfo(const UboInfo& obj_uni, const T& val, FrameRenderer::DsBindingCount& collated_layouts, UboManager& ubo_manager)
	{
		collated_layouts[obj_uni.layout].first = vk::DescriptorType::eUniformBuffer;
		//collated_layouts[obj_uni.layout].second++;
		auto&& [trf_buffer, trf_offset] = ubo_manager.Add(val);
		//collated_bindings[obj_uni.set].emplace_back(
		return ProcessedRO::BindingInfo
		{
			obj_uni.binding,
			trf_buffer,
			trf_offset,
			0,
			obj_uni.size,
			obj_uni.layout
		};
		//);
	}

	template<typename T>
	void PreProcUniform(const UboInfo& obj_uni, const T& val, FrameRenderer::DsBindingCount& collated_layouts, collated_bindings_t& collated_bindings, UboManager& ubo_manager)
	{
		//collated_layouts[obj_uni.layout].first = vk::DescriptorType::eUniformBuffer;
		////collated_layouts[obj_uni.layout].second++;
		//auto&& [trf_buffer, trf_offset] = ubo_manager.Add(val);
		auto& bindings = collated_bindings[obj_uni.set];
		bindings.emplace_back(
			CreateBindingInfo(obj_uni, val, collated_layouts, ubo_manager)
			//ProcessedRO::BindingInfo
			//{
			//	obj_uni.binding,
			//	trf_buffer,
			//	trf_offset,
			//	0,
			//	obj_uni.size
			//}
		);
	}
	template<typename WTF>
	void PreProcUniform(const UboInfo& obj_uni, uint32_t index, RscHandle<Texture> val, FrameRenderer::DsBindingCount& collated_layouts, collated_bindings_t& collated_bindings)
	{
		collated_layouts[obj_uni.layout].first = vk::DescriptorType::eCombinedImageSampler;
		//collated_layouts[obj_uni.layout].second++;
		//auto&& [trf_buffer, trf_offset] = ubo_manager.Add(val);
		auto& texture = val.as<VknTexture>();
		auto& bindings = collated_bindings[obj_uni.set];
		bindings.emplace_back(
			ProcessedRO::BindingInfo
			{
				obj_uni.binding,
				ProcessedRO::image_t{texture.ImageView(),*texture.sampler,vk::ImageLayout::eGeneral},
				0,
				index,
				obj_uni.size,
				obj_uni.layout
			}
		);
	}
	template<typename WTF>
	void PreProcUniform(const UboInfo& obj_uni,uint32_t index, RscHandle<RenderTarget> val, FrameRenderer::DsBindingCount& collated_layouts, collated_bindings_t& collated_bindings)
	{
		//collated_layouts[obj_uni.layout].first = vk::DescriptorType::eCombinedImageSampler;
		//collated_layouts[obj_uni.layout].second++;
		//auto&& [trf_buffer, trf_offset] = ubo_manager.Add(val);
		PreProcUniform<WTF>(obj_uni,index, val->GetDepthBuffer(),collated_layouts, collated_bindings);
	}
	template<typename lol=void>
	void BindBones(const UboInfo& info,const AnimatedRenderObject& aro, const vector<SkeletonTransforms>& bones, UboManager& ubos, FrameRenderer::DsBindingCount & collated_layouts, collated_bindings_t & collated_bindings)
	{
		auto&&[buffer,offset]=ubos.Add(bones[aro.skeleton_index]);
		auto& bindings = collated_bindings[info.set];
		bindings.emplace_back(info.binding, buffer, offset, 0, info.size);
	}
	struct GraphicsStateInterface
	{
		//RscHandle<ShaderProgram>             mesh_vtx;
		//RscHandle<ShaderProgram>             skinned_mesh_vtx;
		LayerMask mask = ~(LayerMask{});
		array<RscHandle<ShaderProgram>, VertexShaders::VMax>   renderer_vertex_shaders;
		array<RscHandle<ShaderProgram>, FragmentShaders::FMax>   renderer_fragment_shaders;
		const vector<const RenderObject*>*         mesh_render;
		const vector<const AnimatedRenderObject*>* skinned_mesh_render;
		const vector<InstRenderObjects>* inst_ro;
		std::variant<GraphicsSystem::RenderRange, GraphicsSystem::LightRenderRange> range;
		const SharedGraphicsState* shared_state = {};
		GraphicsStateInterface() = default;

		GraphicsStateInterface(const CoreGraphicsState& state)
		{
			mesh_render = &state.mesh_render;
			skinned_mesh_render = &state.skinned_mesh_render;
			shared_state = state.shared_gfx_state;
			inst_ro = state.shared_gfx_state->instanced_ros;
		}
		GraphicsStateInterface(const GraphicsState& state) : GraphicsStateInterface{static_cast<const CoreGraphicsState&>(state)}
		{
			renderer_vertex_shaders =   state.shared_gfx_state->renderer_vertex_shaders;
			renderer_fragment_shaders = state.shared_gfx_state->renderer_fragment_shaders;
			range = state.range;

			mask = state.camera.culling_flags;
		}
		GraphicsStateInterface(const PreRenderData& state) : GraphicsStateInterface{ static_cast<const CoreGraphicsState&>(state) }
		{
			renderer_vertex_shaders   = state.shared_gfx_state->renderer_vertex_shaders;
			renderer_fragment_shaders = state.shared_gfx_state->renderer_fragment_shaders;
		}
		GraphicsStateInterface(const PostRenderData& state) : GraphicsStateInterface{ static_cast<const CoreGraphicsState&>(state) }
		{
			renderer_vertex_shaders   = state.shared_gfx_state->renderer_vertex_shaders;
			renderer_fragment_shaders = state.shared_gfx_state->renderer_fragment_shaders;
		}
	};


	PipelineThingy ProcessRoUniforms(const GraphicsStateInterface& state, UboManager& ubo_manager,StandardBindings& binders)
	{
		auto& mesh_vtx            = state.renderer_vertex_shaders[VNormalMesh];
		auto& skinned_mesh_vtx    = state.renderer_vertex_shaders[VSkinnedMesh];
		auto& skinned_mesh_render = *state.skinned_mesh_render;

		//auto& binders = *binder;
		PipelineThingy the_interface{};
		the_interface.SetRef(ubo_manager);

		the_interface.BindShader(ShaderStage::Vertex, mesh_vtx);
		the_interface.reserve(state.mesh_render->size() + state.skinned_mesh_render->size());
		binders.Bind(the_interface);
		{
			//auto range_opt = state.range;
			//if (!range_opt)
			//	range_opt = GraphicsSystem::RenderRange{ CameraData{},0,state.inst_ro->size() };
			
			//auto& inst_draw_range = *range_opt;
			std::visit([&](auto& inst_draw_range) {
				for (auto itr = state.inst_ro->data() + inst_draw_range.inst_mesh_render_begin,
					end = state.inst_ro->data() + inst_draw_range.inst_mesh_render_end;
					itr != end; ++itr
					)
				{
					auto& dc = *itr;
					auto& mat_inst = *dc.material_instance;
					if (mat_inst.material && !binders.Skip(the_interface,dc))
					{
						binders.Bind(the_interface, dc);
						the_interface.BindMeshBuffers(dc);
						the_interface.BindAttrib(4,state.shared_state->inst_mesh_render_buffer.buffer(),0);
						the_interface.FinalizeDrawCall(dc, dc.num_instances, dc.instanced_index);
					}
				}
			},state.range);

		}
		{
			const vector<const AnimatedRenderObject*>& draw_calls = skinned_mesh_render;
			the_interface.BindShader(ShaderStage::Vertex, skinned_mesh_vtx);
			binders.Bind(the_interface);
			for (auto& ptr_dc : draw_calls)
			{
				auto& dc = *ptr_dc;
				auto& mat_inst = *dc.material_instance;
				if (mat_inst.material && dc.layer_mask&state.mask && !binders.Skip(the_interface, dc))
				{
					binders.Bind(the_interface, dc);
					if(!the_interface.BindMeshBuffers(dc))
						continue;
					the_interface.FinalizeDrawCall(dc);

				}
			}//End of draw_call loop
		}


		return std::move(the_interface);
	}
	PipelineThingy ProcessRoUniforms(const GraphicsState& state, UboManager& ubo_manager, StandardBindings& binders)
	{
		return ProcessRoUniforms(GraphicsStateInterface{ state }, ubo_manager, binders);
	}
	template<typename T,typename...Args>
	using has_setstate = decltype(std::declval<T>().SetState(std::declval<Args>()...));
	//Possible Idea: Create a Pipeline object that tracks currently bound descriptor sets
	PipelineThingy FrameRenderer::ProcessRoUniforms(const GraphicsState& state, UboManager& ubo_manager)
	{
		{
			PbrFwdMaterialBinding binders;
			binders.for_each_binder<has_setstate>([](auto& binder, const GraphicsState& state) {binder.SetState(state); }, state);
			return vkn::ProcessRoUniforms(state,ubo_manager,binders);

		}

	}

	vk::Framebuffer GetFrameBuffer(const CameraData& camera_data, uint32_t)
	{
		//TODO Actually get the framebuffer from camera_data
		//auto& e = camera_data.render_target.as<VknFrameBuffer>();
		return camera_data.render_target.as<VknRenderTarget>().Buffer();
	}



	RscHandle<ShaderProgram> LoadShader(string filename)
	{
		return *Core::GetResourceManager().Load<ShaderProgram>(filename, false);
	}
	void FrameRenderer::Init(VulkanView* view, vk::CommandPool cmd_pool) {
		_pimpl = std::make_shared<PImpl>();
		//Todo: Initialize the stuff
		_view = view;
		//Do only the stuff per frame
		uint32_t num_fo = 1;
		uint32_t num_concurrent_states = 1;

		_cmd_pool = cmd_pool;
		auto device = *View().Device();
		auto pri_buffers = device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ cmd_pool,vk::CommandBufferLevel::ePrimary, num_fo }, vk::DispatchLoaderDefault{});
		_pri_buffer = std::move(pri_buffers[0]);
		auto t_buffers = device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ cmd_pool,vk::CommandBufferLevel::eSecondary, num_fo }, vk::DispatchLoaderDefault{});
		{
			_transition_buffer = std::move(t_buffers[0]);
		}

		GrowStates(_states,num_concurrent_states);
		//Temp
		for (auto i = num_concurrent_states; i-- > 0;)
		{
			auto thread = CreateRenderThread();
			thread->Init(this);
			_render_threads.emplace_back(std::move(thread));
		}
		_pre_render_complete = device.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
		_post_render_complete = device.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
		_convoluter.pipeline_manager(*_pipeline_manager);
		_convoluter.Init(
			Core::GetSystem<GraphicsSystem>().renderer_vertex_shaders[VPBRConvolute],
			Core::GetSystem<GraphicsSystem>().renderer_fragment_shaders[FPBRConvolute],
			Core::GetSystem<GraphicsSystem>().renderer_geometry_shaders[GSinglePassCube]
		);
		_particle_renderer.InitConfig();
		_font_renderer.InitConfig();
		_canvas_renderer.InitConfig();
	}
	void FrameRenderer::SetPipelineManager(PipelineManager& manager)
	{
		_pipeline_manager = &manager;
	}
//
	void FrameRenderer::PreRenderGraphicsStates(const PreRenderData& state, uint32_t frame_index)
	{
		_pimpl->mat_inst_cache.UpdateUniformBuffers();
		UNREFERENCED_PARAMETER(frame_index);
		_pimpl->graph.Reset();

		auto& lights = (*state.shadow_ranges);
		const size_t num_conv_states = 1;
		const size_t num_instanced_buffer_state = 1;
		const size_t num_color_pick_states = 1;
		
		auto total_pre_states = lights.size() + state.active_dir_lights.size() + num_conv_states+ num_instanced_buffer_state + num_color_pick_states;
		GrowStates(_pre_states, total_pre_states);
		for (auto& pre_state : _pre_states)
		{
			pre_state.Reset();
		}
		size_t curr_state = 0;


		std::optional<vk::Semaphore> copy_semaphore{};
		{

			dbg::BeginLabel(View().GraphicsQueue(), "PreRender GraphicsStates", color{ 0.3f,0.0f,0.3f });
			auto copy_state_ind = curr_state++;
			auto& copy_state = _pre_states[copy_state_ind];

			auto& instanced_data = *state.inst_mesh_buffer;
			
			copy_semaphore = *copy_state.signal.render_finished;

			auto cmd_buffer = copy_state.CommandBuffer();
			cmd_buffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
			if (instanced_data.size())
			{
				state.shared_gfx_state->inst_mesh_render_buffer.resize(hlp::buffer_size(instanced_data));
				state.shared_gfx_state->inst_mesh_render_buffer.update<const InstancedData>(vk::DeviceSize{ 0 }, instanced_data, cmd_buffer);
				dbg::NameObject(state.shared_gfx_state->inst_mesh_render_buffer.buffer(), "InstMeshrenderBuffer");
				for (auto& [buffer, data,offset] : state.shared_gfx_state->update_instructions)
				{
					if (data.size())
					{
						buffer->update(offset,s_cast<uint32_t>(data.size()),cmd_buffer,std::data(data));
					}
				}
			}
			if (state.shared_gfx_state->particle_data && state.shared_gfx_state->particle_data->size())
			{
				auto& particle_data = *state.shared_gfx_state->particle_data;
				auto& buffer = state.shared_gfx_state->particle_buffer;
				buffer.resize(hlp::buffer_size(particle_data));
				buffer.update<const ParticleObj>(0, particle_data, cmd_buffer);
			}
			if (state.shared_gfx_state->fonts_data && state.shared_gfx_state->fonts_data->size())
			{
				auto& font_data = *state.shared_gfx_state->fonts_data;
				auto& buffer = state.shared_gfx_state->font_buffer;
				
				buffer.resize(font_data.size());
				for (unsigned i = 0; i < font_data.size(); ++i)
				{
					auto& b = buffer[i];
					b.resize(hlp::buffer_size(font_data[i].coords));
					b.update<const FontPoint>(0, font_data[i].coords, cmd_buffer);
				}
				
			}

			cmd_buffer.end();
			//copy_state.FlagRendered();//Don't flag, we want to submit this separately.

			vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eTransfer };
			vk::SubmitInfo submit_info
			{
				0
				,nullptr
				,waitStages
				,1,&copy_state.CommandBuffer()
				,1,&*copy_semaphore
			};

			auto queue = View().GraphicsQueue();
			queue.submit(submit_info, vk::Fence{}, vk::DispatchLoaderDefault{});
		}

		{
			auto& color_picker = _pimpl->color_picker;
			auto& requests = _pimpl->color_pick_requests;
			auto& shared_gs = *state.shared_gfx_state;
			auto& rs = _pre_states[curr_state++];
			auto cmd_buffer = rs.CommandBuffer();
			cmd_buffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
			color_picker.PreRender(requests, shared_gs, state.inst_mesh_buffer->size(), cmd_buffer);
			color_picker.Render(requests, shared_gs, rs);
		}
		//Do post pass here
		//Canvas pass

		//For each camera, get the fustrum-bounded sphere

		//For each light, calculate the cascade center and cascade radius to
		//calculate the cascade projection matrix
		auto& cameras = *state.cameras;

		auto& rs = _pre_states[curr_state];
		{

			auto dispatcher = vk::DispatchLoaderDefault{};
			vk::CommandBuffer cmd_buffer = rs.CommandBuffer();
			vk::CommandBufferBeginInfo begin_info{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit,nullptr };
			cmd_buffer.begin(begin_info, dispatcher);
			//renderpasses::Add445_75Test(_pimpl->graph, state);

			for (auto& range : lights)
			{
				PreRenderShadow(range, state);
			}
			cmd_buffer.end();
			rs.ubo_manager.UpdateAllBuffers();
		}
		++curr_state;
		
		//TODO: Submit the command buffers

		vector<vk::CommandBuffer> buffers{};

		if (cameras.size()==0)
		{
			auto& convolute_state = _pre_states[curr_state];
			_convoluter.pipeline_manager(*_pipeline_manager);
			_convoluter.BeginQueue(convolute_state.ubo_manager,{});
			for (auto& camera : cameras)
			{
				std::visit([&](auto& clear)
					{
						if constexpr (std::is_same_v<std::decay_t<decltype(clear)>, RscHandle<CubeMap>>)
						{
							RscHandle<CubeMap> cubemap = clear;
							VknCubemap& cm = cubemap.as<VknCubemap>();
							RscHandle<VknCubemap> conv = cm.GetConvoluted();
							if (!conv->is_convoluted && conv != RscHandle<VknCubemap>{})
							{	
								convolute_state.FlagRendered();
								_convoluter.QueueConvoluteCubeMap(RscHandle<CubeMap>{cubemap}, RscHandle<CubeMap>{conv});
								conv->is_convoluted = true;
							}
						}
					}, camera.clear_data);
			}
			auto cmd_buffer = convolute_state.CommandBuffer();
			cmd_buffer.begin(vk::CommandBufferBeginInfo
				{
					vk::CommandBufferUsageFlagBits::eOneTimeSubmit
				}
			);
			_convoluter.ProcessQueue(cmd_buffer);
			cmd_buffer.end();

		}

		for (auto& pre_state : _pre_states)
		{
			if (pre_state.has_commands)
				buffers.emplace_back(pre_state.CommandBuffer());
		}

		vector<vk::Semaphore> arr_ready_sem{ *_pre_render_complete };
		vector<vk::Semaphore> arr_wait_sem {};
		if (copy_semaphore)
			arr_wait_sem.emplace_back(*copy_semaphore);
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands };


		vk::SubmitInfo submit_info
		{
			hlp::arr_count(arr_wait_sem) ,std::data(arr_wait_sem)
			,waitStages
			,hlp::arr_count(buffers),std::data(buffers)
			,hlp::arr_count(arr_ready_sem) ,std::data(arr_ready_sem)
		};

		auto queue = View().GraphicsQueue();
		queue.submit(submit_info, vk::Fence{}, vk::DispatchLoaderDefault{});
		dbg::EndLabel(queue);
	}
	VulkanView& View();
//// 
	void RenderPipelineThingy(
		[[maybe_unused]] const SharedGraphicsState& shared_state,
		PipelineThingy&     the_interface      ,
		PipelineManager&    pipeline_manager   ,
		vk::CommandBuffer   cmd_buffer         , 
		const vector<vec4>& clear_colors       ,
		vk::Framebuffer     frame_buffer       ,
		RenderPassObj       rp                 ,
		bool                has_depth_stencil  ,
		vk::Rect2D          render_area        ,
		vk::Rect2D          viewport           ,
		uint32_t            frame_index      
		)
	{
		VulkanPipeline* prev_pipeline = nullptr;
		vector<RscHandle<ShaderProgram>> shaders;

		std::array<float, 4> a{};

		//auto& cd = std::get<vec4>(state.camera.clear_data);
		//TODO grab the appropriate framebuffer and begin renderpass
		
		vector<vk::ClearValue> clear_value(clear_colors.size());
		for (size_t i = 0; i < clear_value.size(); ++i)
		{
			clear_value[i] = vk::ClearColorValue{ r_cast<const std::array<float,4>&>(clear_colors[i]) };
		}

		vk::RenderPassBeginInfo rpbi
		{
			*rp, frame_buffer,
			render_area,hlp::arr_count(clear_value),std::data(clear_value)
		};


		cmd_buffer.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		auto& processed_ro = the_interface.DrawCalls();
		shared_ptr<const pipeline_config> prev_config{};
		for (auto& p_ro : processed_ro)
		{
			bool is_mesh_renderer = p_ro.vertex_shader == Core::GetSystem<GraphicsSystem>().renderer_vertex_shaders[VNormalMesh];
			auto& obj = p_ro.Object();
			if (p_ro.rebind_shaders||prev_config!=obj.config)
			{
				prev_config = obj.config;
				shaders.resize(0);
				if (p_ro.frag_shader)
					shaders.emplace_back(*p_ro.frag_shader);
				if (p_ro.vertex_shader)
					shaders.emplace_back(*p_ro.vertex_shader);
				if (p_ro.geom_shader)
					shaders.emplace_back(*p_ro.geom_shader);

				auto config = *obj.config;
				config.viewport_offset = ivec2{ s_cast<uint32_t>(viewport.offset.x),s_cast<uint32_t>(viewport.offset.y) };
				config.viewport_size = uvec2{ s_cast<uint32_t>(viewport.extent.width),s_cast<uint32_t>(viewport.extent.height) };

				if (is_mesh_renderer)
					config.buffer_descriptions.emplace_back(
						buffer_desc
						{
							buffer_desc::binding_info{ std::nullopt,sizeof(mat4) * 2,VertexRate::eInstance},
							{buffer_desc::attribute_info{AttribFormat::eMat4,4,0,true},
							 buffer_desc::attribute_info{AttribFormat::eMat4,8,sizeof(mat4),true}
							 }
						}
				);
				auto& pipeline = pipeline_manager.GetPipeline(config, shaders, frame_index, rp, has_depth_stencil);
				pipeline.Bind(cmd_buffer, View());
				SetViewport(cmd_buffer, *config.viewport_offset, *config.viewport_size);
				prev_pipeline = &pipeline;
			}
			auto& pipeline = *prev_pipeline;
			//TODO Grab everything and render them
			//auto& mat = obj.material_instance.material.as<VulkanMaterial>();
			//auto& mesh = obj.mesh.as<VulkanMesh>();
			{
				uint32_t set = 0;
				for (auto& ods : p_ro.descriptor_sets)
				{
					if (ods)
					{
						auto& ds = *ods;
						cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline.pipelinelayout, set, ds, {});
					}
					++set;
				}
			}

			//auto& renderer_req = *obj.renderer_req;

			for (auto&& [location, attrib] : p_ro.attrib_buffers)
			{
				cmd_buffer.bindVertexBuffers(*pipeline.GetBinding(location), attrib.buffer, vk::DeviceSize{ attrib.offset }, vk::DispatchLoaderDefault{});
			}
			auto& oidx = p_ro.index_buffer;
			if (oidx)
			{
				cmd_buffer.bindIndexBuffer(oidx->buffer, 0, oidx->index_type, vk::DispatchLoaderDefault{});
				cmd_buffer.drawIndexed(s_cast<uint32_t>(p_ro.num_vertices), static_cast<uint32_t>(p_ro.num_instances), 0, 0, static_cast<uint32_t>(p_ro.inst_offset), vk::DispatchLoaderDefault{});
			}
			else
			{
				cmd_buffer.draw(s_cast<uint32_t>(p_ro.num_vertices), s_cast<uint32_t>(p_ro.num_instances), 0, s_cast<uint32_t>(p_ro.inst_offset), vk::DispatchLoaderDefault{});
			}
		}
	}
//#pragma optimize("", off)
	void FrameRenderer::PreRenderShadow(GraphicsSystem::LightRenderRange shadow_range, const PreRenderData& state)
	{
		renderpasses::AddShadowPass(_pimpl->graph, shadow_range, state);
		return;
	}

	void FrameRenderer::PostRenderCanvas(size_t& ui_elem_count, size_t& text_count, RscHandle<RenderTarget> rr, const vector<UIRenderObject>& canvas_data, const PostRenderData& state, RenderStateV2& rs, uint32_t frame_index)
	{
		auto& rt = rr.as<VknRenderTarget>();
		//auto& swapchain = view.Swapchain();
		auto dispatcher = vk::DispatchLoaderDefault{};
		vk::CommandBuffer cmd_buffer = rs.CommandBuffer();
		vk::CommandBufferBeginInfo begin_info{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit,nullptr };
		GraphicsStateInterface gsi = { state };

		//Generate this pipeline thingy
		PipelineThingy the_interface{};
		
		the_interface.SetRef(rs.ubo_manager);
		_canvas_renderer.DrawCanvas(ui_elem_count, text_count, the_interface, state, rs, canvas_data);
		the_interface.GenerateDS(rs.dpools);

		cmd_buffer.begin(begin_info, dispatcher);
		//auto sz = light.light_map->DepthAttachment().buffer->Size();
		//auto sz = wewew.DepthAttachment().buffer->Size();
		
		vk::Framebuffer fb = rt.Buffer();
		auto  rp = rt.GetRenderPass(false, false);

		rt.PrepareDraw(cmd_buffer);
		
		auto sz = rt.Size();
		vk::Rect2D render_area
		{
			vk::Offset2D{0,0},
			vk::Extent2D{s_cast<uint32_t>(rt.size.x),s_cast<uint32_t>(rt.size.y)}
		};

		vector<vec4> clear_colors
		{
			vec4{1},
			vec4{0.f}
		};
		if (the_interface.DrawCalls().size())
			rs.FlagRendered();
		RenderPipelineThingy(*state.shared_gfx_state, the_interface, GetPipelineManager(), cmd_buffer, clear_colors, fb, rp, true, render_area, render_area, frame_index);

		rs.ubo_manager.UpdateAllBuffers();
		cmd_buffer.endRenderPass();
		cmd_buffer.end();
	}
//
	namespace gt
	{
		void GraphDeferredTest(const CoreGraphicsState& gfx_state, RenderStateV2& rs);
	}
	dbg::time_log& GetGfxTimeLog();

	void FrameRenderer::RenderGraphicsStates(const vector<GraphicsState>& gfx_states, uint32_t frame_index)
	{
		//GetGfxTimeLog().push_level("RenderGraphicsState");
		_pimpl->gfx_state_index = 0;
		_current_frame_index = frame_index;
		//Update all the resources that need to be updated.
		auto& curr_frame = *this;
		const auto frame_graph_count = 1;
		GrowStates(_states, gfx_states.size() + frame_graph_count);
		size_t num_concurrent = curr_frame._render_threads.size();
		auto& pri_buffer = curr_frame._pri_buffer;
		auto& transition_buffer = curr_frame._transition_buffer;
		auto queue = View().GraphicsQueue();
		auto& swapchain = View().Swapchain();
		for (auto& state : curr_frame._states)
		{
			state.Reset();
		}
		bool rendered = false;
		{
			;
			for (size_t i = 0; i  < gfx_states.size(); i += num_concurrent)
			{
				auto curr_concurrent = std::min(num_concurrent,gfx_states.size()-i);
				//Spawn/Assign to the threads
				for (size_t j = 0; j < curr_concurrent; ++j) {
					auto& state = gfx_states[i + j];
					auto& rs = _states[i + j];
					//gt::GraphDeferredTest(state, rs);
					_pimpl->testing=1;// |= (i == 0 && j == 0);
						
					_render_threads[j]->Render(state, rs);
					rendered = true;
					//TODO submit command buffer here and signal the framebuffer's stuff.
					//TODO create two renderpasses, detect when a framebuffer is used for the first time, use clearing renderpass for the first and non-clearing for the second onwards.
					//OR sort the gfx states so that we process all the gfx_states that target the same render target within the same command buffer/render pass.
					//RenderGraphicsState(state, curr_frame.states[j]);//We may be able to multi thread this
				}
			}

		}
		//Join with all the threads
		for (size_t j = 0; j < _render_threads.size(); ++j) {
			auto& thread = _render_threads[j];
			thread->Join();
		}



		{
			dbg::stopwatch timer;
			timer.start();
			GetGfxTimeLog().start("Render Graph");
			GetGfxTimeLog().start("Compile");
			_pimpl->graph.Compile();
			GetGfxTimeLog().end_then_start("AllocRsc & BuildRP");
			const auto& graph = _pimpl->graph;

			const auto kNameShowDbgLifetimes = "Show Dbg Lifetimes";
			auto& gfx_sys = Core::GetSystem<GraphicsSystem>();
			gfx_sys.extra_vars.SetIfUnset(kNameShowDbgLifetimes, false);
			if(*gfx_sys.extra_vars.Get<bool>(kNameShowDbgLifetimes))
				_pimpl->graph.GetLifetimeManager().DebugArrange(_pimpl->_dbg_lifetimes, graph.GetResourceManager());

			gfx_sys.extra_vars.Set(gfxdbg::kLifetimeName, static_cast<void*>(&_pimpl->_dbg_lifetimes));
			auto& state = _states.back();
			_pimpl->graph.SetDefaultUboManager(state.ubo_manager);
			_pimpl->graph.AllocateResources();
			_pimpl->graph.BuildRenderPasses();
			_pimpl->graph.SetPipelineManager(*this->_pipeline_manager);
			GetGfxTimeLog().end_then_start("Execute");
			_pimpl->graph.Execute();
			GetGfxTimeLog().end_then_start("ProcBatches");


			RenderBundle rb{ state.CommandBuffer() ,state.dpools };
			rb._cmd_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			dbg::BeginLabel(rb._cmd_buffer, "Framegraph in RenderGraphicsStates starting.", color{ 0.3f, 0.4f, 0.f });
			_pimpl->graph.ProcessBatches(rb);
			GetGfxTimeLog().end_then_start("Update Ubo Buffers");
			state.ubo_manager.UpdateAllBuffers();
			GetGfxTimeLog().end_then_start("Misc");
			dbg::EndLabel(rb._cmd_buffer);
			rb._cmd_buffer.end();
			state.FlagRendered();
			timer.stop();
			GetGfxTimeLog().end();//Misc
			GetGfxTimeLog().end();//Render Graph
		}
		pri_buffer->reset({}, vk::DispatchLoaderDefault{});
		vector<vk::CommandBuffer> buffers{};
		for (auto& state : curr_frame._states)
		{
			if (state.has_commands)
				buffers.emplace_back(state.CommandBuffer());
		}
		vk::CommandBufferBeginInfo begin_info{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		pri_buffer->begin(begin_info, vk::DispatchLoaderDefault{});
		//if(buffers.size())
		//	pri_buffer->executeCommands(buffers, vk::DispatchLoaderDefault{});
		vk::CommandBufferInheritanceInfo iinfo
		{
		};

		vk::ImageSubresourceRange subResourceRange = {};
		subResourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subResourceRange.baseMipLevel = 0;
		subResourceRange.levelCount = 1;
		subResourceRange.baseArrayLayer = 0;
		subResourceRange.layerCount = 1;

		if (!rendered)
		{
			vk::ImageMemoryBarrier presentToClearBarrier = {};
			presentToClearBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			presentToClearBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			presentToClearBarrier.oldLayout = vk::ImageLayout::eUndefined;
			presentToClearBarrier.newLayout = vk::ImageLayout::eGeneral;
			presentToClearBarrier.srcQueueFamilyIndex = *View().QueueFamily().graphics_family;
			presentToClearBarrier.dstQueueFamilyIndex = *View().QueueFamily().graphics_family;
			presentToClearBarrier.image = swapchain.m_graphics.Images()[swapchain.curr_index];
			presentToClearBarrier.subresourceRange = subResourceRange;
			begin_info.pInheritanceInfo = &iinfo;
			transition_buffer->begin(begin_info, vk::DispatchLoaderDefault{});
			transition_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, nullptr, nullptr, presentToClearBarrier, vk::DispatchLoaderDefault{});
			transition_buffer->end();
			//hlp::TransitionImageLayout(*transition_buffer, queue, swapchain.images[swapchain.curr_index], vk::Format::eUndefined, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR,&iinfo);
			pri_buffer->executeCommands(*transition_buffer, vk::DispatchLoaderDefault{});
		}
		pri_buffer->end();

		if (!rendered)
			buffers.emplace_back(*pri_buffer);
		auto& current_signal = View().CurrPresentationSignals();

		vector<vk::Semaphore> waitSemaphores{ *current_signal.image_available, *_pre_render_complete };
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands,vk::PipelineStageFlagBits::eAllCommands };
		vk::Semaphore readySemaphores =*_states[0].signal.render_finished;
		hash_set<vk::Semaphore> ready_semaphores;
		for (auto& state : gfx_states)
		{
			auto semaphore = state.camera.render_target.as<VknRenderTarget>().ReadySignal();
			if(semaphore)
			ready_semaphores.emplace(semaphore);
		}
		//Temp, get rid of this once the other parts no longer depend on render_finished
		ready_semaphores.emplace(readySemaphores);
		vector<vk::Semaphore> arr_ready_sem(ready_semaphores.begin(), ready_semaphores.end());
		auto inflight_fence = *_states[0].signal.inflight_fence();



		vk::SubmitInfo submit_info
		{
			hlp::arr_count(waitSemaphores)
			,std::data(waitSemaphores)
			,waitStages
			,hlp::arr_count(buffers),std::data(buffers)
			,hlp::arr_count(arr_ready_sem) ,std::data(arr_ready_sem)
		};


		View().Device()->resetFences(1, &inflight_fence, vk::DispatchLoaderDefault{});
		dbg::BeginLabel(queue, "Render GraphicsStates", color{ 0.3f,0.0f,0.3f });
		queue.submit(submit_info, inflight_fence, vk::DispatchLoaderDefault{});
		dbg::EndLabel(queue);
		auto copy = View().Swapchain().m_graphics.Images();
		copy[View().vulkan().rv ]= RscHandle<VknRenderTarget>()->GetColorBuffer().as<VknTexture>().Image();
		View().Swapchain().m_graphics.Images(std::move(copy));
		//GetGfxTimeLog().pop_level();
	}

	void ConvertToNonSRGB(RenderStateV2& rs,gt::GraphTest& gtest)
	{
		gtest.SrgbConversionTest(rs);
	}

//#pragma optimize ("",off)
	void FrameRenderer::PostRenderGraphicsStates(const PostRenderData& state, uint32_t frame_index)
	{
		//auto& lights = *state.shared_gfx_state->lights;

		auto& canvas = *state.shared_gfx_state->ui_canvas;
		size_t num_conv_states = 1;
		size_t num_instanced_buffer_state = 1;
		size_t num_gamma_conv = 1;
		auto total_post_states = canvas.size() + num_conv_states + num_instanced_buffer_state + num_gamma_conv;
		GrowStates(_post_states, total_post_states);
		for (auto& pos_state : _post_states)
			pos_state.Reset();
		
		size_t curr_state = 0;
		std::optional<vk::Semaphore> copy_semaphore{};
		{

			auto copy_state_ind = curr_state++;
			auto& copy_state = _post_states[copy_state_ind];

			copy_semaphore = *copy_state.signal.render_finished;

			auto cmd_buffer = copy_state.CommandBuffer();
			cmd_buffer.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

			if (state.shared_gfx_state->ui_canvas && state.shared_gfx_state->ui_canvas->size())
			{
				auto& pos_buffer = state.shared_gfx_state->ui_buffer_pos;
				auto& uv_buffer = state.shared_gfx_state->ui_buffer_uv;
				auto& color_buffer = state.shared_gfx_state->ui_buffer_color;
				auto& doto = *state.shared_gfx_state->ui_attrib_data;

				pos_buffer.resize(doto.size());
				uv_buffer.resize(doto.size());
				color_buffer.resize(doto.size());

				for (size_t i = 0; i < doto.size(); ++i)
				{
					auto& elem = doto[i];
					
					if (elem.pos.size())
					{
						pos_buffer[i].resize(hlp::buffer_size(elem.pos));
						pos_buffer[i].update<const vec2>(0, elem.pos, cmd_buffer);

						uv_buffer[i].resize(hlp::buffer_size(elem.uv));
						uv_buffer[i].update<const vec2>(0, elem.uv, cmd_buffer);
					}

					color_buffer[i].resize(hlp::buffer_size(elem.color));
					color_buffer[i].update<const color>(0, elem.color, cmd_buffer);
				}
				
			}
			cmd_buffer.end();
			//Don't flag, we want to submit this separately.

			vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eTransfer };
			vk::SubmitInfo submit_info
			{
				1
				,&*_states[0].signal.render_finished
				,waitStages
				,1,&copy_state.CommandBuffer()
				,1,&*copy_semaphore
			};

			auto queue = View().GraphicsQueue();
			queue.submit(submit_info, vk::Fence{}, vk::DispatchLoaderDefault{});
		}



		//Do post pass here
		//Canvas pass
		size_t i = 0, j = 0;
		for (auto& elem : canvas)
		{
			auto& rs = _post_states[curr_state++];
			//if(elem.render_target) //Default render target is null. Don't ignore it.
			PostRenderCanvas(i, j, elem.render_target, elem.ui_ro, state, rs, frame_index);
		}

		if (Core::GetSystem<GraphicsSystem>().extra_vars.Get<float>("gamma_correction"))
		{
			auto& rs = _post_states[curr_state++];
			auto& graph = _pimpl->gtest_graph;
			graph.SetDefaultUboManager(rs.ubo_manager);
			graph.SetPipelineManager(GetPipelineManager());
			ConvertToNonSRGB(rs,_pimpl->test);
		}

		//TODO: Submit the command buffers

		vector<vk::CommandBuffer> buffers{};

		for (auto& post_state : _post_states)
		{
			if (post_state.has_commands)
				buffers.emplace_back(post_state.CommandBuffer());
		}

		vector<vk::Semaphore> arr_ready_sem{ *_post_render_complete };
		vector<vk::Semaphore> arr_wait_sem{};
		if (copy_semaphore)
			arr_wait_sem.emplace_back(*copy_semaphore);
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands };


		vk::SubmitInfo submit_info
		{
			hlp::arr_count(arr_wait_sem) ,std::data(arr_wait_sem)
			,waitStages
			,hlp::arr_count(buffers),std::data(buffers)
			,hlp::arr_count(arr_ready_sem) ,std::data(arr_ready_sem)
		};

		auto queue = View().GraphicsQueue();
		queue.submit(submit_info, vk::Fence{}, vk::DispatchLoaderDefault{});
	}
	PresentationSignals& FrameRenderer::GetMainSignal()
	{
		return _states[0].signal;
	}
	void FrameRenderer::GrowStates(vector<RenderStateV2>& states, size_t new_min_size)
	{
		auto device = *View().Device();
		if (new_min_size > states.size())
		{
			auto diff = s_cast<uint32_t>(new_min_size - states.size());
			for (auto i = diff; i-- > 0;)
			{
				auto cmd_pool = View().vulkan().CreateGfxCommandPool();
				auto&& buffers = device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ *cmd_pool,vk::CommandBufferLevel::ePrimary, 1}, vk::DispatchLoaderDefault{});
				auto& buffer = buffers[0];
				states.emplace_back(RenderStateV2{std::move(cmd_pool), std::move(buffer),UboManager{View()},PresentationSignals{},DescriptorsManager{View()} }).signal.Init(View());
				//_state_cmd_buffers.emplace_back(std::move(buffer));
			}
		}
	}
	//Assumes that you're in the middle of rendering other stuff, i.e. command buffer's renderpass has been set
	//and command buffer hasn't ended
	void FrameRenderer::RenderDebugStuff(const GraphicsState& , RenderStateV2& , rect )
	{
#if 0 
		auto dispatcher = vk::DispatchLoaderDefault{};
		vk::CommandBuffer cmd_buffer = rs.CommandBuffer();
		//TODO: figure out inheritance pipeline inheritance and inherit from dbg_pipeline for various viewport sizes
		//auto& pipelines = state.dbg_pipeline;

		//Preprocess MeshRender's uniforms
		//auto&& [processed_ro, layout_count] = ProcessRoUniforms(state, rs.ubo_manager);
		//rs.ubo_manager.UpdateAllBuffers();
		//auto alloced_dsets = rs.dpools.Allocate(layout_count);
		rs.FlagRendered();
		const VulkanPipeline* prev = nullptr;
		RenderInterface* pContext;
		RenderInterface& context = *pContext;
		for (auto& p_dc : state.dbg_render)
		{
			if (prev!=p_dc->pipeline)
			{
				auto pipeline = p_dc->pipeline;
				pipeline->Bind(cmd_buffer, *_view);
				SetViewport(cmd_buffer, vp_pos, vp_size);
				//Bind the uniforms
				auto& layouts = pipeline->uniform_layouts;
				uint32_t trf_set = 0;
				auto itr = layouts.find(trf_set);
				if (itr != layouts.end())
				{
					DescriptorUpdateData dud;
					auto ds_layout = itr->second;
					auto allocated = rs.dpools.Allocate(hash_table<vk::DescriptorSetLayout, std::pair<vk::DescriptorType, uint32_t>>{ {ds_layout, { vk::DescriptorType::eUniformBuffer,2 }}});
					auto aitr = allocated.find(ds_layout);
					if (aitr != allocated.end())
					{
						auto&& [view_buffer, vb_offset] = rs.ubo_manager.Add(state.camera.view_matrix);
						auto&& [proj_buffer, pb_offset] = rs.ubo_manager.Add(mat4{ 1,0,0,0,   0,1,0,0,   0,0,0.5f,0.5f, 0,0,0,1 }*state.camera.projection_matrix);
						auto ds = aitr->second.GetNext();
						UpdateUniformDS(ds, vector<ProcessedRO::BindingInfo>{
							ProcessedRO::BindingInfo{
								0,view_buffer,vb_offset,0,sizeof(mat4),itr->second
							},
								ProcessedRO::BindingInfo{ 1,proj_buffer,pb_offset,0,sizeof(mat4),itr->second }
						}, dud
						);
						dud.SendUpdates();
						cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->pipelinelayout, 0, ds, {});
					}
				}
				prev = pipeline;

			}
			auto& dc = *p_dc;
			if (context.SetPipeline(*dc.pipeline))
			{
				context.SetViewport(vp);
				FakeMat4 view_mat = state.camera.view_matrix;
				FakeMat4 proj_mat = mat4{ 1,0,0,0,   0,1,0,0,   0,0,0.5f,0.5f, 0,0,0,1 }*state.camera.projection_matrix;

				context.BindUniform("CameraBlock"    , 0, hlp::to_data(proj_mat                      ));
				context.BindUniform("ObjectMat4Block", 0, hlp::to_data(std::pair{ view_mat,proj_mat }));
			}
			dc.Bind(cmd_buffer);
			//cmd_buffer.bindVertexBuffers(0,
			//	{
			//		 *mesh.Get(attrib_index::Position).buffer(),//dc.mesh_buffer[DbgBufferType::ePerVtx].find(0)->second.buffer,
			//		dc.mesh_buffer[DbgBufferType::ePerInst].find(1)->second.buffer
			//	},
			//	{
			//		0,0
			//	}
			//	);
			//cmd_buffer.bindIndexBuffer(dc.index_buffer.buffer, 0, vk::IndexType::eUint16);
			//cmd_buffer.drawIndexed(mesh.IndexCount(), dc.nu, 0, 0, 0);
			dc.Draw(cmd_buffer);
			
		}
#endif
	}
	vk::RenderPass FrameRenderer::GetRenderPass(const GraphicsState& state, VulkanView&)
	{
		//vk::RenderPass result = view.BasicRenderPass(BasicRenderPasses::eRgbaColorDepth);
		//if (state.camera.is_shadow)
		//	result = view.BasicRenderPass(BasicRenderPasses::eDepthOnly);
		return *state.camera.render_target.as<VknRenderTarget>().GetRenderPass(state.clear_render_target && state.camera.clear_data.index() != meta::IndexOf <std::remove_const_t<decltype(state.camera.clear_data)>,DontClear>::value);
	}


	void TransitionFrameBuffer(const CameraData& camera, vk::CommandBuffer cmd_buffer, VulkanView& )
	{
		auto& vkn_fb = camera.render_target.as<VknRenderTarget>();
		vkn_fb.PrepareDraw(cmd_buffer);
	}


	pipeline_config ConfigWithVP(pipeline_config config, const CameraData& , const ivec2& offset, const uvec2& size)
	{
		config.viewport_offset = offset;
		config.viewport_size = size ;
		return config;
	}

	void CopyDepthBuffer(vk::CommandBuffer cmd_buffer, vk::Image rtd, vk::Image gd)
	{
		//Transit RTD -> Transfer
		vk::ImageMemoryBarrier depth
		{
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eTransferDstOptimal,
			*View().QueueFamily().graphics_family,
			*View().QueueFamily().graphics_family,
			rtd,
			vk::ImageSubresourceRange
			{
				vk::ImageAspectFlagBits::eDepth,
				0,1,0,1
			}
		};
		cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, depth);

		//Blit
		//expect depth attachments to be the exact same size and format.
		cmd_buffer.copyImage(
			gd, vk::ImageLayout::eTransferSrcOptimal,
			rtd, vk::ImageLayout::eTransferDstOptimal,
			vk::ImageCopy
			{
				vk::ImageSubresourceLayers
				{
					vk::ImageAspectFlagBits::eDepth,
					0u,0u,1u
				},
				vk::Offset3D{0,0,0},
				vk::ImageSubresourceLayers
				{
					vk::ImageAspectFlagBits::eDepth,
					0ui32,0ui32,1ui32
				},
				vk::Offset3D{0,0,0}
			}
		);


		//Transit RTD -> General
		std::array<vk::ImageMemoryBarrier, 2> return_barriers = { depth,depth };
		std::swap(return_barriers[0].dstAccessMask, return_barriers[0].srcAccessMask);
		std::swap(return_barriers[0].oldLayout, return_barriers[0].newLayout);
		return_barriers[0].setDstAccessMask({});// vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		return_barriers[0].setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		//Transit GD  -> DepthAttachmentOptimal
		return_barriers[1].setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
		return_barriers[1].setDstAccessMask({});
		return_barriers[1].setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
		return_barriers[1].setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		return_barriers[1].setImage(gd);

		cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, return_barriers);

	}
	void FrameRenderer::ColorPick(vector<ColorPickRequest>&& pick_buffer)
	{
		_pimpl->color_pick_requests = std::move(pick_buffer);
	}
	void FrameRenderer::RenderGraphicsState(const GraphicsState& state, RenderStateV2& rs)
	{
		//TODO, account for forward.

		auto& graph = _pimpl->graph;
		graph.MarkRegion(std::to_string(_pimpl->gfx_state_index++));
		FrameGraphResource color, depth;
		{
			auto [col, dep] = renderpasses::DeferredRendering::MakePass(graph, RscHandle<VknRenderTarget>{ state.camera.render_target }, state, rs);
			color = col;
			depth = dep;
		}
		{
			auto [col, dep] = renderpasses::AddTransparentPass(graph, color, depth, state);
			color = col;
			depth = dep;
		}
		{
			auto [col, dep] = _pimpl->text_mesh_renderer.AddPass(graph, state, color, depth);
			color = col;
			depth = dep;
		}
		{
			auto [col, dep] = _pimpl->particle_renderer.AddPass(graph, state, color, depth);
			color = col;
			depth = dep;
		}
		{
			auto [col, dep] = renderpasses::AddDebugDrawPass(graph, state.camera.viewport, state,color, depth);
			color = col;
			depth = dep;
		}
	}

	MaterialInstanceCache& FrameRenderer::GetMatInstCache()
	{
		return _pimpl->mat_inst_cache;
	}

	FrameRenderer::FrameRenderer(FrameRenderer&&)= default;

	FrameRenderer::~FrameRenderer() {}

	PipelineManager& FrameRenderer::GetPipelineManager()
	{
		return *_pipeline_manager;
	}

	VulkanPipeline& FrameRenderer::GetPipeline(const pipeline_config& config,const vector<RscHandle<ShaderProgram>>& modules, std::optional<RenderPassObj> rp)
	{
		return GetPipelineManager().GetPipeline(config,modules,_current_frame_index,rp);
	}

	void FrameRenderer::NonThreadedRender::Init(FrameRenderer* renderer)
	{
		_renderer = renderer;
	}

	void FrameRenderer::NonThreadedRender::Render(const GraphicsState& state, RenderStateV2& rs)
	{
		_renderer->RenderGraphicsState(state, rs);
	}

	void FrameRenderer::NonThreadedRender::Join()
	{
	}

}