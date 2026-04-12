#pragma once
#include <vector>
#include <Math/Vector3.h>

class Scene;
class ModelInstance;

// Loads a prefab JSON file and spawns its objects into a scene.
// Same JSON format as scenes. Positions are local offsets from spawnPos.
class Prefab
{
public:
	std::vector<ModelInstance*> Spawn(Scene* scene, const char* jsonPath, Vector3f spawnPos = {});
};
