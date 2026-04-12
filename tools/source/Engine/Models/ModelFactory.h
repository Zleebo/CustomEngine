#pragma once
#include <unordered_map>
#include <ModelInstance.h>
#include <memory>
#include <string>


class Model;
//struct ID3D11Device;

class ModelFactory
{
public:
	ModelFactory();
	~ModelFactory() = default;

	ModelInstance CreateInstance(const char *model_path);

private:
	Model* GetModel(const char*);

private:
	std::unordered_map<std::string, std::unique_ptr<Model>> myModelCache;
};
