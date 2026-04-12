#include "TerrainTool.h"

#include <Engine.h>
#include <GraphicsEngine/GraphicsEngine.h>
#include <GraphicsEngine/Terrain/TerrainRenderer.h>

#include <imgui.h>

TerrainTool::TerrainTool(GraphicsEngine* aEngine)
	: myEngine(aEngine)
	{
}

void TerrainTool::Draw()
{
	ImGui::Begin("Terrain");

	TerrainRenderer* renderer = myEngine->GetTerrainRenderer();
	if (!renderer) { ImGui::End(); return; }

	TerrainParams& p = renderer->GetParams();

	bool dirty = false;

	// Seed
	int seed = (int)(p.seed & 0x7FFFFFFF);
	if (ImGui::DragInt("Seed", &seed, 1.0f, 0, INT_MAX))
	{
		p.seed = (uint64_t)(unsigned int)seed;
		dirty  = true;
	}

	// Height scale
	if (ImGui::DragFloat("Height Scale", &p.heightScale, 0.1f, 0.1f, 50.0f))
	{
		dirty = true;
	}

	// Y Offset
	if (ImGui::DragFloat("Y Offset", &p.yOffset, 0.5f, -500.0f, 500.0f))
	{
		dirty = true;
	}

	ImGui::Spacing();
	ImGui::Checkbox("Water", &renderer->GetDrawWater());

	ImGui::Spacing();

	// Apply changes either on demand or automatically
	static bool livePreview = false;
	ImGui::Checkbox("Live Preview", &livePreview);

	if ((livePreview && dirty) || ImGui::Button("Regenerate  (R)"))
	{
		renderer->RegenerateTerrain();
	}

	// Shortcut: R key when window focused
	if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed('R') && !livePreview)
	{
		renderer->RegenerateTerrain();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Seed controls the random shape.");
	ImGui::TextDisabled("Height Scale controls peak amplitude.");
	ImGui::TextDisabled("Y Offset shifts the whole terrain vertically.");

	ImGui::End();
}
