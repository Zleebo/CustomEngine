#include <stdafx.h>
#include "AnimationClip.h"
#include <algorithm>

BoneKeyframe BoneTrack::Sample(float aTime) const
{
	if (myKeyframes.empty()) return {};
	if (myKeyframes.size() == 1) return myKeyframes[0];

	int next = 0;
	for (int i = 0; i < (int)myKeyframes.size(); i++)
	{
		if (myKeyframes[i].myTime >= aTime) { next = i; break; }
		next = (int)myKeyframes.size() - 1;
	}

	int prev = max(0, next - 1);
	if (prev == next) return myKeyframes[prev];

	const auto& a = myKeyframes[prev];
	const auto& b = myKeyframes[next];

	float t = (aTime - a.myTime) / (b.myTime - a.myTime);
	t = max(0.0f, min(1.0f, t));

	BoneKeyframe result;
	result.myTime     = aTime;
	result.myPosition = a.myPosition + (b.myPosition - a.myPosition) * t;
	result.myScale    = a.myScale    + (b.myScale    - a.myScale)    * t;
	// nlerp, good enough for keyframe blending
	Quaternionf qa = a.myRotation;
	Quaternionf qb = b.myRotation;
	result.myRotation = (qa * (1.0f - t) + qb * t).GetNormalized();
	return result;
}

const BoneTrack* AnimationClip::GetTrack(const std::string& aBoneName) const
{
	for (const auto& track : myTracks)
	{
		if (track.myBoneName == aBoneName) return &track;
	}
	return nullptr;
}
