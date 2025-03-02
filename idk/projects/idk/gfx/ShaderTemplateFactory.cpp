#include "stdafx.h"
#include "ShaderTemplateFactory.h"
#include <sstream>

const idk::string_view default_template = R"(
#version 450

layout(location=0)out vec4 out_color;

void main()
{
	vec4 color = vec4(1.0,0.0,1.0f,1.0);

	//__MATERIAL_CODE__;

	out_color = color;
})";

namespace idk
{
	unique_ptr<ShaderTemplate> ShaderTemplateFactory::GenerateDefaultResource()
	{
		return std::make_unique<ShaderTemplate>(default_template);
	}
}