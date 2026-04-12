#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <ToolsInterface/ToolsInterface.h>

class Scene;
class ModelInstance;
struct Transform;

class SceneHierarchy : public ToolsInterface
{
public:
	SceneHierarchy(std::shared_ptr<Scene> aScene);

	virtual void Draw() override;

private:
	void DrawBranch(const std::vector < ModelInstance*>& aBranch, ModelInstance * const theParent = nullptr);

private:
	std::shared_ptr<Scene> myScene;
};
