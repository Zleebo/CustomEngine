#include <stdafx.h>
#include "AssetRegistry.h"

AssetRegistry& AssetRegistry::Get()
{
	static AssetRegistry instance;
	return instance;
}

bool AssetRegistry::IsLoaded(const std::string& aPath) const
{
	return myAssets.count(aPath) > 0;
}

void AssetRegistry::Unload(const std::string& aPath)
{
	myAssets.erase(aPath);
}

void AssetRegistry::UnloadAll()
{
	myAssets.clear();
}
