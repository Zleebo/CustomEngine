#include "Game.h"
#include "CarController.h"
#include "RaycastVehicleController.h"
#include "FollowCameraComponent.h"

#include <Engine.h>
#include <GraphicsEngine/GraphicsEngine.h>
#include <Scene/Prefab.h>
#include <Physics/PhysicsWorld.h>

namespace
{
	constexpr const char* kCarModelPath = "kenney_race_body.obj";
	constexpr const char* kWheelModelPath = "kenney_wheel_default.obj";
	constexpr const char* kCarTexturePath = "kenney_car_colormap.png";

	bool IsExpectedModel(ModelInstance* aModelInstance, const char* aExpectedPath)
	{
		if (!aModelInstance || !aModelInstance->GetModel() || !aExpectedPath)
		{
			return false;
		}

		const char* currentPath = aModelInstance->GetModel()->GetPath();
		return currentPath && std::string(currentPath) == aExpectedPath;
	}
}

Game::Game(std::shared_ptr<Scene> scene) : myScene(scene)
{
	REGISTER_COMPONENT(CarController);
	REGISTER_COMPONENT(FollowCameraComponent);

	auto size = DX11Framework::GetResolution();
	input_rect = { 0,0, (float)size.X, (float)size.Y };

	for (auto* mi : myScene->GetModelList())
	{
		myGameObjects.push_back(GameObject(mi));
	}
}

void Game::Init(float x, float y, float w, float h)
{
	input_rect = { x, y, w, h };

	myGameObjects.clear();

	auto findCarAndWheels = [&](ModelInstance*& outCar, ModelInstance* outWheels[4], bool aRequireExpectedMeshes)
		{
			outCar = nullptr;
			outWheels[0] = outWheels[1] = outWheels[2] = outWheels[3] = nullptr;
			for (auto* obj : myScene->GetModelList())
			{
				std::string n = obj->GetName();
				if (n == "Car")
				{
					if (!aRequireExpectedMeshes || IsExpectedModel(obj, kCarModelPath))
					{
						outCar = obj;
					}
				}
				else if (n == "WheelFL")
				{
					if (!aRequireExpectedMeshes || IsExpectedModel(obj, kWheelModelPath)) outWheels[0] = obj;
				}
				else if (n == "WheelFR")
				{
					if (!aRequireExpectedMeshes || IsExpectedModel(obj, kWheelModelPath)) outWheels[1] = obj;
				}
				else if (n == "WheelBL")
				{
					if (!aRequireExpectedMeshes || IsExpectedModel(obj, kWheelModelPath)) outWheels[2] = obj;
				}
				else if (n == "WheelBR")
				{
					if (!aRequireExpectedMeshes || IsExpectedModel(obj, kWheelModelPath)) outWheels[3] = obj;
				}
			}
		};

	auto removeVehicleLikeObjects = [&]()
		{
			for (auto* obj : myScene->GetModelList())
			{
				if (!obj) continue;
				const std::string n = obj->GetName();
				if (n == "Car" || n == "WheelFL" || n == "WheelFR" || n == "WheelBL" || n == "WheelBR")
				{
					myScene->RemoveModelInstance(obj);
				}
			}
		};

	ModelInstance* carBody = nullptr;
	ModelInstance* wheels[4] = {};
	findCarAndWheels(carBody, wheels, false);

	const bool hasCompleteVehicle =
		carBody && wheels[0] && wheels[1] && wheels[2] && wheels[3];

	const bool hasCarController =
		carBody && (carBody->GetComponent<CarController>() != nullptr
		         || carBody->GetComponent<RaycastVehicleController>() != nullptr);

	if (!hasCompleteVehicle || !hasCarController)
	{
		removeVehicleLikeObjects();
		Prefab prefab;
		const Vector3f spawnXZ = { 0.0f, 0.0f, 0.0f };
		const float terrainY = PhysicsWorld::Get().GetTerrainHeight(spawnXZ.X, spawnXZ.Z);
		const Vector3f spawnPos = { spawnXZ.X, terrainY + 1.0f, spawnXZ.Z };

		auto spawned = prefab.Spawn(myScene.get(), "Prefabs/Car.prefab", spawnPos);
		if (spawned.empty())
		{
			spawned = prefab.Spawn(myScene.get(), "../ExampleGame/Assets/Prefabs/Car.prefab", spawnPos);
		}
		if (spawned.empty())
		{
			spawned = prefab.Spawn(myScene.get(), "Prefabs/Car.json", spawnPos);
		}
		if (spawned.empty())
		{
			spawned = prefab.Spawn(myScene.get(), "../ExampleGame/Assets/Prefabs/Car.json", spawnPos);
		}
		findCarAndWheels(carBody, wheels, true);
		if (!carBody)
		{
			OutputDebugStringA("[ExampleGame] Failed to spawn car prefab from all fallback paths.\n");
		}
	}
	else
	{
		for (auto* obj : myScene->GetModelList())
		{
			if (!obj) continue;
			const std::string n = obj->GetName();
			if ((n == "Car" && obj != carBody) ||
				(n == "WheelFL" && obj != wheels[0]) ||
				(n == "WheelFR" && obj != wheels[1]) ||
				(n == "WheelBL" && obj != wheels[2]) ||
				(n == "WheelBR" && obj != wheels[3]))
			{
				myScene->RemoveModelInstance(obj);
			}
		}
	}

	if (carBody)
	{
		carBody->SetTexture(kCarTexturePath);
		for (auto* wheel : wheels)
		{
			if (wheel)
			{
				wheel->SetTexture(kCarTexturePath);
			}
		}

		if (auto* ctrl = carBody->GetComponent<CarController>())
		{
			ctrl->SetWheels(wheels[0], wheels[1], wheels[2], wheels[3]);
		}
		if (auto* ctrl = carBody->GetComponent<RaycastVehicleController>())
		{
			ctrl->SetWheels(wheels[0], wheels[1], wheels[2], wheels[3]);
		}
		if (auto* cam = carBody->GetComponent<FollowCameraComponent>())
		{
			cam->SetScene(myScene.get());
		}
	}

	for (auto* mi : myScene->GetModelList())
	{
		myGameObjects.push_back(GameObject(mi));
	}
}

