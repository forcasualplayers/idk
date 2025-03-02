#include <pch_common.h>
#include <core/Core.inl>
#include <win32/WindowsApplication.h>
#include <win32/XInputSystem.h>

#include <prefab/Prefab.h>
#include <res/CompiledAssetLoader.inl>

#include <res/CompiledAssets.h>
#include <vkn/VulkanWin32GraphicsSystem.h>
#include <vkn/VulkanMesh.h>
#include <vkn/VknTexture.h>
#include <vkn/VknFontAtlas.h>
#include <opengl/system/OpenGLGraphicsSystem.h>
#include <opengl/resource/OpenGLMesh.h>
#include <opengl/resource/OpenGLTexture.h>
#include <editor/IDE.h>
#include <file/FileSystem.h>
#include <debug/LogSystem.h>

#include <vkn/ExtraConfigs.h>

bool HasArg(std::wstring_view arg, LPWSTR* args, int num_args);

HWND& GetHWND()
{
	static HWND hwnd;
	return hwnd;
}
void AddSystems(idk::unique_ptr<idk::Core>& c, HINSTANCE hInstance, int nCmdShow, LPWSTR* command_lines, int num_args)
{
	idk::hack::LogSystemConfig::GetSingleton().enabled = HasArg(L"--log", command_lines, num_args);
	using namespace idk;
	auto& windows = c->AddSystem<Windows>(hInstance, nCmdShow);
	windows.SetFullscreen(false);
	GetHWND()=windows.GetWindowHandle();
	c->AddSystem<win::XInputSystem>();

	GraphicsSystem* gSys = nullptr;
	auto gfx_api = HasArg(L"--opengl", command_lines, num_args) ? GraphicsAPI::OpenGL : GraphicsAPI::Vulkan;
	switch (gfx_api)
	{
	case GraphicsAPI::Vulkan:
	{
		auto& sys = c->AddSystem<vkn::VulkanWin32GraphicsSystem>();
		if (HasArg(L"--simulate", command_lines, num_args))
		{
			vkn::ExtraConfigs ec{ .enable_simulation = true };
			sys.SetExtraConfigs(ec);
		}
		Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<CompiledMesh, vkn::VulkanMesh>>();
		Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<CompiledTexture, vkn::VknTexture>>();
		Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<CompiledFontAtlas, vkn::VknFontAtlas>>();
		gSys = &sys;
		if (HasArg(L"--validation", command_lines, num_args))
			sys.Instance().EnableValidation();
		break;
	}
	case GraphicsAPI::OpenGL:
		gSys = &c->AddSystem<ogl::Win32GraphicsSystem>();
		Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<CompiledMesh, ogl::OpenGLMesh>>();
		Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<CompiledTexture, ogl::OpenGLTexture>>();
		break;
	default:
		break;

	}
	gSys->is_deferred(!HasArg(L"--forward", command_lines, num_args));
	Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<Prefab, Prefab, false>>();
	Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<anim::Animation, anim::Animation>>();
	Core::GetResourceManager().RegisterAssetLoader<CompiledAssetLoader<anim::Skeleton, anim::Skeleton>>();
	Core::GetResourceManager().RegisterCompilableExtension(".tga");
	Core::GetResourceManager().RegisterCompilableExtension(".png");
	Core::GetResourceManager().RegisterCompilableExtension(".gif");
	Core::GetResourceManager().RegisterCompilableExtension(".bmp");
	Core::GetResourceManager().RegisterCompilableExtension(".jpeg");
	Core::GetResourceManager().RegisterCompilableExtension(".jpg");
	Core::GetResourceManager().RegisterCompilableExtension(".dds");
	Core::GetResourceManager().RegisterCompilableExtension(".fbx");
	Core::GetResourceManager().RegisterCompilableExtension(".obj");
	Core::GetResourceManager().RegisterCompilableExtension(".ma");
	Core::GetResourceManager().RegisterCompilableExtension(".ttf");

	c->AddSystem<IDE>();

}
void ConfigAppDataAndLogger()
{
	using namespace idk;
	namespace fs = std::filesystem;

	fs::path idk_app_data = Core::GetSystem<Application>().GetAppData().sv();
	idk_app_data /= "idk";
	if (!fs::exists(idk_app_data))
		fs::create_directory(idk_app_data);

	idk_app_data /= "logs";
	if (!fs::exists(idk_app_data))
		fs::create_directory(idk_app_data);
	Core::GetSystem<LogSystem>().SetLogDir(idk_app_data.string());
}