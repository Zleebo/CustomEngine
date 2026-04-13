#include <stdafx.h>

#include "ModelFactory.h"
#include "ModelInstance.h"
#include "Model.h"

ModelFactory::ModelFactory()
{
	myModelCache = std::unordered_map<std::string, std::unique_ptr<Model>>();
}

ModelInstance ModelFactory::CreateInstance(const char* aModelPath)
{
	return ModelInstance(GetModel(aModelPath));
}

Model* ModelFactory::GetModel(const char* aModelPath)
{
	const std::string key = aModelPath ? aModelPath : "";
	auto it = myModelCache.find(key);
	if (it != myModelCache.end())
	{
		return it->second.get();
	}

	auto model = std::make_unique<Model>(key.c_str());
	Model* out = model.get();
	myModelCache.emplace(key, std::move(model));
	return out;
}
