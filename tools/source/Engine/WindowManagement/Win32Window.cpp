#include <stdafx.h>

#include "Win32Window.h"
#include <Application.h>
#include <cstdlib>

namespace
{
	int ReadEnvInt(const char* aName, int aFallback)
	{
		char* value = nullptr;
		size_t len = 0;
		if (_dupenv_s(&value, &len, aName) != 0 || value == nullptr || len == 0)
		{
			return aFallback;
		}

		const int parsed = std::atoi(value);
		free(value);
		return parsed;
	}

	bool ReadEnvFlag(const char* aName)
	{
		char* value = nullptr;
		size_t len = 0;
		if (_dupenv_s(&value, &len, aName) != 0 || value == nullptr || len == 0)
		{
			return false;
		}

		const bool enabled = value[0] != '0';
		free(value);
		return enabled;
	}
}

bool WindowHandler::Init(Settings::WindowData someWindowData)
{
	someWindowData.x = ReadEnvInt("TE_WINDOW_X", someWindowData.x);
	someWindowData.y = ReadEnvInt("TE_WINDOW_Y", someWindowData.y);
	someWindowData.w = ReadEnvInt("TE_WINDOW_W", someWindowData.w);
	someWindowData.h = ReadEnvInt("TE_WINDOW_H", someWindowData.h);

	myWidth = static_cast<float>(someWindowData.w);
	myHeight = static_cast<float>(someWindowData.h);

	WNDCLASS windowClass = {};
	windowClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = WinProc;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = L"tools";
	RegisterClass(&windowClass);

	std::string s = Settings::window_data.title;
	std::wstring stemp = std::wstring(s.begin(), s.end());

	const bool noActivate = ReadEnvFlag("TE_AUTOTEST_NOACTIVATE");
	DWORD exStyle = 0;
	if (noActivate)
	{
		exStyle |= WS_EX_NOACTIVATE | WS_EX_APPWINDOW;
	}

	myWindowHandle = CreateWindowEx(
		exStyle,
		L"tools",
		stemp.c_str(),
		WS_OVERLAPPEDWINDOW | WS_POPUP,
		someWindowData.x,
		someWindowData.y,
		someWindowData.w,
		someWindowData.h,
		nullptr, nullptr, nullptr,
		this
	);

	if (myWindowHandle != nullptr)
	{
		ShowWindow(myWindowHandle, noActivate ? SW_SHOWNOACTIVATE : SW_SHOW);
		UpdateWindow(myWindowHandle);
	}

	return true;
}

WindowHandler::WindowHandler()
{
	myWindowHandle = nullptr;
	myWidth = 0;
	myHeight = 0;
}

WindowHandler::~WindowHandler()
{
	myWindowHandle = nullptr;
	myWidth = 0;
	myHeight = 0;
}
