#include <Game.h>

#include <Engine.h>
#include <Application/ApplicationEntryPoint.h>
#include <Scene/Scene.h>
#include <WASDOverlay.h>
#include <CollisionOverlay.h>
#include <memory>
#include <string>

// ImGui runtime HUD overlay
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class GameLauncher : public Application
{
public:
	GameLauncher() : Application()
	{
		myScene.reset(myGraphicsEngine->GetScene());

		myGraphicsEngine->myIsRuntime = true;
		myScene->Load("Game.json");

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(myGraphicsEngine->GetWindowHandler()->GetWindowHandle());
		ImGui_ImplDX11_Init(DX11Framework::Device, DX11Framework::Context);

		auto size = DX11Framework::GetResolution();
		myGame = std::make_unique<Game>(myScene);
		myGame->Init(0.0f, 0.0f, static_cast<float>(size.X), static_cast<float>(size.Y));
	}

	virtual ~GameLauncher()
	{
		if (myGame)
		{
			myGame->Deinit();
		}
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void BeginFrame()
	{
		::Application::BeginFrame();

	}

	void Render()
	{
		myGraphicsEngine->Update(myDeltaTime);
		if (myGame)
		{
			myGame->update();
		}
		myScene->Update(myDeltaTime);

		::Application::Render();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		WasdState wasd{};
		auto& input = InputManager::Get();
		wasd.w = input.IsKeyDown('W');
		wasd.a = input.IsKeyDown('A');
		wasd.s = input.IsKeyDown('S');
		wasd.d = input.IsKeyDown('D');

		auto size = DX11Framework::GetResolution();
		const float hudH = 124.0f;
		CollisionOverlay::DrawCarCollisionOverlay(
			myScene.get(),
			myScene->GetActiveCamera(),
			{ 0.0f, 0.0f },
			{ static_cast<float>(size.X), static_cast<float>(size.Y) });
		DrawWASDOverlay(wasd, { 12.0f, static_cast<float>(size.Y) - hudH - 12.0f });

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void EndFrame()
	{
		::Application::EndFrame();
	}

private:
	std::shared_ptr<Scene> myScene;
	std::unique_ptr<Game> myGame;
};


Application* CreateApplication()
{
	return new GameLauncher();
}

LRESULT CALLBACK WinProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	static WindowHandler* windowHandler = nullptr;

	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	if (uMsg == WM_DESTROY || uMsg == WM_CLOSE)
	{
		PostQuitMessage(0);
	}
	else if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* createdStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		windowHandler = static_cast<WindowHandler*>(createdStruct->lpCreateParams);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
