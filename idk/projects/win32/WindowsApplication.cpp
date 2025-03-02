#include "pch.h"

#include <thread>
#include <iostream>
#include <direct.h>
#include <tchar.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include <stdio.h>  
#include <locale>
#include <codecvt>
#include <commdlg.h>

#include <core/Core.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <processthreadsapi.h>

#include "InputManager.h"
#include "WindowsApplication.h"

#include <ds/span.inl>
#include <event/Signal.inl>

#include "WinMessageTable.h"

static HCURSOR prevCursor;

namespace idk::win
{
	Windows::Windows(HINSTANCE _hInstance, int nCmdShow, HICON icon)
		: hInstance{ _hInstance }, _input_manager{std::make_unique<InputManager>()}, icon{icon}
	{
		instance = this;
		// Initialize global strings
		//LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		//LoadStringW(hInstance, IDC_GAME, szWindowClass, MAX_LOADSTRING);
		/*
		#ifdef _DEBUG
		{
			AllocConsole();
			AttachConsole(GetCurrentProcessId());
			FILE* pCout;
			freopen_s(&pCout, "conout$", "w", stdout); //returns 0
			freopen_s(&pCout, "conout$", "w", stderr); //returns 0
			SetConsoleTitle(L"IDK 0.1a");
		}
		#endif
		*/
		MyRegisterClass();

		InitInstance(nCmdShow); 
		//SetFullscreen(true);
		//hAccelTable = LoadAccelerators(hInstance, 0);
	}

	Windows::~Windows()
	{
	}

	void Windows::SetIcon(HICON icon)
	{
		this->icon = icon;
	}

	void Windows::PollEvents()
	{
		MSG msg;
		_input_manager->SwapBuffers();
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

	}

	string Windows::Exec(string_view path, span<const char*> argv, bool wait)
	{
		vector<const char*> args{ path.data() };
		for (auto& elem : argv)
			args.emplace_back(elem);
		args.emplace_back(nullptr);

		intptr_t ret{};

		if (wait)
		{
			SECURITY_ATTRIBUTES saAttr;

			printf("\n->Start of parent execution.\n");

			// Set the bInheritHandle flag so pipe handles are inherited. 

			saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
			saAttr.bInheritHandle = TRUE;
			saAttr.lpSecurityDescriptor = NULL;

			HANDLE g_hChildStd_OUT_Rd = NULL;
			HANDLE g_hChildStd_OUT_Wr = NULL;

			PROCESS_INFORMATION info;
			ZeroMemory(&info, sizeof(info));
			STARTUPINFOA startup;
			ZeroMemory(&startup, sizeof(startup));

			auto command = string{};
			for (auto& elem : argv)
				command += string{ elem } +" ";



			//*/
			// create a pipe for the output

			if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
				return "failed to create output pipe";

			// Ensure the read handle to the pipe for STDOUT is not inherited.
			if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
				return "I have no idea";


			startup.dwFlags |= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			startup.wShowWindow = SW_HIDE;
			startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			startup.hStdError = g_hChildStd_OUT_Wr;
			startup.hStdOutput = g_hChildStd_OUT_Wr;

			if (CreateProcessA(path.data(), command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &info))
			{
				string output_value; 

				CloseHandle(g_hChildStd_OUT_Wr);
				CloseHandle(info.hProcess);
				CloseHandle(info.hThread);
				for (;;)
				{
					DWORD dwRead;
					char readbuf[256];
					auto bSuccess = ReadFile(g_hChildStd_OUT_Rd, readbuf, 256, &dwRead, nullptr);
					if (!bSuccess || dwRead == 0)
						break;
					output_value += string_view(readbuf, dwRead);
				}

				return output_value;
			}
			else
			{
				char error_buffer[256];
				auto dw = GetLastError();
				FormatMessageA(
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					dw,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					error_buffer,
					0, NULL);
				LOG_TO(LogPool::SYS, "%s", error_buffer);

				return "fail";
			}
			/*/
			auto pfile = _popen((string{ path } + " " + command).data(), "rt");
			if (!pfile)
				return {};

			string retval;
			char printbuff[256];
			while (fgets(printbuff, 256, pfile))
				retval += printbuff;

			if (feof(pfile))
				LOG_TO(LogPool::SYS, "Process returned %d", _pclose(pfile));
			else
				LOG_TO(LogPool::SYS, "Error: failed to read file to the end");
				
			return retval;
			*/
		}
		else
		{
			ret = _spawnvp(wait ? P_WAIT : P_NOWAIT, path.data(), args.data() + 1);
			LOG_TO(LogPool::SYS, "Executing %s with child %p", path.data(), ret);

			if (children.size() >= std::thread::hardware_concurrency())
				WaitForChildren();
			children.emplace_back(ret);

			// no output
			return {};
		}
	}
	void Windows::WaitForChildren()
	{
		for (auto& elem : children)
		{
			int ret;
			_cwait(&ret, elem, 0);
			LOG_TO(LogPool::SYS, "Child %p ended with retcode %d", elem, ret);
		}
		children.clear();
	}
#pragma optimize ("", on)
	int Windows::GetReturnVal()
	{
		return retval;
	}
	void Windows::Init()
	{
	}
	void Windows::LateInit()
	{
		prevCursor = SetCursor(NULL);
	}
	ivec2 Windows::GetScreenSize() 
	{
		RECT rect;
		if (!GetClientRect(hWnd, &rect))
			return ivec2{};
		return ivec2((rect.right - rect.left), (rect.bottom - rect.top));
	}
	vec2 Windows::GetMouseScreenPos()
	{
		ivec2 sSize = GetScreenSize();
		return vec2{ 
			static_cast<float>(screenpos.x) / static_cast<float>(sSize.x),
			static_cast<float>(screenpos.y) / static_cast<float>(sSize.y) };
	}
	vec2 Windows::GetMouseScreenDel()
	{
		//ivec2 sSize = GetScreenSize();
		return ndc_screendel;
	}
	ivec2 Windows::GetMousePixelPos()
	{
		return screenpos;
	}
	ivec2 Windows::GetMousePixelDel()
	{
		return screendel;
	}
	ivec2 Windows::GetMouseScroll()
	{
		return _input_manager->GetMouseScroll();
	}
	void Windows::PushWinProcEvent(std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> func)
	{
		winProcList.push_back(func); 
	}

