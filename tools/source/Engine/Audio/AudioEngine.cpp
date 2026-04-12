#include <stdafx.h>
#include "AudioEngine.h"
#include <fstream>
#include <cstring>

#pragma comment(lib, "xaudio2.lib")

AudioEngine& AudioEngine::Get()
{
	static AudioEngine instance;
	return instance;
}

bool AudioEngine::Init()
{
	HRESULT hr = XAudio2Create(&myXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) return false;

	hr = myXAudio2->CreateMasteringVoice(&myMasterVoice);
	if (FAILED(hr))
	{
		myXAudio2->Release();
		myXAudio2 = nullptr;
		return false;
	}

	return true;
}

void AudioEngine::Shutdown()
{
	for (auto& [id, voice] : myActiveVoices)
	{
		if (voice) { voice->Stop(); voice->DestroyVoice(); }
	}
	myActiveVoices.clear();

	if (myMasterVoice) { myMasterVoice->DestroyVoice(); myMasterVoice = nullptr; }
	if (myXAudio2)     { myXAudio2->Release(); myXAudio2 = nullptr; }
}

bool AudioEngine::LoadWav(const std::string& aPath, SoundData& outData)
{
	std::ifstream file(aPath, std::ios::binary);
	if (!file.is_open()) return false;

	char riff[4]; file.read(riff, 4);
	if (strncmp(riff, "RIFF", 4) != 0) return false;

	uint32_t chunkSize; file.read(reinterpret_cast<char*>(&chunkSize), 4);
	char wave[4]; file.read(wave, 4);
	if (strncmp(wave, "WAVE", 4) != 0) return false;

	while (!file.eof())
	{
		char id[4]; file.read(id, 4);
		uint32_t size; file.read(reinterpret_cast<char*>(&size), 4);

		if (strncmp(id, "fmt ", 4) == 0)
		{
			file.read(reinterpret_cast<char*>(&outData.myFormat), sizeof(WAVEFORMATEX) - 2);
			if (size > 16) file.seekg(size - 16, std::ios::cur);
		}
		else if (strncmp(id, "data", 4) == 0)
		{
			outData.myPcmData.resize(size);
			file.read(reinterpret_cast<char*>(outData.myPcmData.data()), size);
			break;
		}
		else
		{
			file.seekg(size, std::ios::cur);
		}
	}

	return !outData.myPcmData.empty();
}

uint32_t AudioEngine::LoadSound(const std::string& aPath)
{
	if (!myXAudio2) return 0;

	SoundData data;
	if (!LoadWav(aPath, data)) return 0;

	uint32_t id = myNextId++;
	mySounds[id] = std::move(data);
	return id;
}

void AudioEngine::Play(uint32_t aSoundId, bool aLooping)
{
	if (!myXAudio2) return;

	auto it = mySounds.find(aSoundId);
	if (it == mySounds.end()) return;

	const auto& sound = it->second;

	IXAudio2SourceVoice* voice = nullptr;
	HRESULT hr = myXAudio2->CreateSourceVoice(&voice, &sound.myFormat);
	if (FAILED(hr)) return;

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = static_cast<UINT32>(sound.myPcmData.size());
	buffer.pAudioData = sound.myPcmData.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = aLooping ? XAUDIO2_LOOP_INFINITE : 0;

	voice->SubmitSourceBuffer(&buffer);
	voice->Start(0);

	myActiveVoices[aSoundId] = voice;
}

void AudioEngine::Stop(uint32_t aSoundId)
{
	auto it = myActiveVoices.find(aSoundId);
	if (it == myActiveVoices.end()) return;

	it->second->Stop();
	it->second->DestroyVoice();
	myActiveVoices.erase(it);
}

void AudioEngine::SetMasterVolume(float aVolume)
{
	if (myMasterVoice)
	{
		myMasterVoice->SetVolume(aVolume);
	}
}
