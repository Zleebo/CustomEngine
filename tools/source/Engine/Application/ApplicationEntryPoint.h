#pragma once
#include <Core/EngineSettings.h>

extern Application* CreateApplication();

//#ifdef PLATFORM_WINDOWS
#include <Windows.h>

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, char*, int /*nShowCmd*/)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Settings::load_config_files();
	auto app = CreateApplication();
	app->Run();
	CoUninitialize();

	return 0;
}

//#endif // PLATFORM_WINDOWS
