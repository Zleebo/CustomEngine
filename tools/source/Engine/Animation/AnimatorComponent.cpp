#include <stdafx.h>
#include "AnimatorComponent.h"

void AnimatorComponent::OnStart()
{
	if (mySkeleton)
	{
		myBoneMatrices.resize(mySkeleton->myBones.size(), Matrix4x4f());
		myGlobalTransforms.resize(mySkeleton->myBones.size(), Matrix4x4f());
	}
}

void AnimatorComponent::SetSkeleton(std::shared_ptr<Skeleton> aSkeleton)
{
	mySkeleton = std::move(aSkeleton);
	myBoneMatrices.resize(mySkeleton->myBones.size(), Matrix4x4f());
	myGlobalTransforms.resize(mySkeleton->myBones.size(), Matrix4x4f());
}

void AnimatorComponent::AddClip(std::shared_ptr<AnimationClip> aClip)
{
	myClips.push_back(std::move(aClip));
}

void AnimatorComponent::Play(const std::string& aClipName)
{
	for (auto& clip : myClips)
	{
		if (clip->myName == aClipName)
		{
			myCurrentClip = clip.get();
			myCurrentTime = 0.0f;
			return;
		}
	}
}

void AnimatorComponent::Update(float aDeltaTime)
{
	if (!myCurrentClip || !mySkeleton) return;

	myCurrentTime += aDeltaTime;
	if (myCurrentClip->myLooping && myCurrentTime > myCurrentClip->myDuration)
	{
		myCurrentTime = fmodf(myCurrentTime, myCurrentClip->myDuration);
	}

	EvaluatePose();
}

void AnimatorComponent::EvaluatePose()
{
	const auto& bones = mySkeleton->myBones;

	for (int i = 0; i < (int)bones.size(); i++)
	{
		const auto& bone = bones[i];

		Matrix4x4f localTransform = bone.myLocalTransform;

		const BoneTrack* track = myCurrentClip->GetTrack(bone.myName);
		if (track)
		{
			BoneKeyframe kf = track->Sample(myCurrentTime);

			// Build local TRS matrix from keyframe
			Matrix4x4f t, r, s;
			t(4, 1) = kf.myPosition.X;
			t(4, 2) = kf.myPosition.Y;
			t(4, 3) = kf.myPosition.Z;

			r = kf.myRotation.GetRotationMatrix44();

			s(1, 1) = kf.myScale.X;
			s(2, 2) = kf.myScale.Y;
			s(3, 3) = kf.myScale.Z;

			localTransform = s * r * t;
		}

		if (bone.myParentIndex < 0)
		{
			myGlobalTransforms[i] = localTransform;
		}
		else
		{
			myGlobalTransforms[i] = localTransform * myGlobalTransforms[bone.myParentIndex];
		}

		myBoneMatrices[i] = bone.myBindPoseInverse * myGlobalTransforms[i];
	}
}
