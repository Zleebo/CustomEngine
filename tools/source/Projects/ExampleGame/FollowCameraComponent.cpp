#include "FollowCameraComponent.h"

#include <Scene/Scene.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <cmath>

FollowCameraComponent::FollowCameraComponent()
{
	EXPOSE_FLOAT_EX("Distance",  myDistance,  0.1f, 1.0f,  50.0f);
	EXPOSE_FLOAT_EX("Height",    myHeight,    0.1f, 0.0f,  20.0f);
	EXPOSE_FLOAT_EX("Lag Speed", myLagSpeed,  0.1f, 0.1f,  30.0f);
}

void FollowCameraComponent::OnStart()
{
}

void FollowCameraComponent::Update(float aDeltaTime)
{
	if (!myScene || !myOwner) return;

	Camera* camera = myScene->GetActiveCameraMutable();
	if (!camera) return;

	constexpr float kPi = 3.14159265358979f;
	constexpr float kDegToRad = kPi / 180.0f;
	constexpr float kRadToDeg = 180.0f / kPi;

	const Vector3f carPos = myOwner->GetLocation();
	const float yawDeg    = myOwner->GetTransform().GetRotation().Yaw;
	const float yawRad    = yawDeg * kDegToRad;

	// Desired camera position: directly behind and above the car
	const Vector3f behind = { -std::sinf(yawRad), 0.0f, -std::cosf(yawRad) };
	const Vector3f desiredPos = {
		carPos.X + behind.X * myDistance,
		carPos.Y + myHeight,
		carPos.Z + behind.Z * myDistance
	};

	// Exponential lag toward desired position
	const float alpha = 1.0f - std::expf(-myLagSpeed * aDeltaTime);
	const Vector3f curPos = camera->GetTransform().GetPosition();
	const Vector3f smoothPos = {
		curPos.X + (desiredPos.X - curPos.X) * alpha,
		curPos.Y + (desiredPos.Y - curPos.Y) * alpha,
		curPos.Z + (desiredPos.Z - curPos.Z) * alpha
	};

	// Look at a point slightly above the car centre
	const Vector3f lookTarget = { carPos.X, carPos.Y + 1.0f, carPos.Z };
	const float dx = lookTarget.X - smoothPos.X;
	const float dy = lookTarget.Y - smoothPos.Y;
	const float dz = lookTarget.Z - smoothPos.Z;

	const float lookYaw   =  std::atan2f(dx, dz) * kRadToDeg;
	const float horizDist =  std::sqrtf(dx * dx + dz * dz);
	const float lookPitch = -std::atan2f(dy, horizDist) * kRadToDeg;

	camera->set_position(smoothPos);
	camera->set_rotation(Rotator(lookPitch, lookYaw, 0.0f));
}
