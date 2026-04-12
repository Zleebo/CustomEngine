#include <stdafx.h>
#include "LightManager.h"

LightManager& LightManager::Get()
{
	static LightManager instance;
	return instance;
}

void LightManager::SetDirectionalLight(const DirectionalLight& aLight)
{
	myDirectionalLight = aLight;
}

void LightManager::AddPointLight(const PointLight& aLight)
{
	myPointLights.push_back(aLight);
}
