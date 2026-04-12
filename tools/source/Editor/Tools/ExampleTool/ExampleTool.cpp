#include "ExampleTool.h"
#include <GraphicsEngine/GraphicsEngine.h>
#include <Core/EngineSettings.h>

#include <array>

#include <imgui.h>

ExampleTool::ExampleTool(std::shared_ptr<GraphicsEngine> aGraphicsEngine) : myGraphicsEngine(aGraphicsEngine)
{

}

void ExampleTool::Draw()
{
	std::array<float, 4> color = Settings::window_data.clear_color;

	if (ImGui::ColorEdit4("Clear Color", color.data()))
	{
		Settings::window_data.clear_color = color;
	}
}

