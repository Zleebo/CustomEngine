#pragma once
#include <Math/Matrix4x4.h>
#include <string>
#include <vector>

struct Bone
{
	std::string myName;
	int myParentIndex = -1;
	Matrix4x4f myBindPoseInverse;
	Matrix4x4f myLocalTransform;
};

struct Skeleton
{
	std::vector<Bone> myBones;

	int FindBone(const std::string& aName) const
	{
		for (int i = 0; i < (int)myBones.size(); i++)
		{
			if (myBones[i].myName == aName) return i;
		}
		return -1;
	}
};