	bool Windows::GetKeyDown(Key key)
	{
		return _input_manager->GetKeyDown(static_cast<int>(key));
	}
	bool Windows::GetKey(Key key)
	{
		return _input_manager->GetKey(static_cast<int>(key));
	}
	bool Windows::GetKeyUp(Key key)
	{
		return _input_manager->GetKeyUp(static_cast<int>(key));
	}
	char Windows::GetChar()
	{
		return _input_manager->GetChar();
	}

	void Windows::SwapInputBuffers()
	{
		_input_manager->SwapBuffers();
	}

	bool Windows::GetFullscreen() const
	{
		DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
		return !(dwStyle & WS_OVERLAPPEDWINDOW);
	}

	bool Windows::SetFullscreen(bool fullscreen)
    {
		// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353

		DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);

		if (fullscreen && (dwStyle & WS_OVERLAPPEDWINDOW))
		{
			MONITORINFO mi{ sizeof(mi) };
			if (GetWindowPlacement(hWnd, &wp_prev) &&
				GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &mi))
			{
				SetWindowLong(hWnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
				SetWindowPos(hWnd, HWND_TOP,
							 mi.rcMonitor.left, mi.rcMonitor.top,
							 mi.rcMonitor.right - mi.rcMonitor.left,
							 mi.rcMonitor.bottom - mi.rcMonitor.top,
							 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			}
		}
		else if(!fullscreen && !(dwStyle & WS_OVERLAPPEDWINDOW))
		{
			SetWindowLong(hWnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
			SetWindowPlacement(hWnd, &wp_prev);
			SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
						 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}

        return true;
    }
	
	void Windows::SetTitle(string_view new_title)
	{
		std::wstring wide_str{ new_title.begin(), new_title.end() };
		SetWindowText(hWnd, wide_str.data());
	}
	string Windows::GetExecutableDir()
	{
		char buffer[MAX_PATH] = { 0 };

		// Get the program directory
		int bytes = GetModuleFileNameA(NULL, buffer, MAX_PATH);
		if (bytes == 0)
			std::cout << "[File System] Unable to get program directory." << std::endl;
		auto exe_dir = string{ buffer };
		auto pos = exe_dir.find_last_of("\\");
		return exe_dir.substr(0, pos);
	}
	string Windows::GetAppData()
	{
		char* env_buff;
		size_t len;
		auto err = _dupenv_s(&env_buff, &len, "appdata");
		if (err)
			std::cout << "[File System] Unable to get solution directory." << std::endl;
		auto appdata = string{ env_buff ? env_buff : "" };
		free(env_buff);
		return appdata;
	}
	string Windows::GetCurrentWorkingDir()
	{
		char buffer[MAX_PATH] = { 0 };
		if (!_getcwd(buffer, sizeof(buffer)))
			std::cout << "[File System] Unable to get solution directory." << std::endl;
		return string{ buffer };
	}
	opt<string> Windows::OpenFileDialog(const DialogOptions& dialog)
	{
		OPENFILENAMEA ofn;       // common dialog box structure
		CHAR szFile[260];       // buffer for file name
		HWND hwnd{};              // owner window

        string filter{ dialog.filter_name };
        filter += " (*";
        filter += dialog.extension;
        filter += ")";

        filter += '\0';
        filter += '*';
        filter += dialog.extension;
        filter += '\0';

		auto init = GetExecutableDir();

		// Initialize OPENFILENAME
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFile = szFile;
		// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
		// use the contents of szFile to initialize itself.
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter.c_str();
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = init.data();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		// Display the Open dialog box. 
		switch (dialog.type)
		{
		case DialogType::Save:
			if (GetSaveFileNameA(&ofn) == TRUE)
			{
                return ofn.lpstrFile;
			}
			break;
		case DialogType::Open:
			if (GetOpenFileNameA(&ofn) == TRUE)
			{
                return ofn.lpstrFile;
			}
		}
		return std::nullopt;
	}
	LRESULT Windows::WndProc(HWND _hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (_hWnd != hWnd)
			return DefWindowProc(_hWnd, message, wParam, lParam);

		for (auto& elem : winProcList)
			elem(_hWnd, message, wParam, lParam);

		switch (message)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:

			if (_focused)
				_input_manager->SetKeyDown((int)wParam);
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			_input_manager->SetKeyUp((int)wParam);
			break;
		case WM_CHAR:
			_input_manager->SetChar((char)wParam);
			break;
		case WM_MBUTTONDOWN:
			grabScreenCoordinates(lParam);

			if (_focused)
				_input_manager->SetMouseDown((int)Key::MButton);

			break;
		case WM_LBUTTONDOWN:
			grabScreenCoordinates(lParam);

			if (_focused)
				_input_manager->SetMouseDown((int)Key::LButton);

			break;
		case WM_RBUTTONDOWN:
			grabScreenCoordinates(lParam);

			if (_focused)
				_input_manager->SetMouseDown((int)Key::RButton);

			break;
		case WM_LBUTTONUP:
			grabScreenCoordinates(lParam);

			_input_manager->SetMouseDown((int)Key::LButton);

			break;
		case WM_RBUTTONUP:
			grabScreenCoordinates(lParam);

			_input_manager->SetMouseUp((int)Key::RButton);
			break;
		case WM_MBUTTONUP:
			grabScreenCoordinates(lParam);

			_input_manager->SetMouseUp((int)Key::MButton);
			break;
		case WM_MOUSEMOVE:
			grabScreenCoordinates(lParam);
			break;
		case WM_MOUSEWHEEL:
			_input_manager->SetMouseScroll(ivec2{ 0, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA });
			break;
		case WM_PAINT:
			ValidateRect(hWnd, 0);
			break;
		case WM_NCCREATE:
		{
			auto ptr = reinterpret_cast<CREATESTRUCTW*&>(lParam);
			OnScreenSizeChanged.Fire(ivec2{ ptr->cx, ptr->cy });
		}
		break;
		case WM_SETFOCUS:
			OnFocusGain.Fire();
			prevCursor = SetCursor(NULL);
			_focused = true;
			break;
		case WM_KILLFOCUS:
			OnFocusLost.Fire();
			SetCursor(prevCursor);
			prevCursor = NULL;
			_focused = false;
			_input_manager->FlushCurrentBuffer();
			break;
		case WM_SIZE:
			OnScreenSizeChanged.Fire(ivec2{ LOWORD(lParam), HIWORD(lParam) });
			break;
		case WM_DESTROY:
            OnClosed.Fire();
			PostQuitMessage(0);
			break;
		case WM_NCDESTROY:
			Core::Shutdown();
			break;
		case WM_QUIT:
			retval = (int)wParam;
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	}

	void Windows::grabScreenCoordinates(LPARAM lParam)
	{
		old_screenpos = screenpos;
		screenpos.x = LOWORD(lParam);
		screenpos.y = HIWORD(lParam);
		
		
		vec2 sSize = vec2{ GetScreenSize() };
		ndc_screendel = vec2{ screenpos.x, screenpos.y } / sSize - vec2{ old_screenpos.x, old_screenpos.y } / sSize;

		screendel = screenpos - old_screenpos;
	}

	HINSTANCE Windows::GetInstance()
	{
		return hInstance;
	}

	HWND Windows::GetWindowHandle()
	{
		return hWnd;
	}

	ATOM Windows::MyRegisterClass()
	{
		WNDCLASSEXW wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = ::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = icon;  
		wcex.hCursor = NULL;
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = 0;  
		wcex.lpszClassName = szWindowClass;
		wcex.hIconSm = icon;

		return RegisterClassExW(&wcex);
	}

	BOOL Windows::InitInstance([[maybe_unused]]int nCmdShow)
	{
		//hWnd = CreateWindowW(szWindowClass, L"Hyde & Seek", WS_OVERLAPPEDWINDOW,
		//					 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
		int w = GetSystemMetrics(SM_CXSCREEN);
		int h = GetSystemMetrics(SM_CYSCREEN);
        hWnd = CreateWindowW(szWindowClass, L"Hyde & Seek", WS_OVERLAPPEDWINDOW,
			0, 0, w, h, nullptr, nullptr, hInstance, nullptr);

		if (!hWnd)
		{
			return FALSE;
		}

		ShowWindow(hWnd, SW_MAXIMIZE);
		SetForegroundWindow(hWnd);
		SetFocus(hWnd);
		UpdateWindow(hWnd);

		return TRUE;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (idk::win::Windows::instance)  // See Note 1 below
		// Call member function if instance is available
		return idk::win::Windows::instance->WndProc(hWnd, message, wParam, lParam);
	else
		// Otherwise perform default message handling
		return DefWindowProc(hWnd, message, wParam, lParam);
}
