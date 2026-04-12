#pragma once

#include <ToolsInterface/ToolsInterface.h>

#include <memory>
#include <string>
#include <vector>

class ModelInstance;
struct Transform;

class ModelProperties : public ToolsInterface
{
public:
	ModelProperties(std::vector<ModelInstance*> &aModelList);

	virtual void Draw() override;

private:
	std::vector<ModelInstance*> myModelList;
	std::unique_ptr<Transform> myStartTransform;
	std::string myStartName;
};