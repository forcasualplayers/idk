#pragma once
#include <idk.h>
#include <gfx/Camera.h>
#include <vkn/utils/utils.inl>
namespace idk
{

	struct RenderObject;
	struct AnimatedRenderObject;

	namespace vkn
	{
		enum class PbrCubeMapVars
		{
			eIrradiance,
			eEnvironmentProbe,
		};
		using PbrCubeMapVarsPack = meta::enum_pack< PbrCubeMapVars,
			PbrCubeMapVars::eIrradiance,
			PbrCubeMapVars::eEnvironmentProbe
		>;
		using PbrCubeMapVarsInfo = meta::enum_info < PbrCubeMapVars, PbrCubeMapVarsPack>;
		enum class PbrTexVars
		{
			eBrdfLut, //Brdf LookUp Table
		};
		using PbrTexVarsPack = meta::enum_pack< PbrTexVars,
			PbrTexVars::eBrdfLut //Brdf LookUp Table
		>;
		using PbrTexVarsInfo = meta::enum_info < PbrTexVars, PbrTexVarsPack>;
	
		class  PipelineThingy;
		struct GraphicsState;

		struct StandardBindings
		{
			virtual bool Skip(PipelineThingy& , const  RenderObject& ) { return false; }
			//Stuff that should be bound at the start, before the renderobject/animated renderobject loop.
			virtual void Bind(PipelineThingy& the_interface);
			//Stuff that needs to be bound with every renderobject/animated renderobject
			virtual void Bind(PipelineThingy& the_interface, const  RenderObject& dc);
			//Stuff that needs to be bound with every animated renderobject (default behaviour invokes 
			// void Bind(PipelineThingy& the_interface, const  RenderObject& dc)
			//before invoking BindAni 
			//(this distinction is really just for overridable convenience. We could force everyone
			// who needs to write a new binder to manually invoke the RenderObject bind on their own.)
			virtual void Bind(PipelineThingy& the_interface, const AnimatedRenderObject& dc);
			//Stuff that needs to be bound only for animated renderobject.
			virtual void BindAni(PipelineThingy& the_interface, const AnimatedRenderObject& dc);

			//Stuff that needs to be bound only for font renderobject
			virtual void BindFont(PipelineThingy& the_interface, const FontRenderData& dc);
		
			//Stuff that needs to be bound only for canvas renderobject
			virtual void BindCanvas(PipelineThingy& the_interface, const TextData& dc, const UIRenderObject& dc_one);
			virtual void BindCanvas(PipelineThingy& the_interface, const ImageData& dc, const UIRenderObject& dc_one);

		
		};

		//Standard binding for vertex stuff
		struct StandardVertexBindings : StandardBindings
		{
			//const GraphicsState* _state;
			//const GraphicsState& State();
			const vector<SkeletonTransforms>* skeletons;
			mat4 view_trf, proj_trf;
			void SetState(const GraphicsState& vstate);
			void SetState(const CameraData& camera, const vector<SkeletonTransforms>& skel);

			void Bind(PipelineThingy& the_interface)override;
			void Bind(PipelineThingy& the_interface, const RenderObject& dc)override;
			void Bind(PipelineThingy& the_interface, const  AnimatedRenderObject& dc);
			void BindAni(PipelineThingy& the_interface, const AnimatedRenderObject& dc)override;

		};

		struct ParticleVertexBindings : StandardBindings
		{
			//const GraphicsState* _state;
			//const GraphicsState& State();
			mat4 view_trf, proj_trf;
			void SetState(const GraphicsState& vstate);
			void SetState(const CameraData& camera);

			void Bind(PipelineThingy& the_interface)override;

		};
		struct FontVertexBindings : StandardBindings
		{
			//const GraphicsState* _state;
			//const GraphicsState& State();
			mat4 view_trf, proj_trf, obj_trf;
			vec4 color;
			void SetState(const GraphicsState& vstate);
			void SetState(const CameraData& camera);

			void Bind(PipelineThingy& the_interface)override;
			void BindFont(PipelineThingy& the_interface, const FontRenderData& dc)override;


		};

		struct CanvasVertexBindings : StandardBindings
		{
			//const GraphicsState* _state;
			//const GraphicsState& State();
			mat4 view_trf, proj_trf, obj_trf;
			vec4 color;
			//void SetState(const PostRenderData& vstate);
			void SetState(const CameraData& camera);

