#pragma once

#include <memory>
#include <vector>

#include <Engine.h>

#include <Math/Math.hpp>
#include <Math/Vector3.h>
#include <Math/Transform.h>
#include <Models/ModelInstance.h>

class Scene;

namespace SelectState
{
	const Vector3f selected{ 0.5f, 0.5f, 0.5f };
	const Vector3f unselected{ 1.5f, 1.5f, 1.5f };
}

class GameObject
{
public:
	GameObject(ModelInstance* aMI) : myMI(aMI)
	{
		if (myMI)
		{
			mySelectedScale = myMI->GetTransform().GetScale();
		}
		myUnselectedScale = mySelectedScale * 0.25f;
	}
	
	void ToggleSelect()
	{
		if (!myMI) return;
		mySelected = myMI->GetTransform().GetScale() == mySelectedScale;
		myMI->SetScale(mySelected ? myUnselectedScale : mySelectedScale);
	}

	int32_t GetID() const { return myMI ? myMI->GetID() : -1; }

private:
	ModelInstance* myMI = nullptr;
	Vector3f myUnselectedScale;
	Vector3f mySelectedScale;
	bool mySelected = false;
};

class Game
{
public:
	Game(std::shared_ptr<Scene> scene);
	~Game() {}

	void Init(float x, float y, float w, float h);
	void Deinit();

	void SetInput(int anInput) { myInput = anInput; }

	void update();
	std::shared_ptr<Scene> scene() { return myScene; }

	bool MouseClicked = false;

private:
	std::shared_ptr<Scene> myScene;
	std::vector<GameObject> myGameObjects;
	int myInput = -1;
	struct rect
	{
		float x, y, w, h;
	} input_rect;
};
