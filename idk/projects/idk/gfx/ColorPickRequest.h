#pragma once
#include <core/GameObject.h>
#include <future>
#include "ColorPickResult.h"
#include <gfx/RenderObject.h> //Camera Data
namespace idk
{
	struct PickReqData
	{
		vec2 point{};
		CameraData camera;
		size_t inst_mesh_render_begin{}, inst_mesh_render_end{};
		size_t inst_skinned_mesh_render_begin{}, inst_skinned_mesh_render_end{};
		vector<ColorPickResult::result_t> handles;
		shared_ptr<vector<ColorPickResult::result_t>> ani_handles;
	};
	struct ColorPickRequest
	{
		using result_t = ColorPickResult::result_t;
		ColorPickResult promise(vec2 normalized_point, CameraData camera_data);

		void select(uint32_t index);
		void set_result(result_t value);

		
		PickReqData data;
	private:
		result_t GetHandle(uint32_t id)const;
		std::promise<result_t> result;
	};
}