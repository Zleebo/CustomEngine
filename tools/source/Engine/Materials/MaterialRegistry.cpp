#include <stdafx.h>
#include "MaterialRegistry.h"

MaterialRegistry& MaterialRegistry::Get()
{
	static MaterialRegistry instance;
	return instance;
}

Material* MaterialRegistry::Create(const std::string& aName)
{
	auto& mat = myMaterials[aName];
	mat.myName = aName;
	return &mat;
}

Material* MaterialRegistry::GetOrCreate(const std::string& aName)
{
	auto it = myMaterials.find(aName);
	if (it != myMaterials.end()) return &it->second;
	return Create(aName);
}

Material* MaterialRegistry::Find(const std::string& aName)
{
	auto it = myMaterials.find(aName);
	return it != myMaterials.end() ? &it->second : nullptr;
}
