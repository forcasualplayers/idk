#include "pch.h" 
#include "OpenGLTexture.h"

#include <opengl/resource/OpenGLTextureRenderMeta.h>

namespace idk::ogl
{
	OpenGLTexture::OpenGLTexture()
	{
		GL_CHECK();
		glGenTextures(1, &_id);
		GL_CHECK();
		Bind();
		GL_CHECK();
		SetUVMode(UVMode::Clamp);
		GL_CHECK();
		SetFilteringMode(FilterMode::Linear);
		GL_CHECK();
		Buffer(nullptr, 0, _size, _internal_format);
	}

	OpenGLTexture::OpenGLTexture(TextureInternalFormat format, uvec2 size, unsigned)
	{
		glGenTextures(1, &_id);
		Bind();
		_size = size;
		_internal_format = format;
		_is_compressed = format >= TextureInternalFormat::SRGB_FIRST && format <= TextureInternalFormat::SRGB_FIRST;
		_mip_level = 0;

		SetUVMode(UVMode::Repeat);
		SetFilteringMode(FilterMode::Linear);
		GL_CHECK();
		Buffer(nullptr, 0, size, format);
		GL_CHECK();
	}

	OpenGLTexture::OpenGLTexture(const CompiledTexture& compiled_texture)
		: _is_compressed{compiled_texture.is_srgb},
		_mip_level{compiled_texture.generate_mipmaps}
	{
		glGenTextures(1, &_id);
		Bind();
		_size = compiled_texture.size;
		_internal_format = ToInternalFormat(compiled_texture.internal_format, true);
		SetUVMode(compiled_texture.mode);
		SetFilteringMode(compiled_texture.filter_mode);

		glCompressedTexImage2D(GL_TEXTURE_2D, 0,
			detail::ogl_GraphicsFormat::ToInternal(_internal_format),
			_size.x,
			_size.y,
			0,
			static_cast<GLsizei>(compiled_texture.pixel_buffer.size()),
			compiled_texture.pixel_buffer.data());
		GL_CHECK();
		if (_mip_level)
			glGenerateMipmap(_id);
	}

	OpenGLTexture::OpenGLTexture(OpenGLTexture&& rhs)
		: Texture{ std::move(rhs) }, _id{ rhs._id }, _is_compressed{rhs._is_compressed}, _mip_level{rhs._mip_level}
	{
		rhs._id = 0;
	}

	OpenGLTexture& OpenGLTexture::operator=(OpenGLTexture&& rhs)
	{
		Texture::operator=(std::move(rhs));
		std::swap(_id, rhs._id);
		std::swap(_is_compressed, rhs._is_compressed);
		std::swap(_mip_level, rhs._mip_level);
		return *this;
	}
	
	OpenGLTexture::~OpenGLTexture()
	{
		glDeleteTextures(1, &_id);
	}
	
	void OpenGLTexture::Bind()
	{
		glBindTexture(GL_TEXTURE_2D, _id);
	}
	
	void OpenGLTexture::BindToUnit(GLuint unit)
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		Bind();
	}

	void OpenGLTexture::Buffer(void* data, size_t, uvec2 texture_size, TextureInternalFormat format, GLenum, GLenum incoming_type)
	{
		_size = texture_size;
		_internal_format = format;
		auto gl_format = detail::ogl_GraphicsFormat::ToInternal(_internal_format);
		auto components = detail::ogl_GraphicsFormat::ToComponents(_internal_format);
		Bind();
		GL_CHECK();
		glTexImage2D(GL_TEXTURE_2D, 0, gl_format, _size.x, _size.y, 0, components, incoming_type, data);
		GL_CHECK();
	}

	void OpenGLTexture::Buffer(void* data, size_t buffer_size, uvec2 size, ColorFormat format, bool compressed)
	{
		_size = size;
		Bind();

		_internal_format = ToInternalFormat(format, compressed);
		auto gl_format = detail::ogl_GraphicsFormat::ToInternal(_internal_format);

		if (compressed)
		{
			switch (format)
			{
			case ColorFormat::Alpha_8:
				glTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, GL_R, GL_UNSIGNED_BYTE, data);
				break;
			case ColorFormat::RGB_16bit:
			case ColorFormat::RGB_24bit:
				glTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
				break;
			case ColorFormat::RGBA_32bit:
				glTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				break;
			}
		}
		else
		{
			switch (format)
			{
			case ColorFormat::Alpha_8:
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, (GLsizei) buffer_size, data);
				break;
			case ColorFormat::RGB_16bit:
			case ColorFormat::RGB_24bit:
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, (GLsizei) buffer_size, data);
				break;
			case ColorFormat::RGBA_32bit:
				glCompressedTexImage2D(GL_TEXTURE_2D, 0, gl_format, size.x, size.y, 0, (GLsizei) buffer_size, data);
				break;
			}
		}
	}


	uvec2 OpenGLTexture::Size(uvec2 new_size)
	{
		Texture::Size(new_size);
		Buffer(nullptr, 0, _size, _internal_format);
		return _size;
	}

	void* OpenGLTexture::ID() const
	{
		return r_cast<void*>(_id);
	}


	void OpenGLTexture::SetUVMode(UVMode uv_mode)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, detail::ogl_GraphicsFormat::ToUVMode(uv_mode));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, detail::ogl_GraphicsFormat::ToUVMode(uv_mode));
		GL_CHECK();
	}

	void OpenGLTexture::SetFilteringMode(FilterMode f_mode)
	{
		if (_mip_level)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, detail::ogl_GraphicsFormat::ToFilter(f_mode));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, detail::ogl_GraphicsFormat::ToFilter(f_mode));
		}
		GL_CHECK();
	}
}
