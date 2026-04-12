#pragma once
#include <Engine.h>

class Scene;

class FollowCameraComponent : public ScriptComponent
{
public:
	FollowCameraComponent();
	void OnStart() override;
	void Update(float aDeltaTime) override;

	// Called by Game::Init() to give the component access to the scene camera.
	void SetScene(Scene* aScene) { myScene = aScene; }

	float myDistance = 8.0f;  // Distance behind the car
	float myHeight   = 3.5f;  // Height above the car
	float myLagSpeed = 6.0f;  // Higher = snappier follow, lower = more lag

private:
	Scene* myScene = nullptr;
};
