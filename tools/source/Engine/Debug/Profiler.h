#pragma once
#include <string>
#include <unordered_map>
#include <chrono>

class Profiler
{
public:
	static Profiler& Get();

	void BeginSample(const std::string& aName);
	void EndSample(const std::string& aName);

	float GetLastTime(const std::string& aName) const;
	float GetAverageTime(const std::string& aName) const;

	void DrawOverlay();

private:
	Profiler() = default;

	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::time_point<Clock>;

	struct Sample
	{
		TimePoint myStart;
		float myLastTime = 0.0f;
		float myAccumulated = 0.0f;
		int myCount = 0;
	};

	std::unordered_map<std::string, Sample> mySamples;
};

struct ProfileScope
{
	explicit ProfileScope(const std::string& aName) : myName(aName)
	{
		Profiler::Get().BeginSample(myName);
	}
	~ProfileScope()
	{
		Profiler::Get().EndSample(myName);
	}
	std::string myName;
};

#define PROFILE_SCOPE(name) ProfileScope _profScope_##__LINE__(name)
