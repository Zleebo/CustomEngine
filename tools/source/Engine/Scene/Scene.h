#pragma once
#include <vector>
#include <memory>
#include <string>
#include <functional>

class ModelInstance;
class ModelFactory;
class Camera;

class Scene
{
public:
	Scene();
	~Scene();

	ModelInstance* CreateModelInstance(const char* path);
	void           RemoveModelInstance(ModelInstance* mi);  // removes from scene, does NOT delete
	void           AddModelInstance(ModelInstance* mi);     // re-adds (for undo)

	const std::vector<ModelInstance*> SceneModels() const { return myModels; }
	void Save(const char* path);
	void Load(const char* path);
	void Update(float aTimeDelta);
	std::vector<ModelInstance*> CullModels() const;
	
	const Camera *GetActiveCamera() const { return myMainCamera.get(); }
	Camera *GetActiveCameraMutable()      { return myMainCamera.get(); }
	std::vector<ModelInstance*> GetModelList() { return myModels; }

	ModelInstance* GetModelInstanceByID(uint32_t anID) const;

	void ClearScene();

	// Called just before ClearScene() inside Load()
	std::function<void()> onBeforeClear;

public:
	void SetName(const char* name) { myName = name; }

private:
	friend class SceneSerializer;

	std::string myName;
	std::unique_ptr<ModelFactory> myModelFactory;

	std::vector<ModelInstance*> myModels;
	std::unique_ptr<Camera> myMainCamera;
};