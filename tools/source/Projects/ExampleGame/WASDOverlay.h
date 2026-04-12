#pragma once

#include <imgui.h>

struct WasdState
{
	bool w = false;
	bool a = false;
	bool s = false;
	bool d = false;
};

inline void DrawWASDOverlay(const WasdState& state, const ImVec2& anchor)
{
	const float panelW = 106.0f;
	const float panelH = 124.0f;
	const float keyW = 26.0f;
	const float keyH = 30.0f;
	const float gap = 6.0f;
	const float padX = 10.0f;
	const float topLabelY = 9.0f;
	const float keysTop = 31.0f;

	ImDrawList* draw = ImGui::GetForegroundDrawList();
	if (!draw)
	{
		return;
	}

	const ImVec2 panelMin = anchor;
	const ImVec2 panelMax = ImVec2(anchor.x + panelW, anchor.y + panelH);

	const ImU32 panelFill = IM_COL32(26, 31, 40, 210);
	const ImU32 panelBorder = IM_COL32(72, 225, 255, 220);
	const ImU32 labelColor = IM_COL32(220, 235, 255, 230);

	draw->AddRectFilled(panelMin, panelMax, panelFill, 8.0f);
	draw->AddRect(panelMin, panelMax, panelBorder, 8.0f, 0, 1.2f);
	draw->AddText(ImVec2(anchor.x + 8.0f, anchor.y + topLabelY), labelColor, "WASD");

	const auto drawKey = [&](float x, float y, bool pressed, const char* label)
	{
		const ImVec2 minP = ImVec2(anchor.x + x, anchor.y + y);
		const ImVec2 maxP = ImVec2(minP.x + keyW, minP.y + keyH);
		const ImU32 fill = pressed ? IM_COL32(116, 186, 255, 240) : IM_COL32(33, 41, 55, 220);
		const ImU32 border = pressed ? IM_COL32(186, 227, 255, 255) : IM_COL32(214, 223, 234, 220);
		const ImU32 text = pressed ? IM_COL32(10, 25, 42, 255) : IM_COL32(245, 247, 250, 240);

		draw->AddRectFilled(minP, maxP, fill, 5.0f);
		draw->AddRect(minP, maxP, border, 5.0f, 0, 1.0f);

		ImVec2 textSize = ImGui::CalcTextSize(label);
		ImVec2 textPos = ImVec2(
			minP.x + (keyW - textSize.x) * 0.5f,
			minP.y + (keyH - textSize.y) * 0.5f - 1.0f
		);
		draw->AddText(textPos, text, label);
	};

	const float centerX = padX + keyW + gap;
	drawKey(centerX, keysTop, state.w, "W");
	drawKey(padX, keysTop + keyH + gap, state.a, "A");
	drawKey(centerX, keysTop + keyH + gap, state.s, "S");
	drawKey(centerX + keyW + gap, keysTop + keyH + gap, state.d, "D");
}
