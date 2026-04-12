#include <stdafx.h>

#include "SceneSerializer.h"

#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>

#include <Scene.h>
#include <ModelInstance.h>
#include <Model.h>

#include <rapidjson/writer.h>
#include <rapidjson/document.h>

#include <UUIDManager.h>

using namespace rapidjson;

SceneSerializer::SceneSerializer(Scene& aScene) 
	: myScene(aScene)
{}

static void SerializeModelInstance(Writer<StringBuffer>& json, const ModelInstance* mi)
{
	
}

void SceneSerializer::Serialize(const char* filepath)
{
	
}

void SceneSerializer::SerializeBinary(const char* filepath)
{

}

bool SceneSerializer::Deserialize(const char* filepath)
{
	myScene.ClearScene();

	return true;
}

bool SceneSerializer::DeserializeBinary(const char* filepath)
{
	assert(false && "DeserializeBinary not implemented");
	return false;
}
