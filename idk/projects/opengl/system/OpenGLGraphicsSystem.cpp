#include "pch.h"

#include <glad/glad.h>
#include <glad/glad_wgl.h>
#include <core/Core.h>
#include <res/EasyFactory.h>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>
#include <res/SaveableResourceLoader.inl>
#include <event/Dispatcher.inl>
#include <gfx/MeshRenderer.h>
#include <gfx/ShaderTemplateFactory.h>
#include <gfx/ShaderTemplateLoader.h>
#include <gfx/MaterialInstance.h>
#include <gfx/MaterialFactory.h>
#include <anim/Animation.h>
#include <anim/SkeletonFactory.h>
#include <opengl/resource/FrameBufferFactory.h>
#include <opengl/resource/OpenGLMeshFactory.h>
#include <opengl/resource/OpenGLTextureFactory.h>
#include <opengl/resource/OpenGLRenderTargetLoader.h>
#include <opengl/program/GLSLLoader.h>
#include <opengl/program/ProgramFactory.h>
#include <opengl/system/OpenGLState.h>
#include <opengl/resource/OpenGLFontAtlasFactory.h>
#include <opengl/resource/OpenGLFontAtlasLoader.h>


#include "OpenGLGraphicsSystem.h"

#include <iostream>

namespace idk::ogl
{
	Win32GraphicsSystem::Win32GraphicsSystem()
		: cubemaps_to_convolute{
		&Win32GraphicsSystem::ConvoluteCubeMap
	}
	{
	};

	Win32GraphicsSystem::~Win32GraphicsSystem() = default;

	void Win32GraphicsSystem::Init()
	{
		CreateContext();
		InitOpenGL();

		_opengl = std::make_unique<OpenGLState>();
		_opengl->Setup();

		_viewport_size = Core::GetSystem<Application>().GetScreenSize();
		Core::GetSystem<Application>().OnScreenSizeChanged.Listen([this](ivec2 new_size)
		{
			_viewport_size = new_size;
		});
		InitResourceLoader();
	}

	void Win32GraphicsSystem::LateInit()
	{
		GraphicsSystem::LateInit();
		_opengl->GenResources();
	}

	void Win32GraphicsSystem::Shutdown()
	{
		_opengl.reset();
		DestroyContext();
	}

	GraphicsAPI Win32GraphicsSystem::GetAPI()
	{
		return GraphicsAPI::OpenGL;
	}

	void Win32GraphicsSystem::RenderRenderBuffer()
	{
		glViewport(0, 0, _viewport_size.x, _viewport_size.y);
		glClearColor(0.f, 0.f, 0.25f, 1.f);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		_opengl->RenderDrawBuffer();	
	}
	void Win32GraphicsSystem::SwapBuffer()
	{
		::SwapBuffers(_windows_context);
	}
	void Win32GraphicsSystem::EnqueueCubemapForConvolution(RscHandle<ogl::OpenGLCubemap> handle)
	{
		cubemaps_to_convolute.enqueue(this, handle);
	}
	PixelData Win32GraphicsSystem::SelectObjViewport(const vec2& vp_pos)
	{
		//Set flag 
		_opengl->IsPicking();
		//get pixeldata
		return _opengl->PickData(vp_pos);
	}
	void Win32GraphicsSystem::LoadShaderImpl()
	{
		_opengl->LoadShaderImpl();
	}
	OpenGLState& Win32GraphicsSystem::Instance()
	{
		return *_opengl;
	}

	void Win32GraphicsSystem::Prerender()
	{
		if (prev_brdf != renderer_fragment_shaders[FBrdf])
		{
			_opengl->ComputeBRDF(RscHandle<ogl::Program>{ renderer_fragment_shaders[FBrdf]});
			prev_brdf = renderer_fragment_shaders[FBrdf];
		}
		cubemaps_to_convolute.invoke_all();
		
	}

	void Win32GraphicsSystem::CreateContext()
	{
		_windows_context = GetDC(Core::GetSystem<Windows>().GetWindowHandle());

		DEVMODE devMode = { 0 };
		devMode.dmSize = sizeof(DEVMODE);
		const BOOL b = EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &devMode);
		if (b == 0)
			throw;

		// create pixel format descriptor
		PIXELFORMATDESCRIPTOR pfdesc;
		memset(&pfdesc, 0, sizeof(PIXELFORMATDESCRIPTOR));

