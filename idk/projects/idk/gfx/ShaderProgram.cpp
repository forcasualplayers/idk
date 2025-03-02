#include "stdafx.h"
#include "ShaderProgram.h"
#include <gfx/IShaderProgramFactory.h>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>

namespace idk
{
	ShaderBuildResult ShaderProgram::BuildShader(ShaderStage stage, string_view glsl_code)
	{
		if (auto* factory = &Core::GetResourceManager().GetFactory<IShaderProgramFactory>())
			return factory->BuildGLSL(GetHandle(), stage, glsl_code);
		return ShaderBuildResult::Err_NonexistentFactory;
	}
}
