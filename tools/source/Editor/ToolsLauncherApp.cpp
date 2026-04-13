#include <Game.h>

#include <Engine.h>
#include <GraphicsEngine/GraphicsEngine.h>
#include <Application/ApplicationEntryPoint.h>

#include <Tools/SceneHierarchy/SceneHierarchy.h>
#include <Tools/ModelProperties/ModelProperties.h>
#include <Tools/AssetBrowser/AssetBrowser.h>
#include <Tools/Gizmos/Gizmos.h>
#include <Tools/TerrainTool/TerrainTool.h>

#include <CommandManager.h>
#include <Commands/DeleteCommand.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <ImGuizmo.h>
#include <Selection.h>
#include <WASDOverlay.h>
#include <CollisionOverlay.h>

#include <Scene/SceneSerializer.h>
#include <Scene/Prefab.h>
#include <filesystem>
#include <fstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



// Accessed by WinProc (defined later in this file) to forward WM_SIZE to GraphicsEngine.
static GraphicsEngine* g_graphicsEngine = nullptr;

class ToolsLauncher : public Application
{
public:
	ToolsLauncher()
		: Application()
	{
		g_graphicsEngine = myGraphicsEngine.get();
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		myActiveScene.reset(myGraphicsEngine->GetScene());
		myActiveScene->onBeforeClear = []{ Selection::ClearSelection(); };

		ImGui_ImplWin32_Init( myGraphicsEngine->GetWindowHandler()->GetWindowHandle());
		ImGui_ImplDX11_Init(DX11Framework::Device, DX11Framework::Context);


		DragAcceptFiles(myGraphicsEngine->GetWindowHandler()->GetWindowHandle(), TRUE);

		myGame.reset(new Game(myActiveScene));

		// Load scene AFTER Game is constructed so REGISTER_COMPONENT calls complete first
		myActiveScene->Load("scene.json");
		{
			std::error_code ec;
			mySceneLastWriteTime = std::filesystem::last_write_time("scene.json", ec);
		}

		myTools.push_back(local<ModelProperties>(new ModelProperties(myActiveScene->GetModelList())));
		myTools.push_back(local<SceneHierarchy>(new SceneHierarchy(myActiveScene)));
		myTools.push_back(local<AssetBrowser>(new AssetBrowser(myActiveScene)));
		myTools.push_back(local<TerrainTool>(new TerrainTool(myGraphicsEngine.get())));
		myGizmos.reset(new Gizmos(myActiveScene));
	}

