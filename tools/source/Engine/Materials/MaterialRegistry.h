#pragma once
#include "Material.h"
#include <unordered_map>

class MaterialRegistry
{
public:
	static MaterialRegistry& Get();

	Material* Create(const std::string& aName);
	Material* GetOrCreate(const std::string& aName);
	Material* Find(const std::string& aName);

private:
	MaterialRegistry() = default;
	std::unordered_map<std::string, Material> myMaterials;
};
