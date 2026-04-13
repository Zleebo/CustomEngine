#pragma once
#include <unordered_map>
#include <ModelInstance.h>
#include <memory>
#include <string>


class Model;

class ModelFactory
{
public:
	ModelFactory();
	~ModelFactory() = default;

	ModelInstance CreateInstance(const char* aModelPath);

private:
	Model* GetModel(const char* aModelPath);
	std::unordered_map<std::string, std::unique_ptr<Model>> myModelCache;
};
