#pragma once
#include <idk.h>

#include <glad/glad.h>

#include <gfx/vertex_descriptor.h>
#include <opengl/resource/OpenGLTypeDescriptors.h>


namespace idk::ogl
{
	struct OpenGLBuffer
	{
	public:
		vector<OpenGLDescriptor> descriptor;

		OpenGLBuffer() = default;
		OpenGLBuffer(GLuint type, const vector<OpenGLDescriptor>& desc);
		OpenGLBuffer(OpenGLBuffer&& buf);
		~OpenGLBuffer();
		
		GLuint  type() const;
		GLsizei count() const;

		OpenGLBuffer& Bind();
		OpenGLBuffer& BindForDraw(const renderer_attributes& locations);
		OpenGLBuffer& Buffer(const void* data, size_t stride, GLsizei count);

		OpenGLBuffer& operator=(OpenGLBuffer&& buf);

		OpenGLBuffer(const OpenGLBuffer&) = delete;
		OpenGLBuffer& operator=(const OpenGLBuffer&) = delete;
	private:
		GLuint  _type = 0;
		GLuint  _id = 0;
		GLsizei _size = 0;
	};
}