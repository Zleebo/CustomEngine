#include <stdafx.h>

#include "ModelFactory.h"
#include "ModelInstance.h"
#include "Model.h"

#include <fstream>

#include <DX11Framework.h>

ModelFactory::ModelFactory()
{
	myModelCache = std::unordered_map<std::string, std::unique_ptr<Model>>();
}

ModelInstance ModelFactory::CreateInstance(const char* model_path)
{
	return ModelInstance(GetModel(model_path));
}

Model* ModelFactory::GetModel(const char* someFilePath)
{
	const std::string key = someFilePath ? someFilePath : "";
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
