#pragma once
#include <Math/Vector.h>
#include <string>

struct Material
{
	std::string myName;
	std::string myAlbedoPath;
	std::string myNormalPath;
	std::string myRoughnessPath;
	std::string myMetalnessPath;
	Vector4f    myTint{ 1.0f, 1.0f, 1.0f, 1.0f };
	float       myRoughnessScale = 1.0f;
	float       myMetalnessScale = 0.0f;
};
