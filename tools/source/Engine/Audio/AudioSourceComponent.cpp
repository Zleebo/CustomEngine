#include <stdafx.h>
#include "AudioSourceComponent.h"
#include "AudioEngine.h"

void AudioSourceComponent::OnStart()
{
	if (!myPath.empty())
	{
		mySoundId = AudioEngine::Get().LoadSound(myPath);
	}
	if (myPlayOnStart && mySoundId != 0)
	{
		Play(myLoop);
	}
}

void AudioSourceComponent::SetSound(const std::string& aPath)
{
	myPath = aPath;
	mySoundId = 0;
}

void AudioSourceComponent::Play(bool aLooping)
{
	if (mySoundId == 0 && !myPath.empty())
	{
		mySoundId = AudioEngine::Get().LoadSound(myPath);
	}
	if (mySoundId != 0)
	{
		AudioEngine::Get().Play(mySoundId, aLooping);
	}
}

void AudioSourceComponent::Stop()
{
	if (mySoundId != 0)
	{
		AudioEngine::Get().Stop(mySoundId);
	}
}
