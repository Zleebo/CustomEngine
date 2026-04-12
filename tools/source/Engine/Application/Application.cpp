#include <stdafx.h>

#include "Application.h"
#include "GraphicsEngine/GraphicsEngine.h"
#include "Input/InputManager.h"
#include "Audio/AudioEngine.h"
#include "Components/ComponentFactory.h"
#include "Physics/RigidbodyComponent.h"
#include "Physics/SuspensionConstraintComponent.h"
#include "Audio/AudioSourceComponent.h"
#include "Animation/AnimatorComponent.h"

#include <DX11Framework.h>

#include <Windows.h>
#include <chrono>
#include <comdef.h>
#include <Debug/Profiler.h>


Application::Application()
{
	REGISTER_COMPONENT(RigidbodyComponent);
	REGISTER_COMPONENT(SuspensionConstraintComponent);
	REGISTER_COMPONENT(AudioSourceComponent);
	REGISTER_COMPONENT(AnimatorComponent);

	myGraphicsEngine = std::shared_ptr<GraphicsEngine>(new GraphicsEngine());
	myGraphicsEngine->Init();
	AudioEngine::Get().Init();
}

Application::~Application()
{
	AudioEngine::Get().Shutdown();
}

void Application::BeginFrame()
{
	myGraphicsEngine->BeginFrame();
}

void Application::Render()
{
	myGraphicsEngine->RenderFrame();
}

void Application::EndFrame()
{
	myGraphicsEngine->EndFrame();
}

void Application::Run()
{
	using clock = std::chrono::high_resolution_clock;
	auto lastFrame = clock::now();

	bool bShouldRun = true;
	while (bShouldRun)
	{
		auto now = clock::now();
		myDeltaTime = std::chrono::duration<float>(now - lastFrame).count();
		if (myDeltaTime > 0.1f) myDeltaTime = 0.1f;
		lastFrame = now;

		InputManager::Get().Update();

		static int dbgFrame = 0;
		if (dbgFrame < 5)
		{
			char buf[64];
			sprintf_s(buf, "[RUN] Frame %d start\n", dbgFrame);
			OutputDebugStringA(buf);
		}

		try
		{
			{ PROFILE_SCOPE("BeginFrame"); BeginFrame(); }
			if (dbgFrame < 5) OutputDebugStringA("[RUN] BeginFrame OK\n");
			{ PROFILE_SCOPE("Render");     Render(); }
			if (dbgFrame < 5) OutputDebugStringA("[RUN] Render OK\n");
			{ PROFILE_SCOPE("EndFrame");   EndFrame(); }
			if (dbgFrame < 5) OutputDebugStringA("[RUN] EndFrame OK\n");
		}
		catch (const _com_error& e)
		{
			char buf[256];
			sprintf_s(buf, "[RUN] _com_error: hr=0x%08lX msg=%S\n", e.Error(), e.ErrorMessage());
			OutputDebugStringA(buf);
			try { EndFrame(); } catch (...) {}
		}
		catch (const std::exception& e)
		{
			char buf[256];
			sprintf_s(buf, "[RUN] std::exception: %s\n", e.what());
			OutputDebugStringA(buf);
			try { EndFrame(); } catch (...) {}
		}
		catch (...)
		{
			OutputDebugStringA("[RUN] Unknown exception in render loop\n");
			try { EndFrame(); } catch (...) {}
		}
		++dbgFrame;

		MSG msg = { 0 };
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bShouldRun = false;
			}
		}
	}
}
