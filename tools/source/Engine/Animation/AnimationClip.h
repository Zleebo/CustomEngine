#pragma once
#include <Math/Vector.h>
#include <Math/Quaternion.h>
#include <string>
#include <vector>
#include <unordered_map>

struct BoneKeyframe
{
	float myTime = 0.0f;
	Vector3f myPosition{};
	Quaternionf myRotation{};
	Vector3f myScale{ 1, 1, 1 };
};

struct BoneTrack
{
	std::string myBoneName;
	std::vector<BoneKeyframe> myKeyframes;

	BoneKeyframe Sample(float aTime) const;
};

struct AnimationClip
{
	std::string myName;
	float myDuration = 0.0f;
	float myTicksPerSecond = 30.0f;
	bool myLooping = true;
	std::vector<BoneTrack> myTracks;

	const BoneTrack* GetTrack(const std::string& aBoneName) const;
};
