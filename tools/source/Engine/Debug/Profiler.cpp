#include <stdafx.h>
#include "Profiler.h"

Profiler& Profiler::Get()
{
	static Profiler instance;
	return instance;
}

void Profiler::BeginSample(const std::string& aName)
{
	mySamples[aName].myStart = Clock::now();
}

void Profiler::EndSample(const std::string& aName)
{
	auto it = mySamples.find(aName);
	if (it == mySamples.end()) return;

	auto& sample = it->second;
	float elapsed = std::chrono::duration<float, std::milli>(Clock::now() - sample.myStart).count();
	sample.myLastTime = elapsed;
	sample.myAccumulated += elapsed;
	sample.myCount++;
}

float Profiler::GetLastTime(const std::string& aName) const
{
	auto it = mySamples.find(aName);
	return it != mySamples.end() ? it->second.myLastTime : 0.0f;
}

float Profiler::GetAverageTime(const std::string& aName) const
{
	auto it = mySamples.find(aName);
	if (it == mySamples.end() || it->second.myCount == 0) return 0.0f;
	return it->second.myAccumulated / it->second.myCount;
}

void Profiler::DrawOverlay()
{
#ifdef IMGUI_VERSION
	ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
	for (const auto& [name, sample] : mySamples)
	{
		ImGui::Text("%-30s  %.3f ms", name.c_str(), sample.myLastTime);
	}
	ImGui::End();
#endif
}