		pfdesc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfdesc.nVersion = 1;
		pfdesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER;
		pfdesc.iPixelType = PFD_TYPE_RGBA;
		pfdesc.cColorBits = (BYTE)devMode.dmBitsPerPel;//32; //24 bit color for front and back buffer
		pfdesc.cDepthBits = 24;//24 bit depth buffer - not used in this demo
		pfdesc.cStencilBits = 8; //8 bit stencil buffer - not used in this demo

		const int pf = ChoosePixelFormat(_windows_context, &pfdesc);//checks if the graphics card can support the pixel format requested
		if (pf == 0)
		{
			ReleaseDC(Core::GetSystem<Windows>().GetWindowHandle(), _windows_context);
			throw;
		}


		BOOL ok = SetPixelFormat(_windows_context, pf, &pfdesc);
		if (!ok)
		{
			ReleaseDC(Core::GetSystem<Windows>().GetWindowHandle(), _windows_context);
			throw;
		}
		//set the OpenGL context
		
		_opengl_context = wglCreateContext(_windows_context);

		if (!_opengl_context)
		{
			ReleaseDC(Core::GetSystem<Windows>().GetWindowHandle(), _windows_context);
			throw;
		}


		ok = wglMakeCurrent(_windows_context, _opengl_context);
		if (!ok)
		{
			wglDeleteContext(_opengl_context);
			ReleaseDC(Core::GetSystem<Windows>().GetWindowHandle(), _windows_context);
			throw;
		}

	}
	void Win32GraphicsSystem::InitOpenGL()
	{
		gladLoadGL();
		const int attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 1,
			WGL_CONTEXT_MINOR_VERSION_ARB, 0,
			WGL_CONTEXT_FLAGS_ARB, 0,
			0
		};
		gladLoadWGL(_windows_context);
		auto new_context = wglCreateContextAttribsARB(_windows_context, _opengl_context, attribs);
		
		if (new_context)
		{
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(_opengl_context);
			_opengl_context = new_context;
			wglMakeCurrent(_windows_context, _opengl_context);
		}
		else
			throw;
		std::cout << (char*) glGetString(GL_VERSION) << "\n";
	}

	void Win32GraphicsSystem::InitResourceLoader()
	{
		// register factories
		Core::GetResourceManager().RegisterFactory<MaterialFactory>();
		Core::GetResourceManager().RegisterFactory<EasyFactory<MaterialInstance>>();
		Core::GetResourceManager().RegisterFactory<ShaderTemplateFactory>();
		Core::GetResourceManager().RegisterFactory<OpenGLMeshFactory>();
		Core::GetResourceManager().RegisterFactory<anim::SkeletonFactory>();
		Core::GetResourceManager().RegisterFactory<EasyFactory<anim::Animation>>();
		Core::GetResourceManager().RegisterFactory<ProgramFactory>();
		Core::GetResourceManager().RegisterFactory<OpenGLTextureFactory>();
		Core::GetResourceManager().RegisterFactory<OpenGLRenderTargetFactory>();
		Core::GetResourceManager().RegisterFactory<OpenGLFrameBufferFactory>();
		Core::GetResourceManager().RegisterFactory<OpenGLFontAtlasFactory>();

		// register extensions
		Core::GetResourceManager().RegisterLoader<MaterialLoader>(Material::ext);
		Core::GetResourceManager().RegisterLoader<GLSLLoader>(".vert");
		Core::GetResourceManager().RegisterLoader<GLSLLoader>(".geom");
		Core::GetResourceManager().RegisterLoader<GLSLLoader>(".frag");
		Core::GetResourceManager().RegisterLoader<GLSLLoader>(".pfrag");
		Core::GetResourceManager().RegisterLoader<OpenGLRenderTargetLoader>(RenderTarget::ext);
		Core::GetResourceManager().RegisterLoader<ShaderTemplateLoader>(".tmpt");
		//Core::GetResourceManager().RegisterLoader<OpenGLFontAtlasLoader>(".ttf");
	}

	void Win32GraphicsSystem::ConvoluteCubeMap(RscHandle<ogl::OpenGLCubemap> handle)
	{
		_opengl->ConvoluteCubeMap(handle);
	}

	void Win32GraphicsSystem::DestroyContext()
	{
		if (_opengl_context)
		{
			if (!wglMakeCurrent(NULL, NULL))
			{
				//log
			}
		}

		if (!wglDeleteContext(_opengl_context))
		{
			//log
		}
		_opengl_context = NULL;

		if (_windows_context && !ReleaseDC(Core::GetSystem<Windows>().GetWindowHandle(), _windows_context))
		{
			_windows_context = NULL;
		}
	}
}