	~ToolsLauncher()
	{
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
		static uint64_t lastTick = GetTickCount64();
		uint64_t now = GetTickCount64();
		float dt = min((now - lastTick) / 1000.f, 0.1f);
		lastTick = now;
		if (myRunState == GameState::EDITOR)
		{
			myActiveScene->GetActiveCameraMutable()->Update(dt, mySceneViewportHovered);

			// Hot-reload: reload scene.json if it was modified on disk
			static uint64_t lastFileCheck = 0;
			const uint64_t nowMs = GetTickCount64();
			if (nowMs - lastFileCheck >= 1000)
			{
				lastFileCheck = nowMs;
				std::error_code ec;
				const auto wt = std::filesystem::last_write_time("scene.json", ec);
				if (!ec && wt != mySceneLastWriteTime)
				{
					mySceneLastWriteTime = wt;
					Selection::ClearSelection();
					myActiveScene->Load("scene.json");
				}
			}
		}
		myGraphicsEngine->Update(dt);

		::Application::Render();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		static bool show_dockspace = true;
		ShowExampleAppDockSpace(&show_dockspace);
		
		for (const auto& tool : myTools)
		{
			tool->Draw();
		}


		ImGui::Begin("Tools");
		{
			const char* playOrPause = myRunState == GameState::EDITOR ? "Play" : "Stop";
			if (ImGui::Button(playOrPause))
			{
				Selection::ClearSelection();

				if (myRunState == GameState::EDITOR)
				{
					myRunState = GameState::RUNTIME;

					try
					{
						myActiveScene->Save("Editor.json");
						myActiveScene->Save("Game.json");
					} catch (const std::exception& e)
					{
						OutputDebugStringA("[PLAY] Save failed: ");
						OutputDebugStringA(e.what());
						OutputDebugStringA("\n");
					}
					myGame->Init(myViewportRect.x, myViewportRect.y, myViewportRect.w, myViewportRect.h);
					myRuntimeInitialized = true;
				}
				else if (myRunState == GameState::RUNTIME)
				{
					if (myRuntimeInitialized)
					{
						myGame->Deinit();
						myRuntimeInitialized = false;
					}
					myRunState = GameState::EDITOR;
					myActiveScene->Load("Editor.json");
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Save") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('S')))
			{
				myActiveScene->Save("scene.json");
			}
			ImGui::SameLine();
			myGizmos->Draw();

			// Global undo / redo (guarded against text-input fields)
			if (!ImGui::GetIO().WantTextInput)
			{
				if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('Z')) CommandManager::Undo();
				if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed('Y')) CommandManager::Redo();
			}
		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
		ImGui::Begin("Scene");
		{
			auto tmp = ImGui::GetContentRegionAvail();
			auto res = myGraphicsEngine->GetFramework()->GetResolution();
			float aspect = float(res.Y) / res.X;
			auto rs = ImVec2(tmp.x, tmp.x*aspect);

			if (rs.y > tmp.y)
			{
				aspect = float(res.X) / res.Y;
				rs = ImVec2(tmp.y* aspect, tmp.y);
			}

			// Capture exact image screen rect so hover/gizmo use the image bounds, not the full panel
			ImVec2 imageMin = ImGui::GetCursorScreenPos();
			ImVec2 imageMax = { imageMin.x + rs.x, imageMin.y + rs.y };

			ImGui::Image(
				(void *)myGraphicsEngine->GetRenderedImage(),
				rs
			);

			CollisionOverlay::DrawCarCollisionOverlay(
				myActiveScene.get(),
				myActiveScene->GetActiveCamera(),
				imageMin,
				rs);

			// Drop prefab onto viewport → spawn all prefab objects via Prefab::Spawn
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(".prefab"))
				{
					const char* prefabPath = (const char*)payload->Data;
					Prefab prefab;
					auto spawned = prefab.Spawn(myActiveScene.get(), prefabPath, { 0.0f, 1.25f, 0.0f });
					Selection::ClearSelection();
					for (auto* mi : spawned)
						Selection::AddToSelection(mi);
				}
				ImGui::EndDragDropTarget();
			}

			ImGuizmo::SetRect(imageMin.x, imageMin.y, rs.x, rs.y);

			myViewportRect = { imageMin.x, imageMin.y, imageMax.x, imageMax.y };
			mySceneViewportHovered = ImGui::IsMouseHoveringRect(imageMin, imageMax);

			if (myRunState == GameState::EDITOR)
			{
				myGizmos->DrawGizmos();

				// Delete selected objects when hovering the viewport
				if (mySceneViewportHovered
					&& !Selection::GetSelection().empty()
					&& ImGui::IsKeyPressed(ImGuiKey_Delete))
				{
					CommandManager::DoCommand(
						std::make_shared<DeleteCommand>(myActiveScene, Selection::GetSelection()));
					Selection::ClearSelection();
				}

				if (mySceneViewportHovered)
				{
					ImGui::GetForegroundDrawList()->AddRect(imageMin, imageMax, ImColor(0.0f, 0.5f, 0.5f), 0, 0, 4.0f);
					float x = ImGui::GetMousePos().x - imageMin.x;
					float y = ImGui::GetMousePos().y - imageMin.y;

					float sx = DX11Framework::GetResolution().X / rs.x;
					float sy = DX11Framework::GetResolution().Y / rs.y;
					ImGui::SetNextWindowSize({ 100, 100 });
					ImGui::SetNextWindowPos({ 100, 100 });

					int id = myGraphicsEngine->MouseOver((uint32_t)(x * sx), (uint32_t)(y * sy));
					char id_text[12];
					sprintf_s(id_text, sizeof(id_text), "%d", id);
					ImGui::GetWindowDrawList()->AddText({ imageMin.x + 10, imageMin.y + 10 }, ImColor(1.0f, 1.0f, 1.0f), id_text);
				
					if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && id >= 0 && !ImGuizmo::IsOver())
					{
						ModelInstance *mi = myActiveScene->GetModelInstanceByID(id);
						if (mi != nullptr)
						{
							if (id == -1)
							{
								Selection::ClearSelection();
							}
							else if (ImGui::GetIO().KeyCtrl)
							{
								Selection::ToggleSelect(myActiveScene->GetModelInstanceByID(id));
							}
							else
							{
								Selection::ClearSelection();
								Selection::AddToSelection(mi);
							}						
						}
					}
				}
			}
			else
			{
				if (!myRuntimeInitialized)
				{
					myGame->Init(myViewportRect.x, myViewportRect.y, myViewportRect.w, myViewportRect.h);
					myRuntimeInitialized = true;
				}

				{
					WasdState wasd{};
					auto& input = InputManager::Get();
					wasd.w = input.IsKeyDown('W');
					wasd.a = input.IsKeyDown('A');
					wasd.s = input.IsKeyDown('S');
					wasd.d = input.IsKeyDown('D');

					const float hudW = 106.0f;
					const float hudH = 124.0f;
					const ImVec2 hudAnchor = {
						imageMin.x + 12.0f,
						imageMax.y - hudH - 12.0f
					};
					DrawWASDOverlay(wasd, hudAnchor);
				}

				if (mySceneViewportHovered)
				{
					ImGui::GetForegroundDrawList()->AddRect(imageMin, imageMax, ImColor(1.0f, 0.5f, 0.5f), 0, 0, 4.0f);
					
					if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					{
						myGame->MouseClicked = true;
					}
				
					
					
				}
				static int clicked = 0;
				if (ImGui::Button("PAUSE"))
					clicked++;
				if (clicked & 1)
				{
			
				
				}
				else
				{
					myActiveScene->Update(myDeltaTime);
				}
			}
		}
		ImGui::End();
		ImGui::PopStyleVar();

		Profiler::Get().DrawOverlay();
		ImGui::Render();
		
		// Update and Render additional Platform Windows
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void EndFrame() {		
		ImGui::EndFrame();
		::Application::EndFrame();
		
	}

