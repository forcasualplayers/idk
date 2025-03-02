#include "pch.h"
#include "OpenGLCubemap.h"

#include <idk.h>
#include <core/Core.h>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>

#include <opengl/resource/OpenGLTextureRenderMeta.h>
#include <ds/span.inl>

namespace idk::ogl
{
	
	OpenGLCubemap::OpenGLCubemap()
	{
		glGenTextures(1, &_id);
		//*texture = OpenGLTexture{ _id, _size, FilterMode::Linear, UVMode::ClampToBorder};
		glBindTexture(GL_TEXTURE_CUBE_MAP, _id);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 9);

		UpdateUV(UVMode::Clamp);
		
		for (auto& i : TextureTarget::values)
			Buffer(i, nullptr, _size, TextureInternalFormat::RGBA_16_F);

		//Todo GET FILE HANDLE
		//Core::GetResourceManager().GetFactory<ShaderProgramFactory>().Create();

		// generate convoluted cubemap
		//unsigned int x = 0;
		glGenTextures(2, _convoluted_id);
		
		for (auto j = 0; j < 1; ++j)
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP, _convoluted_id[j]);
			for (int i = 0; i < 6; ++i)
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 9);
		}
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	OpenGLCubemap::OpenGLCubemap(OpenGLCubemap&& rhs)
		: CubeMap{ std::move(rhs) }
		, _id{ rhs._id }
		, texture{rhs.texture}
		, convoluted_texture{*std::data(rhs.convoluted_texture)}
		, _convoluted_id{*std::data(rhs._convoluted_id)}
	{
		rhs._id = 0;
		rhs.texture = RscHandle<OpenGLTexture>{};
	}

	OpenGLCubemap& OpenGLCubemap::operator=(OpenGLCubemap&& rhs)
	{
		CubeMap::operator=(std::move(rhs));
		std::swap(_id, rhs._id);
		std::swap(texture, rhs.texture);
		std::swap(convoluted_texture, rhs.convoluted_texture);
		std::swap(_convoluted_id, rhs._convoluted_id);
		return *this;
	}

	OpenGLCubemap::~OpenGLCubemap()
	{
		glDeleteTextures(1, &_id);
		glDeleteTextures(2, _convoluted_id);
	}

	void OpenGLCubemap::Bind()
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
	}

	void OpenGLCubemap::BindToUnit(GLuint unit)
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
	}

	void OpenGLCubemap::BindConvolutedToUnit(GLuint texture_unit)
	{
		glActiveTexture(GL_TEXTURE0 + texture_unit);
		glBindTexture(GL_TEXTURE_CUBE_MAP, _convoluted_id[0]);
		//GL_CHECK();
	}

	void OpenGLCubemap::Buffer(TextureTarget face_value, void* data, uvec2 size, TextureInternalFormat internal_format, GLenum color_components, GLenum incoming_type)
	{
		_size = size;
		_internal_format = internal_format;
		glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_value, 0, detail::ogl_GraphicsFormat::ToInternal(internal_format), size.x, size.y, 0, color_components, incoming_type, data);
		GL_CHECK();
	}


	void OpenGLCubemap::Size(uvec2 new_size)
	{
		CubeMap::Size(new_size);
		for (auto& i : TextureTarget::values)
			Buffer(i, nullptr, _size, _internal_format);
	}

	void* OpenGLCubemap::ID() const
	{
		return r_cast<void*>(_id);
	}

	span<const GLuint> OpenGLCubemap::ConvolutedID() const
	{
		return span<const GLuint>{_convoluted_id};
	}

	void OpenGLCubemap::UpdateUV(UVMode uv_mode)
	{
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, detail::ogl_GraphicsFormat::ToUVMode(uv_mode));
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, detail::ogl_GraphicsFormat::ToUVMode(uv_mode));
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, detail::ogl_GraphicsFormat::ToUVMode(uv_mode));
		GL_CHECK();
	}

	RscHandle<Texture> OpenGLCubemap::Tex() const noexcept
	{
		return RscHandle<Texture>{texture};
	}

};