void Game::Deinit()
{
	myGameObjects.clear();
}

void Game::update()
{
	if (myGameObjects.size() <= 0)
	{
		return;
	}

	if (MouseClicked)
	{
		MouseClicked = false;
		POINT point;
		GetCursorPos(&point);

		auto viewport_size = DX11Framework::GetResolution();
		point.x -= input_rect.x;
		point.y -= input_rect.y;

		float sx = DX11Framework::GetResolution().X / (input_rect.w - input_rect.x);
		float sy = DX11Framework::GetResolution().Y / (input_rect.h - input_rect.y);

		int32_t id = GraphicsEngine::MouseOver(point.x * sx, point.y * sy);
		if (id >= 0)
		{
			for (int i = 0; i < myGameObjects.size(); i++)
			{
				GameObject& go = myGameObjects[i];

				if (go.GetID() == id)
				{
					go.ToggleSelect();

					int left = i - 1;
					int top = i - 4;
					int right = i + 1;
					int bottom = i + 4;

					if (left >= 0 && left % 4 < id % 4)
					{
						myGameObjects[left].ToggleSelect();
					}
					if (right < myGameObjects.size() && right % 4 > id % 4)
					{
						myGameObjects[right].ToggleSelect();
					}
					if (top >= 0)
					{
						myGameObjects[top].ToggleSelect();
					}
					if (bottom < myGameObjects.size())
					{
						myGameObjects[bottom].ToggleSelect();
					}
				}
			}
		}
	}
}