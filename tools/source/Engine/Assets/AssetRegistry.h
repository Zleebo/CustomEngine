#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

class AssetRegistry
{
public:
	static AssetRegistry& Get();

	template<typename T>
	std::shared_ptr<T> Load(const std::string& aPath, std::function<std::shared_ptr<T>()> aLoader)
	{
		auto it = myAssets.find(aPath);
		if (it != myAssets.end())
		{
			return std::static_pointer_cast<T>(it->second);
		}
		auto asset = aLoader();
		if (asset)
		{
			myAssets[aPath] = asset;
		}
		return asset;
	}

	bool IsLoaded(const std::string& aPath) const;
	void Unload(const std::string& aPath);
	void UnloadAll();

private:
	AssetRegistry() = default;
	std::unordered_map<std::string, std::shared_ptr<void>> myAssets;
};
