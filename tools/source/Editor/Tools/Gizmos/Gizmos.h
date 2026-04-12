#pragma once

#include <memory>
#include <ToolsInterface/ToolsInterface.h>
#include <Math/Matrix4x4.h>
#include <vector>

struct Transform;
class Scene;
class ModelInstance;

class Gizmos : public ToolsInterface
{
public:
	Gizmos(std::shared_ptr<Scene> aScene);

	virtual void Draw() override;

	void DrawGizmos();

private:
	const std::shared_ptr<Scene> myScene;
	
	std::vector<Matrix4x4f> myStartTransforms;
	std::vector<Matrix4x4f> myEndTransforms;

	std::vector<ModelInstance*> mySceneModels;
	
	Matrix4x4f myStartTransform;
	Matrix4x4f myTransform;
	Matrix4x4f myStartTransformInverse;

	int myGizmoType = 7;
	float mySnap[3] = { 0.0f, 0.0f, 0.0f };
};
