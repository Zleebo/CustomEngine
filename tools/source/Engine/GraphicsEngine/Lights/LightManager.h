#pragma once
#include <Math/Vector.h>
#include <vector>

struct DirectionalLight
{
	Vector4f myDirection{ 0.5f, -0.8f, 0.3f, 0.0f };
	Vector4f myColour{ 1.0f, 0.9f, 0.8f, 1.0f };
};

struct PointLight
{
	Vector4f myPosition{ 0, 0, 0, 1 };
	Vector4f myColour{ 1, 1, 1, 1 };
	float myRange = 10.0f;
	float myIntensity = 1.0f;
	float myPadding[2]{};
};

class LightManager
{
public:
	static LightManager& Get();

	void SetDirectionalLight(const DirectionalLight& aLight);
	void AddPointLight(const PointLight& aLight);

	const DirectionalLight& GetDirectionalLight() const { return myDirectionalLight; }
	const std::vector<PointLight>& GetPointLights() const { return myPointLights; }

	void ClearPointLights() { myPointLights.clear(); }

private:
	LightManager() = default;

	DirectionalLight myDirectionalLight;
	std::vector<PointLight> myPointLights;
};
