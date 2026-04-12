#include <stdafx.h>
#include "Scene.h"
#include "nohlman/json.hpp"
#include <Camera/Camera.h>

#include <ModelInstance.h>
#include <ModelFactory.h>
#include <Model.h>
#include <Components/ComponentFactory.h>
#include "Vector3.h"
#include <Physics/PhysicsWorld.h>
#include <fstream>
#include <algorithm>
#include <typeinfo>
#include <unordered_map>

Scene::Scene()
{
	myMainCamera = std::make_unique<Camera>();
	myMainCamera->set_position({ 0.0f, 2.0f, -10.0f });

	myName = "untitled";
	myModelFactory = std::unique_ptr<ModelFactory>(new ModelFactory());
}

Scene::~Scene()
{
	for (auto* mi : myModels) delete mi;
}


ModelInstance *Scene::CreateModelInstance(const char* path)
{
	myModels.push_back(new ModelInstance(std::move(myModelFactory->CreateInstance(path))));
	return myModels.back();
}

void Scene::RemoveModelInstance(ModelInstance* mi)
{
	if (!mi) return;

	mi->SetParent(nullptr);
	std::vector<ModelInstance*> children = mi->GetChildren();
	for (ModelInstance* child : children)
	{
		if (child)
		{
			child->SetParent(nullptr);
		}
	}

	auto it = std::find(myModels.begin(), myModels.end(), mi);
	if (it != myModels.end())
	{
		myModels.erase(it);
		delete mi;
	}
}

void Scene::AddModelInstance(ModelInstance* mi)
{
	myModels.push_back(mi);
}

void Scene::ClearScene()
{
	for (auto* mi : myModels) delete mi;
	myModels.clear();
}

void Scene::Save(const char* path)
{
	auto jsonObjects = nlohmann::json::array();
	std::unordered_map<ModelInstance*, int> modelIndex;
	modelIndex.reserve(myModels.size());
	for (int i = 0; i < static_cast<int>(myModels.size()); ++i)
	{
		modelIndex[myModels[i]] = i;
	}

	for (auto& object : myModels)
	{
		nlohmann::json j;
		j["Name"]    = object->GetName();
		j["Model"]   = object->GetModel() ? object->GetModel()->GetPath() : "Cube";
		j["Texture"] = object->GetTexture();
		j["Position"]["X"] = object->GetTransform().GetPosition().X;
		j["Position"]["Y"] = object->GetTransform().GetPosition().Y;
		j["Position"]["Z"] = object->GetTransform().GetPosition().Z;
		j["Rotation"]["X"] = object->GetTransform().GetRotation().X;
		j["Rotation"]["Y"] = object->GetTransform().GetRotation().Y;
		j["Rotation"]["Z"] = object->GetTransform().GetRotation().Z;
		j["Scale"]["X"] = object->GetTransform().GetScale().X;
		j["Scale"]["Y"] = object->GetTransform().GetScale().Y;
		j["Scale"]["Z"] = object->GetTransform().GetScale().Z;
		ModelInstance* parent = object->GetParent();
		if (parent && modelIndex.count(parent) > 0)
		{
			j["ParentIndex"] = modelIndex[parent];
		} else
		{
			j["ParentIndex"] = -1;
		}

		auto componentsJson = nlohmann::json::array();
		for (const auto& comp : object->GetAllComponents())
		{
			const char* raw = typeid(*comp.get()).name();
			const char* typeName = raw;
			if (strncmp(raw, "class ", 6) == 0)  typeName = raw + 6;
			else if (strncmp(raw, "struct ", 7) == 0) typeName = raw + 7;

			nlohmann::json cj;
			cj["type"] = typeName;
			for (const auto& prop : comp->myExposedProperties)
			{
				switch (prop.type)
				{
					case ExposedProperty::Type::Float:
						cj["props"][prop.label] = *(float*)prop.ptr;
						break;
					case ExposedProperty::Type::Int:
						cj["props"][prop.label] = *(int*)prop.ptr;
						break;
					case ExposedProperty::Type::Bool:
						cj["props"][prop.label] = *(bool*)prop.ptr;
						break;
					case ExposedProperty::Type::Vec3:
					{
						float* v = (float*)prop.ptr;
						cj["props"][prop.label] = { v[0], v[1], v[2] };
						break;
					}
				}
			}
			componentsJson.push_back(cj);
		}
		j["Components"] = componentsJson;

		jsonObjects.push_back(j);
	}
	std::ofstream out(path);
	out << jsonObjects;
}

