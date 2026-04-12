#pragma once
#include <Components/Component.h>
#include <Math/Matrix4x4.h>
#include "Skeleton.h"
#include "AnimationClip.h"
#include <memory>
#include <vector>
#include <string>

class AnimatorComponent : public Component
{
public:
	static constexpr int MaxBones = 128;

	void OnStart() override;
	void Update(float aDeltaTime) override;

	void SetSkeleton(std::shared_ptr<Skeleton> aSkeleton);
	void Play(const std::string& aClipName);
	void AddClip(std::shared_ptr<AnimationClip> aClip);

	const std::vector<Matrix4x4f>& GetBoneMatrices() const { return myBoneMatrices; }

private:
	void EvaluatePose();

	std::shared_ptr<Skeleton> mySkeleton;
	std::vector<std::shared_ptr<AnimationClip>> myClips;
	AnimationClip* myCurrentClip = nullptr;

	std::vector<Matrix4x4f> myBoneMatrices;
	std::vector<Matrix4x4f> myGlobalTransforms;

	float myCurrentTime = 0.0f;
};
