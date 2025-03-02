#include "pch.h"
#include "FrameBufferManager.h"
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>
#include <gfx/RenderTarget.h>
#include <opengl/resource/FrameBuffer.h>
#include <opengl/resource/OpenGLTexture.h>
#include <res/ResourceMeta.inl>

#include <gfx/ViewportUtil.h>
#include <gfx/FramebufferFactory.h>
#include <res/Guid.inl>
#include <ds/span.inl>

namespace idk::ogl
{
	FrameBufferManager::FrameBufferManager()
	{
		glGenFramebuffers(1, &_fbo_id);
		//glGenRenderbuffers(1, &_rbo_id);
	}

	FrameBufferManager::FrameBufferManager(FrameBufferManager&& rhs)
		: _fbo_id{ rhs._fbo_id }
	{
		rhs._fbo_id = 0;
	}
	FrameBufferManager& FrameBufferManager::operator=(FrameBufferManager&& rhs)
	{
		std::swap(_fbo_id, rhs._fbo_id);
		return *this;
	}
	FrameBufferManager::~FrameBufferManager()
	{
		glDeleteFramebuffers(1, &_fbo_id);
	}
	void FrameBufferManager::SetRenderTarget(const RscHandle<OpenGLCubemap>& target, bool for_convolution)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);
		if (for_convolution)
		{
			glViewport(0, 0, 32, 32);// set texture targets
			const auto ids = target->ConvolutedID();
			glBindTexture(GL_TEXTURE_CUBE_MAP, ids[0]);

			vector<GLenum> buffers{GL_COLOR_ATTACHMENT0};
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ids[0], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			GL_CHECK();
			glDrawBuffers(s_cast<GLsizei>(buffers.size()), buffers.data());
			GL_CHECK();
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else
		{
			const auto sz = target->Size();
			glViewport(0, 0, sz.x, sz.y);

            auto id = static_cast<GLint>(reinterpret_cast<ptrdiff_t>(target->ID()));

			vector<GLenum> buffers;
			//auto& yolo = *meta.textures[0];
			for (int i = 0; i < 6; ++i)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER,
					GL_COLOR_ATTACHMENT0,
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
					id,
					0);
			}
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		CheckFBStatus();
	}

	void FrameBufferManager::SetRenderTarget(RscHandle<OpenGLTexture> target, const std::optional<rect>& oviewport, bool clear)
	{
		glEnable(GL_SCISSOR_TEST);
		GL_CHECK();
		if (oviewport)
		{
			auto& viewport = *oviewport;
			auto&& [position, size] = ComputeViewportExtents(viewport.size, viewport);
			glViewport((position.x), position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.x));
			glScissor((position.x), position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.x));
		}
		else
		{
			glViewport(0, 0, target->Size().x, target->Size().y);
			glScissor(0, 0, target->Size().x, target->Size().y);
		}
		if (target->IsDepthTexture())
		{
			glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);
			glDepthFunc(GL_LEQUAL);

			target->Bind();
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_cast<GLuint>(r_cast<intptr_t>(target->ID())), 0);
			GLuint buffers[] = { GL_DEPTH_ATTACHMENT };
			glDrawBuffers(1, buffers);
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);
			target->Bind();

			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, s_cast<GLuint>(r_cast<intptr_t>(target->ID())), 0);
			const GLuint buffers[] = { GL_COLOR_ATTACHMENT0 };
			if(target->InternalFormat() == TextureInternalFormat::R_32_UI)
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
			else
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glDrawBuffers(1, buffers);
		}


		CheckFBStatus();
		if (clear)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_SCISSOR_TEST);
		GL_CHECK();

	}
	void FrameBufferManager::SetRenderTarget(RscHandle<OpenGLRenderTarget> target, const std::optional<rect>& oviewport)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);

		glEnable(GL_SCISSOR_TEST);
		GL_CHECK();
		auto& meta = *target;
		if (oviewport)
		{
			auto& viewport = *oviewport;
			auto&& [position, size] = ComputeViewportExtents(vec2{ target->Size() }, viewport);
			glViewport((position.x), position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.y));
			glScissor(position.x, position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.y));
		}
		else
		{
			glViewport(0, 0, meta.size.x, meta.size.y);
			glScissor(0, 0, meta.size.x, meta.size.y);
		}
		

		// set texture targets
		vector<GLenum> buffers;

		auto tex = target->GetColorBuffer();
		if(tex){
			auto& tex_ = tex.as<OpenGLTexture>();
			assert(tex_.InternalFormat() == TextureInternalFormat::RGBA_16_F);

			tex_.Bind();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , s_cast<GLuint>(r_cast<intptr_t>(tex_.ID())), 0);
			buffers.push_back(GL_COLOR_ATTACHMENT0 );
		}

		GL_CHECK();
		auto dp = target->GetDepthBuffer();
		if (dp)
		{
			auto& dp_ = dp.as<OpenGLTexture>();
			dp_.Bind();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_cast<GLuint>(r_cast<intptr_t>(dp_.ID())), 0);
		}
		else
		{
			dp = Core::GetResourceManager().Create<OpenGLTexture>();
			auto& dp_ = dp.as<OpenGLTexture>();
			dp_.Bind();
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_cast<GLuint>(r_cast<intptr_t>(dp_.ID())), 0);
		}

		if (!tex)
		{
			glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, target->Size().x);
			glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, target->Size().y);
			glDrawBuffer(GL_NONE);
		}
		else
		{
			glDrawBuffers(s_cast<GLsizei>(buffers.size()), buffers.data());
		}

		CheckFBStatus();

		glDisable(GL_SCISSOR_TEST);
		GL_CHECK();
	}
	void FrameBufferManager::SetRenderTarget(RscHandle<OpenGLFrameBuffer> target, const std::optional<rect>& oviewport, bool clear)
	{
		IDK_ASSERT_MSG(&*target,"Attempting to use a default framebuffer. Default framebuffer should not be used.");//make sure it's not null. 
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);

		glEnable(GL_SCISSOR_TEST);
		//GL_CHECK();
		auto& meta = *target;
		if (oviewport)
		{
			auto& viewport = *oviewport;
			auto&& [position, size] = ComputeViewportExtents(viewport.size, viewport);
			glViewport((position.x), position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.x));
			glScissor((position.x), position.y, s_cast<GLsizei>(size.x), s_cast<GLsizei>(size.x));
		}
		else
		{
			glViewport(0, 0, meta.size.x, meta.size.y);
			glScissor(0, 0, meta.size.x, meta.size.y);

		}

		vector<GLenum> buffers;

		for (int i = 0; i < target->NumColorAttachments(); ++i)
		{
			auto& attachment = target->GetAttachment(i);
			auto& tex = attachment.buffer.as<OpenGLTexture>();
			assert(tex.InternalFormat() == TextureInternalFormat::RGBA_16_F);
			tex.Bind();
			auto attachment_index = GL_COLOR_ATTACHMENT0 + i;
			glFramebufferTexture(GL_FRAMEBUFFER, attachment_index, s_cast<GLuint>(r_cast<intptr_t>(tex.ID())), 0);
			buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
		}
		//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target->DepthBuffer());
		if (target->HasDepthAttachment())
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_cast<GLuint>(r_cast<intptr_t>(target->DepthAttachment().buffer->ID())), 0);

		if (!target->NumColorAttachments())
		{
			glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH , target->Size().x);
			glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, target->Size().y);
			glDrawBuffer(GL_NONE);
		}
		else
		{
			glDrawBuffers(s_cast<GLsizei>(buffers.size()), buffers.data());
		}

		CheckFBStatus();

		if(clear)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	void FrameBufferManager::ResetFramebuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	GLuint FrameBufferManager::ID() const
	{
		return _fbo_id;
	}

	void FrameBufferManager::CheckFBStatus()
	{
		const auto fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		switch (fb_status)
		{
		case GL_FRAMEBUFFER_UNDEFINED:                     LOG_TO(LogPool::GFX, "undefined\n");      break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         LOG_TO(LogPool::GFX, "incomplete\n");	 break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: LOG_TO(LogPool::GFX, "missing\n");		 break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        LOG_TO(LogPool::GFX, "incomplete draw\n"); break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        LOG_TO(LogPool::GFX, "incomplete read\n"); break;
		case GL_FRAMEBUFFER_UNSUPPORTED:                   LOG_TO(LogPool::GFX, "unsupported\n");	 break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:        LOG_TO(LogPool::GFX, "multisample\n");	 break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:      LOG_TO(LogPool::GFX, "layer target\n");	 break;
		}
		assert(fb_status == GL_FRAMEBUFFER_COMPLETE);
	}
}