private:
	void ShowExampleAppDockSpace(bool* p_open);

private:
	std::shared_ptr<Scene> myActiveScene;

	std::vector<local<ToolsInterface>> myTools;
	local<Gizmos> myGizmos;
	local<Game> myGame;

	enum class GameState
	{
		EDITOR,
		RUNTIME
	};
	GameState myRunState = GameState::EDITOR;
	bool myRuntimeInitialized = false;
	std::filesystem::file_time_type mySceneLastWriteTime{};

	struct ViewportRect
	{
		float x, y, w, h;
	} myViewportRect{ 0.f, 0.f, 0.f, 0.f };

	bool mySceneViewportHovered = false;
};

Application* CreateApplication()
{
	return new ToolsLauncher();
}

void ToolsLauncher::ShowExampleAppDockSpace(bool* p_open)
{
	static bool opt_fullscreen = true;
	static bool opt_padding = false;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	else
	{
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
	}
	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", p_open, window_flags);
	if (!opt_padding)
		ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
		if (node == nullptr || node->IsLeafNode())
		{
			const ImGuiViewport* vp = ImGui::GetMainViewport();
			ImGui::DockBuilderRemoveNode(dockspace_id);
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

			ImGuiID remaining = dockspace_id;
			ImGuiID dockLeft, dockRight, dockBottom, dockTools, dockCenter;

			ImGui::DockBuilderSplitNode(remaining,  ImGuiDir_Left,  0.175f, &dockLeft,   &remaining);
			ImGui::DockBuilderSplitNode(remaining,  ImGuiDir_Right, 0.212f, &dockRight,  &remaining);
			ImGui::DockBuilderSplitNode(remaining,  ImGuiDir_Down,  0.31f,  &dockBottom, &remaining);
			ImGui::DockBuilderSplitNode(remaining,  ImGuiDir_Up,    0.07f,  &dockTools,  &dockCenter);

			ImGui::DockBuilderDockWindow("Scene List",      dockLeft);
			ImGui::DockBuilderDockWindow("Properties",      dockRight);
			ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
			ImGui::DockBuilderDockWindow("Tools",           dockTools);
			ImGui::DockBuilderDockWindow("Scene",           dockCenter);
			ImGui::DockBuilderFinish(dockspace_id);
		}
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				myActiveScene->Save("scene.json");
			}
			if (ImGui::MenuItem("Load Scene"))
			{
				Selection::ClearSelection();
				myActiveScene->Load("scene.json");
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
}

LRESULT CALLBACK WinProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	if (uMsg == WM_DESTROY || uMsg == WM_CLOSE)
	{
		PostQuitMessage(0);
	}
	if (uMsg == WM_SIZE)
	{
		if (g_graphicsEngine && wParam != SIZE_MINIMIZED)
		{
			uint32_t w = LOWORD(lParam);
			uint32_t h = HIWORD(lParam);
			g_graphicsEngine->Resize(w, h);
		}
	}
		
	else if (uMsg == WM_DROPFILES)
	{
		HDROP drop = (HDROP)wParam;
		UINT num_paths = DragQueryFileW(drop, 0xFFFFFFFF, 0, 512);

		wchar_t* filename = nullptr;
		UINT max_filename_len = 0;

		std::filesystem::path asset_dir = Settings::paths::game_assets;
		asset_dir += "/Textures/";

		for (UINT i = 0; i < num_paths; ++i)
		{
			UINT filename_len = DragQueryFileW(drop, i, nullptr, 512) + 1;
			if (filename_len > max_filename_len)
			{
				max_filename_len = filename_len;
				wchar_t *tmp = (wchar_t*)realloc(filename, max_filename_len * sizeof(*filename));
				if (tmp != nullptr)
				{
					filename = tmp;
				}
			}
			DragQueryFileW(drop, i, filename, filename_len);
			std::filesystem::path from_path = filename;
			std::filesystem::path to_path = asset_dir.concat(from_path.filename().string());

			if (std::filesystem::exists(to_path) == false && to_path.filename().extension() == ".dds")
			{
				std::filesystem::copy_file(from_path, to_path);
			}
		}
		free(filename);
		DragFinish(drop);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
