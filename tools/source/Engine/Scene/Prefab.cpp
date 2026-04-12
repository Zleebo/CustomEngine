#include <stdafx.h>
#include "Prefab.h"

#include "Scene.h"
#include <ModelInstance.h>
#include <Components/ComponentFactory.h>

#include "nohlman/json.hpp"
#include <fstream>

std::vector<ModelInstance*> Prefab::Spawn(Scene* scene, const char* jsonPath, Vector3f spawnPos)
{
	std::vector<ModelInstance*> spawned;

	std::ifstream in(jsonPath);
	if (!in.is_open()) return spawned;

	nlohmann::json j;
	try
	{
		j = nlohmann::json::parse(in);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::string msg = std::string("[Prefab] JSON parse error in '") + jsonPath + "': " + e.what() + "\n";
		OutputDebugStringA(msg.c_str());
		return spawned;
	}
	catch (const std::exception& e)
	{
		std::string msg = std::string("[Prefab] JSON load error in '") + jsonPath + "': " + e.what() + "\n";
		OutputDebugStringA(msg.c_str());
		return spawned;
	}

	if (!j.is_array())
	{
		std::string msg = std::string("[Prefab] Expected array in '") + jsonPath + "'\n";
		OutputDebugStringA(msg.c_str());
		return spawned;
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

	for (auto& element : j)
	{
		if (!element.is_object()) continue;

		std::string modelPath = element.contains("Model") ? element["Model"].get<std::string>() : "Cube";
		ModelInstance* mi = scene->CreateModelInstance(modelPath.c_str());

		if (element.contains("Name"))
			mi->SetName(element["Name"].get<std::string>().c_str());

		Vector3f localPos = readVec3(element, "Position", Vector3f(0.0f, 0.0f, 0.0f));
		mi->SetLocation(spawnPos + localPos);

		if (element.contains("Rotation"))
		{
			Vector3f rot = readVec3(element, "Rotation", Vector3f(0.0f, 0.0f, 0.0f));
			mi->SetRotation(Rotator(rot.X, rot.Y, rot.Z));
		}

		if (element.contains("Scale"))
			mi->SetScale(readVec3(element, "Scale", Vector3f(1.0f, 1.0f, 1.0f)));

		if (element.contains("Texture") && element["Texture"].is_string())
			mi->SetTexture(element["Texture"].get_ref<const std::string&>().c_str());

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
				mi->AddComponent(std::move(comp));
			}
		}

		spawned.push_back(mi);
	}

	// Optional explicit ParentIndex per element; defaults to root-parenting for entries [1..N-1]
	// to support common prefab layouts (e.g. Car body + child wheels).
	for (int i = 0; i < static_cast<int>(spawned.size()); ++i)
	{
		if (i >= static_cast<int>(j.size()) || !j[i].is_object()) continue;
		const int parentIndex = j[i].value("ParentIndex", (i == 0 ? -1 : 0));
		if (parentIndex >= 0 && parentIndex < static_cast<int>(spawned.size()) && parentIndex != i)
		{
			spawned[i]->SetParent(spawned[parentIndex]);
		}
	}

	return spawned;
}
