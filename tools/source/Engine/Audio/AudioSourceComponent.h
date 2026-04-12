#pragma once
#include <Components/Component.h>
#include <string>

class AudioSourceComponent : public Component
{
public:
	AudioSourceComponent()
	{
		EXPOSE_BOOL("Play On Start", myPlayOnStart);
		EXPOSE_BOOL("Loop",          myLoop);
	}

	void OnStart() override;

	void SetSound(const std::string& aPath);
	void Play(bool aLooping = false);
	void Stop();

	bool myPlayOnStart = false;
	bool myLoop = false;

private:
	uint32_t mySoundId = 0;
	std::string myPath;
};