void Scene::Load(const char* path)
{
	std::ifstream in(path);
	if (!in.is_open()) return;

	nlohmann::json j;
	try
	{
		j = nlohmann::json::parse(in);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::string msg = std::string("[Scene] JSON parse error in '") + path + "': " + e.what() + "\n";
		OutputDebugStringA(msg.c_str());
		return;
	}
	catch (const std::exception& e)
	{
		std::string msg = std::string("[Scene] JSON load error in '") + path + "': " + e.what() + "\n";
		OutputDebugStringA(msg.c_str());
		return;
	}

	if (!j.is_array())
	{
		std::string msg = std::string("[Scene] Expected array in '") + path + "'\n";
		OutputDebugStringA(msg.c_str());
		return;
	}

	auto readVec3 = [](const nlohmann::json& obj, const char* key, const Vector3f& fallback) -> Vector3f
	{
		if (!obj.contains(key) || !obj[key].is_object()) return fallback;
		const auto& v = obj[key];
		return Vector3f(
			v.value("X", fallback.X),
			v.value("Y", fallback.Y),
			v.value("Z", fallback.Z)
		);
	};

	if (onBeforeClear) onBeforeClear();
	ClearScene();

	int pos = 0;
	std::vector<int> parentIndices;
	parentIndices.reserve(j.size());
	for (auto& element : j)
	{
		if (!element.is_object()) continue;
		std::string modelPath = element.contains("Model") ? element["Model"].get<std::string>() : "Cube";
		myModels.push_back(new ModelInstance(std::move(myModelFactory->CreateInstance(modelPath.c_str()))));

		if (element.contains("Name"))
			myModels[pos]->SetName(element["Name"].get<std::string>().c_str());

		myModels[pos]->SetLocation(readVec3(element, "Position", Vector3f(0.0f, 0.0f, 0.0f)));
		if (element.contains("Rotation"))
		{
			Vector3f rot = readVec3(element, "Rotation", Vector3f(0.0f, 0.0f, 0.0f));
			myModels[pos]->SetRotation(Rotator(rot.X, rot.Y, rot.Z));
		}
		if (element.contains("Scale"))
			myModels[pos]->SetScale(readVec3(element, "Scale", Vector3f(1.0f, 1.0f, 1.0f)));
		if (element.contains("Texture") && element["Texture"].is_string())
			myModels[pos]->SetTexture(element["Texture"].get_ref<const std::string&>().c_str());
		parentIndices.push_back(element.value("ParentIndex", -1));

		if (element.contains("Components"))
		{
			for (const auto& cj : element["Components"])
			{
				if (!cj.is_object() || !cj.contains("type") || !cj["type"].is_string()) continue;
				std::string typeName = cj["type"].get<std::string>();
				auto comp = ComponentFactory::Get().Create(typeName);
				if (!comp) continue;

				if (cj.contains("props"))
				{
					const auto& props = cj["props"];
					for (auto& exposed : comp->myExposedProperties)
					{
						if (!props.contains(exposed.label)) continue;
						switch (exposed.type)
						{
							case ExposedProperty::Type::Float:
								*(float*)exposed.ptr = props[exposed.label].get<float>();
								break;
							case ExposedProperty::Type::Int:
								*(int*)exposed.ptr = props[exposed.label].get<int>();
								break;
							case ExposedProperty::Type::Bool:
								*(bool*)exposed.ptr = props[exposed.label].get<bool>();
								break;
							case ExposedProperty::Type::Vec3:
							{
								float* v = (float*)exposed.ptr;
								auto arr = props[exposed.label];
								if (arr.is_array() && arr.size() >= 3)
								{
									v[0] = arr[0].get<float>();
									v[1] = arr[1].get<float>();
									v[2] = arr[2].get<float>();
								}
								break;
							}
						}
					}
				}
				myModels[pos]->AddComponent(std::move(comp));
			}
		}

		pos++;
	}

	for (int i = 0; i < static_cast<int>(myModels.size()) && i < static_cast<int>(parentIndices.size()); ++i)
	{
		const int p = parentIndices[i];
		if (p >= 0 && p < static_cast<int>(myModels.size()) && p != i)
		{
			myModels[i]->SetParent(myModels[p]);
		}
	}
}

void Scene::Update(float aTimeDelta)
{
	for (auto& gameObject : myModels)
	{
		gameObject->UpdateComponents(aTimeDelta);
	}

	PhysicsWorld::Get().Update(aTimeDelta);

	for (auto& gameObject : myModels)
	{
		gameObject->LateUpdateComponents(aTimeDelta);
	}
}

std::vector<ModelInstance*> Scene::CullModels() const
{
	return myModels;
}

ModelInstance* Scene::GetModelInstanceByID(uint32_t anID) const
{
	for (ModelInstance* mi : myModels)
	{
		if (mi->GetID() == anID)
		{
			return mi;
		}
	}
	return nullptr;
}