			void Bind(PipelineThingy& the_interface)override;
			void BindCanvas(PipelineThingy& the_interface, const TextData& dc, const UIRenderObject& dc_one)override;
			void BindCanvas(PipelineThingy& the_interface, const ImageData& dc, const UIRenderObject& dc_one)override;
		};
		struct PbrFwdBindings : StandardBindings
		{
			const GraphicsState* _state;
			CameraData cam;
			const GraphicsState& State();
			string light_block;
			string dlight_block;
			mat4 view_trf, pbr_trf, proj_trf;
			bool rebind_light = false;

			std::optional<std::pair<size_t, size_t>> light_range;

			vector<RscHandle<Texture>> shadow_maps;
			vector <RscHandle<Texture>> shadow_maps_directional;
			//vector<mat4> directional_vp;

			string                     pbr_cube_map_names[PbrCubeMapVarsInfo::size()];
			vector<RscHandle<CubeMap>> pbr_cube_maps;
			vector<RscHandle<Texture>> pbr_texs;

			vector<std::pair<size_t,size_t>> pbr_cube_maps_ranges;
			vector<std::pair<size_t,size_t>> pbr_texs_ranges;

			void LoadStuff(const GraphicsState& vstate);

			void ResetCubeMaps(size_t reserve_size = 4);
			void AddCubeMaps(PbrCubeMapVars var, span<const RscHandle<CubeMap>> Cube_maps);
			span<const RscHandle<CubeMap>> GetCubeMap(PbrCubeMapVars var)const;

			void ResetTexVars(size_t reserve_size = 4);
			void AddTexVars(PbrTexVars var, span<const RscHandle<Texture>> Env_maps);
			span<const RscHandle<Texture>> GetTexVars(PbrTexVars var)const;

			
			void SetState(const GraphicsState& vstate);


			void Bind(PipelineThingy& the_interface, const RenderObject& dc)override;
		};

		struct StandardMaterialFragBindings : StandardBindings
		{
			//Assumes that the material is valid.
			void Bind(PipelineThingy& the_interface, const  RenderObject& dc)override;

		};
		struct StandardMaterialBindings : StandardBindings
		{
			const GraphicsState* _state;
			const GraphicsState& State() { return *_state; }
			RscHandle<MaterialInstance> prev_material_inst{};
			void SetState(const GraphicsState& vstate);

			//Assumes that the material is valid.
			void Bind(PipelineThingy& the_interface, const RenderObject& dc) override;

			void BindAni(PipelineThingy& the_interface, const AnimatedRenderObject& dc)override;

		};
		//A combined set of binders that will bind in the order that is specified in the template arguments.
		template<typename ...Args>
		struct CombinedBindings :StandardBindings
		{
			std::tuple<Args...> binders;
			bool Skip(PipelineThingy& the_interface, const  RenderObject& dc) override;
			//for each binder, execute f(binder,FArgs...) if cond<decltype(binder)> is true.
			template<template<typename...>typename cond = meta::always_true_va, typename Fn, typename ...FArgs >
			void for_each_binder(Fn&& f, FArgs& ...args);
			void Bind(PipelineThingy& the_interface) override;
			void Bind(PipelineThingy& the_interface, const RenderObject& dc) override;
			void BindAni(PipelineThingy& the_interface, const  AnimatedRenderObject& dc) override;
		};
		using PbrFwdMaterialBinding = CombinedBindings<StandardVertexBindings, StandardMaterialFragBindings, PbrFwdBindings, StandardMaterialBindings>;
		
		struct UnlitFilter :StandardBindings
		{
			bool Skip(PipelineThingy& the_interface, const  RenderObject& dc) override;
		};
		struct ShadowFilter :StandardBindings
		{
			LayerMask filter;
			bool Skip(PipelineThingy& the_interface, const  RenderObject& dc) override;
			void SetState(const CameraData& cam, const vector<SkeletonTransforms>& skel);
		};
		using UnlitMaterialBinding = CombinedBindings<UnlitFilter,StandardVertexBindings, StandardMaterialFragBindings, StandardMaterialBindings>;
		using ShadowBinding = CombinedBindings<ShadowFilter,StandardVertexBindings>;
	}
}