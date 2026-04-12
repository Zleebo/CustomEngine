#pragma once

#include <memory>

class Scene;

class SceneSerializer
{
public:
	SceneSerializer(Scene& aScene);

	void Serialize(const char* filepath);
	void SerializeBinary(const char* filepath);

	bool Deserialize(const char* filepath);
	bool DeserializeBinary(const char* filepath);

private:
	Scene &myScene;
};