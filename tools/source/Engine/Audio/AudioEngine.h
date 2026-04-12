#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <string>
#include <unordered_map>
#include <vector>

class AudioEngine
{
public:
	static AudioEngine& Get();

	bool Init();
	void Shutdown();

	uint32_t LoadSound(const std::string& aPath);
	void Play(uint32_t aSoundId, bool aLooping = false);
	void Stop(uint32_t aSoundId);
	void SetMasterVolume(float aVolume);

private:
	AudioEngine() = default;

	struct SoundData
	{
		std::vector<BYTE> myPcmData;
		WAVEFORMATEX myFormat{};
	};

	bool LoadWav(const std::string& aPath, SoundData& outData);

	IXAudio2* myXAudio2 = nullptr;
	IXAudio2MasteringVoice* myMasterVoice = nullptr;

	std::unordered_map<uint32_t, SoundData> mySounds;
	std::unordered_map<uint32_t, IXAudio2SourceVoice*> myActiveVoices;
	uint32_t myNextId = 1;
};
